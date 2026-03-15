#pragma once
#include "config.h"

void requestStatusCyclic(uint8_t interval_ms);
void requestStatusOnce();
void requestSpeedCyclic(uint8_t interval_ms);
void requestDCBusOnce();
void clearErrors();
void enableDrive();
void disableDrive();
void sendTorqueCommand(int16_t torqueValue);
void configureCanTimeout(uint16_t ms);
void readCanMessages();
