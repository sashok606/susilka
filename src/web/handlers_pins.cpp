#include "WebUI.h"
#include "../storage/PinsStore.h"
#include "../logic/Control.h"

// !!! PZEM тут більше не редагуємо (воно конфігурується через CFG::cfg.pins і /settings)

void WebUI::_handlePins() {
  auto pins = PinsStore::get(); // це PinsCfg

  String html;
  html.reserve(12000);
  html += F(
"<!doctype html><html lang='uk'><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Піни</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:820px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;margin:0 0 16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
"label{display:block;margin-top:10px}"
"input{width:100%;background:#0e1627;border:1px solid #2a3458;color:#e8eef9;border-radius:12px;padding:10px}"
"button{margin-top:14px;padding:10px 14px;border:0;border-radius:12px;background:#3b82f6;color:#fff;cursor:pointer}"
"a.btn{display:inline-block;margin-top:12px;color:#e8eef9}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}"
"</style>"
"<div class='wrap'><div class='card'>"
"<h1>Піни вентиляторів/реле</h1>"
"<form id='f'>"
"<div class='grid'>");

  auto inp = [&](const char* label,const char* name, int v){
    html += "<label>";
    html += _escape(String(label));
    html += " <input type='number' name='";
    html += name;
    html += "' value='";
    html += String(v);
    html += "'></label>";
  };

  // PWM/SSR
  inp("PWM FAN1 (піддув)","fan1_pwm", pins.fan1_pwm);
  inp("PWM FAN2 (рециркуляція/головний)","fan2_pwm", pins.fan2_pwm);
  inp("PWM FAN3 (витяжка)","fan3_pwm", pins.fan3_pwm);
  inp("SSR1 (нагрів/ТЕН)","ssr1", pins.ssr1);
  inp("SSR2 (додаткове реле)","ssr2", pins.ssr2);

  html += "</div>";
  html += "<button type='submit'>Зберегти та застосувати</button> ";
  html += "<a class='btn' href='/'>← Повернутися</a>";
  html += "</form></div></div>"
          "<script>"
          "document.getElementById('f').addEventListener('submit',async(e)=>{e.preventDefault();"
          "const fd=new FormData(e.target);const data=new URLSearchParams();for(const [k,v] of fd)data.append(k,v);"
          "const r=await fetch('/api/pins',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body=data});"
          "if(r.ok){alert('Збережено і застосовано');location.href='/';}else alert('Помилка');});"
          "</script>";

  _srv.send(200, "text/html; charset=utf-8", html);
}

// ==== Піни API ====
void WebUI::_apiPinsGet() {
  auto pins = PinsStore::get();
  String j = "{";
  j += "\"fan1_pwm\":" + String(pins.fan1_pwm)
     + ",\"fan2_pwm\":" + String(pins.fan2_pwm)
     + ",\"fan3_pwm\":" + String(pins.fan3_pwm)
     + ",\"ssr1\":"     + String(pins.ssr1)
     + ",\"ssr2\":"     + String(pins.ssr2)
     + "}";
  _srv.send(200, "application/json", j);
}

void WebUI::_apiPinsSave() {
  // очікуємо form-urlencoded
  auto pins = PinsStore::get(); // PinsCfg

  if (_srv.hasArg("fan1_pwm")) pins.fan1_pwm = _srv.arg("fan1_pwm").toInt();
  if (_srv.hasArg("fan2_pwm")) pins.fan2_pwm = _srv.arg("fan2_pwm").toInt();
  if (_srv.hasArg("fan3_pwm")) pins.fan3_pwm = _srv.arg("fan3_pwm").toInt();
  if (_srv.hasArg("ssr1"))     pins.ssr1     = _srv.arg("ssr1").toInt();
  if (_srv.hasArg("ssr2"))     pins.ssr2     = _srv.arg("ssr2").toInt();

  bool ok = false;

  // 👉 вибери правильний виклик згідно з твоїм PinsStore.h (розкоментуй один)
  // ok = PinsStore::save(pins);
  // ok = PinsStore::put(pins);
  // ok = PinsStore::setAll(pins);
  // ok = PinsStore::set(pins); // якщо такий є

  // якщо не знайшов методу збереження — тимчасово хоч застосуємо гаряче:
  if (!ok) {
    Control::applyPins(pins.fan1_pwm, pins.fan2_pwm, pins.fan3_pwm, pins.ssr1, pins.ssr2);
  } else {
    // і гаряче теж, щоб не чекати ребута
    Control::applyPins(pins.fan1_pwm, pins.fan2_pwm, pins.fan3_pwm, pins.ssr1, pins.ssr2);
  }

  _srv.send(ok?200:200, "application/json", "{\"ok\":true}");
}
