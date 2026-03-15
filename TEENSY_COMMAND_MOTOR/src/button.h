#pragma once
#include "config.h"

#define BUTTON_PIN 2  // fill in actual pin

void buttonInit();
bool buttonPressed();              // true once per press (falling edge, debounced)
bool buttonHeldFor(uint32_t ms);  // true once per hold event after ms milliseconds
void buttonResetHold();            // clear hold state after a short press
