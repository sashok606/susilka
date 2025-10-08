#pragma once
#include <Arduino.h>

namespace TempDS18B20 {
  void setPin(int gpio);   // ← нове
  bool begin();
  void setResolution(uint8_t bits);
  void loop();
  float lastCelsius();
  int   deviceCount();
  int   pin();             // ← опційно, для логів
}