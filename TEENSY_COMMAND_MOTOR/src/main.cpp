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
const int chipSelect = BUILTIN_SDCARD; // Teensy 4.x onboard SD

// ---------- Globals ----------
File logFile;
int currentStep = 0;
int16_t currentTorque = 0;
uint32_t lastTorqueSend = 0;

// ---------- Prototypes ----------
void handleSerialInput();
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
void dumpLogToSerial();


// ---------- Logging helpers ----------
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

  // RegID
  if (msg.len > 0) logFile.printf(",0x%02X", msg.buf[0]);
  else logFile.print(",");

  // Fixed 8 data byte columns
  for (int i = 0; i < 8; i++) {
    if (i < msg.len) logFile.printf(",0x%02X", msg.buf[i]);
    else logFile.print(",");
  }

  // Decoded text
  String decoded = interpretBamocarMessage(msg);
  logFile.print("\"");  // open quote without extra comma

  // Escape and clean text
  for (unsigned int i = 0; i < decoded.length(); i++) {
    char c = decoded[i];
    if (c == '"') logFile.print("\"\"");   // escape quotes
    else if (c == '\r' || c == '\n') logFile.print(' ');
    else logFile.print(c);
  }

  logFile.print("\"\r\n");  // close quote and end line
  logFile.flush();
}


void sendCAN(const CAN_message_t &msg) {
  Can1.write(msg);
  logCANFrame(msg, "TX");
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  while (!Serial);

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

  Serial.println("=== BAMOCAR Bring-Up + Logger ===");
  Serial.print("Logging to: ");
  Serial.println(filename);
  Serial.println("Sequence:");
  Serial.println("1. Start cyclic STATUS and RPM heartbeat");
  Serial.println("2. Read DC bus voltage");
  Serial.println("3. Clear errors");
  Serial.println("4. Configure CAN timeout");
  Serial.println("5. Lock → Enable drive + STATUS check");
  Serial.println("6. Zero torque sanity check");
  Serial.println("7. Begin torque control");
  Serial.println("8. Disable drive");
  Serial.println("9. Dump CSV log contents to terminal");
  Serial.println("-------------------------------------------");
}

// ---------- Main loop ----------
void loop() {
  handleSerialInput();
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

// ---------- Step control ----------
void handleSerialInput() {
  if (Serial.available()) {
    Serial.read();
    currentStep++;
    executeStep(currentStep);
  }
}

void executeStep(int step) {
  switch (step) {
    case 1: 
      requestStatusCyclic(100);
      requestSpeedCyclic(100);
      break; // heartbeat every 100 ms
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
      disableDrive();
      Serial.println("Drive disabled");
      break;
    case 9:
      Serial.println("Dumping log contents to Serial...");
      dumpLogToSerial();
      break;
    default:
      Serial.println("All steps complete.");
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
  Serial.println("Sent: Request STATUS cyclic");
}

void requestStatusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D; msg.buf[1] = 0x40; msg.buf[2] = 0x00;
  sendCAN(msg);
  Serial.println("Sent: Request STATUS once");
}

void requestSpeedCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D; msg.buf[1] = 0x30; msg.buf[2] = interval_ms;
  sendCAN(msg);
  Serial.println("Sent: Request SPEED_ACTUAL cyclic");
}

void requestDCBusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D; msg.buf[1] = 0xEB; msg.buf[2] = 0x00;
  sendCAN(msg);
  Serial.println("Sent: Request DC bus voltage");
}

void clearErrors() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x8E; msg.buf[1] = 0x00; msg.buf[2] = 0x00;
  sendCAN(msg);
  Serial.println("Sent: Clear errors");
}

void configureCanTimeout(uint16_t ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0xD0;
  msg.buf[1] = ms & 0xFF;
  msg.buf[2] = (ms >> 8) & 0xFF;
  sendCAN(msg);
  Serial.printf("Sent: Configure CAN timeout (%d ms)\n", ms);
}

void enableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x51; msg.buf[1] = 0x04; msg.buf[2] = 0x00;
  sendCAN(msg);
  Serial.println("Sent: Lock/Disable");
  delay(100);
  msg.buf[1] = 0x00;
  sendCAN(msg);
  Serial.println("Sent: Enable drive");
}

void disableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x51; msg.buf[1] = 0x04; msg.buf[2] = 0x00;
  sendCAN(msg);
  Serial.println("Sent: Disable drive");
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

    Serial.printf("RX 0x%03X: ", msg.id);
    for (int i = 0; i < msg.len; i++) Serial.printf("%02X ", msg.buf[i]);
    Serial.println();

    if (msg.id == BAMOCAR_TX_ID && msg.len >= 3) {
      uint8_t reg = msg.buf[0];
      if (reg == 0x40) {
        uint16_t status = msg.buf[1] | (msg.buf[2] << 8);
        Serial.printf("→ STATUS 0x%04X | Enabled:%d Ready:%d Fault:%d\n",
                      status,
                      (status & 0x0001) ? 1 : 0,
                      (status & 0x0004) ? 1 : 0,
                      (status & 0x0040) ? 1 : 0);
      }
    }
  }
}

// ---------- Interpret all known registers ----------
String interpretBamocarMessage(const CAN_message_t &msg) {
  char buffer[100];
  uint8_t reg = msg.buf[0];

  // TX (to Bamocar)
  if (msg.id == BAMOCAR_RX_ID) {
    switch (reg) {
      case 0x3D: sprintf(buffer, "Request register 0x%02X", msg.buf[1]); break;
      case 0x8E: sprintf(buffer, "Clear all error flags"); break;
      case 0x51:
        if (msg.buf[1] == 0x04) sprintf(buffer, "Lock/Disable drive");
        else if (msg.buf[1] == 0x00) sprintf(buffer, "Enable drive");
        else sprintf(buffer, "Drive control command 0x%02X", msg.buf[1]);
        break;
      case 0x90: {
        int16_t tq = msg.buf[1] | (msg.buf[2] << 8);
        sprintf(buffer, "Set torque command = %d", tq);
        break;
      }
      case 0xD0: {
        int timeout = msg.buf[1] | (msg.buf[2] << 8);
        sprintf(buffer, "Set CAN timeout = %d ms", timeout);
        break;
      }
      default: sprintf(buffer, "Command 0x%02X sent", reg); break;
    }
    return String(buffer);
  }

  // RX (from Bamocar)
  if (msg.id == BAMOCAR_TX_ID) {
    switch (reg) {
      case 0x30: {
        int16_t rpm = msg.buf[1] | (msg.buf[2] << 8);
        sprintf(buffer, "Speed feedback = %d rpm", rpm);
        break;
      }
      case 0x40: {
        uint16_t status = msg.buf[1] | (msg.buf[2] << 8);
        sprintf(buffer, "Status word 0x%04X → Enabled:%d Ready:%d Fault:%d",
                status, (status & 0x0001), (status & 0x0004), (status & 0x0040));
        break;
      }
      case 0xEB: {
        uint16_t val = msg.buf[1] | (msg.buf[2] << 8);
        float volts = val * 0.1f;
        sprintf(buffer, "DC bus voltage = %.1f V", volts);
        break;
      }
      case 0x5F: {
        int16_t amps = msg.buf[1] | (msg.buf[2] << 8);
        sprintf(buffer, "Actual current = %.1f A", amps * 0.1f);
        break;
      }
      case 0xA0: {
        int16_t tq = msg.buf[1] | (msg.buf[2] << 8);
        sprintf(buffer, "Torque feedback = %.1f %%", tq / 327.67f);
        break;
      }
      default: sprintf(buffer, "Reply register 0x%02X", reg); break;
    }
    return String(buffer);
  }

  return "";
}

// ---------- Potentiometer control ----------
void updateTorqueFromPot() {
  int potValue = analogRead(A0);
  float potPercent = potValue / 4095.0f * MAX_ACCEL_PERCENT;
  currentTorque = (int16_t)(TORQUE_MAX * (potPercent / 100.0f));
  Serial.printf("Pot: %4d → %5.1f%% accel → Torque %d\n",
                potValue, potPercent, currentTorque);
}

void dumpLogToSerial() {
  logFile.flush();  // ensure everything is written
  logFile.close();

  File readFile = SD.open(logFile.name(), FILE_READ);
  if (!readFile) {
    Serial.println("Error reopening log file for reading.");
    return;
  }

  Serial.println("\n===== CSV LOG DUMP BEGIN =====");
  while (readFile.available()) {
    Serial.write(readFile.read());
  }
  Serial.println("\n===== CSV LOG DUMP END =====");

  readFile.close();
  // Reopen log file in append mode if user continues using CAN
  logFile = SD.open(logFile.name(), FILE_WRITE);
}
