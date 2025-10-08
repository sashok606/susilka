#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <rom/rtc.h>
#include <soc/rtc.h>
#include <SPI.h>
#include <time.h>

#include "Config.h"
#include "sensors/TempDS18B20.h"
#include "sensors/Sht31TwoBus.h"
#include "sensors/Pzem004.h"
#include "logic/Control.h"
#include "storage/ProfileStore.h"
#include "logic/CalibrationStore.h" // залишено, якщо користуєшся
#include "web/WebUI.h"

// ===== Умовна підтримка WireGuard =====
#if defined(__has_include)
  #if __has_include(<WireGuard-ESP32.h>)
    #include <WireGuard-ESP32.h>
    #define HAVE_WG 1
  #else
    #define HAVE_WG 0
  #endif
#else
  // старші тулчейни можуть не мати __has_include
  #define HAVE_WG 0
#endif

#if HAVE_WG
static WireGuard wg;
#endif

WebUI WEB;

// ===== Kyiv timezone / NTP =====
static const char* TZ_EUROPE_KYIV = "EET-2EEST,M3.5.0/3,M10.5.0/4";

// ==== helpers (логування) ===============================================
static const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT (external)";
    case ESP_RST_SW:        return "SW (software)";
    case ESP_RST_PANIC:     return "PANIC (crash)";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "???";
  }
}

static void logChipInfo() {
  esp_chip_info_t c{};
  esp_chip_info(&c);

  Serial.println(F("=== CHIP INFO ==="));
  Serial.printf("Model: ESP32-S3, cores: %d, rev: %d\n", c.cores, c.revision);
  Serial.printf("Features: %s%s%s\n",
    (c.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
    (c.features & CHIP_FEATURE_BLE)      ? "BLE "  : "",
    (c.features & CHIP_FEATURE_BT)       ? "BT "   : "");
  Serial.printf("CPU Freq: %u MHz\n", (unsigned) getCpuFrequencyMhz());
  Serial.printf("Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM: %s, free: %u\n",
    ESP.getPsramSize() ? "YES" : "NO", ESP.getFreePsram());
  Serial.println();
}

static void logPinsConfig() {
  Serial.println(F("=== PINS (from CFG) ==="));
  Serial.printf("FAN1 (PWM/DC): %d\n", CFG::cfg.pins.fan1_pin);
  Serial.printf("FAN2 (DIGITAL): %d\n", CFG::cfg.pins.fan2_pin);
  Serial.printf("FAN3 (PWM/DC): %d\n", CFG::cfg.pins.fan3_pin);
  Serial.printf("SSR1 (DIGITAL): %d\n", CFG::cfg.pins.ssr1_pin);
  Serial.printf("SSR2 (DIGITAL center): %d\n", CFG::cfg.pins.ssr2_pin);
  Serial.printf("OneWire: %d\n", CFG::cfg.pins.onewire_pin);
  Serial.printf("PZEM RX/TX: %d / %d\n", CFG::cfg.pins.pzem_rx, CFG::cfg.pins.pzem_tx);
  Serial.println();
}

static void logProfileShort() {
  const auto &p = Control::profile();
  Serial.println(F("=== ACTIVE PROFILE (short) ==="));
  Serial.printf("Boiler: set=%.1f hyst=%.1f, FAN1[min=%d max=%d k=%.2f]\n",
    p.t_boiler_set, p.t_boiler_hyst, p.fan1_min, p.fan1_max, p.fan1_k);
  Serial.printf("Recirc: dT_on=%.1f dT_off=%.1f, Fan2 T_on=%.1f T_off=%.1f\n",
    p.dT_diff_on, p.dT_diff_off, p.t_fan2_on, p.t_fan2_off);
  Serial.printf("Exhaust(FAN3): RH_set=%.1f, min=%d step=%d max=%d\n",
    p.rh_set, p.fan3_min, p.fan3_step, p.fan3_max);
  Serial.printf("Center SSR: dT_on=%.1f dT_off=%.1f\n",
    p.ssr_dT_on, p.ssr_dT_off);
  Serial.printf("Phases/safety: test=%d s, ignition=%d s, dT_min=%.1f, t_min_air=%.1f, wood_end_check=%d s, t_prod_max=%.1f\n",
    p.test_sec, p.phase1_sec, p.dT_min, p.t_min_air, p.wood_end_check_sec, p.t_prod_max);
  Serial.println();
}

static const char* phaseStr(Phase ph) {
  switch (ph) {
    case Phase::IDLE:             return "IDLE";
    case Phase::START_TEST:       return "START_TEST";
    case Phase::PHASE1_IGNITION:  return "IGNITION";
    case Phase::PHASE2_RECIRC:    return "RECIRC";
    case Phase::PHASE3_DRYING:    return "DRYING";
    case Phase::PHASE4_WOOD_END:  return "WOOD_END";
    case Phase::FINISHED:         return "FINISHED";
    case Phase::FAULT:            return "FAULT";
    default:                      return "?";
  }
}

static String ipToStr(IPAddress ip) { return ip.toString(); }

static String nowLocalStr() {
  struct tm t{};
  char buf[40];
  if (getLocalTime(&t, 50)) {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return String(buf);
  }
  return String("n/a");
}

// ==== NTP ================================================================
static void initTimeNTP() {
  Serial.println(F("NTP: setting TZ and starting configTime()..."));
  setenv("TZ", TZ_EUROPE_KYIV, 1);
  tzset();
  configTime(0, 0, "time.google.com", "pool.ntp.org", "time.windows.com");

  struct tm t{};
  bool ok = false;
  for (int i = 0; i < 200; ++i) {    // до ~10 с (200 * 50 ms)
    if (getLocalTime(&t, 100)) { ok = true; break; }
    delay(50);
  }
  Serial.printf("NTP: %s; now=%s\n", ok ? "SYNCED" : "NOT SYNCED", nowLocalStr().c_str());
}

// ==== Wi-Fi ==============================================================
static void startWiFiFromConfig() {
  Serial.println(F("WiFi: setting mode AP+STA..."));
  WiFi.mode(WIFI_AP_STA);

  if (strlen(CFG::cfg.ap_ssid)) {
    bool apok = WiFi.softAP(CFG::cfg.ap_ssid, CFG::cfg.ap_pass);
    Serial.printf("WiFi AP: %s (%s), ip=%s\n",
      apok ? "UP" : "FAIL",
      CFG::cfg.ap_ssid,
      ipToStr(WiFi.softAPIP()).c_str());
  } else {
    Serial.println(F("WiFi AP: disabled (empty SSID in config)"));
  }

  if (strlen(CFG::cfg.sta_ssid)) {
    Serial.printf("WiFi STA: connecting to '%s'...\n", CFG::cfg.sta_ssid);
    WiFi.begin(CFG::cfg.sta_ssid, CFG::cfg.sta_pass);
  } else {
    Serial.println(F("WiFi STA: disabled (empty SSID in config)"));
  }

  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t i){
    Serial.printf("WiFi STA: GOT_IP %s\n", IPAddress(i.got_ip.ip_info.ip.addr).toString().c_str());
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t){
    Serial.println(F("WiFi STA: DISCONNECTED"));
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

// ==== SETUP ==============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("=            SUSHILKA BOOT             ="));
  Serial.println(F("========================================"));
  Serial.printf("Reset reason: %s\n", resetReasonStr(esp_reset_reason()));
  logChipInfo();

  // --- Load config
  Serial.println(F("CFG: begin/load..."));
  CFG::begin();
  CFG::load();
  logPinsConfig();

  // --- Wi-Fi first (AP/STA)
  startWiFiFromConfig();

  // --- NTP after Wi-Fi
  initTimeNTP();

#if HAVE_WG
  // --- WireGuard: згенерувати та показати ключі (без збереження)
  Serial.println(F("WG: generating keypair..."));
  String wgPriv = wg.generatePrivateKey();     // 44 base64 chars
  String wgPub  = wg.generatePublicKey(wgPriv);
  Serial.println(F("=== WireGuard Keys (ESP32) ==="));
  Serial.print (F("Private key: ")); Serial.println(wgPriv);
  Serial.print (F("Public  key: ")); Serial.println(wgPub);
  Serial.println(F("Put ESP32 PUBLIC key into MikroTik peer; put MikroTik PUBLIC key into device config later."));
#else
  Serial.println(F("WG: library not present -> skipping WireGuard key generation."));
#endif

  // --- Control
  Serial.println(F("CONTROL: begin()..."));
  Control::begin();

  // --- Sensors
  Serial.println(F("DS18B20: init..."));
  TempDS18B20::setPin(CFG::cfg.pins.onewire_pin);
  if (TempDS18B20::begin()) {
    TempDS18B20::setResolution(12);
    Serial.printf("DS18B20 OK, pin=%d, devices=%d\n",
                  TempDS18B20::pin(), TempDS18B20::deviceCount());
  } else {
    Serial.printf("DS18B20 NOT FOUND on pin %d\n", TempDS18B20::pin());
  }

  Serial.println(F("SHT31xTwoBus: init..."));
  bool sht_ok = SHT31xTwoBus::begin();
  Serial.printf("SHT31xTwoBus %s\n", sht_ok ? "OK" : "NOT FOUND");

  if (CFG::cfg.pins.pzem_rx && CFG::cfg.pins.pzem_tx) {
    Serial.printf("PZEM: init (rx=%d, tx=%d)...\n", CFG::cfg.pins.pzem_rx, CFG::cfg.pins.pzem_tx);
    if (PZEM::begin(CFG::cfg.pins.pzem_rx, CFG::cfg.pins.pzem_tx)) {
      Serial.println(F("PZEM OK"));
    } else {
      Serial.println(F("PZEM init FAILED"));
    }
  } else {
    Serial.println(F("PZEM: pins not set — skip"));
  }

  // --- Profiles
  Serial.println(F("ProfileStore: begin..."));
  ProfileStore::begin();
  int pc = ProfileStore::count();
  Serial.printf("ProfileStore: %d profiles\n", pc);
  if (pc == 0) {
    int idx = -1;
    if (ProfileStore::create("Default", idx)) {
      ProfileStore::setCurrentIndex(idx);
      Serial.println(F("ProfileStore: created default profile"));
    } else {
      Serial.println(F("ProfileStore: FAILED to create default profile!"));
    }
  }
  if (!ProfileStore::loadActiveToControl()) {
    Serial.println(F("ProfileStore: FAILED to load active profile to Control!"));
  } else {
    Serial.printf("ProfileStore: active index=%d\n", ProfileStore::currentIndex());
  }
  logProfileShort();

  // Якщо використовуєш калібрування — розкоментуй:
  // Serial.println(F("CalibrationStore: begin/useProfile..."));
  // CalibrationStore::begin();
  // CalibrationStore::useProfile(ProfileStore::currentIndex());

  // --- Web
  Serial.println(F("WEB: begin..."));
  WEB.begin();
  Serial.println(F("WEB: / routes are up"));

  Serial.println(F("SETUP: done."));
  Serial.printf("AP IP: %s | STA IP: %s\n",
                ipToStr(WiFi.softAPIP()).c_str(),
                ipToStr(WiFi.localIP()).c_str());
  Serial.println();
}

// ==== LOOP ===============================================================
static uint32_t lastLogMs = 0;

void loop() {
  // Sensors periodic work
  TempDS18B20::loop();
  SHT31xTwoBus::loop();

  // Build readings, with simple validation logs
  Readings r{};
  r.t_sensor = TempDS18B20::lastCelsius();
  r.t_top    = SHT31xTwoBus::tempC(0);
  r.t_bottom = SHT31xTwoBus::tempC(1);
  r.rh       = SHT31xTwoBus::humidity(0);

  if (isnan(r.t_sensor)) Serial.println(F("[WARN] t_sensor is NaN"));
  if (isnan(r.t_top))    Serial.println(F("[WARN] t_top is NaN"));
  if (isnan(r.t_bottom)) Serial.println(F("[WARN] t_bottom is NaN"));
  if (isnan(r.rh) || r.rh < 0 || r.rh > 100) {
    Serial.printf("[WARN] RH invalid: %.1f\n", r.rh);
  }

  // Send to control
  Control::onReadings(r);

  // Control tick
  Control::tick();

  // Web
  WEB.loop();

  // 1 Hz telemetry log
  uint32_t now = millis();
  if (now - lastLogMs >= 1000) {
    lastLogMs = now;

    const auto &st = Control::getStatus();
    const auto &p  = Control::profile();

    Serial.println(F("---- TICK ----"));
    Serial.printf("Time: %s | Uptime: %lus | Heap: %u\n",
                  nowLocalStr().c_str(), (unsigned)(now/1000), ESP.getFreeHeap());
    Serial.printf("WiFi AP=%s STA=%s | AP_IP=%s STA_IP=%s\n",
                  WiFi.softAPSSID().c_str(),
                  WiFi.SSID().c_str(),
                  ipToStr(WiFi.softAPIP()).c_str(),
                  ipToStr(WiFi.localIP()).c_str());

    Serial.printf("Readings: T_sensor=%.1f  T_top=%.1f  T_bottom=%.1f  RH=%.1f\n",
                  r.t_sensor, r.t_top, r.t_bottom, r.rh);

    Serial.printf("Phase: %s  note='%s'  fault=%s (%s)\n",
                  phaseStr(st.phase), (st.note ? st.note : "-"),
                  st.fault ? "YES" : "no",
                  st.fault ? st.fault_reason.c_str() : "-");

    Serial.printf("Outputs: FAN1=%u  FAN2=%u  FAN3=%u  SSR1=%s  SSR2=%s\n",
                  st.out.fan1, st.out.fan2, st.out.fan3,
                  st.out.ssr1 ? "ON" : "off",
                  st.out.ssr2 ? "ON" : "off");

    Serial.printf("Profile: BoilerSet=%.1f RH_set=%.1f | FAN1[min=%d max=%d k=%.2f] | FAN3[min=%d step=%d max=%d]\n",
                  p.t_boiler_set, p.rh_set,
                  p.fan1_min, p.fan1_max, p.fan1_k,
                  p.fan3_min, p.fan3_step, p.fan3_max);

    Serial.println();
  }

  // маленька пауза, щоб не забивати CPU
  delay(2);
}
