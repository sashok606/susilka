#pragma once
#include <Arduino.h>

// ===== Фази =====
enum class Phase : uint8_t {
  IDLE = 0,
  START_TEST,
  PHASE1_IGNITION,
  PHASE2_RECIRC,
  PHASE3_DRYING,
  PHASE4_WOOD_END,
  FINISHED,
  FAULT
};

// ===== Профіль налаштувань =====
struct ProfileCfg {
  // Start/Test
  uint16_t test_sec = 5;

  // Phase1 — розпал
  uint16_t phase1_sec = 300; // 5 хв
  float dT_min = 3.0f;

  // Phase2 — ΔT top-bottom (для рециркуляції)
  float dT_diff_on  = 2.0f;
  float dT_diff_off = 1.0f;

  // Phase3 — RH → FAN3
  float   rh_set   = 65.0f;
  uint8_t fan3_min = 20;
  uint8_t fan3_step= 10;
  uint8_t fan3_max = 100;

  // «Дрова скінчились» / мінімальна робоча T повітря у камері
  float    t_min_air          = 40.0f;
  uint16_t wood_end_check_sec = 300;

  // Перегрів
  float t_prod_max = 80.0f;

  // ===== Котел (DS18B20) / головний двигун =====
  float t_boiler_set  = 65.0f;  // ціль котла, °C
  float t_boiler_hyst = 2.0f;   // гістерезис, °C

  // FAN1 (піддув) — P-регулятор
  uint8_t fan1_min = 30;        // %
  uint8_t fan1_max = 100;       // %
  float   fan1_k   = 5.0f;      // % на кожен °C браку

  // FAN2 (головний/рециркуляція) — пороги по T котла
  float t_fan2_on  = 45.0f;     // ON
  float t_fan2_off = 40.0f;     // OFF

  float ssr_dT_on  = 3.0f;  // вмикати SSR, якщо |Tверх - Tниз| > ssr_dT_on
  float ssr_dT_off = 1.0f;  // вимикати SSR, якщо |Tверх - Tниз| < ssr_dT_off
};

// ===== Поточні вимірювання =====
struct Readings {
  float t_sensor = NAN;   // DS18B20 (котел)
  float t_top    = NAN;   // SHT top
  float t_bottom = NAN;   // SHT bottom
  float rh       = NAN;   // RH (верхній датчик)
};

// ===== Виходи =====
struct Outputs {
  uint8_t fan1 = 0; // піддув
  uint8_t fan2 = 0; // головний/рециркуляція
  uint8_t fan3 = 0; // витяжка
  bool ssr1    = false;
  bool ssr2    = false;
};

// ===== Статус =====
struct Status {
  Phase       phase = Phase::IDLE;
  const char* note  = "Очікування";
  bool        fault = false;
  String      fault_reason;
  Outputs     out;

  bool     manual_mode = false;
  Outputs  manual_out; // значення у ручному режимі
};

// ===== API контролера =====
namespace Control {
  // ініціалізація (PWM/виходи в 0)
  void begin();

  // прив'язати пін-аут під час роботи (PWM для FAN1..3 і два SSR)
  void applyPins(int fan1_pin, int fan2_pin, int fan3_pin, int ssr1_pin, int ssr2_pin);

  // машина станів
  void tick();

  // нові дані сенсорів
  void onReadings(const Readings&);

  // команди
  void cmdStart(); // START
  void cmdStop();  // STOP

  // ручний режим
  void setManual(bool enable,
                 uint8_t f1, uint8_t f2, uint8_t f3,
                 bool s1, bool s2);

  // статус (для веб-UI) та профіль
  const Status& getStatus();
  ProfileCfg&   profile();
    // ... інше вже є
  static void stepHumidityControl();   // крок регулювання FAN3 за RH

}
