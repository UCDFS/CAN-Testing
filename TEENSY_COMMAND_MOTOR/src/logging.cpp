#include "logging.h"

// ---------- Write buffer ----------
// Batches SD writes to reduce per-frame latency. Drains automatically when
// full; committed to flash by logFlush() every 500 ms from the main loop.
static char     _buf[4096];
static uint16_t _bufLen = 0;

static void _drain() {
  if (_bufLen > 0) {
    logFile.write((const uint8_t *)_buf, _bufLen);
    _bufLen = 0;
  }
}

static void _append(const char *data, uint16_t len) {
  if (!logFile) return;
  if (_bufLen + len > (uint16_t)sizeof(_buf)) {
    _drain();
  }
  memcpy(_buf + _bufLen, data, len);
  _bufLen += len;
}

// ---------- File management ----------
String generateFilename() {
  int index = 1;
  char filename[32];
  do {
    sprintf(filename, "CAN_log_%04d.csv", index);
    index++;
  } while (SD.exists(filename));
  return String(filename);
}

void logWriteHeader() {
  static const char hdr[] =
    "# C,ms,dir,id,len,b0,b1,b2,b3,b4,b5,b6,b7\n"
    "# S,ms,apps1_raw,apps2_raw,pedal_fault,torque_cmd,rpm,dcbus_dV\n";
  _append(hdr, sizeof(hdr) - 1);
}

// ---------- CAN frame logging ----------
// Record: C,<ms>,<dir>,<id_hex>,<len>,<b0_hex>,...,<b7_hex>
// Always 13 columns; unused byte fields are empty.
void logCANFrame(const CAN_message_t &msg, const char *dir) {
  char line[80];
  int n = sprintf(line, "C,%lu,%s,%03lX,%d", millis(), dir, msg.id, msg.len);
  for (int i = 0; i < 8; i++) {
    if (i < msg.len) n += sprintf(line + n, ",%02X", msg.buf[i]);
    else              line[n++] = ',';
  }
  line[n++] = '\n';
  _append(line, n);
}

// ---------- Sensor snapshot logging ----------
// Record: S,<ms>,<apps1_raw>,<apps2_raw>,<pedal_fault>,<torque_cmd>,<rpm>,<dcbus_dV>
// dcbus_dV = dcBusVoltage * 10 (integer decivolts, avoids float formatting).
// pedal_fault = 1 if APPS plausibility fault active.
void logSensor(int apps1Raw, int apps2Raw, bool fault, int16_t torque, int rpm, int dcbusDV) {
  char line[56];
  int n = sprintf(line, "S,%lu,%d,%d,%d,%d,%d,%d\n",
                  millis(), apps1Raw, apps2Raw, (int)fault, (int)torque, rpm, dcbusDV);
  _append(line, n);
}

// ---------- Periodic flush ----------
// Drains the RAM buffer to the SD library, then commits to flash.
// Called every 500 ms from the main loop — not on every frame.
void logFlush() {
  if (!logFile) return;
  _drain();
  logFile.flush();
}
