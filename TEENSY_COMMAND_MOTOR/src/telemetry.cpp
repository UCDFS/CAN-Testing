#include "telemetry.h"

void sendTelemetry() {
  Serial2.printf("RPM:%d\n", rpmFeedback);
  Serial2.printf("TORQUE:%d\n", currentTorque);
  Serial2.printf("STATUS:%d\n", statusWord);
  Serial2.printf("DCBUS:%.1f\n", dcBusVoltage);
}
