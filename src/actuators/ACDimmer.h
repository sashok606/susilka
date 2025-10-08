#pragma once
#include <Arduino.h>

// Простой 2-канальный фазовый диммер для ESP32.
// Логика: по фронту zero-cross ставимо одновимірні таймери, які через
// затримку "delay_us[ch]" підпалюють симістор коротким імпульсом (~100 мкс).

class ACDimmer {
public:
  // zcPin: GPIO нульового переходу (інверсія допускається в констр-ре)
  // ch1Pin/ch2Pin: виходи на DIM1/DIM2 модуля
  // mainsHz: 50 або 60 (за потреби можна авто)
  static void begin(int zcPin, int ch1Pin, int ch2Pin, int mainsHz = 50, bool zcFalling = true);

  // Встановити відсотки для каналу (0..100). ch=0 -> DIM1 (FAN1), ch=1 -> DIM2 (FAN3)
  static void setPercent(uint8_t ch, uint8_t pct);

  // Опціонально: вказати мін/макс межі в мкс затримки (для калібрування лінійності)
  static void tuneDelays(uint16_t min_us, uint16_t max_us);

  static uint8_t getPercent(uint8_t ch);

private:
  static void IRAM_ATTR onZeroCrossISR();
  static void IRAM_ATTR onFire1ISR();
  static void IRAM_ATTR onFire2ISR();

  static void scheduleFiring();

  static volatile uint8_t  s_pct[2];
  static volatile uint32_t s_delay_us[2];
  static uint16_t          s_delayMin, s_delayMax; // межі сканування кута

  static int  s_pinZC, s_pinCh[2];
  static bool s_zcFalling;

  static hw_timer_t* s_tmrFire1;
  static hw_timer_t* s_tmrFire2;
  static portMUX_TYPE s_mux;

  static uint32_t s_halfPeriod_us; // 10'000 для 50 Гц, 8'333 для 60 Гц
};
