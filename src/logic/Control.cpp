#include "Control.h"
#include <Arduino.h>
#include <math.h>
#include "../Config.h"
#include "Calibration.h"         // для корекції вологості
#include "../actuators/ACDimmer.h"  // фазовий диммер для AC моторів

// ===== PWM / Pins (для DC-варіантів; залишаємо як резерв) =====
static const int PWM_FREQ = 25000;
static const int PWM_RES  = 8;
static const int CH_FAN1  = 0; // Піддув (PWM, якщо DC)
static const int CH_FAN3  = 2; // Витяжка (PWM, якщо DC)

// Поточні піни (можуть бути перевизначені у CFG::cfg.pins)
// ЗАУВАЖ: ці пін-и стосуються DC-керування ledc. Для AC-диммера — дивись ZC/DIM нижче.
static int PIN_FAN1 = 14; // PWM (DC варіант)
static int PIN_FAN2 = 15; // DIGITAL ON/OFF (рециркуляція)
static int PIN_FAN3 = 16; // PWM (DC варіант)
static int PIN_SSR1 = 23; // DIGITAL
static int PIN_SSR2 = 5; // DIGITAL (вентилятор усередині камери, окремий двигун)

// ===== AC-dimmer пін-и (ZeroCross і канали DIM1/DIM2) =====
// Підстав тут свої реальні GPIO, що йдуть на модуль диммера.
#define ZC_PIN    4   // нульовий перехід
#define DIM1_PIN  17  // канал 0 диммера -> FAN1 (піддув)
#define DIM2_PIN  18  // канал 1 диммера -> FAN3 (витяжка)

static ProfileCfg   g_prof;
static Readings     g_rd;
static Status       g_st;
static uint32_t     g_phaseSince    = 0;
static float        g_refT_onSSR1   = NAN;
static uint32_t     g_fan1FullSince = 0;

static inline uint32_t ms(){ return millis(); }
static inline bool elapsed(uint32_t s, uint32_t d){ return (ms() - s) >= d; }

// ---- guard для GPIO
static inline bool okPin(int p){ return p >= 0 && p <= 48; }
static bool chSetup[8] = {false,false,false,false,false,false,false,false};

// ---- helpers (ledc — залишено для DC-вентилів/сумісності)
static void pwmAttach(int ch, int pin){
  ledcSetup(ch, PWM_FREQ, PWM_RES);
  chSetup[ch] = true;
  if (okPin(pin)) ledcAttachPin(pin, ch);
}
static void pwmDetachPinSafe(int pin){
#if defined(ARDUINO) && defined(ESP32)
  #if ARDUINO_ESP32_MAJOR_VERSION >= 2
    if (okPin(pin)) ledcDetachPin(pin);
  #endif
#endif
}
static void pwmWritePct(int ch, uint8_t pct){
  if (!chSetup[ch]) return;
  uint32_t v = map(constrain(pct,0,100), 0,100, 0,255);
  ledcWrite(ch, v);
}
static void digitalOut(int pin, bool on){
  if (!okPin(pin)) return;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, on?HIGH:LOW);
}

// ---- Єдине місце, що виставляє виходи + оновлює g_st ----
// ТУТ переведено FAN1 та FAN3 на AC-диммер (ACDimmer::setPercent).
static void setOutputs(uint8_t f1, uint8_t f2, uint8_t f3, bool s1, bool s2){
  // FAN1 — піддув (AC канал диммера 0)
  ACDimmer::setPercent(0, f1);

  // FAN2 — рециркуляція (ON/OFF реле/SSR)
  if (okPin(PIN_FAN2)) {
    pinMode(PIN_FAN2, OUTPUT);
    digitalWrite(PIN_FAN2, (f2 >= 50) ? HIGH : LOW);
  }

  // FAN3 — витяжка (AC канал диммера 1)
  ACDimmer::setPercent(1, f3);

  // SSR1/SSR2 — цифрові виходи (окремі двигуни/навантаження)
  digitalOut(PIN_SSR1, s1);
  digitalOut(PIN_SSR2, s2);

  // Відзеркалюємо стан у статус
  g_st.out.fan1 = f1;
  g_st.out.fan2 = (f2 >= 50) ? 100 : 0;
  g_st.out.fan3 = f3;
  g_st.out.ssr1 = s1;
  g_st.out.ssr2 = s2;
}

static void setPhase(Phase p, const char* note){
  g_st.phase = p;
  g_st.note  = note;
  g_phaseSince = ms();
}

static void fault(const String& why){
  g_st.fault = true; g_st.fault_reason = why;
  setOutputs(100,100,100,false,false);
  setPhase(Phase::FAULT, "Аварія");
}

void Control::begin(){
  // Якщо у конфігу задані інші піни — підставляємо
  if (CFG::cfg.pins.fan1_pin) PIN_FAN1 = CFG::cfg.pins.fan1_pin;
  if (CFG::cfg.pins.fan2_pin) PIN_FAN2 = CFG::cfg.pins.fan2_pin;
  if (CFG::cfg.pins.fan3_pin) PIN_FAN3 = CFG::cfg.pins.fan3_pin;
  if (CFG::cfg.pins.ssr1_pin) PIN_SSR1 = CFG::cfg.pins.ssr1_pin;
  if (CFG::cfg.pins.ssr2_pin) PIN_SSR2 = CFG::cfg.pins.ssr2_pin;

  // 1) Ініціалізуємо AC-диммер (FAN1/FAN3 як AC)
  ACDimmer::begin(ZC_PIN, DIM1_PIN, DIM2_PIN, 50 /*Гц*/, true /* falling */);
  // За потреби підкрути межі затримок:
  // ACDimmer::tuneDelays(200, 9000);

  // 2) Ініціалізація DC-PWM каналів (на випадок, якщо один з вентів — DC)
  pwmAttach(CH_FAN1, PIN_FAN1);
  pwmAttach(CH_FAN3, PIN_FAN3);

  // 3) SSR виходи
  if (okPin(PIN_SSR1)) pinMode(PIN_SSR1, OUTPUT);
  if (okPin(PIN_SSR2)) pinMode(PIN_SSR2, OUTPUT);

  setOutputs(0,0,0,false,false);
  g_st = {};
  setPhase(Phase::IDLE, "Очікування");
}

void Control::applyPins(int fan1_pin, int fan2_pin, int fan3_pin, int ssr1_pin, int ssr2_pin){
  // Відчепимо старі PWM, якщо були (для DC сумісності)
  pwmDetachPinSafe(PIN_FAN1);
  pwmDetachPinSafe(PIN_FAN3);

  PIN_FAN1 = fan1_pin;
  PIN_FAN2 = fan2_pin;
  PIN_FAN3 = fan3_pin;
  PIN_SSR1 = ssr1_pin;
  PIN_SSR2 = ssr2_pin;

  pwmAttach(CH_FAN1, PIN_FAN1);
  pwmAttach(CH_FAN3, PIN_FAN3);

  if (okPin(PIN_SSR1)) pinMode(PIN_SSR1, OUTPUT);
  if (okPin(PIN_SSR2)) pinMode(PIN_SSR2, OUTPUT);

  setOutputs(0,0,0,false,false);
}

void Control::onReadings(const Readings& r){ g_rd = r; }

void Control::cmdStart(){
  g_st.fault = false; g_st.fault_reason = "";
  g_st.manual_mode = false;
  // Під час старт-тесту: увімкнемо все для перевірки крім FAN3
  setOutputs(100,100,0,true,true);
  setPhase(Phase::START_TEST, "Старт/тест: перевірка виходів");
  g_refT_onSSR1 = NAN;
  g_fan1FullSince = 0;
}

void Control::cmdStop(){
  setOutputs(0,0,0,false,false);
  setPhase(Phase::IDLE, "Зупинено");
}

void Control::setManual(bool enable, uint8_t f1, uint8_t f2, uint8_t f3, bool s1, bool s2){
  g_st.manual_mode = enable;
  g_st.manual_out.fan1 = f1;
  g_st.manual_out.fan2 = f2;
  g_st.manual_out.fan3 = f3;
  g_st.manual_out.ssr1 = s1;
  g_st.manual_out.ssr2 = s2;
  if (enable) { setOutputs(f1,f2,f3,s1,s2); g_st.note = "РУЧНИЙ РЕЖИМ"; }
}

ProfileCfg&    Control::profile(){ return g_prof; }
const Status&  Control::getStatus(){ return g_st; }

static uint8_t clampPct(int v){ return (uint8_t)constrain(v, 0, 100); }

// --- ПІДДув (FAN1): підтримка T котла
static void control_fan1_boiler_hold(){
  if (isnan(g_rd.t_sensor)) return;
  float err = g_prof.t_boiler_set - g_rd.t_sensor;
  int base = (err > 0) ? (int)roundf(g_prof.fan1_min + g_prof.fan1_k * err)
                       : (int)g_prof.fan1_min;
  uint8_t f = clampPct(base);
  if (f < g_prof.fan1_min) f = g_prof.fan1_min;
  if (f > g_prof.fan1_max) f = g_prof.fan1_max;
  setOutputs(f, g_st.out.fan2, g_st.out.fan3, g_st.out.ssr1, g_st.out.ssr2);
}

// --- ВИТЯЖКА (FAN3): по вологості, з урахуванням калібровки
static void control_fan3_exhaust_by_rh(){
  float rh_raw = g_rd.rh;
  if (isnan(rh_raw) || rh_raw <= 0 || rh_raw > 100) return;
  float rh = Calibration::correctHumidity(rh_raw);

  float dh = rh - g_prof.rh_set;
  if (dh > 1.0f) {
    uint8_t f = min<uint8_t>(g_prof.fan3_max, g_st.out.fan3 + g_prof.fan3_step);
    if (f < g_prof.fan3_min) f = g_prof.fan3_min;
    setOutputs(g_st.out.fan1, g_st.out.fan2, f, g_st.out.ssr1, g_st.out.ssr2);
  } else if (dh < -1.0f) {
    uint8_t f = max<uint8_t>(g_prof.fan3_min, g_st.out.fan3 - g_prof.fan3_step);
    setOutputs(g_st.out.fan1, g_st.out.fan2, f, g_st.out.ssr1, g_st.out.ssr2);
  }
}

// --- SSR2 (вентилятор усередині): по ΔT тільки у фазі сушіння
static void update_center_ssr_by_delta(){
  bool s2 = g_st.out.ssr2;

  if (!isnan(g_rd.t_top) && !isnan(g_rd.t_bottom)) {
    float d = fabsf(g_rd.t_top - g_rd.t_bottom);
    // окремі пороги з профілю
    if (!s2 && d > g_prof.ssr_dT_on)   s2 = true;   // увімкнути при ΔT_on
    if (s2  && d < g_prof.ssr_dT_off)  s2 = false;  // вимкнути при ΔT_off
    if (s2 != g_st.out.ssr2) {
      setOutputs(g_st.out.fan1, g_st.out.fan2, g_st.out.fan3, g_st.out.ssr1, s2);
    }
  }
}

void Control::tick(){
  if (g_st.manual_mode) {
    setOutputs(g_st.manual_out.fan1, g_st.manual_out.fan2, g_st.manual_out.fan3,
               g_st.manual_out.ssr1, g_st.manual_out.ssr2);
    return;
  }

  switch (g_st.phase) {
    case Phase::IDLE:
      break;

    case Phase::START_TEST:
      if (elapsed(g_phaseSince, (uint32_t)g_prof.test_sec*1000)) {
        // Переходимо до розпалу: FAN2 вимикаємо, далі FAN1 на максимум
        setOutputs(100,0,0,true,true);
        g_refT_onSSR1 = g_rd.t_sensor;
        setPhase(Phase::PHASE1_IGNITION, "Розпал");
      }
      break;

    case Phase::PHASE1_IGNITION:
      if (elapsed(g_phaseSince, (uint32_t)g_prof.phase1_sec*1000)) {
        float dT = g_rd.t_sensor - g_refT_onSSR1;
        if (isnan(dT) || dT < g_prof.dT_min) fault("Немає приросту температури у фазі розпалу");
        else setPhase(Phase::PHASE2_RECIRC, "Рециркуляція");
      } else {
        setOutputs(100, 0, g_st.out.fan3, g_st.out.ssr1, g_st.out.ssr2);
      }
      break;

    case Phase::PHASE2_RECIRC:
      // 🔸 FAN2 — ПРАЦЮЄ ЗАВЖДИ у фазі рециркуляції
      if (g_st.out.fan2 != 100)
        setOutputs(g_st.out.fan1, 100, g_st.out.fan3, g_st.out.ssr1, g_st.out.ssr2);

      control_fan1_boiler_hold();

      if (elapsed(g_phaseSince, 60000)) {
        setPhase(Phase::PHASE3_DRYING, "Сушіння");
        setOutputs(g_st.out.fan1, 100, g_prof.fan3_min, g_st.out.ssr1, g_st.out.ssr2);
        g_fan1FullSince = (g_st.out.fan1==100) ? ms() : 0;
      }
      break;

    case Phase::PHASE3_DRYING: {
      // 🔸 FAN2 — ПРАЦЮЄ ЗАВЖДИ у фазі сушіння
      if (g_st.out.fan2 != 100)
        setOutputs(g_st.out.fan1, 100, g_st.out.fan3, g_st.out.ssr1, g_st.out.ssr2);

      control_fan3_exhaust_by_rh();
      control_fan1_boiler_hold();

      // SSR2 за ΔT усередині камери
      update_center_ssr_by_delta();

      if (g_st.out.fan1==100) { if (!g_fan1FullSince) g_fan1FullSince = ms(); }
      else g_fan1FullSince = 0;

      if (g_fan1FullSince && elapsed(g_fan1FullSince, (uint32_t)g_prof.wood_end_check_sec*1000)) {
        if (!isnan(g_rd.t_sensor) && g_rd.t_sensor < g_prof.t_min_air) {
          setOutputs(0,0,0,false,false);
          setPhase(Phase::PHASE4_WOOD_END, "Дрова закінчились");
        }
      }
      float t_guard = !isnan(g_rd.t_top) ? g_rd.t_top : g_rd.t_sensor;
      if (!isnan(t_guard) && t_guard > g_prof.t_prod_max) {
        setOutputs(100,100,100,false,false);
        g_st.note = "ПЕРЕГРІВ — охолодження";
      }
    } break;

    case Phase::PHASE4_WOOD_END:
      // все вимкнено; очікуємо команди
      break;

    case Phase::FINISHED:  break;
    case Phase::FAULT:     break;
  }
}
