#pragma once
#include <FlexCAN_T4.h>
#include <SD.h>
#include <SPI.h>

// ---------- BAMOCAR IDs ----------
#define BAMOCAR_RX_ID 0x201  // Teensy → Bamocar
#define BAMOCAR_TX_ID 0x181  // Bamocar → Teensy

// ---------- Motor constants ----------
#define MAX_ACCEL_PERCENT 100
#define TORQUE_MAX 32767
#define RPM_MAX    6000    // EMRAX 208: BAMOCAR inverter cap (1000 Hz, 10 pole pairs)
#define CAN_TIMEOUT_MS 100  // if no messages received within this time, assume BAMOCAR offline

// ---------- APPS (pedal sensor) config ----------
// Set REST to ADC reading with pedal physically released.
// Set FULL to ADC reading at maximum pedal travel.
// Formula handles both rising and falling sensor directions.
#define APPS1_PIN  A0
#define APPS2_PIN  A1
#define APPS1_REST 2884   // calibrate: ADC at physical zero
#define APPS1_FULL 1835   // calibrate: ADC at full pedal
#define APPS2_REST 2910   // calibrate: ADC at physical zero
#define APPS2_FULL 1845   // calibrate: ADC at full pedal

// Dead band: any reading below this % is treated as zero torque.
// Absorbs calibration offset so pedal at rest never produces creep.
#define PEDAL_DEADBAND_PERCENT    3

// Plausibility: if APPS1 and APPS2 disagree by more than this %, fault.
// Fault clears only when both sensors return below the dead band.
#define PEDAL_PLAUSIBILITY_PERCENT 10

// ---------- Button ----------
#define BUTTON_PIN 2

// ---------- Drive ----------
#define DRIVE_HOLD_MS 3000  // ms the button must be held to enable/re-enable drive

// ---------- CAN bus ----------
extern FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;
extern const int chipSelect;

// ---------- Globals ----------
extern File logFile;
extern int currentStep;
extern int16_t currentTorque;
extern uint32_t lastTorqueSend;
extern bool bamocarOnline;
extern int rpmFeedback;
extern int statusWord;
extern uint32_t bamocarErrorWord;
extern int16_t actualCurrent;
extern float motorTemp;
extern float inverterTemp;
extern float dcBusVoltage;
extern int apps1Raw;
extern int apps2Raw;
extern bool pedalFault;
extern bool driveEnabled;
