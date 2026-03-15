#include "button.h"

#define DEBOUNCE_MS 50

// --- Edge detect state ---
static bool     _lastState = HIGH;
static uint32_t _lastTime  = 0;

// --- Hold detect state ---
static uint32_t _pressStart = 0;
static bool     _holdFired  = false;

void buttonInit() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

// Returns true exactly once per press (falling edge, debounced).
bool buttonPressed() {
  bool state = digitalRead(BUTTON_PIN);
  uint32_t now = millis();

  if (_lastState == HIGH && state == LOW && (now - _lastTime) > DEBOUNCE_MS) {
    _lastTime  = now;
    _lastState = LOW;
    return true;
  }

  _lastState = state;
  return false;
}

// Returns true exactly once per hold event, after button has been held
// continuously for ms milliseconds. Resets when button is released.
bool buttonHeldFor(uint32_t ms) {
  bool low = (digitalRead(BUTTON_PIN) == LOW);
  if (low) {
    if (_pressStart == 0) _pressStart = millis();
    if (!_holdFired && (millis() - _pressStart) >= ms) {
      _holdFired = true;
      return true;
    }
  } else {
    _pressStart = 0;
    _holdFired  = false;
  }
  return false;
}

// Clears hold tracking — call after a press that should not count toward
// the next hold (e.g. after disabling drive via a short press).
void buttonResetHold() {
  _pressStart = 0;
  _holdFired  = false;
}
