#pragma once
#include <stdint.h>

// KTY81-210 resistance -> temperature LUT
static const float kty_resistance[] = {
  980,1030,1135,1247,1367,1495,1630,1772,1922,2000,
  2080,2245,2417,2597,2785,2980,3182,3392,3607,3817,
  3915,4008,4166,4280
};
static const float kty_temp[] = {
  -55,-50,-40,-30,-20,-10,0,10,20,25,
  30,40,50,60,70,80,90,100,110,120,
  125,130,140,150
};

// IGBT ADC -> temperature LUT
static const float igbt_adc[] = {
  16308,16387,16487,16609,16757,16938,17151,17400,
  17688,18017,18387,18797,19247,19733,20250,20793,
  21357,21933,22515,23097,23671,24232,24775,25296,
  25792,26261,26702,27114,27497,27851,28179,28480
};
static const float igbt_temp[] = {
  -30,-25,-20,-15,-10,-5,0,5,
  10,15,20,25,30,35,40,45,
  50,55,60,65,70,75,80,85,
  90,95,100,105,110,115,120,125
};

static float interpolate(const float *xs, const float *ys, int len, float x) {
  if (x <= xs[0])   return ys[0];
  if (x >= xs[len-1]) return ys[len-1];
  for (int i = 1; i < len; i++) {
    if (x <= xs[i]) {
      float t = (x - xs[i-1]) / (xs[i] - xs[i-1]);
      return ys[i-1] + t * (ys[i] - ys[i-1]);
    }
  }
  return ys[len-1];
}

inline float motorADCToTemp(uint16_t adc) {
  const float kSeries = 4000.0f;
  const float kADCMax = 32768.0f;
  float resistance = kSeries * adc / (kADCMax - adc);
  return interpolate(kty_resistance, kty_temp, 24, resistance);
}

inline float igbtADCToTemp(uint16_t adc) {
  return interpolate(igbt_adc, igbt_temp, 32, (float)adc);
}