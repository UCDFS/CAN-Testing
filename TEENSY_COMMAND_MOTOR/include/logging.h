#pragma once
#include "config.h"

// Schema
// CAN frame:    C,<ms>,<dir>,<id_hex>,<len>,<b0_hex>,...,<b7_hex>
// Sensor snap:  S,<ms>,<apps1_raw>,<apps2_raw>,<pedal_fault>,<torque_cmd>,<rpm>,<dcbus_dV>
// id and bytes are uppercase hex without 0x prefix.
// Unused CAN byte fields are empty (fixed 13-column records).
// dcbus_dV = dcBusVoltage * 10, integer decivolts.
// pedal_fault = 1 if APPS plausibility fault active, else 0.

String generateFilename();
void logWriteHeader();
void logCANFrame(const CAN_message_t &msg, const char *dir);
void logSensor(int apps1Raw, int apps2Raw, bool fault, int16_t torque, int rpm, int dcbusDV);
void logFlush();
