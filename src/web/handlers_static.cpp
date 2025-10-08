// src/web/handlers_static.cpp
#include "WebUI.h"

// ---------------- CSS ----------------
static const char APP_CSS[] PROGMEM = R"CSS(
/* app.css — мінімум для гейджів */
.gauge{position:relative; width:260px; height:160px}
.g-title{position:absolute; left:0; right:0; bottom:40px; text-align:center; opacity:.8; font-size:12px}
.g-value{position:absolute; left:0; right:0; bottom:12px; text-align:center; font-weight:700}
)CSS";

// ---------------- gauge.js ----------------
static const char GAUGE_JS[] PROGMEM = R"JS(
(function(){
  const TAU = Math.PI*2;
  function clamp(v,min,max){ return Math.max(min, Math.min(max, v)); }
  function mk(tag,cls){ const e=document.createElement(tag); if(cls) e.className=cls; return e; }

  function create(root, cfg){
    const opt = Object.assign({
      min:20, max:120, title:"", value:NaN,
      segments:[{stop:60,color:"#f7c948"},{stop:90,color:"#f08c00"},{stop:120,color:"#d64545"}]
    }, cfg||{});

    root.classList.add("gauge"); root.innerHTML="";
    const W=root.clientWidth||260, H=root.clientHeight||160, cx=W/2, cy=H*0.9, r=Math.min(W,H*2)*0.36;

    const svgNS="http://www.w3.org/2000/svg", svg=document.createElementNS(svgNS,"svg");
    svg.setAttribute("viewBox",`0 0 ${W} ${H}`); svg.setAttribute("width","100%"); svg.setAttribute("height","100%");
    root.appendChild(svg);

    const aMin=Math.PI, aMax=2*Math.PI;
    function arc(a0,a1){ const x0=cx+r*Math.cos(a0), y0=cy+r*Math.sin(a0), x1=cx+r*Math.cos(a1), y1=cy+r*Math.sin(a1);
      const large=(a1-a0)>Math.PI?1:0; return `M ${x0} ${y0} A ${r} ${r} 0 ${large} 1 ${x1} ${y1}`; }

    let prev=opt.min;
    opt.segments.forEach(s=>{
      const stop=clamp(s.stop,opt.min,opt.max);
      const a0=aMin+(prev-opt.min)/(opt.max-opt.min)*(aMax-aMin);
      const a1=aMin+(stop-opt.min)/(opt.max-opt.min)*(aMax-aMin);
      const p=document.createElementNS(svgNS,"path");
      p.setAttribute("d",arc(a0,a1)); p.setAttribute("fill","none");
      p.setAttribute("stroke",s.color); p.setAttribute("stroke-width",Math.round(r*0.18));
      p.setAttribute("stroke-linecap","round"); svg.appendChild(p); prev=stop;
    });

    const inner=document.createElementNS(svgNS,"circle");
    inner.setAttribute("cx",cx); inner.setAttribute("cy",cy); inner.setAttribute("r",r*0.55);
    inner.setAttribute("fill","#2b334a"); inner.setAttribute("stroke","#46527a"); inner.setAttribute("stroke-width",3);
    svg.appendChild(inner);

    const needle=document.createElementNS(svgNS,"polygon"); svg.appendChild(needle);

    const title=mk("div","g-title"); title.textContent=opt.title||""; root.appendChild(title);
    const gval =mk("div","g-value"); root.appendChild(gval);

    function draw(v){
      const val=clamp(v,opt.min,opt.max);
      const a=aMin+(val-opt.min)/(opt.max-opt.min)*(aMax-aMin), L=r*0.9, W=Math.max(6,r*0.12);
      const x=cx+Math.cos(a)*L, y=cy+Math.sin(a)*L, xL=cx+Math.cos(a+TAU/4)*W, yL=cy+Math.sin(a+TAU/4)*W, xR=cx+Math.cos(a-TAU/4)*W, yR=cy+Math.sin(a-TAU/4)*W;
      needle.setAttribute("points",`${xL},${yL} ${xR},${yR} ${x},${y}`); needle.setAttribute("fill","#ddd");
    }
    function set(v){ if(Number.isFinite(v)){ gval.textContent=v.toFixed(1)+" °C"; draw(v);} else { gval.textContent="—"; } }
    set(opt.value);
    return { set };
  }
  window.Gauge={ create };
})();
)JS";

// ---------------- app.js ----------------
static const char APP_JS[] PROGMEM = R"JS(
(function(){
  const el=id=>document.getElementById(id);
  const mv=(id,v)=>el(id).textContent=v;
  const PHASES=['Очікування','Старт/тест','Розпал','Рециркуляція','Сушіння','Дрова закінчились','Завершено','Аварія'];

  let gBoiler=null,gTop=null,gBottom=null;
  function ensureGauges(){
    if(!gBoiler){
      gBoiler=Gauge.create(el('g_boiler'),{min:20,max:120,title:'Котел'});
      gTop   =Gauge.create(el('g_top'),   {min:20,max:120,title:'Верх'});
      gBottom=Gauge.create(el('g_bottom'),{min:20,max:120,title:'Низ'});
    }
  }

  async function poll(){
    try{
      const r=await fetch('/api/state',{cache:'no-store'}); const j=await r.json();
      ensureGauges();
      if(j.t1_c!=null)      gBoiler.set(+j.t1_c);
      if(j.t_sht1_c!=null)  gTop.set(+j.t_sht1_c);
      if(j.t_sht2_c!=null)  gBottom.set(+j.t_sht2_c);

      el('profname').textContent=j.profile_name||'—';
      el('phase').textContent=(j.phase>=0&&j.phase<PHASES.length)?PHASES[j.phase]:'—';
      el('note').textContent=j.note||'';
      el('fault').textContent=j.fault?('ТАК: '+(j.fault_reason||'')):'ні';
      el('t1').textContent=(j.t1_c==null?'—':(Number(j.t1_c).toFixed(2)+' °C'));
      el('ttb').textContent=((j.t_sht1_c==null)?'—':Number(j.t_sht1_c).toFixed(2))+' / '+((j.t_sht2_c==null)?'—':Number(j.t_sht2_c).toFixed(2))+' °C';
      el('rh').textContent=(j.rh_sht1==null?'—':(Number(j.rh_sht1).toFixed(1)+' %'));
      el('fans').textContent=(j.fan1??'—')+' / '+(j.fan2??'—')+' / '+(j.fan3??'—')+' %';
      el('ssrs').textContent=(j.ssr1?'УВІМК':'вимк')+' / '+(j.ssr2?'УВІМК':'вимк');
      el('wifi').textContent=(j.wifi_mode||'')+' | STA: '+(j.ip_sta||'-')+' | AP: '+(j.ip_ap||'-')+' | RSSI: '+(j.rssi??'-')+' dBm';
      el('uptime').textContent=(j.uptime_s??0)+' с';
      el('mmode').checked=!!j.manual_mode;
      el('mf1').value=j.mf1||0; mv('mf1v',el('mf1').value);
      el('mf2').value=j.mf2||0; mv('mf2v',el('mf2').value);
      el('mf3').value=j.mf3||0; mv('mf3v',el('mf3').value);
      el('ms1').checked=!!j.ms1; el('ms2').checked=!!j.ms2;

      el('v').textContent=(j.pzem_v==null?'—':(Number(j.pzem_v).toFixed(1)+' В'));
      el('i').textContent=(j.pzem_i==null?'—':(Number(j.pzem_i).toFixed(2)+' А'));
      el('p').textContent=(j.pzem_p==null?'—':(Number(j.pzem_p).toFixed(0)+' Вт'));
      el('kwh').textContent=(j.pzem_kwh==null?'—':(Number(j.pzem_kwh).toFixed(3)+' кВт·год'));
      el('pk').textContent=(j.price_kwh==null?'—':(Number(j.price_kwh).toFixed(2)+' грн/кВт·год'));
      el('cost').textContent=(j.cost_total==null?'—':(Number(j.cost_total).toFixed(2)+' грн'));
    }catch(e){ console.warn('poll error',e); }
  }

  async function sendManual(){
    const body={manual:el('mmode').checked, f1:+el('mf1').value, f2:+el('mf2').value, f3:+el('mf3').value,
                s1:el('ms1').checked, s2:el('ms2').checked};
    try{ await fetch('/api/manual',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}catch(_){}
  }
  window.sendManual=sendManual;

  setInterval(poll,1000); poll();
})();
)JS";

void WebUI::_mountStatic() {
  _srv.on("/ui/app.css",  HTTP_GET, [this]{ _srv.send_P(200,"text/css",               APP_CSS);  });
  _srv.on("/ui/gauge.js", HTTP_GET, [this]{ _srv.send_P(200,"application/javascript", GAUGE_JS); });
  _srv.on("/ui/app.js",   HTTP_GET, [this]{ _srv.send_P(200,"application/javascript", APP_JS);   });
}
