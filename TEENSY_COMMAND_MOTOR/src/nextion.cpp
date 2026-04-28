#include "nextion.h"

// Send a Nextion command: flush any pending RX bytes, print the command
// string, then terminate with three 0xFF bytes.
// Flushing before each send prevents stale ACK/event bytes from the
// display accumulating and corrupting subsequent reads (ref: NexHardware.cpp).
static void sendCommand(const char *cmd) {
  while (NEXTION_SERIAL.available()) NEXTION_SERIAL.read();
  NEXTION_SERIAL.print(cmd);
  NEXTION_SERIAL.write(0xFF);
  NEXTION_SERIAL.write(0xFF);
  NEXTION_SERIAL.write(0xFF);
}

void nextionBegin() {
  NEXTION_SERIAL.begin(NEXTION_BAUD);
  delay(100);
  // Initialisation sequence (ref: nexInit() in NexHardware.cpp):
  //   empty command  — clears any garbage in the display's RX buffer
  //   bkcmd=1        — display sends a return code after every command
  //   page 0         — ensure we start on the boot page
  sendCommand("");
  sendCommand("bkcmd=1");
  sendCommand("page 0");
}

void nextionPage(uint8_t page) {
  char cmd[16];
  snprintf(cmd, sizeof(cmd),"page %d", page);
  sendCommand(cmd);
}

void nextionText(const char *component, const char *text) {
  char cmd[80];
  snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", component, text);
  sendCommand(cmd);
}

void nextionNum(const char *component, int value) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "%s.val=%d", component, value);
  sendCommand(cmd);
}

void nextionBootStatus(const char *phase, const char *detail) {
  nextionText(NX_BOOT_STATUS, phase);
  if (detail[0] != '\0') nextionText(NX_BOOT_DETAIL, detail);
}

// rpmFeedback is normalised: 32767 = RPM_MAX. Convert to actual RPM then km/h.
// km/h formula: v = (2π × n × r_w) / (60 × G), r_w=0.247m, G=5.25 → 0.01775 km/h per RPM
static float rpm_to_kmh(int normalised) {
  float actual_rpm = (float)normalised / 32767.0f * RPM_MAX;
  return 0.01775f * actual_rpm;
}

void nextionUpdateDrive() {
  int actual_rpm = (int)((float)rpmFeedback / 32767.0f * RPM_MAX);
  nextionNum(NX_DRIVE_SPEED,  (int)rpm_to_kmh(rpmFeedback));
  nextionNum(NX_DRIVE_RPM,    actual_rpm);
  nextionNum(NX_DRIVE_TORQUE, (int)((float)currentTorque / TORQUE_MAX * 100.0f));
  nextionNum(NX_DRIVE_DCBUS,  (int)dcBusVoltage);
  nextionText(NX_DRIVE_FAULT, pedalFault ? "FAULT" : "OK");
  nextionText(NX_DRIVE_STATE, driveEnabled ? "ON" : "OFF");
  nextionNum(NX_DRIVE_MTEMP, (int)motorTemp);
  nextionNum(NX_DRIVE_ITEMP, (int)inverterTemp);
}
