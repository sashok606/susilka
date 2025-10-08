#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../logic/Control.h"
#include <vector>

namespace ProfileStore {
  struct Item {
    String name;
    ProfileCfg cfg;
  };

  void begin();                           // ініціалізація NVS
  int  count();                           // кількість профілів
  int  currentIndex();                    // активний індекс
  bool setCurrentIndex(int idx);          // встановити активний

  bool loadOne(int idx, Item& out);       // прочитати один
  bool saveOne(int idx, const Item& in);  // зберегти один

  bool create(const String& name, int& newIdx, const ProfileCfg* tpl = nullptr);
  bool rename(int idx, const String& name);
  bool remove(int idx);

  bool loadActiveToControl();             // → Control::profile()
  bool saveControlToActive();             // ← Control::profile()
}
