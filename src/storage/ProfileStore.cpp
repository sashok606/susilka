#include "ProfileStore.h"
#include <ArduinoJson.h>
#include <vector>

namespace {
  Preferences pref;
  const char* NS = "profiles";

  bool readJson(const char* key, DynamicJsonDocument& doc) {
    if (!pref.isKey(key)) return false;
    String s = pref.getString(key, "");
    if (!s.length()) return false;
    return (deserializeJson(doc, s) == DeserializationError::Ok);
  }
void writeJson(const char* key, const DynamicJsonDocument& doc) {
  String s; 
  serializeJson(doc, s);
  pref.putString(key, s);
  pref.end();
  pref.begin(NS, false);   // флаш
}

  void ensureInit() {
  if (!pref.isKey("list")) {
    DynamicJsonDocument j(2048);
    JsonArray arr = j.createNestedArray("items");
    JsonObject a = arr.createNestedObject();
    a["name"] = "Default";
    JsonObject c = a.createNestedObject("cfg");
    ProfileCfg def;
    c["t_boiler_set"] = def.t_boiler_set;
    c["rh_set"]       = def.rh_set;

    c["ssr_dT_on"]  = def.ssr_dT_on;
    c["ssr_dT_off"] = def.ssr_dT_off;

    writeJson("list", j);
    pref.putInt("cur", 0);
    pref.end();
    pref.begin(NS,false);  // 🔥 флаш
  }
  if (!pref.isKey("cur")) pref.putInt("cur", 0);
}
}

void ProfileStore::begin() {
  pref.begin(NS, false);
  ensureInit();
}

int ProfileStore::count() {
  DynamicJsonDocument j(4096);
  if (!readJson("list", j)) return 0;
  JsonArray arr = j["items"].as<JsonArray>();
  return arr ? (int)arr.size() : 0;
}

int ProfileStore::currentIndex() { return pref.getInt("cur", 0); }

bool ProfileStore::setCurrentIndex(int idx) {
  int n = count(); if (idx < 0 || idx >= n) return false;
  pref.putInt("cur", idx);
  return true;
}

static bool loadAll(std::vector<ProfileStore::Item>& out){
  out.clear();
  DynamicJsonDocument j(16384);
  if (!readJson("list", j)) return false;
  JsonArray arr = j["items"].as<JsonArray>();
  if (arr.isNull()) return false;

  for (JsonObject it : arr) {                    // ← правильна ітерація по JSON-масиву
    ProfileStore::Item x;
    const char* nm = it["name"] | "Unnamed";     // ← дефолт через ArduinoJson «|»
    x.name = String(nm);

    ProfileCfg p;                                // значення за замовчуванням
    JsonObject c = it["cfg"];
    if (!c.isNull()) {
      // Start/Test + Phase1
      p.test_sec    = c["test_sec"]    | p.test_sec;
      p.phase1_sec  = c["phase1_sec"]  | p.phase1_sec;
      p.dT_min      = c["dT_min"]      | p.dT_min;

      // Phase2 (ΔT)
      p.dT_diff_on  = c["dT_diff_on"]  | p.dT_diff_on;
      p.dT_diff_off = c["dT_diff_off"] | p.dT_diff_off;

      // Phase3 (RH/FAN3)
      p.rh_set      = c["rh_set"]      | p.rh_set;
      p.fan3_min    = c["fan3_min"]    | p.fan3_min;
      p.fan3_step   = c["fan3_step"]   | p.fan3_step;
      p.fan3_max    = c["fan3_max"]    | p.fan3_max;

      // Wood end / guard
      p.t_min_air   = c["t_min_air"]   | p.t_min_air;
      p.wood_end_check_sec = c["wood_end_check_sec"] | p.wood_end_check_sec;
      p.t_prod_max  = c["t_prod_max"]  | p.t_prod_max;

      // Boiler / FAN1
      p.t_boiler_set  = c["t_boiler_set"]  | p.t_boiler_set;
      p.t_boiler_hyst = c["t_boiler_hyst"] | p.t_boiler_hyst;
      p.fan1_min      = c["fan1_min"]      | p.fan1_min;
      p.fan1_max      = c["fan1_max"]      | p.fan1_max;
      p.fan1_k        = c["fan1_k"]        | p.fan1_k;

      // FAN2 thresholds
      p.t_fan2_on   = c["t_fan2_on"]   | p.t_fan2_on;
      p.t_fan2_off  = c["t_fan2_off"]  | p.t_fan2_off;
      
      // SSR (вентилятор усередині)
      p.ssr_dT_on  = c["ssr_dT_on"]  | p.ssr_dT_on;
      p.ssr_dT_off = c["ssr_dT_off"] | p.ssr_dT_off;

    }

    x.cfg = p;
    out.push_back(x);
  }
  return true;
}

bool ProfileStore::loadOne(int idx, Item& out) {
  std::vector<Item> items;
  if (!loadAll(items)) return false;
  if (idx < 0 || idx >= (int)items.size()) return false;
  out = items[idx];
  return true;
}

static bool saveAll(const std::vector<ProfileStore::Item>& items){
  DynamicJsonDocument j(32768);
  JsonArray arr = j.createNestedArray("items");
  for (const auto &it : items) {
    JsonObject o = arr.createNestedObject();
    o["name"] = it.name;
    JsonObject c = o.createNestedObject("cfg");
    const ProfileCfg &p = it.cfg;
    c["test_sec"]=p.test_sec; c["phase1_sec"]=p.phase1_sec; c["dT_min"]=p.dT_min;
    c["dT_diff_on"]=p.dT_diff_on; c["dT_diff_off"]=p.dT_diff_off;
    c["rh_set"]=p.rh_set; c["fan3_min"]=p.fan3_min; c["fan3_step"]=p.fan3_step; c["fan3_max"]=p.fan3_max;
    c["t_min_air"]=p.t_min_air; c["wood_end_check_sec"]=p.wood_end_check_sec; c["t_prod_max"]=p.t_prod_max;
    c["t_boiler_set"]=p.t_boiler_set; c["t_boiler_hyst"]=p.t_boiler_hyst;
    c["fan1_min"]=p.fan1_min; c["fan1_max"]=p.fan1_max; c["fan1_k"]=p.fan1_k;
    c["t_fan2_on"]=p.t_fan2_on; c["t_fan2_off"]=p.t_fan2_off;
    // SSR (вентилятор усередині)
    c["ssr_dT_on"]  = p.ssr_dT_on;
    c["ssr_dT_off"] = p.ssr_dT_off;

  }
  writeJson("list", j);
  pref.end();
  pref.begin(NS, false);  // 🔥 флаш
  return true;
}

bool ProfileStore::saveOne(int idx, const Item& in) {
  std::vector<Item> items; if (!loadAll(items)) return false;
  if (idx < 0 || idx >= (int)items.size()) return false;
  items[idx] = in;
  return saveAll(items);
}

bool ProfileStore::create(const String& name, int& newIdx, const ProfileCfg* tpl) {
  std::vector<Item> items; if (!loadAll(items)) return false;
  Item it; it.name = name; it.cfg = tpl ? *tpl : ProfileCfg();
  items.push_back(it);
  if (!saveAll(items)) return false;
  newIdx = (int)items.size()-1;
  return true;
}

bool ProfileStore::rename(int idx, const String& name) {
  Item it; if (!loadOne(idx, it)) return false;
  it.name = name;
  return saveOne(idx, it);
}

bool ProfileStore::remove(int idx) {
  std::vector<Item> items; if (!loadAll(items)) return false;
  if (idx < 0 || idx >= (int)items.size()) return false;
  items.erase(items.begin()+idx);
  if (!saveAll(items)) return false;
  int cur = currentIndex();
  int n = (int)items.size();
  if (n == 0) { // якщо все видалили — створимо дефолт
    int newIdx=-1; create("Default", newIdx);
    setCurrentIndex(0);
  } else if (cur >= n) {
    setCurrentIndex(n-1);
  }
  return true;
}

bool ProfileStore::loadActiveToControl() {
  int idx = currentIndex();
  Item it; if (!loadOne(idx, it)) return false;
  Control::profile() = it.cfg;
  return true;
}

bool ProfileStore::saveControlToActive() {
  int idx = currentIndex();
  Item it; if (!loadOne(idx, it)) return false;
  it.cfg = Control::profile();
  return saveOne(idx, it);
}
