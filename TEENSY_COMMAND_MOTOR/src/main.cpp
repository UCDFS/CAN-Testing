#include "config.h"
#include "logging.h"
#include "bamocar.h"
#include "pedal.h"
#include "nextion.h"
#include "button.h"
#include "MpuController.h"

// ---------- Global definitions ----------
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;
const int chipSelect = BUILTIN_SDCARD;
File logFile;
int8_t currentStep = 0;
int16_t currentTorque = 0;
uint32_t lastTorqueSend = 0;
bool bamocarOnline = false;
int16_t rpmFeedback = 0;
int16_t statusWord = 0;
uint32_t bamocarErrorWord = 0;
int16_t actualCurrent = 0;
float motorTemp    = 0.0f;
float inverterTemp = 0.0f;
float dcBusVoltage = 0.0;
int16_t apps1Raw = 0;
int16_t apps2Raw = 0;
bool pedalFault = false;
bool driveEnabled = false;
uint32_t lastBAMOCARRx = 0;
Adafruit_MPU6050 mpu;
MpuController mpuController(mpu);

// ---------- Helpers ----------

// Fills buf (must be ≥13 bytes) with an ASCII progress bar like [####      ]
// elapsed/total drives fill level; bar is always 10 chars wide.
static void holdBar(uint32_t elapsed, uint32_t total, char *buf) {
  int filled = (int)(elapsed * 10 / total);
  if (filled > 10) filled = 10;
  buf[0] = '[';
  for (int i = 0; i < 10; i++) buf[i + 1] = (i < filled) ? '#' : ' ';
  buf[11] = ']';
  buf[12] = '\0';
}

// Blocks until button pressed. Keeps CAN drained and sends a heartbeat
// every 500 ms so the BAMOCAR CAN timeout never expires while waiting.
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
// Releasing and re-pressing resets the timer. t_detail shows a progress bar.
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
      uint32_t elapsed = millis() - holdStart;
      if (elapsed >= DRIVE_HOLD_MS) break;
      char bar[13];
      holdBar(elapsed, DRIVE_HOLD_MS, bar);
      nextionText(NX_BOOT_DETAIL, bar);
    } else {
      if (holdStart != 0) nextionText(NX_BOOT_DETAIL, "hold 3s to enable");
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

// Full re-enable handshake: mirrors startup steps 3-7.
// Blocks until handshake is complete and pedal is released.
static void reenableDriveSequence() {
  nextionBootStatus("RE-ENABLE", "clearing errors...");
  requestStatusCyclic(100);
  requestErrorsCyclic(100);
  requestSpeedCyclic(100);
  requestCurrentCyclic(100);
  requestTempsCyclic(500);
  clearErrors();
  delay(200);
  readCanMessages();

  nextionBootStatus("RE-ENABLE", "configuring timeout...");
  configureCanTimeout(CAN_TIMEOUT_MS);
  delay(200);

  nextionBootStatus("RE-ENABLE", "enabling drive...");
  clearErrors();
  delay(100);
  enableDrive();
  requestStatusOnce();
  delay(500);
  sendTorqueCommand(0);

  nextionBootStatus("RELEASE PEDAL");
  while (!pedalAtRest()) {
    readCanMessages();
    delay(50);
  }

  driveEnabled = true;
  nextionPage(NX_PAGE_DRIVE);
}

// ---------- Step Execution ----------
void executeStep(int step) {
  currentStep = step;
  switch (step) {
    case 1: requestStatusCyclic(100); requestErrorsCyclic(100); requestSpeedCyclic(100); requestCurrentCyclic(100); requestTempsCyclic(500); break;
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
  buttonInit();
  nextionBegin();
  nextionBootStatus("INITIALISING");

  Can1.begin();
  Can1.setBaudRate(500000);
  analogReadResolution(12);

  mpuController.begin();

  if (!SD.begin(chipSelect)) {
    nextionBootStatus("SD: NONE", "logging disabled");
  } else {
    String filename = generateFilename();
    logFile = SD.open(filename.c_str(), FILE_WRITE);
    if (!logFile) {
      nextionBootStatus("SD: ERROR", "file open failed");
    } else {
      logWriteHeader();
      nextionBootStatus("SD: OK", filename.c_str());
    }
  }
  delay(800);

  // --- Step 1: press to start, wait for BAMOCAR ---
  waitForButton("PRESS TO START");
  nextionBootStatus("WAITING BAMOCAR");
  executeStep(1);
  while (!bamocarOnline) {
    requestStatusOnce();
    readCanMessages();
    delay(300);
  }
  nextionBootStatus("BAMOCAR ONLINE");
  delay(400);

  // --- Steps 2-4: DC bus, clear errors, CAN timeout (automatic) ---
  executeStep(2);
  delay(200);
  readCanMessages();

  executeStep(3);
  delay(200);

  executeStep(4);
  delay(200);

  // --- Steps 5-7: hold 3s to enable drive and enter torque control ---
  waitForButtonHeld("HOLD 3s: ENABLE");
  nextionBootStatus("ENABLING DRIVE");
  executeStep(5);
  delay(500);
  executeStep(6);
  delay(500);

  // --- Wait for pedal release (automatic) ---
  nextionBootStatus("RELEASE PEDAL");
  while (!pedalAtRest()) {
    readCanMessages();
    delay(50);
  }

  // --- Step 7: enter torque control ---
  executeStep(7);
  nextionPage(NX_PAGE_DRIVE);
}

// ---------- Loop ----------
void loop() {
  readCanMessages();

  // --- BAMOCAR heartbeat timeout ---
  static bool bamocarOffline = false;
  if (currentStep == 7 && bamocarOnline && millis() - lastBAMOCARRx > 500) {
    if (!bamocarOffline) {
      bamocarOffline = true;
      bamocarOnline = false;
      driveEnabled = false;
      sendTorqueCommand(0);
      currentTorque = 0;
      nextionPage(NX_PAGE_BOOT);
      nextionBootStatus("BAMOCAR OFFLINE", "waiting...");
    }
  } else if (bamocarOffline && bamocarOnline) {
    bamocarOffline = false;  // frames resumed; user must re-enable via 3s hold
  }

  // --- BAMOCAR error detection ---
  // On first error: disable drive, switch to boot page, show ERROR + fault name.
  static bool inErrorState = false;
  if (currentStep == 7 && bamocarErrorWord != 0) {
    if (!inErrorState) {
      inErrorState = true;
      driveEnabled = false;
      sendTorqueCommand(0);
      currentTorque = 0;
      char detail[32];
      bamocarErrorDescription(bamocarErrorWord, detail, sizeof(detail));
      nextionPage(NX_PAGE_BOOT);
      nextionBootStatus("ERROR", detail);
    }
  } else if (inErrorState && bamocarErrorWord == 0) {
    inErrorState = false;
  }

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
    } else if (!driveEnabled) {
      // Update hold progress bar on t_drive while button is being held
      static uint32_t lastBarUpdate = 0;
      if (millis() - lastBarUpdate >= 50) {
        lastBarUpdate = millis();
        uint32_t elapsed = buttonHoldElapsed();
        if (elapsed > 0) {
          char bar[13];
          holdBar(elapsed, DRIVE_HOLD_MS, bar);
          nextionText(NX_DRIVE_STATE, bar);
        }
      }
      // 3-second hold triggers full re-enable handshake (mirrors startup)
      if (buttonHeldFor(DRIVE_HOLD_MS)) {
        reenableDriveSequence();
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
    mpuController.logTelemetry();
    lastTorqueSend = millis();
  }

  // --- Periodic flush + display update ---
  static uint32_t lastFlush = 0;
  if (millis() - lastFlush > 500) {
    logFlush();
    lastFlush = millis();
    requestDCBusOnce();
    nextionUpdateDrive();
    // nextionUpdateDrive writes "OFF" to t_drive; restore bar if still holding
    if (!driveEnabled) {
      uint32_t elapsed = buttonHoldElapsed();
      if (elapsed > 0) {
        char bar[13];
        holdBar(elapsed, DRIVE_HOLD_MS, bar);
        nextionText(NX_DRIVE_STATE, bar);
      }
    }
  }
}
