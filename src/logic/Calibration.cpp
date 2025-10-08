#include "Calibration.h"
#include <algorithm>
#include "CalibrationStore.h"


using namespace CalibrationStore;

static float clampf(float v, float a, float b){ return v<a?a:(v>b?b:v); }

// генерик інтерполяції X->Y
static float interp(const float* x, const float* y, int n, float v) {
  if (n<=0) return NAN;
  if (v<=x[0]) return y[0];
  if (v>=x[n-1]) return y[n-1];
  int i=0;
  while (i<n-1 && !(v>=x[i] && v<=x[i+1])) i++;
  float x0=x[i], x1=x[i+1], y0=y[i], y1=y[i+1];
  float t = (x1==x0)?0.0f: (v-x0)/(x1-x0);
  return y0 + (y1-y0)*t;
}

float Calibration::blowerPercentFromTemp(float tempC) {
  int idx = activeProfile();
  BlowerTable bt{};
  loadBlower(idx, &bt);

  // гарантуємо зростання X (T) — на випадок, якщо заповнили не по порядку
  // зробимо копії і відсортуємо парно
  struct Pair{ float x,y; };
  Pair a[7];
  for (int i=0;i<7;i++) a[i] = {bt.t[i], bt.p[i]};
  std::sort(a, a+7, [](const Pair& A, const Pair& B){ return A.x < B.x; });
  float X[7], Y[7];
  for (int i=0;i<7;i++){ X[i]=a[i].x; Y[i]=a[i].y; }

  float d = interp(X, Y, 7, tempC);
  return clampf(d, 0.f, 100.f);
}

float Calibration::humidityCorrected(float measuredRaw) {
  int idx = activeProfile();
  HumidityTable ht{};
  loadHumidity(idx, &ht);

  // Тут X — RAW, Y — REF: corrected = f(raw)
  struct Pair{ float x,y; };
  Pair a[7];
  for (int i=0;i<7;i++) a[i] = {ht.raw[i], ht.ref[i]};
  std::sort(a, a+7, [](const Pair& A, const Pair& B){ return A.x < B.x; });
  float X[7], Y[7];
  for (int i=0;i<7;i++){ X[i]=a[i].x; Y[i]=a[i].y; }

  float c = interp(X, Y, 7, measuredRaw);
  return clampf(c, 0.f, 100.f);
}

int Calibration::fan3DutyByHumidity(float rhCorrected,
                                    float rhSet,
                                    int currentDuty,
                                    int minDuty,
                                    int maxDuty,
                                    int step,
                                    float deadband)
{
  int duty = currentDuty;
  if (rhCorrected > rhSet + deadband) {
    duty = std::min(maxDuty, currentDuty + step);
  } else if (rhCorrected < rhSet - deadband) {
    duty = std::max(minDuty, currentDuty - step);
  }
  return duty;
}

namespace Calibration {

static HumidityTable gH{};
static bool gLoaded = false;

static void ensureLoaded() {
  if (!gLoaded) {
    loadHumidity(activeProfile(), &gH);
    gLoaded = true;
  }
}

void refreshActiveCalibration() {
  loadHumidity(activeProfile(), &gH);
  gLoaded = true;
}

// piecewise-linear: маємо точки (raw[i] -> ref[i]); інтерполюємо по raw
float correctHumidity(float raw) {
  ensureLoaded();

  // гарантія монотонності за raw (на випадок ручного вводу)
  // просте впорядкування вставкою лише індексів
  int idx[7] = {0,1,2,3,4,5,6};
  for (int i=1;i<7;i++){
    int j=i;
    while (j>0 && gH.raw[idx[j-1]] > gH.raw[idx[j]]) {
      std::swap(idx[j-1], idx[j]);
      --j;
    }
  }

  // крайні випадки: екстраполяція лінійна на першому/останньому відрізку
  if (raw <= gH.raw[idx[0]]) {
    int i0=idx[0], i1=idx[1];
    float x0=gH.raw[i0], y0=gH.ref[i0];
    float x1=gH.raw[i1], y1=gH.ref[i1];
    if (x1==x0) return y0;
    float t=(raw-x0)/(x1-x0);
    return y0 + (y1-y0)*t;
  }
  if (raw >= gH.raw[idx[6]]) {
    int i0=idx[5], i1=idx[6];
    float x0=gH.raw[i0], y0=gH.ref[i0];
    float x1=gH.raw[i1], y1=gH.ref[i1];
    if (x1==x0) return y1;
    float t=(raw-x0)/(x1-x0);
    return y0 + (y1-y0)*t;
  }

  // знайти сегмент [k,k+1] де raw всередині
  for (int k=0;k<6;k++){
    int i0=idx[k], i1=idx[k+1];
    float x0=gH.raw[i0], y0=gH.ref[i0];
    float x1=gH.raw[i1], y1=gH.ref[i1];
    if (raw >= x0 && raw <= x1) {
      if (x1==x0) return (y0+y1)*0.5f;
      float t=(raw-x0)/(x1-x0);
      return y0 + (y1-y0)*t;
    }
  }
  // запасний випадок (не повинно статися)
  return raw;
}

} // namespace Calibration
