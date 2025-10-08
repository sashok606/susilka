#pragma once
#include <Arduino.h>
#include <Wire.h>

inline String i2c_scan_to_string() {
  String out; out.reserve(200);
  out += "[I2C] scan: ";
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      if (found++) out += ", ";
      char b[6]; snprintf(b, sizeof(b), "0x%02X", addr);
      out += b;
    }
  }
  if (!found) out += "none";
  return out;
}
