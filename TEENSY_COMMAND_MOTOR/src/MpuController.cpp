#include "MpuController.h"

MpuController::MpuController(Adafruit_MPU6050 mpu) : mpu(mpu) {}

bool MpuController::begin() {
  bool mpu_found = false;
  for (uint8_t i = 0; i < 3; i++) {
    mpu_found = mpu.begin();
    if (mpu_found) {
      break;
    }
  }
  if (!mpu_found) {
    nextionText(NX_BOOT_DETAIL, "MPU not found after 3 tries, continuing without");
    return false;
  }
  
  mpu.setAccelerometerRange(MPU_ACCEL_RANGE);
  mpu.setGyroRange(MPU_GYRO_RANGE);
  mpu.setFilterBandwidth(MPU_FILTER_BW);
}

void MpuController::logTelemetry() {
  sensors_event_t a, g, temp;
  this->mpu.getEvent(&a, &g, &temp);

  logIMU(a.acceleration.x, a.acceleration.y, a.acceleration.z,
         g.gyro.x, g.gyro.y, g.gyro.z);
}
