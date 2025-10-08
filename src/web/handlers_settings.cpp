#include "WebUI.h"
#include <WiFi.h>
#include "../Config.h"

static String _esc2(const String& s){
  String out; out.reserve(s.length()+8);
  for (char c: s){
    if      (c=='<') out += F("&lt;");
    else if (c=='>') out += F("&gt;");
    else if (c=='&') out += F("&amp;");
    else if (c=='"') out += F("&quot;");
    else out += c;
  }
  return out;
}

void WebUI::_handleSettings() {
  String html = F(
"<!doctype html><html lang=\"uk\"><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Налаштування Wi-Fi, ціна та VPN</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:880px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;margin:0 0 16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
"label{display:block;margin-top:10px}"
"input{width:100%;background:#0e1627;border:1px solid #2a3458;color:#e8eef9;border-radius:12px;padding:10px}"
"button{margin-top:14px;padding:10px 14px;border:0;border-radius:12px;background:#3b82f6;color:#fff;cursor:pointer}"
"a.btn{display:inline-block;margin-top:12px;color:#e8eef9}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}"
".hint{opacity:.75;font-size:12px;margin-top:6px}"
".switch{display:flex;align-items:center;gap:10px;margin-top:10px}"
"</style>"
"<div class=\"wrap\"><div class=\"card\">"
"  <h1>Параметри Wi-Fi, ціни та VPN</h1>"
"  <form id=\"f\">"

"    <h2>Wi-Fi (STA/AP)</h2>"
"    <div class='grid'>"
"      <div><label>STA SSID <input name=\"sta_ssid\" maxlength=\"31\" required value=\"");
  html += _esc2(String(CFG::cfg.sta_ssid));
  html += F("\"></label></div>"
"      <div><label>STA Password <input name=\"sta_pass\" maxlength=\"63\" value=\"");
  html += _esc2(String(CFG::cfg.sta_pass));
  html += F("\"></label></div>"
"      <div><label>AP SSID  <input name=\"ap_ssid\" maxlength=\"31\" required value=\"");
  html += _esc2(String(CFG::cfg.ap_ssid));
  html += F("\"></label></div>"
"      <div><label>AP Password <input name=\"ap_pass\" maxlength=\"63\" value=\"");
  html += _esc2(String(CFG::cfg.ap_pass));
  html += F("\"></label></div>"
"    </div>"

"    <hr style='border-color:#1e2742;margin:16px 0'>"
"    <h2>Ціна електроенергії</h2>"
"    <div class='grid'>"
"      <div><label>грн за 1 кВт·год <input type=\"number\" step=\"0.01\" name=\"price_kwh\" value=\"");
  html += String(CFG::cfg.price_kwh, 2);
  html += F("\"></label></div>"
"    </div>"

"    <hr style='border-color:#1e2742;margin:16px 0'>"
"    <h2>VPN (WireGuard)</h2>"
"    <label class='switch'><input type='checkbox' id='wg_enabled' ");
  html += (CFG::cfg.vpn.enabled ? "checked" : "");
  html += F("> Увімкнути WireGuard</label>"
"    <div class='grid'>"
"      <div><label>Interface Address (наприклад 10.8.0.2/32)"
"        <input name='wg_addr' value='"); html += _esc2(String(CFG::cfg.vpn.address_cidr)); html += F("'></label></div>"
"      <div><label>DNS (необов'язково)"
"        <input name='wg_dns' value='"); html += _esc2(String(CFG::cfg.vpn.dns)); html += F("'></label></div>"
"      <div><label>Private Key (клієнта)"
"        <input name='wg_priv' value='"); html += _esc2(String(CFG::cfg.vpn.private_key)); html += F("'></label></div>"
"      <div><label>Peer Public Key (сервера)"
"        <input name='wg_peer' value='"); html += _esc2(String(CFG::cfg.vpn.peer_public_key)); html += F("'></label></div>"
"      <div><label>Endpoint Host"
"        <input name='wg_host' value='"); html += _esc2(String(CFG::cfg.vpn.endpoint_host)); html += F("'></label></div>"
"      <div><label>Endpoint Port"
"        <input name='wg_port' type='number' min='1' max='65535' value='"); html += String(CFG::cfg.vpn.endpoint_port); html += F("'></label></div>"
"    </div>"
"    <div class='hint'>Після збереження з увімкненим VPN — пристрій підніме тунель після перезавантаження.</div>"

"    <input type='hidden' name='wg_enabled' id='wg_enabled_hidden' value=''>"

"    <button type=\"submit\">Зберегти (та перезавантажити)</button>"
"  </form>"
"  <a class=\"btn\" href=\"/\">← Повернутися</a>"
"</div></div>"
"<script>"
"document.getElementById('f').addEventListener('submit', async (e)=>{"
"  e.preventDefault();"
"  const fd=new FormData(e.target);"
"  fd.set('wg_enabled', document.getElementById('wg_enabled').checked ? '1' : '0');"
"  const data=new URLSearchParams();"
"  for(const [k,v] of fd) data.append(k, String(v));"
"  const r = await fetch('/api/apply',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data});"
"  if(r.ok){ alert('Збережено. Перезавантаження...'); setTimeout(()=>location.href='/',2000); }"
"  else   { alert('Помилка збереження'); }"
"});"
"</script>"
);
  _srv.send(200, "text/html; charset=utf-8", html);
}

void WebUI::_handleApply() {
  if (!_srv.hasArg("sta_ssid") || !_srv.hasArg("ap_ssid")) {
    _srv.send(400, "application/json", "{\"ok\":false,\"err\":\"bad_form\"}");
    return;
  }
  // Wi-Fi
  strlcpy(CFG::cfg.sta_ssid, _srv.arg("sta_ssid").c_str(), sizeof(CFG::cfg.sta_ssid));
  strlcpy(CFG::cfg.sta_pass, _srv.arg("sta_pass").c_str(), sizeof(CFG::cfg.sta_pass));
  strlcpy(CFG::cfg.ap_ssid,  _srv.arg("ap_ssid").c_str(),  sizeof(CFG::cfg.ap_ssid));
  strlcpy(CFG::cfg.ap_pass,  _srv.arg("ap_pass").c_str(),  sizeof(CFG::cfg.ap_pass));

  // Ціна
  if (_srv.hasArg("price_kwh")) {
    CFG::cfg.price_kwh = _srv.arg("price_kwh").toFloat();
  }

  // VPN (WireGuard)
  if (_srv.hasArg("wg_enabled")) CFG::cfg.vpn.enabled = (_srv.arg("wg_enabled") == "1");
  if (_srv.hasArg("wg_addr"))    strlcpy(CFG::cfg.vpn.address_cidr,    _srv.arg("wg_addr").c_str(),   sizeof(CFG::cfg.vpn.address_cidr));
  if (_srv.hasArg("wg_dns"))     strlcpy(CFG::cfg.vpn.dns,             _srv.arg("wg_dns").c_str(),    sizeof(CFG::cfg.vpn.dns));
  if (_srv.hasArg("wg_priv"))    strlcpy(CFG::cfg.vpn.private_key,     _srv.arg("wg_priv").c_str(),   sizeof(CFG::cfg.vpn.private_key));
  if (_srv.hasArg("wg_peer"))    strlcpy(CFG::cfg.vpn.peer_public_key, _srv.arg("wg_peer").c_str(),   sizeof(CFG::cfg.vpn.peer_public_key));
  if (_srv.hasArg("wg_host"))    strlcpy(CFG::cfg.vpn.endpoint_host,   _srv.arg("wg_host").c_str(),   sizeof(CFG::cfg.vpn.endpoint_host));
  if (_srv.hasArg("wg_port"))    CFG::cfg.vpn.endpoint_port = (uint16_t)_srv.arg("wg_port").toInt();

  const bool ok = CFG::save();
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
  if (ok) { delay(300); ESP.restart(); }
}
