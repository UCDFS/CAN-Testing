#include "config.h"
#include "logging.h"
#include "bamocar.h"
#include "pedal.h"
#include "nextion.h"
#include "button.h"

// ---------- Global definitions ----------
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;
const int chipSelect = BUILTIN_SDCARD;
File logFile;
int currentStep = 0;
int16_t currentTorque = 0;
uint32_t lastTorqueSend = 0;
bool bamocarOnline = false;
int rpmFeedback = 0;
int statusWord = 0;
int16_t actualCurrent = 0;
int16_t motorTemp    = 0;
int16_t inverterTemp = 0;
float dcBusVoltage = 0.0;
int apps1Raw = 0;
int apps2Raw = 0;
bool pedalFault = false;
bool driveEnabled = false;

// ---------- Helpers ----------

// Blocks until button pressed. Keeps CAN drained and sends a heartbeat
// every 500 ms so the BAMOCAR CAN timeout never expires while waiting.
#define DRIVE_HOLD_MS 3000

static void waitForButton(const char *prompt) {
  nextionBootStatus(prompt, "press to continue");
  uint32_t lastHeartbeat = 0;
  while (!buttonPressed()) {
    readCanMessages();
    if (millis() - lastHeartbeat > 500) {
      requestStatusOnce();
      lastHeartbeat = millis();
    }
    delay(10);
  }
}

// Blocks until button is held continuously for DRIVE_HOLD_MS.
// Releasing and re-pressing resets the timer.
static void waitForButtonHeld(const char *prompt) {
  nextionBootStatus(prompt, "hold 3s to enable");
  uint32_t lastHeartbeat = 0;
  uint32_t holdStart = 0;
  for (;;) {
    readCanMessages();
    if (millis() - lastHeartbeat > 500) {
      requestStatusOnce();
      lastHeartbeat = millis();
    }
    if (digitalRead(BUTTON_PIN) == HIGH) {
      if (holdStart == 0) holdStart = millis();
      if (millis() - holdStart >= DRIVE_HOLD_MS) break;
    } else {
      holdStart = 0;
    }
    delay(10);
  }
  buttonResetHold();  // sync hold state for loop re-enable
}

static bool pedalAtRest() {
  int raw = analogRead(APPS1_PIN);
  float pct = (float)(APPS1_REST - raw) * 100.0f / (float)(APPS1_REST - APPS1_FULL);
  return pct < PEDAL_DEADBAND_PERCENT;
}

// ---------- Step Execution ----------
void executeStep(int step) {
  currentStep = step;
  switch (step) {
    case 1: requestStatusCyclic(100); requestSpeedCyclic(100); requestCurrentCyclic(100); requestTempsCyclic(500); break;
    case 2: requestDCBusOnce();       break;
    case 3: clearErrors();            break;
    case 4: configureCanTimeout(2000); break;
    case 5:
      clearErrors();
      delay(100);
      enableDrive();
      requestStatusOnce();
      driveEnabled = true;
      break;
    case 6: sendTorqueCommand(0);     break;
    case 7:                           break;
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);  // wait up to 2s for USB serial
  Serial.println("[BOOT] Serial ready");

  buttonInit();
  Serial.println("[BOOT] Button init OK");

  nextionBegin();
  nextionBootStatus("INITIALISING");
  Serial.println("[BOOT] Nextion init OK");

  Can1.begin();
  Can1.setBaudRate(500000);
  analogReadResolution(12);
  Serial.println("[BOOT] CAN1 init OK @ 500kbps");

  if (!SD.begin(chipSelect)) {
    nextionBootStatus("SD: NONE", "logging disabled");
    Serial.println("[BOOT] SD: not detected, logging disabled");
  } else {
    String filename = generateFilename();
    logFile = SD.open(filename.c_str(), FILE_WRITE);
    if (!logFile) {
      nextionBootStatus("SD: ERROR", "file open failed");
      Serial.println("[BOOT] SD: file open failed");
    } else {
      logWriteHeader();
      nextionBootStatus("SD: OK", filename.c_str());
      Serial.print("[BOOT] SD: logging to ");
      Serial.println(filename);
    }
  }
  delay(800);

  // --- Step 1: press to start, wait for BAMOCAR ---
  Serial.println("[STEP 1] Waiting for button...");
  waitForButton("PRESS TO START");
  Serial.println("[STEP 1] Starting cyclic requests, waiting for BAMOCAR online...");
  nextionBootStatus("WAITING BAMOCAR");
  executeStep(1);
  while (!bamocarOnline) {
    requestStatusOnce();
    readCanMessages();
    delay(300);
  }
  nextionBootStatus("BAMOCAR ONLINE");
  Serial.print("[STEP 1] BAMOCAR online. statusWord=0x");
  Serial.println(statusWord, HEX);
  delay(400);

  // --- Steps 2-4: DC bus, clear errors, CAN timeout (automatic) ---
  executeStep(2);
  delay(200);
  readCanMessages();  // process DC bus response before printing
  Serial.print("[STEP 2] DC bus voltage: ");
  Serial.print(dcBusVoltage, 1);
  Serial.println(" V");

  executeStep(3);
  delay(200);
  Serial.println("[STEP 3] Clear errors sent");

  executeStep(4);
  delay(200);
  Serial.println("[STEP 4] CAN timeout configured (2000ms)");

  // --- Steps 5-7: hold 3s to enable drive and enter torque control ---
  Serial.println("[STEP 5] Hold button 3s to enable drive...");
  waitForButtonHeld("HOLD 3s: ENABLE");
  nextionBootStatus("ENABLING DRIVE");
  Serial.println("[STEP 5] Enabling drive...");
  executeStep(5);
  delay(500);
  executeStep(6);
  delay(500);
  Serial.println("[STEP 5/6] Drive enabled, zero torque sent");

  // --- Wait for pedal release (automatic) ---
  nextionBootStatus("RELEASE PEDAL");
  Serial.println("[PEDAL] Waiting for pedal release...");
  while (!pedalAtRest()) {
    readCanMessages();
    delay(50);
  }
  Serial.print("[PEDAL] Pedal at rest. APPS1=");
  Serial.print(analogRead(APPS1_PIN));
  Serial.print("  APPS2=");
  Serial.println(analogRead(APPS2_PIN));

  // --- Step 7: enter torque control (automatic after pedal rest) ---
  executeStep(7);
  nextionPage(NX_PAGE_DRIVE);
  Serial.println("[STEP 7] Entering torque control loop");
}

// ---------- Loop ----------
void loop() {
  readCanMessages();

  // --- Drive enable/disable toggle ---
  if (currentStep == 7) {
    bool pressed = buttonPressed();  // always call to keep state machine in sync
    if (driveEnabled && pressed) {
      // Short press disables drive immediately
      disableDrive();
      driveEnabled = false;
      sendTorqueCommand(0);
      currentTorque = 0;
      nextionText(NX_DRIVE_STATE, "OFF");
      buttonResetHold();  // don't count this press toward re-enable hold
    } else if (!driveEnabled && buttonHeldFor(DRIVE_HOLD_MS)) {
      // 3-second hold required to re-enable
      if (pedalAtRest()) {
        clearErrors();
        delay(100);
        enableDrive();
        driveEnabled = true;
        nextionText(NX_DRIVE_STATE, "ON");
      } else {
        nextionText(NX_DRIVE_STATE, "RELEASE PEDAL");
      }
    }
  }

  // --- Torque control ---
  // Always send torque (0 when disabled) to keep BAMOCAR CAN watchdog alive.
  if (currentStep == 7 && millis() - lastTorqueSend >= 20) {
    if (driveEnabled) updateTorqueFromPedal();
    else currentTorque = 0;
    sendTorqueCommand(currentTorque);
    logSensor(apps1Raw, apps2Raw, pedalFault, currentTorque, rpmFeedback, (int)(dcBusVoltage * 10));
    lastTorqueSend = millis();
  }

  // --- Periodic flush + display update + debug print ---
  static uint32_t lastFlush = 0;
  if (millis() - lastFlush > 500) {
    logFlush();
    lastFlush = millis();
    requestDCBusOnce();
    nextionUpdateDrive();
    Serial.print("[LOOP] drive="); Serial.print(driveEnabled ? "ON" : "OFF");
    Serial.print("  fault=");      Serial.print(pedalFault  ? "YES" : "NO");
    Serial.print("  torque=");     Serial.print(currentTorque);
    Serial.print("  rpm=");        Serial.print((int)((float)rpmFeedback / 32767.0f * RPM_MAX));
    Serial.print("  iact=");       Serial.print(actualCurrent);
    Serial.print("  mtemp=");      Serial.print(motorTemp);
    Serial.print("C  itemp=");     Serial.print(inverterTemp); Serial.print("C");
    Serial.print("  dcbus=");      Serial.print(dcBusVoltage, 1);
    Serial.print("V  A1=");        Serial.print(apps1Raw);
    Serial.print("  A2=");         Serial.println(apps2Raw);
    // Decode STATUS word key bits
    Serial.print("[STATUS 0x");    Serial.print(statusWord, HEX);
    Serial.print("] active=");     Serial.print((statusWord >> 1) & 1);
    Serial.print("  fault=");      Serial.print((statusWord >> 2) & 1);
    Serial.print("  warn=");       Serial.print((statusWord >> 3) & 1);
    Serial.print("  ilim=");       Serial.println((statusWord >> 5) & 1);
    if (pedalFault) {
      float pct1 = (float)(APPS1_REST - apps1Raw) * 100.0f / (float)(APPS1_REST - APPS1_FULL);
      float pct2 = (float)(APPS2_REST - apps2Raw) * 100.0f / (float)(APPS2_REST - APPS2_FULL);
      Serial.print("[FAULT] APPS plausibility: A1=");
      Serial.print(pct1, 1);
      Serial.print("%  A2=");
      Serial.print(pct2, 1);
      Serial.print("%  delta=");
      Serial.print(fabsf(pct1 - pct2), 1);
      Serial.println("%");
    }
  }
}
