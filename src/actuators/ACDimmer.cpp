#include "ACDimmer.h"

volatile uint8_t  ACDimmer::s_pct[2]      = {0,0};
volatile uint32_t ACDimmer::s_delay_us[2] = {0,0};
uint16_t          ACDimmer::s_delayMin    = 200;   // мін. затримка (≈макс. потужність)
uint16_t          ACDimmer::s_delayMax    = 9000;  // макс. затримка (≈мін. потужність)

int  ACDimmer::s_pinZC   = -1;
int  ACDimmer::s_pinCh[2]= {-1,-1};
bool ACDimmer::s_zcFalling = true;

hw_timer_t* ACDimmer::s_tmrFire1 = nullptr;
hw_timer_t* ACDimmer::s_tmrFire2 = nullptr;

portMUX_TYPE ACDimmer::s_mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t ACDimmer::s_halfPeriod_us = 10000; // 50 Гц за замовчуванням

static inline uint32_t clamp32(uint32_t v, uint32_t lo, uint32_t hi){
  return (v<lo)?lo:((v>hi)?hi:v);
}

// Проста "майже лінійна" мапа: 0% ≈ max delay, 100% ≈ min delay.
// За бажанням можна зробити cos-профіль.
static inline uint32_t pctToDelay(uint8_t pct, uint16_t dmin, uint16_t dmax) {
  pct = (pct>100)?100:pct;
  // інвертуємо: 100% швидкість -> малий delay; 0% -> великий delay
  uint32_t span = (uint32_t)dmax - (uint32_t)dmin;
  uint32_t d = dmax - (span * pct) / 100;
  return clamp32(d, dmin, dmax);
}

void ACDimmer::begin(int zcPin, int ch1Pin, int ch2Pin, int mainsHz, bool zcFalling) {
  s_pinZC = zcPin;
  s_pinCh[0] = ch1Pin;
  s_pinCh[1] = ch2Pin;
  s_zcFalling = zcFalling;

  pinMode(s_pinZC, INPUT);
  pinMode(s_pinCh[0], OUTPUT);
  pinMode(s_pinCh[1], OUTPUT);
  digitalWrite(s_pinCh[0], LOW);
  digitalWrite(s_pinCh[1], LOW);

  s_halfPeriod_us = (mainsHz==60) ? 8333 : 10000;

  // Таймери для підпалювання симісторів (один на канал), 1 МГц (1 tick = 1 мкс)
  s_tmrFire1 = timerBegin(0, 80, true); // 80MHz/80 = 1MHz
  s_tmrFire2 = timerBegin(1, 80, true);

  timerAttachInterrupt(s_tmrFire1, &ACDimmer::onFire1ISR, true);
  timerAttachInterrupt(s_tmrFire2, &ACDimmer::onFire2ISR, true);

  // Переривання по нульовому переходу
  attachInterrupt(digitalPinToInterrupt(s_pinZC), &ACDimmer::onZeroCrossISR,
                  s_zcFalling ? FALLING : RISING);
}

void ACDimmer::tuneDelays(uint16_t min_us, uint16_t max_us){
  s_delayMin = min_us;
  s_delayMax = max_us;
}

void ACDimmer::setPercent(uint8_t ch, uint8_t pct){
  if (ch>1) return;
  portENTER_CRITICAL(&s_mux);
  s_pct[ch] = (pct>100)?100:pct;
  s_delay_us[ch] = pctToDelay(s_pct[ch], s_delayMin, s_delayMax);
  portEXIT_CRITICAL(&s_mux);
}

uint8_t ACDimmer::getPercent(uint8_t ch){
  if (ch>1) return 0;
  return s_pct[ch];
}

void IRAM_ATTR ACDimmer::onZeroCrossISR() {
  // На кожен нульовий перехід переплановуємо "постріли" для обох каналів
  scheduleFiring();
}

void IRAM_ATTR ACDimmer::scheduleFiring(){
  // Канал 1
  if (s_pinCh[0] >= 0) {
    timerWrite(s_tmrFire1, 0);
    timerAlarmWrite(s_tmrFire1, s_delay_us[0], true); // одноразово (переозброюємо кожен ZC)
    timerAlarmEnable(s_tmrFire1);
  }
  // Канал 2
  if (s_pinCh[1] >= 0) {
    timerWrite(s_tmrFire2, 0);
    timerAlarmWrite(s_tmrFire2, s_delay_us[1], true);
    timerAlarmEnable(s_tmrFire2);
  }
}

// Імпульс запалювання симістора: ~100 мкс HIGH
void IRAM_ATTR ACDimmer::onFire1ISR() {
  digitalWrite(s_pinCh[0], HIGH);
  // короткий "busy wait" тут прийнятний (100 мкс), щоб не виділяти ще таймери:
  for (volatile int i=0; i<3000; ++i) { asm volatile("nop"); } // ~100 мкс @240MHz/80
  digitalWrite(s_pinCh[0], LOW);
  timerAlarmDisable(s_tmrFire1);
}

void IRAM_ATTR ACDimmer::onFire2ISR() {
  digitalWrite(s_pinCh[1], HIGH);
  for (volatile int i=0; i<3000; ++i) { asm volatile("nop"); }
  digitalWrite(s_pinCh[1], LOW);
  timerAlarmDisable(s_tmrFire2);
}
