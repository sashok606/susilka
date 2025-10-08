#include "WebUI.h"
#include <WiFi.h>
#include <time.h>
#include <esp_timer.h>
#include "../Config.h"
#include "../storage/ProfileStore.h"
#include "../logic/Control.h"
#include "../logic/CalibrationStore.h"
using namespace CalibrationStore;

// ------------ helpers ------------
String WebUI::_escape(const String& s){
  String out; out.reserve(s.length()+8);
  for (char c : s) {
    if      (c=='<') out += F("&lt;");
    else if (c=='>') out += F("&gt;");
    else if (c=='&') out += F("&amp;");
    else if (c=='"') out += F("&quot;");
    else out += c;
  }
  return out;
}

// ------------ public ------------
void WebUI::begin() {
  // Головна + API (кореневі хендлери лежать у відповідних _handle* у цьому/інших файлах)
  _srv.on("/",                HTTP_GET,  [this]{ _handleRoot();    });
  _srv.on("/api/state",       HTTP_GET,  [this]{ _handleState();   });
  _srv.on("/api/start",       HTTP_POST, [this]{ _handleStart();   });
  _srv.on("/api/stop",        HTTP_POST, [this]{ _handleStop();    });
  _srv.on("/api/manual",      HTTP_POST, [this]{ _handleManual();  });

  // Wi-Fi / налаштування
  _srv.on("/settings",        HTTP_GET,  [this]{ _handleSettings(); });
  _srv.on("/api/apply",       HTTP_POST, [this]{ _handleApply();    });

  // Піни/PZEM
  _srv.on("/pins",            HTTP_GET,  [this]{ _handlePins();     });
  _srv.on("/api/pins",        HTTP_POST, [this]{ _apiPinsSave();    });
  _srv.on("/api/pins",        HTTP_GET,  [this]{ _apiPinsGet();     });

  // Профілі
  _srv.on("/profiles",        HTTP_GET,  [this]{ _handleProfiles(); });
  _srv.on("/profile",         HTTP_GET,  [this]{ _handleProfile();  });
  _srv.on("/api/prof",        HTTP_POST, [this]{ _handleApplyProf();});

  _srv.on("/api/profiles",        HTTP_GET,  [this]{ _apiProfiles();     });
  _srv.on("/api/profiles/select", HTTP_POST, [this]{ _apiProfSelect();   });
  _srv.on("/api/profiles/create", HTTP_POST, [this]{ _apiProfCreate();   });
  _srv.on("/api/profiles/rename", HTTP_POST, [this]{ _apiProfRename();   });
  _srv.on("/api/profiles/delete", HTTP_POST, [this]{ _apiProfDelete();   });

  // Калібровки
  _srv.on("/calibration",                 HTTP_GET,  [this]{ _handleCalibration();     });
  _srv.on("/api/calibration/blower",      HTTP_GET,  [this]{ _apiCalibGet();           });
  _srv.on("/api/calibration/blower",      HTTP_POST, [this]{ _apiCalibSave();          });

  _srv.on("/calibration/humidity",        HTTP_GET,  [this]{ _handleCalibHumidity();   });
  _srv.on("/api/calibration/humidity",    HTTP_GET,  [this]{ _apiCalibHumGet();        });
  _srv.on("/api/calibration/humidity",    HTTP_POST, [this]{ _apiCalibHumSave();       });

  // ======= НОВЕ: Час і аптайм =======
  _srv.on("/api/time", HTTP_GET, [this]{
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    char iso[32]; // "YYYY-mm-dd HH:MM:SS"
    strftime(iso, sizeof(iso), "%Y-%m-%d %H:%M:%S", &t);

    String json = F("{\"epoch_ms\":");
    json += String((uint32_t)((uint64_t)now * 1000ULL));
    json += F(",\"iso\":\""); json += iso; json += F("\",\"tz\":\"Europe/Kyiv\"}");
    _srv.send(200, "application/json; charset=utf-8", json);
  });

  _srv.on("/api/uptime", HTTP_GET, [this]{
    uint64_t us = esp_timer_get_time();
    uint32_t sec = (uint32_t)(us / 1000000ULL);
    String json = F("{\"seconds\":");
    json += String(sec);
    json += F("}");
    _srv.send(200, "application/json; charset=utf-8", json);
  });

  _srv.on("/api/reboot",      HTTP_POST, [this]{ _handleReboot();  });
  _srv.on("/favicon.ico",     HTTP_ANY,  [this]{ _srv.send(204);   });

  // Статичні ресурси
  _srv.on("/ui/app.css",  HTTP_GET, [this]{ _srv.send(200, "text/css",               _staticAppCss());  });
  _srv.on("/ui/gauge.js", HTTP_GET, [this]{ _srv.send(200, "application/javascript", _staticGaugeJs()); });
  _srv.on("/ui/app.js",   HTTP_GET, [this]{ _srv.send(200, "application/javascript", _staticAppJs());   });

  _mountStatic();

  _srv.onNotFound([this](){ _handleNotFound(); });
  _srv.begin();
  Serial.println(F("WebServer started on :80"));
}

void WebUI::loop() { _srv.handleClient(); }

void WebUI::_handleReboot() {
  _srv.send(200, "text/plain", "rebooting");
  delay(200);
  ESP.restart();
}

void WebUI::_handleNotFound() {
  _srv.send(404, "text/plain; charset=utf-8", "404: Не знайдено");
}

// --------- /ui/app.css ----------
String WebUI::_staticAppCss() {
  return F(
    ".gwrap{display:flex;gap:16px;flex-wrap:wrap;align-items:flex-start}"
    ".gauge{width:280px;height:170px;display:inline-block}"
    ".g-title{font:600 12px system-ui,Segoe UI,Roboto,Arial;fill:#bcd;opacity:.9;text-anchor:middle}"
    ".g-value{font:600 18px system-ui,Segoe UI,Roboto,Arial;fill:#eef;text-anchor:middle}"

    /* ======= Картка часу поруч із SSR ======= */
    ".ssr-and-time{display:flex;align-items:center;gap:18px;flex-wrap:wrap}"
    ".kyiv-time{padding:10px 14px;border-radius:10px;background:rgba(255,255,255,.05);"
      "border:1px solid rgba(255,255,255,.08)}"
    ".kyiv-time-large{font-size:28px;font-weight:700;letter-spacing:.5px;line-height:1}"
    ".kyiv-time-small{opacity:.8;font-size:12px;margin-top:4px}"
    ".uptime-line{margin-top:6px;font-size:13px;opacity:.95}"
  );
}

// --------- /ui/gauge.js ----------
String WebUI::_staticGaugeJs() {
  return F(
  "(function(){\n"
  "function norm(v,a,b){return (v-a)/(b-a);} \n"
  "function polar(cx,cy,r,ang){return [cx+r*Math.cos(ang), cy+r*Math.sin(ang)];}\n"
  "function segPath(cx,cy,r0,r1,a0,a1){\n"
  "  var p0=polar(cx,cy,r0,a0), p1=polar(cx,cy,r0,a1), q1=polar(cx,cy,r1,a1), q0=polar(cx,cy,r1,a0);\n"
  "  var large=(a1-a0)>Math.PI?1:0;\n"
  "  return 'M'+p0[0]+','+p0[1]+' A'+r0+','+r0+' 0 '+large+' 1 '+p1[0]+','+p1[1]\n"
  "       +' L'+q1[0]+','+q1[1]+' A'+r1+','+r1+' 0 '+large+' 0 '+q0[0]+','+q0[1]+' Z';\n"
  "}\n"
  "function colorAt(t){ var c=[[255,200,0],[255,140,0],[230,60,50]]; var i=Math.min(1,Math.max(0,t))*2; var k=i|0; var f=i-k; var a=c[k],b=c[Math.min(2,k+1)]; var r=(a[0]+(b[0]-a[0])*f)|0,g=(a[1]+(b[1]-a[1])*f)|0,bl=(a[2]+(b[2]-a[2])*f)|0; return 'rgb('+r+','+g+','+bl+')'; }\n"
  "function ensureSvg(host){ var w=280,h=170; host.innerHTML=''; var svg=document.createElementNS('http://www.w3.org/2000/svg','svg'); svg.setAttribute('viewBox','0 0 '+w+' '+h); host.appendChild(svg); return svg; }\n"
  "function drawSegments(svg,cx,cy){ var r0=70,r1=90,a0=Math.PI,a1=0,N=6; for(var i=0;i<N;i++){ var t0=i/N,t1=(i+1)/N,aa0=a0+(a1-a0)*t0,aa1=a0+(a1-a0)*t1; var p=document.createElementNS(svg.namespaceURI,'path'); p.setAttribute('d',segPath(cx,cy,r1,r1+16,aa0,aa1)); p.setAttribute('fill',colorAt((t0+t1)/2)); p.setAttribute('stroke','#fff'); p.setAttribute('opacity','0.9'); p.setAttribute('stroke-opacity','0.08'); svg.appendChild(p);} }\n"
  "function makeNeedle(svg,cx,cy){ var g=document.createElementNS(svg.namespaceURI,'g'); var n=document.createElementNS(svg.namespaceURI,'polygon'); n.setAttribute('points','-6,0 0,-72 6,0'); n.setAttribute('fill','#444'); var c=document.createElementNS(svg.namespaceURI,'circle'); c.setAttribute('r','20'); c.setAttribute('fill','#444'); c.setAttribute('stroke','#eee'); c.setAttribute('stroke-width','4'); g.appendChild(n); g.appendChild(c); g.setAttribute('transform','translate('+cx+','+cy+') rotate(-90)'); svg.appendChild(g); return g; }\n"
  "function makeText(svg,cx,cy){ var t=document.createElementNS(svg.namespaceURI,'text'); t.setAttribute('class','g-title'); t.setAttribute('x',cx); t.setAttribute('y',cy-8); svg.appendChild(t); var v=document.createElementNS(svg.namespaceURI,'text'); v.setAttribute('class','g-value'); v.setAttribute('x',cx); v.setAttribute('y',cy+34); svg.appendChild(v); return {title:t,val:v}; }\n"
  "var gauges={};\n"
  "function setGauge(id,value,min,max,label){ var host=document.getElementById(id); if(!host) return; if(value==null||isNaN(value)) value=min; var g=gauges[id]; var cx=140,cy=120; if(!g){ var svg=ensureSvg(host); drawSegments(svg,cx,cy); var needle=makeNeedle(svg,cx,cy); var txt=makeText(svg,cx,cy); g={svg:svg,needle:needle,txt:txt}; gauges[id]=g; } var t=norm(value,min,max); if(t<0)t=0;if(t>1)t=1; var deg=-90+180*t; g.needle.setAttribute('transform','translate('+cx+','+cy+') rotate('+deg+')'); g.txt.title.textContent=label||''; g.txt.val.textContent=value.toFixed(1)+' °C'; }\n"
  "window.setGauge=setGauge;\n"
  "})();\n"
  );
}

// --------- /ui/app.js ----------
String WebUI::_staticAppJs() {
  return F(
  "(function(){\n"
  "function u(v){return (v==null||isNaN(v))?null:Number(v);} \n"
  "window.updateGauges=function(j){ var b=u(j.t1_c), t=u(j.t_sht1_c), n=u(j.t_sht2_c); setGauge('g_boiler', b!=null?b:20, 20,120, 'Котел'); setGauge('g_top', t!=null?t:20, 20,120, 'Верх'); setGauge('g_bottom', n!=null?n:20, 20,120, 'Низ'); };\n"
  "document.addEventListener('DOMContentLoaded', function(){ setGauge('g_boiler',20,20,120,'Котел'); setGauge('g_top',20,20,120,'Верх'); setGauge('g_bottom',20,20,120,'Низ'); });\n"
  "\n"
  "// ===== Kyiv Clock + Human Uptime =====\n"
  "(function(){\n"
  "  function fmt2(n){return (n<10?'0':'')+n;}\n"
  "  function toHumanUptime(sec){ if(sec<60) return '0:01'; var d=Math.floor(sec/86400); sec%=86400; var h=Math.floor(sec/3600); sec%=3600; var m=Math.floor(sec/60); var HH=fmt2(h), MM=fmt2(m); return d>0?(d+'д '+HH+':'+MM):(h+':'+MM); }\n"
  "  function ensureClockCard(){\n"
  "    var exist=document.getElementById('kyivClockCard'); if(exist) return exist;\n"
  "    var card=document.createElement('div'); card.id='kyivClockCard'; card.className='kyiv-time';\n"
  "    card.innerHTML='<div id=\"kyivTimeLarge\" class=\"kyiv-time-large\">--:--:--</div>'+\n"
  "                   '<div id=\"kyivDateSmall\" class=\"kyiv-time-small\">----</div>'+\n"
  "                   '<div class=\"uptime-line\"><span>Аптайм:</span> <strong id=\"uptimeHuman\">--:--</strong></div>';\n"
  "    // Спроба вставити поруч із SSR: шукаємо контейнер із id або текстом\n"
  "    var ssrBox=document.getElementById('ssr')||document.querySelector('.ssr-block');\n"
  "    if(ssrBox && ssrBox.parentElement){\n"
  "      var wrap=document.createElement('div'); wrap.className='ssr-and-time';\n"
  "      ssrBox.parentElement.insertBefore(wrap, ssrBox.nextSibling);\n"
  "      wrap.appendChild(ssrBox);\n"
  "      wrap.appendChild(card);\n"
  "    } else {\n"
  "      document.body.appendChild(card);\n"
  "    }\n"
  "    return card;\n"
  "  }\n"
  "  var card=ensureClockCard();\n"
  "  var elTime=document.getElementById('kyivTimeLarge');\n"
  "  var elDate=document.getElementById('kyivDateSmall');\n"
  "  var elUp  =document.getElementById('uptimeHuman');\n"
  "  var offsetMs=0;\n"
  "  function renderClock(ms){ var d=new Date(ms); elTime.textContent=fmt2(d.getHours())+':'+fmt2(d.getMinutes())+':'+fmt2(d.getSeconds()); elDate.textContent=fmt2(d.getDate())+'.'+fmt2(d.getMonth()+1)+'.'+d.getFullYear(); }\n"
  "  async function syncTime(){ try{ var t0=Date.now(); var r=await fetch('/api/time',{cache:'no-store'}); var t1=Date.now(); if(!r.ok) throw 0; var j=await r.json(); var rtt=t1-t0; if(typeof j.epoch_ms==='number'){ offsetMs=(j.epoch_ms+Math.round(rtt/2))-Date.now(); renderClock(Date.now()+offsetMs);} }catch(e){} }\n"
  "  async function pollUptime(){ try{ var r=await fetch('/api/uptime',{cache:'no-store'}); if(!r.ok) throw 0; var j=await r.json(); if(typeof j.seconds==='number'){ elUp.textContent=toHumanUptime(j.seconds);} }catch(e){} }\n"
  "  (async function(){ await syncTime(); renderClock(Date.now()+offsetMs); setInterval(function(){renderClock(Date.now()+offsetMs);},1000); setInterval(syncTime,60000); pollUptime(); setInterval(pollUptime,10000); })();\n"
  "})();\n"
  "\n"
  "})();\n"
  );
}

// =================== КАЛІБРОВКА ПІДДуВУ ===================
void WebUI::_handleCalibration() {
  String html = F(
"<!doctype html><html lang='uk'><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Калібровка піддуву</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:980px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
".grid{display:grid;gap:12px;grid-template-columns:repeat(7,1fr)}"
"input{width:100%;background:#0e1627;border:1px solid #2a3458;color:#e8eef9;border-radius:10px;padding:10px}"
".rowlbl{opacity:.85;margin:10px 0 6px}"
".btns{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}"
"button{padding:10px 14px;border:0;border-radius:12px;background:#3b82f6;color:#fff;cursor:pointer}"
"a.btn{display:inline-block;margin-left:4px;color:#e8eef9}"
"#st{margin-left:6px;opacity:.85}"
"@media(max-width:860px){.grid{grid-template-columns:repeat(3,1fr)}}"
"@media(max-width:520px){.grid{grid-template-columns:repeat(2,1fr)}}"
"</style>"
"<div class='wrap'>"
"  <div class='card'>"
"    <h1 style='margin:0 0 10px'>Калібровка піддуву: Температура → %</h1>"
"    <div class='rowlbl'>Температура котла, °C</div>"
"    <div class='grid' id='rowT'></div>"
"    <div class='rowlbl'>Відсоток піддуву, %</div>"
"    <div class='grid' id='rowP'></div>"
"    <div class='btns'>"
"      <button id='save'>💾 Зберегти</button>"
"      <button id='reload'>↺ Оновити з пристрою</button>"
"      <a class='btn' href='/profiles'>← Профілі</a>"
"      <a class='btn' href='/calibration/humidity'>Калібровка вологості →</a>"
"      <span id='st'></span>"
"    </div>"
"  </div>"
"</div>"
"<script>"
"const N=7; const rowT=document.getElementById('rowT'), rowP=document.getElementById('rowP');"
"const t=[], p=[]; function mk(min,max,step,ph){const i=document.createElement('input');i.type='number';i.min=min;i.max=max;i.step=step;i.placeholder=ph;return i;}"
"for(let i=0;i<N;i++){const c=mk(-50,300,0.1,'T'+(i+1));t.push(c);rowT.appendChild(c);} "
"for(let i=0;i<N;i++){const c=mk(0,100,0.1,'%'+(i+1));p.push(c);rowP.appendChild(c);} "
"const st=document.getElementById('st');"
"async function load(){ try{ st.textContent='Завантаження...'; const r=await fetch('/api/calibration/blower',{cache:'no-store'}); const j=await r.json();"
"  (j.temps||[]).forEach((v,i)=>{ if(t[i]) t[i].value=v; }); (j.perc||j.percents||[]).forEach((v,i)=>{ if(p[i]) p[i].value=v; }); st.textContent=''; }"
"catch(e){ st.textContent='Помилка завантаження'; }}"
"async function save(){ const params=new URLSearchParams();"
"  for(let i=0;i<N;i++){ if(t[i].value===''){st.textContent='Порожнє T'+(i+1);return;} if(p[i].value===''){st.textContent='Порожній %'+(i+1);return;} params.append('t'+i, t[i].value); params.append('p'+i, p[i].value); }"
"  st.textContent='Збереження...';"
"  try{ const r=await fetch('/api/calibration/blower',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});"
"    st.textContent = r.ok ? 'Збережено ✅' : ('Помилка: '+await r.text()); }"
"  catch(e){ st.textContent='Помилка мережі'; }}"
"document.getElementById('save').addEventListener('click',save);"
"document.getElementById('reload').addEventListener('click',load);"
"load();"
"</script>"
);
  _srv.send(200, "text/html; charset=utf-8", html);
}

// =================== КАЛІБРОВКА ВОЛОГОСТІ ===================
void WebUI::_handleCalibHumidity() {
  String html = F(
"<!doctype html><html lang='uk'><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Калібровка вологості</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:980px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
".grid{display:grid;gap:12px;grid-template-columns:repeat(7,1fr)}"
"input{width:100%;background:#0e1627;border:1px solid #2a3458;color:#e8eef9;border-radius:10px;padding:10px}"
".rowlbl{opacity:.85;margin:10px 0 6px}"
".btns{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}"
"button{padding:10px 14px;border:0;border-radius:12px;background:#3b82f6;color:#fff;cursor:pointer}"
"a.btn{display:inline-block;margin-left:4px;color:#e8eef9}"
"#st{margin-left:6px;opacity:.85}"
"@media(max-width:860px){.grid{grid-template-columns:repeat(3,1fr)}}"
"@media(max-width:520px){.grid{grid-template-columns:repeat(2,1fr)}}"
"</style>"
"<div class='wrap'>"
"  <div class='card'>"
"    <h1 style='margin:0 0 10px'>Калібровка вологості: еталон → показ датчика</h1>"
"    <div class='rowlbl'>Еталонна вологість, %</div>"
"    <div class='grid' id='rowR'></div>"
"    <div class='rowlbl'>Показ датчика, %</div>"
"    <div class='grid' id='rowM'></div>"
"    <div class='btns'>"
"      <button id='save'>💾 Зберегти</button>"
"      <button id='reload'>↺ Оновити з пристрою</button>"
"      <a class='btn' href='/calibration'>← Калібровка піддуву</a>"
"      <a class='btn' href='/profiles'>Профілі</a>"
"      <span id='st'></span>"
"    </div>"
"  </div>"
"</div>"
"<script>"
"const N=7; const rowR=document.getElementById('rowR'), rowM=document.getElementById('rowM');"
"const r=[], m=[]; function mk(min,max,step,ph){const i=document.createElement('input');i.type='number';i.min=min;i.max=max;i.step=step;i.placeholder=ph;return i;}"
"for(let i=0;i<N;i++){const c=mk(0,100,0.1,'ref'+(i+1));r.push(c);rowR.appendChild(c);} "
"for(let i=0;i<N;i++){const c=mk(0,100,0.1,'raw'+(i+1));m.push(c);rowM.appendChild(c);} "
"const st=document.getElementById('st');"
"async function load(){ try{ st.textContent='Завантаження...'; const rj=await fetch('/api/calibration/humidity',{cache:'no-store'}); const j=await rj.json();"
"  (j.ref||[]).forEach((v,i)=>{ if(r[i]) r[i].value=v; }); (j.raw||[]).forEach((v,i)=>{ if(m[i]) m[i].value=v; }); st.textContent=''; }"
"catch(e){ st.textContent='Помилка завантаження'; }}"
"async function save(){ const params=new URLSearchParams();"
"  for(let i=0;i<N;i++){ if(r[i].value===''){st.textContent='Порожній ref'+(i+1);return;} if(m[i].value===''){st.textContent='Порожній raw'+(i+1);return;} params.append('r'+i, r[i].value); params.append('m'+i, m[i].value); }"
"  st.textContent='Збереження...';"
"  try{ const rr=await fetch('/api/calibration/humidity',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});"
"    st.textContent = rr.ok ? 'Збережено ✅' : ('Помилка: '+await rr.text()); }"
"  catch(e){ st.textContent='Помилка мережі'; }}"
"document.getElementById('save').addEventListener('click',save);"
"document.getElementById('reload').addEventListener('click',load);"
"load();"
"</script>"
);
  _srv.send(200, "text/html; charset=utf-8", html);
}

// =================== КАЛІБРОВКИ ЧЕРЕЗ CalibrationStore (per-profile) ===================

// GET /api/calibration/blower
void WebUI::_apiCalibGet() {
  const int id = CalibrationStore::activeProfile();
  CalibrationStore::BlowerTable b{};
  CalibrationStore::loadBlower(id, &b);

  String out = "{\"profile\":"+String(id)+",\"temps\":[";
  for (int i=0;i<7;i++){ if(i) out+=','; out+=String(b.t[i],2); }
  out += "],\"perc\":[";
  for (int i=0;i<7;i++){ if(i) out+=','; out+=String(b.p[i],2); }
  out += "]}";
  _srv.send(200, "application/json", out);
}

// POST /api/calibration/blower
void WebUI::_apiCalibSave() {
  const int id = CalibrationStore::activeProfile();
  CalibrationStore::BlowerTable b{};
  for (int i=0;i<7;i++) {
    const String kt="t"+String(i), kp="p"+String(i);
    if(!_srv.hasArg(kt) || !_srv.hasArg(kp)) { _srv.send(400,"text/plain","missing "+kt+"/"+kp); return; }
    b.t[i] = _srv.arg(kt).toFloat();
    b.p[i] = _srv.arg(kp).toFloat();
    if (b.p[i] < 0 || b.p[i] > 100) { _srv.send(400,"text/plain","percent out of range"); return; }
  }
  bool ok = CalibrationStore::saveBlower(id, &b);
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}

// GET /api/calibration/humidity
void WebUI::_apiCalibHumGet() {
  const int id = CalibrationStore::activeProfile();
  CalibrationStore::HumidityTable h{};
  CalibrationStore::loadHumidity(id, &h);

  String out = "{\"profile\":"+String(id)+",\"ref\":[";
  for (int i=0;i<7;i++){ if(i) out+=','; out+=String(h.ref[i],2); }
  out += "],\"raw\":[";
  for (int i=0;i<7;i++){ if(i) out+=','; out+=String(h.raw[i],2); }
  out += "]}";
  _srv.send(200, "application/json", out);
}

// POST /api/calibration/humidity
void WebUI::_apiCalibHumSave() {
  const int id = CalibrationStore::activeProfile();
  CalibrationStore::HumidityTable h{};
  for (int i=0;i<7;i++) {
    const String kr="r"+String(i), km="m"+String(i);
    if(!_srv.hasArg(kr) || !_srv.hasArg(km)) { _srv.send(400,"text/plain","missing "+kr+"/"+km); return; }
    h.ref[i] = _srv.arg(kr).toFloat();
    h.raw[i] = _srv.arg(km).toFloat();
    if (h.ref[i] < 0 || h.ref[i] > 100 || h.raw[i] < 0 || h.raw[i] > 100) {
      _srv.send(400,"text/plain","range 0..100"); return;
    }
  }
  bool ok = CalibrationStore::saveHumidity(id, &h);
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}
