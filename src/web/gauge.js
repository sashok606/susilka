// gauge.js — простий SVG-спідометр

(() => {
  const TAU = Math.PI * 2;
  const deg2rad = d => d * Math.PI / 180;

  // геометрія
  const W = 260, H = 160;        // розмір контейнера
  const CX = 130, CY = 140;      // центр/вісь обертання
  const R  = 120;                // радіус дуги
  const THICK = 18;              // товщина дуги
  const START = -120, END = 120; // кутовий сектор гейджа

  // плавний градієнт кольорів (жовтий → помаранчевий → червоний)
  const COLS = ["#f7c948", "#f97316", "#ef4444"];

  function arcPath(r0, r1, a0deg, a1deg) {
    const a0 = deg2rad(a0deg), a1 = deg2rad(a1deg);
    const x0o = CX + r0 * Math.cos(a0), y0o = CY + r0 * Math.sin(a0);
    const x1o = CX + r0 * Math.cos(a1), y1o = CY + r0 * Math.sin(a1);
    const x0i = CX + r1 * Math.cos(a1), y0i = CY + r1 * Math.sin(a1);
    const x1i = CX + r1 * Math.cos(a0), y1i = CY + r1 * Math.sin(a0);
    const large = (a1 - a0) > Math.PI ? 1 : 0;
    return `M${x0o},${y0o} A${r0},${r0} 0 ${large} 1 ${x1o},${y1o}
            L${x0i},${y0i} A${r1},${r1} 0 ${large} 0 ${x1i},${y1i} Z`;
  }

  function lerp(a,b,t){return a+(b-a)*t}
  function lerpColor(c0, c1, t){
    const p = n => parseInt(n, 16);
    const c0r=p(c0.slice(1,3)), c0g=p(c0.slice(3,5)), c0b=p(c0.slice(5,7));
    const c1r=p(c1.slice(1,3)), c1g=p(c1.slice(3,5)), c1b=p(c1.slice(5,7));
    const r=Math.round(lerp(c0r,c1r,t)).toString(16).padStart(2,"0");
    const g=Math.round(lerp(c0g,c1g,t)).toString(16).padStart(2,"0");
    const b=Math.round(lerp(c0b,c1b,t)).toString(16).padStart(2,"0");
    return `#${r}${g}${b}`;
  }
  function gradientAt(t){
    if (t<=0) return COLS[0];
    if (t>=1) return COLS[2];
    if (t<0.5) return lerpColor(COLS[0],COLS[1],t*2);
    return lerpColor(COLS[1],COLS[2],(t-0.5)*2);
  }

  function ensureGauge(id){
    let host = document.getElementById(id);
    if (!host) return null;
    if (host.__svg) return host.__svg;

    const svgNS = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(svgNS, "svg");
    svg.setAttribute("viewBox", `0 0 ${W} ${H}`);
    svg.setAttribute("width", W);
    svg.setAttribute("height", H);

    // фонова тонка дуга
    const bg = document.createElementNS(svgNS, "path");
    bg.setAttribute("d", arcPath(R, R-THICK, START, END));
    bg.setAttribute("fill", "#0e1627");
    bg.setAttribute("stroke", "#202a49");
    bg.setAttribute("stroke-width", "1");
    svg.appendChild(bg);

    // кольорові сегменти (тонкі «штрихування», щоб був градієнт)
    const segs = document.createElementNS(svgNS, "g");
    const steps = 40;
    for (let s=0; s<steps; s++){
      const a0 = lerp(START, END, s/steps);
      const a1 = lerp(START, END, (s+1)/steps);
      const p  = document.createElementNS(svgNS, "path");
      p.setAttribute("d", arcPath(R, R-THICK, a0, a1));
      p.setAttribute("fill", gradientAt((s+0.5)/steps));
      p.setAttribute("opacity", "0.95");
      segs.appendChild(p);
    }
    svg.appendChild(segs);

    // «скло» поверх
    const gloss = document.createElementNS(svgNS, "path");
    gloss.setAttribute("d", arcPath(R, R-THICK, START, END));
    gloss.setAttribute("fill", "none");
    svg.appendChild(gloss);

    // підпис
    const label = document.createElementNS(svgNS, "text");
    label.setAttribute("x", CX); label.setAttribute("y", 24);
    label.setAttribute("text-anchor", "middle");
    label.setAttribute("fill", "#9fb4e3");
    label.setAttribute("font-size", "12");
    svg.appendChild(label);

    // значення
    const valueText = document.createElementNS(svgNS, "text");
    valueText.setAttribute("x", CX); valueText.setAttribute("y", CY-8);
    valueText.setAttribute("text-anchor", "middle");
    valueText.setAttribute("fill", "#e8eef9");
    valueText.setAttribute("font-size", "22");
    valueText.setAttribute("font-weight", "600");
    svg.appendChild(valueText);

    // стрілка
    const needle = document.createElementNS(svgNS, "polygon");
    needle.setAttribute("points", `${CX-6},${CY} ${CX+6},${CY} ${CX},${CY-86}`);
    needle.setAttribute("fill", "#3a425f");
    svg.appendChild(needle);

    // центральна «шайба»
    const hubOuter = document.createElementNS(svgNS, "circle");
    hubOuter.setAttribute("cx", CX); hubOuter.setAttribute("cy", CY);
    hubOuter.setAttribute("r", 22); hubOuter.setAttribute("fill", "#0e1627");
    hubOuter.setAttribute("stroke", "#d7deea"); hubOuter.setAttribute("stroke-width","3");
    svg.appendChild(hubOuter);

    const hubInner = document.createElementNS(svgNS, "circle");
    hubInner.setAttribute("cx", CX); hubInner.setAttribute("cy", CY);
    hubInner.setAttribute("r", 14); hubInner.setAttribute("fill", "#4b556d");
    svg.appendChild(hubInner);

    host.innerHTML = "";
    host.appendChild(svg);

    host.__svg = {svg, label, valueText, needle};
    return host.__svg;
  }

  function clamp(v,min,max){ return Math.max(min, Math.min(max, v)); }

  function setGauge(id, val, min=20, max=120, label=""){
    const g = ensureGauge(id); if (!g) return;
    // нормалізуємо в межі
    const v = (val==null || isNaN(val)) ? NaN : +val;
    const vClamped = isNaN(v) ? min : clamp(v, min, max);
    const t = (vClamped - min) / (max - min);
    const angle = START + (END-START) * t;

    g.label.textContent = label ? label : "";
    g.valueText.textContent = isNaN(v) ? "—" : `${v.toFixed(1)} °C`;
    g.needle.setAttribute("transform", `rotate(${angle}, ${CX}, ${CY})`);
  }

  window.setGauge = setGauge;
})();
