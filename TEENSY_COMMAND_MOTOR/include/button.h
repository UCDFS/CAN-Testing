#pragma once
#include "config.h"

void buttonInit();
bool buttonPressed();              // true once per press (rising edge, debounced)
bool buttonHeldFor(uint32_t ms);  // true once per hold event after ms milliseconds
void buttonResetHold();            // clear hold state after a short press
uint32_t buttonHoldElapsed();      // ms held so far, 0 if not currently held
