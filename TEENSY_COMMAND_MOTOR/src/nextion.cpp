#include "nextion.h"

static const uint8_t _term[3] = {0xFF, 0xFF, 0xFF};

static void _send(const char *cmd) {
  NEXTION_SERIAL.print(cmd);
  NEXTION_SERIAL.write(_term, 3);
}

void nextionBegin() {
  NEXTION_SERIAL.begin(NEXTION_BAUD);
  delay(100);
  nextionPage(NX_PAGE_BOOT);
}

void nextionPage(uint8_t page) {
  char cmd[16];
  sprintf(cmd, "page %d", page);
  _send(cmd);
}

void nextionText(const char *component, const char *text) {
  char cmd[80];
  sprintf(cmd, "%s.txt=\"%s\"", component, text);
  _send(cmd);
}

void nextionNum(const char *component, int value) {
  char cmd[32];
  sprintf(cmd, "%s.val=%d", component, value);
  _send(cmd);
}

void nextionBootStatus(const char *phase, const char *detail) {
  nextionText(NX_BOOT_STATUS, phase);
  if (detail[0] != '\0') nextionText(NX_BOOT_DETAIL, detail);
}

static float rpm_to_kmh(float rpm) {
  return 0.01775f * rpm;
}

void nextionUpdateDrive() {
  nextionNum(NX_DRIVE_SPEED,  (int)rpm_to_kmh((float)rpmFeedback));
  nextionNum(NX_DRIVE_RPM,    rpmFeedback);
  nextionNum(NX_DRIVE_TORQUE, (int)((float)currentTorque / TORQUE_MAX * 100.0f));
  nextionNum(NX_DRIVE_DCBUS,  (int)dcBusVoltage);
  nextionText(NX_DRIVE_FAULT, pedalFault ? "FAULT" : "OK");
  nextionText(NX_DRIVE_STATE, driveEnabled ? "ON" : "OFF");
}
