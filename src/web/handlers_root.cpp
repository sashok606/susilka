// src/web/handlers_root.cpp
#include "WebUI.h"
#include <WiFi.h>
#include "../sensors/TempDS18B20.h"
#include "../sensors/Sht31TwoBus.h"
#include "../sensors/Pzem004.h"
#include "../logic/Control.h"
#include "../storage/ProfileStore.h"
#include "../storage/PinsStore.h"
#include "../Config.h"
#include <ArduinoJson.h>

// ===================== UI (головна) =====================
void WebUI::_handleRoot() {
  String html;
  html.reserve(42000);
  html += F(
"<!doctype html><html lang=\"uk\"><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Сушарка — Панель керування</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:1200px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;margin:0 0 16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
"h1{margin:0 0 12px;font-size:22px} h2{margin:0 0 12px;font-size:18px;opacity:.9}"
".row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}"
"button,a.btn,select,input[type=number],input[type=range],input[type=text]{background:#0e1627;border:1px solid #2a3458;color:#e8eef9;border-radius:12px;padding:10px 12px}"
"button,a.btn{cursor:pointer}"
".ok{background:#1f7a5a;border-color:#2ca57a}.danger{background:#812e3f;border-color:#b34b60}.warn{background:#7a5d1f;border-color:#a07a2a}"
".kv{background:#0e1627;border:1px solid #202a49;border-radius:12px;padding:10px}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}"
".mono{font-family:ui-monospace,Menlo,Consolas,monospace}.small{font-size:12px;opacity:.75}.muted{opacity:.75}"
"label{display:block;margin:6px 0}"
".led{display:inline-block;width:14px;height:14px;border-radius:50%;margin-right:8px;border:2px solid #2a3458;background:#5a2430;box-shadow:0 0 0 2px rgba(0,0,0,.15) inset}"
".led.on{background:#1f7a5a;border-color:#2ca57a}"
".dotbtn{display:inline-flex;align-items:center;gap:10px;background:#0e1627;border:1px solid #2a3458;border-radius:20px;padding:6px 10px;user-select:none}"
".dot{width:16px;height:16px;border-radius:50%;background:#5a2430;border:2px solid #2a3458}"
".dot.on{background:#1f7a5a;border-color:#2ca57a}"
".gwrap{display:flex;gap:16px;flex-wrap:wrap;align-items:flex-start}"
".gauge{width:280px;height:180px;display:inline-flex;flex-direction:column;align-items:center;justify-content:flex-start}"
".gtitle{font-size:14px;opacity:.8;margin-bottom:4px}"
".gcanvas{width:260px;height:140px;background:transparent;display:block}"
".gvalue{margin-top:6px;font-size:20px;font-weight:600}"
"</style>"

"<div class=\"wrap\">"
" <div class=\"card\">"
"   <h1>Панель — поточний стан</h1>"
"   <div class=\"row\">"
"     <button class=\"ok\" onclick=\"fetch('/api/start',{method:'POST'}).then(poll)\">Старт</button>"
"     <button class=\"danger\" onclick=\"fetch('/api/stop',{method:'POST'}).then(poll)\">Стоп</button>"
"     <a class=\"btn\" href=\"/profiles\">Профілі</a>"
"     <a class=\"btn\" href=\"/profile\">Налаштування активного профілю</a>"
"     <a class=\"btn\" href=\"/pins\">Піни та PZEM</a>"
"     <a class=\"btn\" href=\"/settings\">Wi-Fi та ціна</a>"
"     <button class=\"warn\" onclick=\"fetch('/api/reboot',{method:'POST'})\">Перезавантажити</button>"
"   </div>"
"   <div class=\"grid\" style=\"margin-top:12px\">"
"     <div class=\"kv\"><b>Поточний профіль</b><div id=\"profname\">—</div></div>"
"     <div class=\"kv\"><b>Стадія процесу</b><div id=\"phase\">—</div><div class=\"small\" id=\"note\"></div></div>"
"     <div class=\"kv\"><b>Вентилятори (FAN1/FAN2/FAN3)</b><div id=\"fans\" class=\"mono\">—</div></div>"
"     <div class=\"kv\"><b>Мережа</b><div id=\"wifi\">—</div></div>"
"     <div class=\"kv\"><b>Час роботи</b><div id=\"uptime\">—</div></div>"
"     <div class=\"kv\"><b>Реле (SSR1/SSR2)</b>"
"       <div><span id=\"led_ssr1\" class=\"led\"></span>SSR1</div>"
"       <div style=\"margin-top:6px\"><span id=\"led_ssr2\" class=\"led\"></span>SSR2</div>"
"     </div>"
"   </div>"
" </div>"

" <div class=\"card\">"
"   <h2>Спідометри</h2>"
"   <div class=\"gwrap\">"
"     <div id=\"g_boiler\" class=\"gauge\"><div class=\"gtitle\">Котел</div><canvas class=\"gcanvas\" width=\"260\" height=\"140\"></canvas><div class=\"gvalue\" id=\"g_boiler_v\">—</div></div>"
"     <div id=\"g_top\"    class=\"gauge\"><div class=\"gtitle\">Верх</div><canvas class=\"gcanvas\" width=\"260\" height=\"140\"></canvas><div class=\"gvalue\" id=\"g_top_v\">—</div></div>"
"     <div id=\"g_bottom\" class=\"gauge\"><div class=\"gtitle\">Низ</div><canvas class=\"gcanvas\" width=\"260\" height=\"140\"></canvas><div class=\"gvalue\" id=\"g_bottom_v\">—</div></div>"
"     <div id=\"g_rh\"     class=\"gauge\"><div class=\"gtitle\">Вологість</div><canvas class=\"gcanvas\" width=\"260\" height=\"140\"></canvas><div class=\"gvalue\" id=\"g_rh_v\">—</div></div>"
"   </div>"
" </div>"

" <div class=\"card\">"
"   <h2>Електроживлення (PZEM-004)</h2>"
"   <div class=\"grid\">"
"     <div class=\"kv\"><b>Напруга</b><div id=\"v\">—</div></div>"
"     <div class=\"kv\"><b>Струм</b><div id=\"i\">—</div></div>"
"     <div class=\"kv\"><b>Потужність</b><div id=\"p\">—</div></div>"
"     <div class=\"kv\"><b>Спожита енергія</b><div id=\"kwh\">—</div></div>"
"     <div class=\"kv\"><b>Ціна, грн/кВт·год</b><div id=\"pk\">—</div><div class=\"small muted\">Змінити: у «Wi-Fi та ціна»</div></div>"
"     <div class=\"kv\"><b>Вартість роботи</b><div id=\"cost\">—</div></div>"
"   </div>"
" </div>"

" <div class=\"card\">"
"   <h2>Ручний режим</h2>"
"   <div class=\"row\"><label><input type=\"checkbox\" id=\"mmode\" onchange=\"sendManual()\"> Увімкнути ручний режим</label></div>"
"   <div class=\"row\" style=\"gap:24px\">"
"     <label style=\"min-width:260px\">Піддув (FAN1)"
"       <input type=\"range\" id=\"mf1\" min=\"0\" max=\"100\" value=\"0\" "
"              oninput=\"mv('mf1v',this.value)\" "
"              onpointerdown=\"lockUI()\" onpointerup=\"unlockUI()\"/></label>"
"     <span id=\"mf1v\">0</span>%"
"     <div class=\"dotbtn\" id=\"btn_fan2\" onclick=\"toggleFan2()\">"
"       <span class=\"dot\" id=\"dot_fan2\"></span>"
"       Рециркуляція / головний (FAN2)"
"     </div>"
"     <label style=\"min-width:260px\">Витяжка (FAN3)"
"       <input type=\"range\" id=\"mf3\" min=\"0\" max=\"100\" value=\"0\" "
"              oninput=\"mv('mf3v',this.value)\" "
"              onpointerdown=\"lockUI()\" onpointerup=\"unlockUI()\"/></label>"
"     <span id=\"mf3v\">0</span>%"
"   </div>"
"   <div class=\"row\" style=\"gap:24px;margin-top:8px\">"
"     <div class=\"dotbtn\" id=\"btn_ssr1\" onclick=\"toggleSSR(1)\"><span id=\"dot_ssr1\" class=\"dot\"></span>SSR1</div>"
"     <div class=\"dotbtn\" id=\"btn_ssr2\" onclick=\"toggleSSR(2)\"><span id=\"dot_ssr2\" class=\"dot\"></span>SSR2</div>"
"     <button onclick=\"sendManual()\">Застосувати</button>"
"   </div>"
" </div>"
"</div>"

/* ===== JS ===== */
"<script>"
"var uiLock=false, uiLockTimer=null;"
"function el(id){return document.getElementById(id);} "
"function mv(id,v){var x=el(id); if(x) x.textContent=v;} "
"const PHASES=['Очікування','Старт/тест','Розпал','Рециркуляція','Сушіння','Дрова закінчились','Завершено','Аварія'];"
"const GMIN=20, GMAX=120;"

/* ---- блокування оновлення ручних контролів під час перетягування ---- */ 
"function lockUI(){uiLock=true; if(uiLockTimer){clearTimeout(uiLockTimer);} }"
"function unlockUI(){uiLockTimer=setTimeout(function(){uiLock=false;},600);}"

/* ---- гейдж (температури зліва->вправо), та окремо гейдж RH 0..100 ---- */
"function drawGauge(canvas,value,min,max,gradA,gradB){"
"  if(!canvas) return;"
"  var ctx=canvas.getContext('2d'); var w=canvas.width, h=canvas.height;"
"  ctx.clearRect(0,0,w,h); var cx=w/2, cy=h*0.95, r=Math.min(w*0.42,h*0.9);"
"  var a0=Math.PI, a1=2*Math.PI;"
"  ctx.lineWidth=18; ctx.strokeStyle='#1f2b46'; ctx.beginPath(); ctx.arc(cx,cy,r,a0,a1,false); ctx.stroke();"
"  var grd=ctx.createLinearGradient(0,0,w,0); grd.addColorStop(0,gradA); grd.addColorStop(1,gradB);"
"  ctx.strokeStyle=grd; ctx.beginPath(); ctx.arc(cx,cy,r,a0,a1,false); ctx.stroke();"
"  var v=isNaN(value)?min:value; if(v<min)v=min; if(v>max)v=max;"
"  var ang=a0+(v-min)/(max-min)*(a1-a0);"
"  var r0=r*0.10, r1=r*0.85;"
"  ctx.lineWidth=4; ctx.strokeStyle='#dfe7ff';"
"  ctx.beginPath(); ctx.moveTo(cx+Math.cos(ang)*r0, cy+Math.sin(ang)*r0); ctx.lineTo(cx+Math.cos(ang)*r1, cy+Math.sin(ang)*r1); ctx.stroke();"
"  ctx.fillStyle='#dfe7ff'; ctx.beginPath(); ctx.arc(cx,cy,6,0,Math.PI*2); ctx.fill();"
"}"

"function ensureCanvas(boxId){var box=el(boxId); if(!box) return null; var c=box.querySelector('canvas'); return c;}"
"function updateGauges(j){"
"  drawGauge(ensureCanvas('g_boiler'), Number(j.t1_c), GMIN, GMAX, '#f6c442', '#e33a2f');"
"  drawGauge(ensureCanvas('g_top'),    Number(j.t_sht1_c), GMIN, GMAX, '#f6c442', '#e33a2f');"
"  drawGauge(ensureCanvas('g_bottom'), Number(j.t_sht2_c), GMIN, GMAX, '#f6c442', '#e33a2f');"
"  drawGauge(ensureCanvas('g_rh'),     Number(j.rh_sht1),  0, 100,  '#7fd3ff', '#1a3b8f');"
"  var v1=(j.t1_c==null?'—':(Number(j.t1_c).toFixed(1)+' °C'));"
"  var v2=(j.t_sht1_c==null?'—':(Number(j.t_sht1_c).toFixed(1)+' °C'));"
"  var v3=(j.t_sht2_c==null?'—':(Number(j.t_sht2_c).toFixed(1)+' °C'));"
"  var v4=(j.rh_sht1==null?'—':(Number(j.rh_sht1).toFixed(1)+' %'));"
"  if(el('g_boiler_v')) el('g_boiler_v').textContent=v1;"
"  if(el('g_top_v'))    el('g_top_v').textContent=v2;"
"  if(el('g_bottom_v')) el('g_bottom_v').textContent=v3;"
"  if(el('g_rh_v'))     el('g_rh_v').textContent=v4;"
"}"

/* ---- керування індикаторами/кнопками ---- */
"function setDot(id,on){var d=el(id); if(!d) return; if(on){d.classList.add('on');} else {d.classList.remove('on');}}"
"function toggleFan2(){var d=el('dot_fan2'); var on=d && d.classList.contains('on'); setDot('dot_fan2',!on); sendManual();}"
"function toggleSSR(which){"
"  if(which==1){ setDot('dot_ssr1', !(el('dot_ssr1').classList.contains('on')) ); }"
"  else { setDot('dot_ssr2', !(el('dot_ssr2').classList.contains('on')) ); }"
"  sendManual();"
"}"

/* ---- опитування ---- */
"async function poll(){"
"  try{"
"    var r=await fetch('/api/state',{cache:'no-store'});"
"    var j=await r.json();"
"    if(el('profname')) el('profname').textContent = (j.profile_name?j.profile_name:'—');"
"    if(el('phase')) el('phase').textContent = ((j.phase>=0&&j.phase<8)?PHASES[j.phase]:'—');"
"    if(el('note')) el('note').textContent  = ((j.manual_mode?'РУЧНИЙ РЕЖИМ ':'') + (j.note?j.note:''));"
"    if(el('fans')) el('fans').textContent = ((j.fan1==null?'—':j.fan1)+' / '+(j.fan2==null?'—':j.fan2)+' / '+(j.fan3==null?'—':j.fan3)+' %');"
"    if(el('wifi')) el('wifi').textContent = ((j.wifi_mode?j.wifi_mode:'')+' | STA: '+(j.ip_sta?j.ip_sta:'-')+' | AP: '+(j.ip_ap?j.ip_ap:'-')+' | RSSI: '+(j.rssi==null?'-':j.rssi)+' dBm');"
"    if(el('uptime')) el('uptime').textContent = ((j.uptime_s==null)?0:j.uptime_s)+' с';"
"    setDot('led_ssr1', !!j.ssr1); setDot('led_ssr2', !!j.ssr2);"
"    updateGauges(j);"

"    /* ==== PZEM UI ==== */"
"    if(el('v'))   el('v').textContent   = (j.pzem_v==null ? '—' : (Number(j.pzem_v).toFixed(1)+' В'));"
"    if(el('i'))   el('i').textContent   = (j.pzem_i==null ? '—' : (Number(j.pzem_i).toFixed(2)+' А'));"
"    if(el('p'))   el('p').textContent   = (j.pzem_p==null ? '—' : (Number(j.pzem_p).toFixed(1)+' Вт'));"
"    if(el('kwh')) el('kwh').textContent = (j.pzem_kwh==null? '—' : (Number(j.pzem_kwh).toFixed(3)+' кВт·год'));"
"    if(el('pk'))  el('pk').textContent  = (j.price_kwh==null? '—' : (Number(j.price_kwh).toFixed(2)+' грн/кВт·год'));"
"    if(el('cost'))el('cost').textContent= (j.cost_total==null? '—' : (Number(j.cost_total).toFixed(2)+' грн'));"

"    /* ручні елементи не перетираємо, якщо користувач тягне */"
"    if(!uiLock){"
"      if(el('mmode')) el('mmode').checked = !!j.manual_mode;"
"      if(el('mf1')) { el('mf1').value=(j.mf1==null?0:j.mf1); mv('mf1v',el('mf1').value); }"
"      if(el('mf3')) { el('mf3').value=(j.mf3==null?0:j.mf3); mv('mf3v',el('mf3').value); }"
"      setDot('dot_fan2', (j.mf2!=null && j.mf2>=50));"
"      setDot('dot_ssr1', !!j.ms1);"
"      setDot('dot_ssr2', !!j.ms2);"
"    }"
"  }catch(e){ console.warn('state/poll error',e); }"
"}"

/* ---- відправка ручного режиму ---- */
"async function sendManual(){"
"  var body={"
"    manual: (el('mmode')?el('mmode').checked:false),"
"    f1: (el('mf1')?+el('mf1').value:0),"
"    f2: (el('dot_fan2')&&el('dot_fan2').classList.contains('on'))?100:0,"
"    f3: (el('mf3')?+el('mf3').value:0),"
"    s1: (el('dot_ssr1')&&el('dot_ssr1').classList.contains('on')),"
"    s2: (el('dot_ssr2')&&el('dot_ssr2').classList.contains('on'))"
"  };"
"  try{ await fetch('/api/manual',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}catch(_){ }"
"}"

/* ---- цикл ---- */
"setInterval(poll,1000); poll();"
"</script>"
  );
  _srv.send(200, "text/html; charset=utf-8", html);
}

// ===================== API (state) =====================
void WebUI::_handleState() {
  const bool staUp = (WiFi.status() == WL_CONNECTED);

  // Сенсори
  const float t1 = TempDS18B20::lastCelsius();
  const bool ok0 = SHT31xTwoBus::hasData(0);
  const bool ok1 = SHT31xTwoBus::hasData(1);
  const float t0 = SHT31xTwoBus::tempC(0);
  const float h0 = SHT31xTwoBus::humidity(0);
  const float tB = SHT31xTwoBus::tempC(1);
  const float hB = SHT31xTwoBus::humidity(1);

  // PZEM
  const float v   = PZEM::voltage();
  const float i   = PZEM::current();
  const float pw  = PZEM::power();
  const float kwh = PZEM::energy_kwh();

  // Контролер
  const auto& st = Control::getStatus();

  // Профіль (ім’я, якщо є)
  int curIdx = ProfileStore::currentIndex();
  ProfileStore::Item itm; String pname="—";
  if (ProfileStore::loadOne(curIdx, itm)) pname = itm.name;

  // Розрахунок вартості
  const float price = CFG::cfg.price_kwh;
  const float cost  = (isnan(kwh) || isnan(price)) ? NAN : (kwh * price);

  StaticJsonDocument<1024> d;

  // Wi-Fi
  d["wifi_mode"] = staUp ? "АП+СТА" : "АП (STA не підключено)";
  d["ip_sta"]    = staUp ? WiFi.localIP().toString() : "-";
  d["ip_ap"]     = WiFi.softAPIP().toString();
  d["rssi"]      = staUp ? WiFi.RSSI() : 0;

  // Профіль/стадія
  d["profile_name"] = pname;
  d["phase"]        = (int)st.phase;
  d["note"]         = st.note;
  d["fault"]        = st.fault;
  d["fault_reason"] = st.fault_reason;

  // Сенсори
  if (!isnan(t1)) d["t1_c"] = t1;
  if (ok0) { d["t_sht1_c"] = t0; d["rh_sht1"] = h0; }
  if (ok1) { d["t_sht2_c"] = tB; d["rh_sht2"] = hB; }

  // Виходи (фактичні)
  d["fan1"] = st.out.fan1;
  d["fan2"] = st.out.fan2;
  d["fan3"] = st.out.fan3;
  d["ssr1"] = st.out.ssr1;
  d["ssr2"] = st.out.ssr2;

  // Ручний режим (для ініціалізації UI)
  d["manual_mode"] = st.manual_mode;
  d["mf1"] = st.manual_out.fan1;
  d["mf2"] = st.manual_out.fan2;
  d["mf3"] = st.manual_out.fan3;
  d["ms1"] = st.manual_out.ssr1;
  d["ms2"] = st.manual_out.ssr2;

  // PZEM + ціна
  if (!isnan(v))   d["pzem_v"]   = v;
  if (!isnan(i))   d["pzem_i"]   = i;
  if (!isnan(pw))  d["pzem_p"]   = pw;
  if (!isnan(kwh)) d["pzem_kwh"] = kwh;
  d["price_kwh"]   = price;
  if (!isnan(cost)) d["cost_total"] = cost;

  d["uptime_s"] = millis()/1000;

  String j; serializeJson(d, j);
  _srv.send(200, "application/json", j);
}

// ===================== API (cmd) =====================
void WebUI::_handleStart() { Control::cmdStart(); _srv.send(200,"application/json","{\"ok\":true,\"state\":\"started\"}"); }
void WebUI::_handleStop()  { Control::cmdStop();  _srv.send(200,"application/json","{\"ok\":true,\"state\":\"stopped\"}"); }

void WebUI::_handleManual(){
  // Очікуємо JSON: {manual:bool, f1,f2,f3:0..100, s1,s2:bool}
  if (!_srv.hasArg("plain")) { _srv.send(400,"application/json","{\"ok\":false,\"err\":\"no_body\"}"); return; }

  StaticJsonDocument<256> d;
  DeserializationError err = deserializeJson(d, _srv.arg("plain"));
  if (err) { _srv.send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

  bool manual = d["manual"] | false;
  int  f1     = d["f1"]     | 0;
  int  f2     = d["f2"]     | 0;   // 0 або 100
  int  f3     = d["f3"]     | 0;
  bool s1     = d["s1"]     | false;
  bool s2     = d["s2"]     | false;

  f1 = constrain(f1,0,100);
  f2 = (f2>=50)?100:0;
  f3 = constrain(f3,0,100);

  Control::setManual(manual,(uint8_t)f1,(uint8_t)f2,(uint8_t)f3,s1,s2);
  _srv.send(200,"application/json","{\"ok\":true}");
}
