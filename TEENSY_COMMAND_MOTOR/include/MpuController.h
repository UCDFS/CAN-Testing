#include "config.h"
#include "nextion.h"
#include "logging.h"

class MpuController {
private:
  Adafruit_MPU6050 mpu;
public:
  MpuController(Adafruit_MPU6050 mpu);
  bool begin();
  void logTelemetry();
};