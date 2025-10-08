#include "Config.h"
#include <Preferences.h>

namespace {
  Preferences pref;
  const char* NS = "cfg";

  // Wi-Fi дефолти
  const char* DEF_STA_SSID = "www.redline.net.ua";
  const char* DEF_STA_PASS = "11223344";
  const char* DEF_AP_SSID  = "DRYER_AP";
  const char* DEF_AP_PASS  = "12345678";

  static String getOrStr(const char* key, const char* defVal) {
    if (pref.isKey(key)) return pref.getString(key, defVal);
    return String(defVal);
  }
}

AppConfig CFG::cfg = {};

void CFG::begin() { pref.begin(NS, false); }

void CFG::load() {
  // ---- Wi-Fi
  String sta_ssid = getOrStr("sta_ssid", DEF_STA_SSID);
  String sta_pass = getOrStr("sta_pass", DEF_STA_PASS);
  String ap_ssid  = getOrStr("ap_ssid",  DEF_AP_SSID);
  String ap_pass  = getOrStr("ap_pass",  DEF_AP_PASS);

  strlcpy(cfg.sta_ssid, sta_ssid.c_str(), sizeof(cfg.sta_ssid));
  strlcpy(cfg.sta_pass, sta_pass.c_str(), sizeof(cfg.sta_pass));
  strlcpy(cfg.ap_ssid,  ap_ssid.c_str(),  sizeof(cfg.ap_ssid));
  strlcpy(cfg.ap_pass,  ap_pass.c_str(),  sizeof(cfg.ap_pass));

  // ---- Піни
  cfg.pins.fan1_pin    = pref.getInt("pin_fan1",    cfg.pins.fan1_pin);
  cfg.pins.fan2_pin    = pref.getInt("pin_fan2",    cfg.pins.fan2_pin);
  cfg.pins.fan3_pin    = pref.getInt("pin_fan3",    cfg.pins.fan3_pin);
  cfg.pins.ssr1_pin    = pref.getInt("pin_ssr1",    cfg.pins.ssr1_pin);
  cfg.pins.ssr2_pin    = pref.getInt("pin_ssr2",    cfg.pins.ssr2_pin);

  cfg.pins.onewire_pin = pref.getInt("pin_onew",    cfg.pins.onewire_pin);

  cfg.pins.i2c0_sda    = pref.getInt("i2c0_sda",    cfg.pins.i2c0_sda);
  cfg.pins.i2c0_scl    = pref.getInt("i2c0_scl",    cfg.pins.i2c0_scl);
  cfg.pins.i2c1_sda    = pref.getInt("i2c1_sda",    cfg.pins.i2c1_sda);
  cfg.pins.i2c1_scl    = pref.getInt("i2c1_scl",    cfg.pins.i2c1_scl);

  cfg.pins.pzem_rx     = pref.getInt("pzem_rx",     cfg.pins.pzem_rx);
  cfg.pins.pzem_tx     = pref.getInt("pzem_tx",     cfg.pins.pzem_tx);

  // ---- Енергетика
  cfg.energy.price_per_kwh = pref.getFloat("price_kwh", cfg.energy.price_per_kwh);
  cfg.price_kwh = cfg.energy.price_per_kwh;  // alias для зворотної сумісності

  // ---- VPN (WireGuard)
String wgpk = getOrStr("wg_priv", "");      strlcpy(cfg.wg_private_key, wgpk.c_str(), sizeof(cfg.wg_private_key));
String wgrm = getOrStr("wg_peerpub", "");   strlcpy(cfg.wg_peer_public, wgrm.c_str(), sizeof(cfg.wg_peer_public));
String wgep = getOrStr("wg_endpoint", "");  strlcpy(cfg.wg_endpoint, wgep.c_str(), sizeof(cfg.wg_endpoint));
String wga  = getOrStr("wg_ips", "");       strlcpy(cfg.wg_allowed_ips, wga.c_str(), sizeof(cfg.wg_allowed_ips));
cfg.wg_listen_port = pref.getUShort("wg_lport", cfg.wg_listen_port);

  // Якщо Wi-Fi ключів немає — записати дефолти
  bool missing = !(pref.isKey("sta_ssid") && pref.isKey("sta_pass") &&
                   pref.isKey("ap_ssid")  && pref.isKey("ap_pass"));
  if (missing) save();
}

bool CFG::save() {
  bool ok = true;
  // Wi-Fi
  ok &= pref.putString("sta_ssid", cfg.sta_ssid) >= 0;
  ok &= pref.putString("sta_pass", cfg.sta_pass) >= 0;
  ok &= pref.putString("ap_ssid",  cfg.ap_ssid)  >= 0;
  ok &= pref.putString("ap_pass",  cfg.ap_pass)  >= 0;

  // Піни / Енергетика
  ok &= savePins();
  ok &= saveEnergy();

  // VPN
  ok &= pref.putBool("wg_en",   cfg.vpn.enabled);
  ok &= pref.putString("wg_priv",    cfg.wg_private_key) >= 0;
  ok &= pref.putString("wg_peerpub", cfg.wg_peer_public) >= 0;
  ok &= pref.putString("wg_endpoint",cfg.wg_endpoint) >= 0;
  ok &= pref.putString("wg_ips",     cfg.wg_allowed_ips) >= 0;
  ok &= pref.putUShort("wg_lport",   cfg.wg_listen_port) >= 0;
  return ok;
}

bool CFG::savePins() {
  bool ok = true;
  ok &= pref.putInt("pin_fan1",    cfg.pins.fan1_pin)    >= 0;
  ok &= pref.putInt("pin_fan2",    cfg.pins.fan2_pin)    >= 0;
  ok &= pref.putInt("pin_fan3",    cfg.pins.fan3_pin)    >= 0;
  ok &= pref.putInt("pin_ssr1",    cfg.pins.ssr1_pin)    >= 0;
  ok &= pref.putInt("pin_ssr2",    cfg.pins.ssr2_pin)    >= 0;

  ok &= pref.putInt("pin_onew",    cfg.pins.onewire_pin) >= 0;

  ok &= pref.putInt("i2c0_sda",    cfg.pins.i2c0_sda)    >= 0;
  ok &= pref.putInt("i2c0_scl",    cfg.pins.i2c0_scl)    >= 0;
  ok &= pref.putInt("i2c1_sda",    cfg.pins.i2c1_sda)    >= 0;
  ok &= pref.putInt("i2c1_scl",    cfg.pins.i2c1_scl)    >= 0;

  ok &= pref.putInt("pzem_rx",     cfg.pins.pzem_rx)     >= 0;
  ok &= pref.putInt("pzem_tx",     cfg.pins.pzem_tx)     >= 0;
  return ok;
}

bool CFG::saveEnergy() {
  cfg.energy.price_per_kwh = cfg.price_kwh; // тримаємо alias у sync
  return pref.putFloat("price_kwh", cfg.energy.price_per_kwh) >= 0;
}

void CFG::resetToDefaults() {
  strlcpy(cfg.sta_ssid, "www.redline.net.ua", sizeof(cfg.sta_ssid));
  strlcpy(cfg.sta_pass, "11223344",           sizeof(cfg.sta_pass));
  strlcpy(cfg.ap_ssid,  "DRYER_AP",           sizeof(cfg.ap_ssid));
  strlcpy(cfg.ap_pass,  "12345678",           sizeof(cfg.ap_pass));

  // Очистити VPN до дефолтів
  cfg.vpn = VPNConfig{};

  // інші поля — як у дефолтах структур
  save();
}
