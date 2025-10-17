#include <FlexCAN_T4.h>
#include <SD.h>
#include <SPI.h>

// ---------- CAN setup ----------
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

// ---------- BAMOCAR IDs ----------
#define BAMOCAR_RX_ID 0x201  // Teensy → Bamocar
#define BAMOCAR_TX_ID 0x181  // Bamocar → Teensy

// ---------- User constants ----------
#define MAX_ACCEL_PERCENT 50
#define TORQUE_MAX 32767
const int chipSelect = BUILTIN_SDCARD;

// ---------- Globals ----------
File logFile;
int currentStep = 0;
int16_t currentTorque = 0;
uint32_t lastTorqueSend = 0;
bool bamocarOnline = false;

// ---------- Function prototypes ----------
void executeStep(int step);
void readCanMessages();
void sendCAN(const CAN_message_t &msg);
void requestStatusCyclic(uint8_t interval_ms);
void requestStatusOnce();
void requestSpeedCyclic(uint8_t interval_ms);
void requestDCBusOnce();
void clearErrors();
void enableDrive();
void disableDrive();
void sendTorqueCommand(int16_t torqueValue);
void configureCanTimeout(uint16_t ms);
void updateTorqueFromPot();
String interpretBamocarMessage(const CAN_message_t &msg);
void logCANFrame(const CAN_message_t &msg, const char *dir);
String generateFilename();
void dumpLogToSerial();

// ---------- Logging ----------
String generateFilename() {
  int index = 1;
  char filename[32];
  do {
    sprintf(filename, "CAN_traffic_logs_%04d.csv", index);
    index++;
  } while (SD.exists(filename));
  return String(filename);
}

void logCANFrame(const CAN_message_t &msg, const char *dir) {
  logFile.printf("%lu,%s,0x%03X,%d", millis(), dir, msg.id, msg.len);
  if (msg.len > 0) logFile.printf(",0x%02X", msg.buf[0]);
  else logFile.print(",");
  for (int i = 0; i < 8; i++) {
    if (i < msg.len) logFile.printf(",0x%02X", msg.buf[i]);
    else logFile.print(",");
  }

  String decoded = interpretBamocarMessage(msg);
  logFile.print("\"");
  for (unsigned int i = 0; i < decoded.length(); i++) {
    char c = decoded[i];
    if (c == '"') logFile.print("\"\"");
    else if (c == '\r' || c == '\n') logFile.print(' ');
    else logFile.print(c);
  }
  logFile.print("\"\r\n");
  logFile.flush();
}

void sendCAN(const CAN_message_t &msg) {
  Can1.write(msg);
  logCANFrame(msg, "TX");
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  Can1.begin();
  Can1.setBaudRate(500000);
  analogReadResolution(12);

  if (!SD.begin(chipSelect)) {
    Serial.println("SD card init failed!");
    while (1);
  }

  String filename = generateFilename();
  logFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!logFile) {
    Serial.println("File open failed!");
    while (1);
  }

  logFile.println("Time(ms),Dir,ID,Len,B0,B1,B2,B3,B4,B5,B6,B7,Decoded");
  logFile.flush();

  Serial.println("=== BAMOCAR Headless Bring-Up ===");
  Serial.print("Logging to: ");
  Serial.println(filename);

  // Step 1: Start cyclic STATUS and RPM
  executeStep(1);

  // Wait for Bamocar to respond to STATUS messages
  Serial.println("Waiting for Bamocar to respond...");
  uint32_t startTime = millis();
  while (!bamocarOnline && millis() - startTime < 10000) {  // 10s timeout
    requestStatusOnce();
    readCanMessages();
    delay(100);
  }

  if (!bamocarOnline) {
    Serial.println("No Bamocar response detected. Aborting.");
    while (1);
  }
  Serial.println("Bamocar online detected.");

  // Step 2 → 8 sequence
  executeStep(2); delay(200);
  executeStep(3); delay(200);
  executeStep(4); delay(200);
  executeStep(5); delay(500);
  executeStep(6); delay(500);

   // Wait for pedal to be released (pot near rest)
  Serial.println("Waiting for pedal release...");
  int potValue = 0;
  for (int i = 0; i < 10; i++) {  // average a few readings
    potValue += analogRead(A0);
    delay(10);
  }
  potValue /= 10;

  // If pedal > ~5% pressed, wait until released
  while ((2930 - potValue) * 100.0f / (2930 - 1860) > 5.0f) {
    potValue = analogRead(A0);
    delay(50);
  }
  Serial.println("Pedal released, continuing...");

  executeStep(7);
}

// ---------- Loop ----------
void loop() {
  readCanMessages();

  if (currentStep == 7 && millis() - lastTorqueSend >= 20) {
    updateTorqueFromPot();
    sendTorqueCommand(currentTorque);
    lastTorqueSend = millis();
  }

  static uint32_t lastFlush = 0;
  if (millis() - lastFlush > 500) {
    logFile.flush();
    lastFlush = millis();
  }
}

// ---------- Step Execution ----------
void executeStep(int step) {
  currentStep = step;
  switch (step) {
    case 1:
      requestStatusCyclic(100);
      requestSpeedCyclic(100);
      Serial.println("Step 1: Cyclic STATUS + RPM started.");
      break;
    case 2: requestDCBusOnce(); break;
    case 3: clearErrors(); break;
    case 4: configureCanTimeout(2000); break;
    case 5:
      clearErrors();
      delay(100);
      enableDrive();
      requestStatusOnce();
      break;
    case 6:
      sendTorqueCommand(0);
      Serial.println("Torque set to 0 for sanity check");
      break;
    case 7:
      Serial.println("Torque control active (A0)");
      Serial.printf("Max accel cap: %d%%\n", MAX_ACCEL_PERCENT);
      break;
    case 8:
      // disableDrive();
      Serial.println("Drive disabled");
      break;
    case 9:
      dumpLogToSerial();
      break;
  }
}

// ---------- CAN Commands ----------
void requestStatusCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x40;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestStatusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x40;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void requestSpeedCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x30;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestDCBusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0xEB;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void clearErrors() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x8E;
  msg.buf[1] = 0x00;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void configureCanTimeout(uint16_t ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0xD0;
  msg.buf[1] = ms & 0xFF;
  msg.buf[2] = (ms >> 8) & 0xFF;
  sendCAN(msg);
}

void enableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x51;
  msg.buf[1] = 0x04;
  msg.buf[2] = 0x00;
  sendCAN(msg);
  delay(100);
  msg.buf[1] = 0x00;
  sendCAN(msg);
}

void disableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x51;
  msg.buf[1] = 0x04;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void sendTorqueCommand(int16_t torqueValue) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x90;
  msg.buf[1] = torqueValue & 0xFF;
  msg.buf[2] = (torqueValue >> 8) & 0xFF;
  sendCAN(msg);
}

// ---------- CAN RX ----------
void readCanMessages() {
  CAN_message_t msg;
  while (Can1.read(msg)) {
    logCANFrame(msg, "RX");

    if (msg.id == BAMOCAR_TX_ID && msg.len >= 3) {
      if (msg.buf[0] == 0x40) {
        bamocarOnline = true;
      }
    }
  }
}

// ---------- Interpret known registers ----------
String interpretBamocarMessage(const CAN_message_t &msg) {
  char buffer[100];
  uint8_t reg = msg.buf[0];

  if (msg.id == BAMOCAR_RX_ID) {
    switch (reg) {
      case 0x3D: sprintf(buffer, "Request register 0x%02X", msg.buf[1]); break;
      case 0x8E: sprintf(buffer, "Clear all error flags"); break;
      case 0x51:
        if (msg.buf[1] == 0x04) sprintf(buffer, "Lock/Disable drive");
        else if (msg.buf[1] == 0x00) sprintf(buffer, "Enable drive");
        else sprintf(buffer, "Drive control command 0x%02X", msg.buf[1]);
        break;
      case 0x90: sprintf(buffer, "Set torque = %d", (msg.buf[1] | (msg.buf[2] << 8))); break;
      case 0xD0: sprintf(buffer, "Set CAN timeout = %d ms", msg.buf[1] | (msg.buf[2] << 8)); break;
      default: sprintf(buffer, "Command 0x%02X sent", reg); break;
    }
    return String(buffer);
  }

  if (msg.id == BAMOCAR_TX_ID) {
    switch (reg) {
      case 0x30: sprintf(buffer, "Speed feedback = %d rpm", msg.buf[1] | (msg.buf[2] << 8)); break;
      case 0x40: sprintf(buffer, "Status word = 0x%04X", msg.buf[1] | (msg.buf[2] << 8)); break;
      case 0xEB: sprintf(buffer, "DC bus voltage = %.1f V", (msg.buf[1] | (msg.buf[2] << 8)) * 0.1f); break;
      default: sprintf(buffer, "Reply register 0x%02X", reg); break;
    }
    return String(buffer);
  }
  return "";
}

// ---------- Potentiometer control ----------
void updateTorqueFromPot() {
  int potValue = analogRead(A0);
  float potPercent = (2930 - potValue) * 100.0f / (2930 - 1860);
  if (potPercent < 0) potPercent = 0;
  if (potPercent > MAX_ACCEL_PERCENT) potPercent = MAX_ACCEL_PERCENT;
  currentTorque = (int16_t)(TORQUE_MAX * (potPercent / 100.0f));
}

// ---------- Log dump ----------
void dumpLogToSerial() {
  logFile.flush();
  logFile.close();
  File readFile = SD.open(logFile.name(), FILE_READ);
  if (!readFile) return;
  while (readFile.available()) Serial.write(readFile.read());
  readFile.close();
  logFile = SD.open(logFile.name(), FILE_WRITE);
}
