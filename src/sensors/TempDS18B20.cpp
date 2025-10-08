#include "TempDS18B20.h"
#include <OneWire.h>
#include <DallasTemperature.h>

namespace {
  int owPin = 4;                 // дефолт; перезапишемо з CFG у setup()
  OneWire* oneWire = nullptr;
  DallasTemperature* sensors = nullptr;

  bool inited = false;
  int  devs = 0;

  enum State { IDLE, REQUESTED, READY };
  State st = IDLE;
  unsigned long tRequest = 0;
  unsigned long convTimeMs = 750;

  float last = NAN;
  uint8_t currentResolution = 12;

  void ensureBuses(){
    if (!oneWire) oneWire = new OneWire(owPin);
    if (!sensors) sensors = new DallasTemperature(oneWire);
  }
}

namespace TempDS18B20 {

void setPin(int gpio){
  if (gpio == owPin) return;
  owPin = gpio;
  if (oneWire) { delete oneWire; oneWire = nullptr; }
  if (sensors) { delete sensors; sensors = nullptr; }
  inited = false;
}

int pin(){ return owPin; }

bool begin() {
  ensureBuses();
  sensors->begin();
  devs = sensors->getDeviceCount();
  sensors->setWaitForConversion(false);
  setResolution(12);
  inited = (devs > 0);
  return inited;
}

void setResolution(uint8_t bits) {
  if (bits < 9) bits = 9;
  if (bits > 12) bits = 12;
  currentResolution = bits;
  sensors->setResolution(bits);
  switch (bits) {
    case 9:  convTimeMs = 94;  break;
    case 10: convTimeMs = 188; break;
    case 11: convTimeMs = 375; break;
    case 12: default: convTimeMs = 750; break;
  }
}

void loop() {
  if (!inited) return;
  unsigned long now = millis();
  switch (st) {
    case IDLE:
      sensors->requestTemperatures();
      tRequest = now;
      st = REQUESTED;
      break;
    case REQUESTED:
      if (now - tRequest >= convTimeMs) {
        float t = sensors->getTempCByIndex(0);
        if (t > -127.0f && t < 125.0f) last = t;
        st = READY;
      }
      break;
    case READY:
      if (now - tRequest >= convTimeMs + 250) st = IDLE;
      break;
  }
}

float lastCelsius() { return last; }
int   deviceCount() { return devs; }

} // namespace
