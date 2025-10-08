#pragma once
#include <Arduino.h>

namespace SHT31xTwoBus {
  bool begin();               // Ініт двох шин і двох датчиків (обидва @0x44)
  void loop();                // опитування 1 Гц
  bool hasData(int idx);      // idx: 0 -> Wire (I2C0), 1 -> Wire1 (I2C1)
  float tempC(int idx);
  float humidity(int idx);
}
