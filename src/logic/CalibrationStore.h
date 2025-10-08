#pragma once
#include <Arduino.h>

// Зберігаємо калібровки у Preferences з префіксом активного профілю.
// Формат ключів: "prof{idx}.bt0..6", "prof{idx}.bp0..6" (blower),
//                 "prof{idx}.hr0..6", "prof{idx}.hm0..6" (humidity).
namespace CalibrationStore {

  // 7-точкові таблиці
  struct BlowerTable {
    float t[7]; // °C
    float p[7]; // % (0..100)
  };

  struct HumidityTable {
    float ref[7]; // еталонна RH, %
    float raw[7]; // показ датчика RH, %
  };

  // Повертає індекс активного профілю (через ProfileStore)
  int activeProfile();

  // Завантажити таблиці (будь-який аргумент може бути nullptr)
  void loadBlower(int profileIdx, BlowerTable* out);     // з дефолтами
  void loadHumidity(int profileIdx, HumidityTable* out); // з дефолтами

  // Зберегти таблиці (будь-який аргумент може бути nullptr)
  bool saveBlower(int profileIdx, const BlowerTable* in);
  bool saveHumidity(int profileIdx, const HumidityTable* in);
}
