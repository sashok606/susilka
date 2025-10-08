#pragma once
#include <Arduino.h>

struct PinsCfg {
  int fan1_pwm = 14; // піддув
  int fan2_pwm = 15; // рециркуляція/головний
  int fan3_pwm = 16; // витяжка
  int ssr1     = 23; // нагрів
  int ssr2     = 19; // дод. реле

  // I2C (для SHT31 дві шини — якщо треба буде редагувати)
  int i2c0_sda = 8;
  int i2c0_scl = 9;
  int i2c1_sda = 10;
  int i2c1_scl = 11;

  // DS18B20
  int onewire  = 4;
};

namespace PinsStore {
  void begin();                  // ініт NVS
  PinsCfg get();                 // прочитати (з дефолтами)
  bool   save(const PinsCfg&);   // зберегти
}
