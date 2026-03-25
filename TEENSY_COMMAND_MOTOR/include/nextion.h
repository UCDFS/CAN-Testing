#pragma once
#include "config.h"

// Nextion connected to Serial7 (TX7/RX7 on Teensy 4.1)
#define NEXTION_SERIAL Serial7
#define NEXTION_BAUD   115200

// ---- Page IDs ----
#define NX_PAGE_BOOT   0
#define NX_PAGE_DRIVE  1

// ---- Boot page component names (from Nextion Editor) ----
#define NX_BOOT_STATUS "t_status"   // text: current phase
#define NX_BOOT_DETAIL "t_detail"   // text: secondary detail

// ---- Drive page component names ----
#define NX_DRIVE_SPEED  "n_speed"   // number: vehicle speed, integer km/h
#define NX_DRIVE_RPM    "n_rpm"     // number: motor RPM
#define NX_DRIVE_TORQUE "n_torque"  // number: torque command, 0-100%
#define NX_DRIVE_DCBUS  "n_dcbus"   // number: DC bus voltage, whole volts
#define NX_DRIVE_FAULT  "t_fault"   // text: "OK" or "FAULT"
#define NX_DRIVE_STATE  "t_drive"   // text: "DRIVE: ON" or "DRIVE: OFF"
#define NX_DRIVE_MTEMP  "n_mtemp"   // number: motor temperature, °C
#define NX_DRIVE_ITEMP  "n_itemp"   // number: inverter temperature, °C

void nextionBegin();
void nextionPage(uint8_t page);
void nextionText(const char *component, const char *text);
void nextionNum(const char *component, int value);
void nextionBootStatus(const char *phase, const char *detail = "");
void nextionUpdateDrive();
