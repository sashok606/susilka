#include "Sht31.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>

namespace {
  Adafruit_SHT31 sht31;
  bool inited = false;
  bool valid  = false;
  float tC = NAN, rh = NAN;
  uint32_t lastMs = 0;
  const uint32_t PERIOD_MS = 1000;
}

namespace SHT31x {

bool begin(uint8_t addr) {
  Wire.begin(PIN_SDA, PIN_SCL); // ← ТІ САМІ піни, що в IDE
  Wire.setClock(100000);        // 100 кГц для стабільності
  inited = sht31.begin(addr);   // спершу 0x44 (дефолт)
  if (!inited && addr != 0x45) {
    inited = sht31.begin(0x45); // пробуємо 0x45, якщо треба
  }
  if (!inited) return false;
  sht31.heater(false);
  return true;
}

void loop() {
  if (!inited) return;
  const uint32_t now = millis();
  if (now - lastMs < PERIOD_MS) return;
  lastMs = now;

  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  if (!isnan(t) && !isnan(h) && h >= 0.0f && h <= 100.0f) {
    tC = t; rh = h; valid = true;
  } else {
    valid = false;
  }
}

bool  hasData()   { return inited && valid; }
float lastTempC() { return tC; }
float lastRH()    { return rh; }

} // namespace SHT31x
