// Serial1 -> Serial passthrough for viewing UART output in the Serial Monitor

#include <Arduino.h>

static constexpr uint32_t USB_BAUD = 115200;
static constexpr uint32_t UART_BAUD = 115200;

void setup() {
  Serial.begin(USB_BAUD);
  while (!Serial) {
    delay(10);
  }

#ifdef HAVE_HWSERIAL1
  Serial1.begin(UART_BAUD);
  Serial.println("Serial bridge ready: forwarding Serial1 -> Serial");
#else
  Serial.println("ERROR: This board/core does not provide Serial1 (HAVE_HWSERIAL1 not defined). الن");
#endif
}

void loop() {
#ifdef HAVE_HWSERIAL1
  while (Serial1.available() > 0) {
    const int b = Serial1.read();
    if (b >= 0) {
      Serial.write(static_cast<uint8_t>(b));
    }
  }
#endif
}