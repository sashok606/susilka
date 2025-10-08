#include "Sht31TwoBus.h"
#include <Wire.h>
#include "../Config.h"   // ← беремо піни I2C з глобального конфіга

namespace {
  // SHT31 адреса (обидва сенсори з адресою 0x44)
  const uint8_t SHT_ADDR = 0x44;

  bool inited[2] = {false, false};
  bool valid [2] = {false, false};
  float tC[2]    = {NAN, NAN};
  float rh[2]    = {NAN, NAN};

  uint32_t lastMs = 0;
  const uint32_t PERIOD_MS = 1000;

  // CRC8 для SHT3x (поліном 0x31, init 0xFF)
  uint8_t sht_crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
      crc ^= data[i];
      for (uint8_t b = 0; b < 8; b++) {
        if (crc & 0x80) crc = (crc << 1) ^ 0x31;
        else            crc <<= 1;
      }
    }
    return crc;
  }

  // Одноразове вимірювання: high repeatability, no clock stretching (0x2400)
  bool sht_measure(TwoWire& w, float& outT, float& outH) {
    // надіслати команду
    w.beginTransmission(SHT_ADDR);
    w.write(0x24);
    w.write(0x00);
    if (w.endTransmission() != 0) return false;

    // час конверсії ~15ms (макс ~20ms)
    delay(20);

    // читання 6 байт: T(MSB,LSB,CRC), RH(MSB,LSB,CRC)
    if (w.requestFrom((int)SHT_ADDR, 6) != 6) return false;
    uint8_t buf[6];
    for (int i = 0; i < 6; i++) buf[i] = w.read();

    if (sht_crc8(buf, 2) != buf[2]) return false;
    if (sht_crc8(buf + 3, 2) != buf[5]) return false;

    uint16_t rawT = (uint16_t(buf[0]) << 8) | buf[1];
    uint16_t rawH = (uint16_t(buf[3]) << 8) | buf[4];

    // формули з даташиту SHT3x
    outT = -45.0f + 175.0f * (float(rawT) / 65535.0f);
    outH = 100.0f * (float(rawH) / 65535.0f);

    // саніті-чек
    if (outH < 0.0f || outH > 100.0f) return false;
    return true;
  }
}

namespace SHT31xTwoBus {

bool begin() {
  // Ініт двох шин з пінів у глобальному конфігу
  Wire.begin (CFG::cfg.pins.i2c0_sda, CFG::cfg.pins.i2c0_scl);
  Wire.setClock(100000); // 100 кГц — стабільно
  Wire1.begin(CFG::cfg.pins.i2c1_sda, CFG::cfg.pins.i2c1_scl);
  Wire1.setClock(100000);

  // Пробне зчитування, щоб позначити, який датчик є
  float t, h;
  inited[0] = sht_measure(Wire,  t, h);   // якщо true — датчик присутній на шині Wire
  if (inited[0]) { tC[0] = t; rh[0] = h; valid[0] = true; }

  inited[1] = sht_measure(Wire1, t, h);   // якщо true — датчик присутній на шині Wire1
  if (inited[1]) { tC[1] = t; rh[1] = h; valid[1] = true; }

  return inited[0] || inited[1];
}

void loop() {
  const uint32_t now = millis();
  if (now - lastMs < PERIOD_MS) return;
  lastMs = now;

  float t, h;
  if (inited[0]) {
    if (sht_measure(Wire, t, h)) { tC[0] = t; rh[0] = h; valid[0] = true; }
    else                          valid[0] = false;
  }
  if (inited[1]) {
    if (sht_measure(Wire1, t, h)) { tC[1] = t; rh[1] = h; valid[1] = true; }
    else                           valid[1] = false;
  }
}

bool  hasData(int idx)   { return (idx>=0 && idx<2 && inited[idx] && valid[idx]); }
float tempC(int idx)     { return (idx>=0 && idx<2 ? tC[idx] : NAN); }
float humidity(int idx)  { return (idx>=0 && idx<2 ? rh[idx] : NAN); }

} // namespace SHT31xTwoBus
