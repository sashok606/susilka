// app.js — оновлення гейджів із /api/state
window.updateGauges = function (j) {
  // котел (DS18B20)
  setGauge("g_boiler", j.t1_c, 20, 120, "Котел");

  // верх/низ (SHT31 на двох шинах)
  setGauge("g_top",    j.t_sht1_c, 20, 120, "Верх");
  setGauge("g_bottom", j.t_sht2_c, 20, 120, "Низ");

  // додано: вологість із верхнього SHT31
  setGauge("g_rh", j.rh_sht1, 0, 100, "Вологість");
};
