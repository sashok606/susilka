#include "PinsStore.h"
#include <Preferences.h>

namespace {
  Preferences pref;
  const char* NS = "pins";
  template<typename T>
  T getOr(const char* k, T defv){ return pref.isKey(k) ? (T)pref.getInt(k, defv) : defv; }
}

void PinsStore::begin(){ pref.begin(NS, false); }

PinsCfg PinsStore::get(){
  PinsCfg p;
  p.fan1_pwm = getOr("fan1_pwm", p.fan1_pwm);
  p.fan2_pwm = getOr("fan2_pwm", p.fan2_pwm);
  p.fan3_pwm = getOr("fan3_pwm", p.fan3_pwm);
  p.ssr1     = getOr("ssr1",     p.ssr1);
  p.ssr2     = getOr("ssr2",     p.ssr2);
  p.i2c0_sda = getOr("i2c0_sda", p.i2c0_sda);
  p.i2c0_scl = getOr("i2c0_scl", p.i2c0_scl);
  p.i2c1_sda = getOr("i2c1_sda", p.i2c1_sda);
  p.i2c1_scl = getOr("i2c1_scl", p.i2c1_scl);
  p.onewire  = getOr("onewire",  p.onewire);
  return p;
}

bool PinsStore::save(const PinsCfg& p){
  bool ok = true;
  ok &= pref.putInt("fan1_pwm", p.fan1_pwm) >= 0;
  ok &= pref.putInt("fan2_pwm", p.fan2_pwm) >= 0;
  ok &= pref.putInt("fan3_pwm", p.fan3_pwm) >= 0;
  ok &= pref.putInt("ssr1",     p.ssr1)     >= 0;
  ok &= pref.putInt("ssr2",     p.ssr2)     >= 0;
  ok &= pref.putInt("i2c0_sda", p.i2c0_sda) >= 0;
  ok &= pref.putInt("i2c0_scl", p.i2c0_scl) >= 0;
  ok &= pref.putInt("i2c1_sda", p.i2c1_sda) >= 0;
  ok &= pref.putInt("i2c1_scl", p.i2c1_scl) >= 0;
  ok &= pref.putInt("onewire",  p.onewire)  >= 0;
  return ok;
}
