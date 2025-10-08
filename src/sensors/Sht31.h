#pragma once
#include <Arduino.h>

namespace SHT31x {
  #ifndef I2C_SDA
  #define I2C_SDA 8
  #endif
  #ifndef I2C_SCL
  #define I2C_SCL 9
  #endif

  static constexpr uint8_t PIN_SDA = static_cast<uint8_t>(I2C_SDA);
  static constexpr uint8_t PIN_SCL = static_cast<uint8_t>(I2C_SCL);

  bool begin(uint8_t i2c_addr = 0x44);
  void loop();
  bool hasData();
  float lastTempC();
  float lastRH();
}
