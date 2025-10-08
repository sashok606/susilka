#include "Pzem004.h"
#include <HardwareSerial.h>
#include <PZEM004Tv30.h>

// використаємо Serial1 (можеш поміняти за потреби)
#define PZSER Serial1

static PZEM004Tv30 pzem;       // дефолтний конструктор
static bool g_ok = false;

namespace PZEM {

bool begin(int rx_pin, int tx_pin){
  // Ініціалізуємо UART і під'єднуємо модуль
  PZSER.begin(9600, SERIAL_8N1, rx_pin, tx_pin);
  // Бібліотека очікує порт і піни в конструкторі:
  pzem = PZEM004Tv30(PZSER, (uint8_t)rx_pin, (uint8_t)tx_pin);
  // Перше читання як sanity-check
  float v = pzem.voltage();
  g_ok = !isnan(v);
  return g_ok;
}

float voltage()    { float v = pzem.voltage();    return isnan(v)?NAN:v; }
float current()    { float a = pzem.current();    return isnan(a)?NAN:a; }
float power()      { float w = pzem.power();      return isnan(w)?NAN:w; }
float energy_kwh() { float e = pzem.energy();     return isnan(e)?NAN:e; }
float frequency()  { float f = pzem.frequency();  return isnan(f)?NAN:f; }
float pf()         { float p = pzem.pf();         return isnan(p)?NAN:p; }

} // namespace
