#pragma once
#include <Arduino.h>

namespace PZEM {
  bool  begin(int rx_pin, int tx_pin);  // ініціалізація по RX/TX
  float voltage();     // V
  float current();     // A
  float power();       // W
  float energy_kwh();  // kWh (накопичено)
  float frequency();   // Hz
  float pf();          // power factor
}
