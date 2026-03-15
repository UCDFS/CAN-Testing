#include "pedal.h"
#include <math.h>

// Converts a raw ADC reading to a 0–100% pedal position.
// REST and FULL can be in either direction (rising or falling sensor).
// Soft-clamped: out-of-range readings never produce an error, just 0% or 100%.
static float appsPercent(int raw, int rest, int full) {
  float pct = (float)(rest - raw) * 100.0f / (float)(rest - full);
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

void updateTorqueFromPedal() {
  apps1Raw = analogRead(APPS1_PIN);
  apps2Raw = analogRead(APPS2_PIN);

  float pct1 = appsPercent(apps1Raw, APPS1_REST, APPS1_FULL);
  float pct2 = appsPercent(apps2Raw, APPS2_REST, APPS2_FULL);

  // Plausibility: sensors must agree within PEDAL_PLAUSIBILITY_PERCENT.
  // On fault: zero torque immediately. Fault latches until pedal returns
  // to rest (both sensors below dead band), preventing resume mid-travel.
  if (fabsf(pct1 - pct2) > PEDAL_PLAUSIBILITY_PERCENT) {
    pedalFault = true;
    currentTorque = 0;
    return;
  }

  if (pedalFault) {
    if (pct1 < PEDAL_DEADBAND_PERCENT && pct2 < PEDAL_DEADBAND_PERCENT) {
      pedalFault = false;
    } else {
      currentTorque = 0;
      return;
    }
  }

  // Take the lower (more conservative) of the two sensor readings.
  float pct = (pct1 < pct2) ? pct1 : pct2;

  // Dead band: absorbs calibration offset at rest, eliminates torque creep.
  if (pct < PEDAL_DEADBAND_PERCENT) pct = 0.0f;

  // Cap at configured maximum acceleration
  if (pct > MAX_ACCEL_PERCENT) pct = (float)MAX_ACCEL_PERCENT;

  currentTorque = (int16_t)(TORQUE_MAX * (pct / 100.0f));
}
