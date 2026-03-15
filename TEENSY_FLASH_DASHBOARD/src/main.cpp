#include <Arduino.h>

void setup() {
  Serial.begin(115200);   // USB serial to your Mac
  Serial1.begin(115200);  // UART to Nextion
}

void loop() {
  // Forward data both ways
  while (Serial.available()) Serial1.write(Serial.read());
  while (Serial1.available()) Serial.write(Serial1.read());
}
