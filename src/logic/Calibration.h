#pragma once
#include <Arduino.h>
#include "CalibrationStore.h"

// Лінійна інтерполяція вгору/вниз з насиченням по краях.
namespace Calibration {

  // % піддуву з T котла (за активним профілем)
  float blowerPercentFromTemp(float tempC);

  // Скоригована вологість із сирого виміру (активний профіль):
  // маємо таблицю REF (еталон) ↔ RAW (датчик), шукаємо corrected = f(measured_raw)
  float humidityCorrected(float measuredRaw);

  // Простий регулятор FAN3: кроком змінює оберти до цілі
  int fan3DutyByHumidity(float rhCorrected,
                         float rhSet,
                         int currentDuty,
                         int minDuty,
                         int maxDuty,
                         int step,
                         float deadband = 0.5f);
}
// === RH calibration mapping (raw -> corrected) ===
namespace Calibration {

// Повертає скориговану RH у % за даними з CalibrationStore (активний профіль)
float correctHumidity(float raw);

// Перечитати таблиці калібровок для активного профілю (коли профіль змінили)
void refreshActiveCalibration();

} // namespace Calibration
