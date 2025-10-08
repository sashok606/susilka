// src/web/handlers_profiles.cpp
#include "WebUI.h"
#include <Arduino.h>
#include "../storage/ProfileStore.h"
#include "../logic/Control.h"

// ==== Профіль: сторінка редагування ====
void WebUI::_handleProfile() {
  int id = _srv.hasArg("id") ? _srv.arg("id").toInt() : ProfileStore::currentIndex();
  ProfileStore::Item it;
  if (!ProfileStore::loadOne(id, it)) { _srv.send(404,"text/plain","Not found"); return; }
  const auto &p = it.cfg;

  auto esc = [&](const String& s){ return _escape(s); };

  String html;
  html.reserve(26000);
  html += F(
"<!doctype html><html lang='uk'><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Налаштування активного профілю</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:1100px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;margin:0 0 16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
"h1,h2{margin:0 0 12px}"
".row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin:0 0 12px}"
"a.btn{display:inline-block;padding:8px 12px;border:1px solid #2a3458;border-radius:12px;text-decoration:none;color:#e8eef9}"
"button{padding:10px 14px;border:0;border-radius:12px;background:#3b82f6;color:#fff;cursor:pointer}"
"fieldset{border:1px solid #1e2742;border-radius:12px;margin:0 0 12px;padding:12px}"
"legend{opacity:.9;padding:0 8px}"
"label{display:block;font-size:13px;opacity:.9;margin:8px 0 4px}"
"input{width:100%;background:#0e1627;border:1px solid #2a3458;color:#e8eef9;border-radius:10px;padding:10px}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}"
"#st{margin-left:8px;opacity:.85}"
"</style>"
"<div class='wrap'>"
" <div class='card'>"
"  <h1>Профіль: ");

  html += esc(it.name);

  html += F(
"</h1>"
"<div class='row'>"
"  <a class='btn' href='/profiles'>← Повернутися до профілів</a>"
"  <a class='btn' href='/calibration'>Калібровка піддуву</a>"
"  <a class='btn' href='/calibration/humidity'>Калібровка вологості</a>"
"  <span id='st'></span>"
"</div>"
"<form id='f'>"

  // === ПІДДув (FAN1) — котел ===
"<fieldset><legend>ПІДДув (FAN1) — котел</legend>"
" <div class='grid'>"
"  <div><label>Цільова температура котла (°C)</label><input name='t_boiler_set' value='"); html += String(p.t_boiler_set,1);
  html += F("'></div>"
"  <div><label>Гістерезис термоконтролю котла (°C)</label><input name='t_boiler_hyst' value='"); html += String(p.t_boiler_hyst,1);
  html += F("'></div>"
"  <div><label>FAN1 мінімум (%)</label><input name='fan1_min' value='"); html += String(p.fan1_min);
  html += F("'></div>"
"  <div><label>FAN1 максимум (%)</label><input name='fan1_max' value='"); html += String(p.fan1_max);
  html += F("'></div>"
"  <div><label>FAN1 коефіцієнт (на 1 °C), %</label><input name='fan1_k' value='"); html += String(p.fan1_k,2);
  html += F("'></div>"
" </div>"
"</fieldset>"

  // === РЕЦИРКУЛЯЦІЯ (FAN2) ===
"<fieldset><legend>РЕЦИРКУЛЯЦІЯ (FAN2)</legend>"
" <div class='grid'>"
"  <div><label>Рециркуляція: ΔT УВІМК (верх–низ), °C</label><input name='dT_diff_on' value='"); html += String(p.dT_diff_on,1);
  html += F("'></div>"
"  <div><label>Рециркуляція: ΔT ВИМК (верх–низ), °C</label><input name='dT_diff_off' value='"); html += String(p.dT_diff_off,1);
  html += F("'></div>"
"  <div><label>Пороги котла FAN2 УВІМК (°C)</label><input name='t_fan2_on' value='"); html += String(p.t_fan2_on,1);
  html += F("'></div>"
"  <div><label>Пороги котла FAN2 ВИМК (°C)</label><input name='t_fan2_off' value='"); html += String(p.t_fan2_off,1);
  html += F("'></div>"
"  <p style='grid-column:1/-1;opacity:.8;margin:4px 0 0'>Примітка: у поточній логіці FAN2 працює постійно у фазах «Рециркуляція» та «Сушіння». Ці пороги залишені для сумісності/майбутніх режимів.</p>"
" </div>"
"</fieldset>"

  // === SSR (вентилятор усередині) ===
"<fieldset><legend>SSR (вентилятор усередині камери)</legend>"
" <div class='grid'>"
"  <div><label>SSR ΔT УВІМК (|верх–низ|), °C</label><input name='ssr_dT_on' value='"); html += String(p.ssr_dT_on,1);
  html += F("'></div>"
"  <div><label>SSR ΔT ВИМК (|верх–низ|), °C</label><input name='ssr_dT_off' value='"); html += String(p.ssr_dT_off,1);
  html += F("'></div>"
" </div>"
"</fieldset>"

  // === ВИТЯЖКА (FAN3) — вологість ===
"<fieldset><legend>ВИТЯЖКА (FAN3) — вологість</legend>"
" <div class='grid'>"
"  <div><label>Цільова вологість RH (верхній датчик), %</label><input name='rh_set' value='"); html += String(p.rh_set,1);
  html += F("'></div>"
"  <div><label>FAN3 мінімум (%)</label><input name='fan3_min' value='"); html += String(p.fan3_min);
  html += F("'></div>"
"  <div><label>FAN3 крок регулювання (%)</label><input name='fan3_step' value='"); html += String(p.fan3_step);
  html += F("'></div>"
"  <div><label>FAN3 максимум (%)</label><input name='fan3_max' value='"); html += String(p.fan3_max);
  html += F("'></div>"
" </div>"
"</fieldset>"

  // === ФАЗИ та БЕЗПЕКА ===
"<fieldset><legend>ФАЗИ та БЕЗПЕКА</legend>"
" <div class='grid'>"
"  <div><label>Стартовий тест (сек)</label><input name='test_sec' value='"); html += String(p.test_sec);
  html += F("'></div>"
"  <div><label>Розпал — тривалість (сек)</label><input name='phase1_sec' value='"); html += String(p.phase1_sec);
  html += F("'></div>"
"  <div><label>Розпал — мін. приріст ΔT (°C)</label><input name='dT_min' value='"); html += String(p.dT_min,1);
  html += F("'></div>"
"  <div><label>Мінімальна робоча T повітря (°C)</label><input name='t_min_air' value='"); html += String(p.t_min_air,1);
  html += F("'></div>"
"  <div><label>Контроль дров — час (сек)</label><input name='wood_end_check_sec' value='"); html += String(p.wood_end_check_sec);
  html += F("'></div>"
"  <div><label>Гранична температура продукту (°C)</label><input name='t_prod_max' value='"); html += String(p.t_prod_max,1);
  html += F("'></div>"
" </div>"
"</fieldset>"

"<input type='hidden' name='id' value='"); html += String(id);
  html += F("'>"
"<button type='submit'>💾 Зберегти зміни</button>"
"</form>"
"</div></div>"
"<script>"
"const st=document.getElementById('st');"
"document.getElementById('f').addEventListener('submit',async(e)=>{"
"  e.preventDefault();"
"  try{ st.textContent='Збереження...';"
"       const fd=new FormData(e.target);"
"       const params=new URLSearchParams();"
"       for(const [k,v] of fd){ params.append(k, String(v).replace(',','.')); }"
"       const r=await fetch('/api/prof',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded; charset=UTF-8'},body:params.toString()});"
"       if(r.ok){ st.textContent='Збережено ✅'; alert('Профіль збережено'); location.href='/profiles'; }"
"       else{ const t=await r.text(); st.textContent='Помилка'; alert('Помилка збереження: '+t); }"
"  }catch(_){ st.textContent='Помилка мережі'; }"
"});"
"</script>"
);

  _srv.send(200, "text/html; charset=utf-8", html);
}

// ==== Профілі (список/CRUD) ====
void WebUI::_handleProfiles() {
  String html;
  html.reserve(14000);
  html += F(
"<!doctype html><html lang='uk'><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Профілі</title>"
"<style>"
"body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;background:#0b1020;color:#e8eef9;margin:0;padding:24px}"
".wrap{max-width:1000px;margin:0 auto}"
".card{background:#121a2b;border:1px solid #1e2742;border-radius:16px;padding:16px;margin:0 0 16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}"
".row{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}"
"input,button{padding:10px;border-radius:12px;border:1px solid #2a3458;background:#0e1627;color:#e8eef9}"
"table{width:100%;border-collapse:collapse;margin-top:12px}"
"th,td{padding:8px;border-bottom:1px solid #1e2742;text-align:left}"
"a.btn{display:inline-block;padding:10px 12px;border:1px solid #2a3458;border-radius:12px;text-decoration:none;color:#e8eef9}"
"#msg{opacity:.85;margin-left:8px}"
"</style>"
"<div class='wrap'><div class='card'>"
"<h1>Профілі сушіння</h1>"
"<div class='row'><a class='btn' href='/'>← На головну</a>"
"<input id='newname' placeholder='Назва нового профілю'>"
"<button onclick='createP()'>Створити</button><span id=\"msg\"></span></div>"
"<table id='tbl'><thead><tr><th>#</th><th>Назва</th><th>Дії</th></tr></thead><tbody></tbody></table>"
"</div></div>"
"<script>"
"const msg=document.getElementById('msg');"
"async function load(){"
"  msg.textContent='';"
"  try{const r=await fetch('/api/profiles',{cache:'no-store'});"
"      const j=await r.json();"
"      const tb=document.querySelector('#tbl tbody'); tb.innerHTML='';"
"      j.items.forEach((it,i)=>{"
"        const tr=document.createElement('tr');"
"        tr.innerHTML=`<td>${i}${i==j.current?' *':''}</td>"
"        <td><input value='${it.name}' data-i='${i}' onblur='ren(this)'></td>"
"        <td><a class='btn' href='/profile?id=${i}'>Редагувати</a> "
"            <button onclick='sel(${i})'>Зробити активним</button> "
"            <button onclick='delp(${i})'>Видалити</button></td>`;"
"        tb.appendChild(tr);"
"      });"
"  }catch(e){ msg.textContent='Помилка завантаження'; }"
"}"
"async function sel(i){ await fetch('/api/profiles/select',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded; charset=UTF-8'},body:'idx='+i}); load(); }"
"async function createP(){ const n=document.getElementById('newname').value||'Новий профіль'; const r=await fetch('/api/profiles/create',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded; charset=UTF-8'},body:'name='+encodeURIComponent(n)}); if(!r.ok){ msg.textContent='Помилка створення'; } load(); }"
"async function ren(inp){ const i=inp.dataset.i; await fetch('/api/profiles/rename',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded; charset=UTF-8'},body:`idx=${i}&name=${encodeURIComponent(inp.value)}`}); }"
"async function delp(i){ await fetch('/api/profiles/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded; charset=UTF-8'},body:'idx='+i}); load(); }"
"load();"
"</script>"
);
  _srv.send(200, "text/html; charset=utf-8", html);
}

// ==== Збереження профілю з форми ====
void WebUI::_handleApplyProf() {
  int id = _srv.hasArg("id") ? _srv.arg("id").toInt() : ProfileStore::currentIndex();
  ProfileStore::Item it;
  if (!ProfileStore::loadOne(id, it)) { _srv.send(404,"application/json","{\"ok\":false}"); return; }

  auto &p = it.cfg;
  // Піддув / FAN1
  if (_srv.hasArg("t_boiler_set"))  p.t_boiler_set  = _srv.arg("t_boiler_set").toFloat();
  if (_srv.hasArg("t_boiler_hyst")) p.t_boiler_hyst = _srv.arg("t_boiler_hyst").toFloat();
  if (_srv.hasArg("fan1_min"))      p.fan1_min      = _srv.arg("fan1_min").toInt();
  if (_srv.hasArg("fan1_max"))      p.fan1_max      = _srv.arg("fan1_max").toInt();
  if (_srv.hasArg("fan1_k"))        p.fan1_k        = _srv.arg("fan1_k").toFloat();

  // Рециркуляція / FAN2 (пороги лишаємо)
  if (_srv.hasArg("dT_diff_on"))    p.dT_diff_on    = _srv.arg("dT_diff_on").toFloat();
  if (_srv.hasArg("dT_diff_off"))   p.dT_diff_off   = _srv.arg("dT_diff_off").toFloat();
  if (_srv.hasArg("t_fan2_on"))     p.t_fan2_on     = _srv.arg("t_fan2_on").toFloat();
  if (_srv.hasArg("t_fan2_off"))    p.t_fan2_off    = _srv.arg("t_fan2_off").toFloat();

  // SSR (вентилятор усередині)
  if (_srv.hasArg("ssr_dT_on"))     p.ssr_dT_on     = _srv.arg("ssr_dT_on").toFloat();
  if (_srv.hasArg("ssr_dT_off"))    p.ssr_dT_off    = _srv.arg("ssr_dT_off").toFloat();

  // Витяжка / FAN3
  if (_srv.hasArg("rh_set"))        p.rh_set        = _srv.arg("rh_set").toFloat();
  if (_srv.hasArg("fan3_min"))      p.fan3_min      = _srv.arg("fan3_min").toInt();
  if (_srv.hasArg("fan3_step"))     p.fan3_step     = _srv.arg("fan3_step").toInt();
  if (_srv.hasArg("fan3_max"))      p.fan3_max      = _srv.arg("fan3_max").toInt();

  // Фази/захист
  if (_srv.hasArg("test_sec"))           p.test_sec           = _srv.arg("test_sec").toInt();
  if (_srv.hasArg("phase1_sec"))         p.phase1_sec         = _srv.arg("phase1_sec").toInt();
  if (_srv.hasArg("dT_min"))             p.dT_min             = _srv.arg("dT_min").toFloat();
  if (_srv.hasArg("t_min_air"))          p.t_min_air          = _srv.arg("t_min_air").toFloat();
  if (_srv.hasArg("wood_end_check_sec")) p.wood_end_check_sec = _srv.arg("wood_end_check_sec").toInt();
  if (_srv.hasArg("t_prod_max"))         p.t_prod_max         = _srv.arg("t_prod_max").toFloat();

  bool ok = ProfileStore::saveOne(id, it);
  if (ok && id == ProfileStore::currentIndex()) {
    Control::profile() = it.cfg;
    ProfileStore::saveControlToActive();
  }

  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}

// ==== API списку профілів (для таблиці) ====
void WebUI::_apiProfiles() {
  int n = ProfileStore::count();
  int cur = ProfileStore::currentIndex();
  String j = "{\"current\":"+String(cur)+",\"items\":[";
  for (int i=0;i<n;i++){
    ProfileStore::Item it;
    ProfileStore::loadOne(i, it);
    if (i) j += ",";
    j += "{\"name\":\""; j += _escape(it.name); j += "\"}";
  }
  j += "]}";
  _srv.send(200, "application/json", j);
}

void WebUI::_apiProfSelect() {
  if (!_srv.hasArg("idx")) { _srv.send(400,"application/json","{\"ok\":false}"); return; }
  int idx = _srv.arg("idx").toInt();
  bool ok = ProfileStore::setCurrentIndex(idx) && ProfileStore::loadActiveToControl();
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}

void WebUI::_apiProfCreate() {
  String name = _srv.hasArg("name") ? _srv.arg("name") : "Новий профіль";
  int newIdx=-1; bool ok = ProfileStore::create(name, newIdx);
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}

void WebUI::_apiProfRename() {
  if (!_srv.hasArg("idx") || !_srv.hasArg("name")) { _srv.send(400,"application/json","{\"ok\":false}"); return; }
  int idx = _srv.arg("idx").toInt(); String name = _srv.arg("name");
  bool ok = ProfileStore::rename(idx, name);
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}

void WebUI::_apiProfDelete() {
  if (!_srv.hasArg("idx")) { _srv.send(400,"application/json","{\"ok\":false}"); return; }
  int idx = _srv.arg("idx").toInt();
  bool ok = ProfileStore::remove(idx);
  _srv.send(ok?200:500, "application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
}
