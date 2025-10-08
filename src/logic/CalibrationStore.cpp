#include "CalibrationStore.h"
#include <Preferences.h>
#include "../storage/ProfileStore.h"

namespace {
  Preferences prefs;
  const char* NS = "cfg"; // той самий namespaces, що й інші налаштування
  bool inited = false;
  void ensure() { if (!inited) { prefs.begin(NS, false); inited = true; } }

  // дефолти
  const float DEF_BT[7] = {40,50,60,70,80,90,100};
  const float DEF_BP[7] = { 0,10,20,35,50,75,100};
  const float DEF_HR[7] = {20,30,40,50,60,70,80};
  const float DEF_HM[7] = {20,30,40,50,60,70,80};
}

int CalibrationStore::activeProfile() {
  return ProfileStore::currentIndex();
}

void CalibrationStore::loadBlower(int idx, BlowerTable* out) {
  if (!out) return;
  ensure();
  char key[24];
  for (int i=0;i<7;i++) {
    snprintf(key, sizeof(key), "prof%d.bt%d", idx, i);
    out->t[i] = prefs.getFloat(key, DEF_BT[i]);
    snprintf(key, sizeof(key), "prof%d.bp%d", idx, i);
    out->p[i] = prefs.getFloat(key, DEF_BP[i]);
  }
}

void CalibrationStore::loadHumidity(int idx, HumidityTable* out) {
  if (!out) return;
  ensure();
  char key[24];
  for (int i=0;i<7;i++) {
    snprintf(key, sizeof(key), "prof%d.hr%d", idx, i);
    out->ref[i] = prefs.getFloat(key, DEF_HR[i]);
    snprintf(key, sizeof(key), "prof%d.hm%d", idx, i);
    out->raw[i] = prefs.getFloat(key, DEF_HM[i]);
  }
}

bool CalibrationStore::saveBlower(int idx, const BlowerTable* in) {
  if (!in) return true;
  ensure();
  bool ok = true; char key[24];
  for (int i=0;i<7;i++) {
    snprintf(key, sizeof(key), "prof%d.bt%d", idx, i);
    ok &= prefs.putFloat(key, in->t[i]) >= 0;
    snprintf(key, sizeof(key), "prof%d.bp%d", idx, i);
    ok &= prefs.putFloat(key, in->p[i]) >= 0;
  }
  return ok;
}

bool CalibrationStore::saveHumidity(int idx, const HumidityTable* in) {
  if (!in) return true;
  ensure();
  bool ok = true; char key[24];
  for (int i=0;i<7;i++) {
    snprintf(key, sizeof(key), "prof%d.hr%d", idx, i);
    ok &= prefs.putFloat(key, in->ref[i]) >= 0;
    snprintf(key, sizeof(key), "prof%d.hm%d", idx, i);
    ok &= prefs.putFloat(key, in->raw[i]) >= 0;
  }
  return ok;
}
