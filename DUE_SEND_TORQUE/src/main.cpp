#include <Arduino.h>
#include <due_can.h>
#include <can_common.h>

// Bamocar register information
const uint32_t BAMOCAR_RX_ID = 0x201;  // Commands sent to the drive
const uint32_t BAMOCAR_TX_ID = 0x181;  // Telemetry received from the drive

// User-configurable parameters
const uint8_t MAX_ACCEL_PERCENT = 50;         // Cap potentiometer travel to this torque percentage
const int16_t TORQUE_MAX = 32767;             // +150% torque command (per Bamocar manual)
const uint16_t TORQUE_SEND_INTERVAL_MS = 20;  // Torque update cadence while in control mode
const uint8_t SPEED_REQUEST_INTERVAL_MS = 100;

// Hardware configuration
const uint8_t TORQUE_INPUT_PIN = A0;

// Runtime state
uint8_t currentStep = 0;
int16_t currentTorqueCommand = 0;
uint32_t lastTorqueSend = 0;

// Forward declarations
void printStepOverview();
void handleSerialInput();
void executeStep(uint8_t step);
void requestStatusOnce();
void requestSpeedCyclic(uint8_t intervalMs);
void enableDrive();
void disableDrive();
void sendTorqueCommand(int16_t torqueValue);
void updateTorqueFromPot();
void readCanMessages();

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    // Wait for USB serial connection
  }

  analogReadResolution(12);  // 0-4095 range on Arduino Due
  pinMode(TORQUE_INPUT_PIN, INPUT);

  Serial.println("=== Bamocar CAN Interactive Test with Potentiometer ===");
  printStepOverview();

  // Initialize CAN controller (Arduino Due CAN0, 500 kbps)
  Can0.begin(CAN_BPS_500K);
}

void loop() {
  handleSerialInput();
  readCanMessages();

  if (currentStep == 4) {
    const uint32_t now = millis();
    if (now - lastTorqueSend >= TORQUE_SEND_INTERVAL_MS) {
      updateTorqueFromPot();
      sendTorqueCommand(currentTorqueCommand);
      lastTorqueSend = now;
    }
  }
}

void printStepOverview() {
  Serial.println("Press any key to advance through the test sequence:");
  Serial.println("1. Request STATUS once");
  Serial.println("2. Request cyclic SPEED_ACTUAL updates");
  Serial.println("3. Enable the drive");
  Serial.println("4. Start torque control via potentiometer on A0");
  Serial.println("5. Stop torque (command 0)");
  Serial.println("6. Disable the drive");
  Serial.println("-------------------------------------------");
  Serial.print("Max accelerator cap: ");
  Serial.print(MAX_ACCEL_PERCENT);
  Serial.println('%');
}

void handleSerialInput() {
  if (Serial.available()) {
    // Consume all characters to avoid queueing multiple step advances
    while (Serial.available()) {
      Serial.read();
    }

    currentStep++;
    executeStep(currentStep);
  }
}

void executeStep(uint8_t step) {
  switch (step) {
    case 1:
      requestStatusOnce();
      break;
    case 2:
      requestSpeedCyclic(SPEED_REQUEST_INTERVAL_MS);
      break;
    case 3:
      enableDrive();
      break;
    case 4:
      Serial.println("Potentiometer torque control active (0-100% travel → 0-" + String(MAX_ACCEL_PERCENT) + "%).");
      lastTorqueSend = 0;  // Send immediately on next loop iteration
      break;
    case 5:
      currentTorqueCommand = 0;
      sendTorqueCommand(0);
      Serial.println("Torque command forced to 0.");
      break;
    case 6:
      disableDrive();
      Serial.println("Drive disabled.");
      break;
    default:
      Serial.println("Sequence complete. Reset the board to restart.");
      break;
  }
}

void requestStatusOnce() {
  CAN_FRAME frame;
  frame.id = BAMOCAR_RX_ID;
  frame.extended = false;
  frame.priority = 0;
  frame.rtr = 0;
  frame.length = 3;
  frame.data.bytes[0] = 0x3D;
  frame.data.bytes[1] = 0x40;
  frame.data.bytes[2] = 0x00;

  Can0.sendFrame(frame);
  Serial.println("Sent: Request STATUS once (0x3D 0x40 0x00)");
}

void requestSpeedCyclic(uint8_t intervalMs) {
  CAN_FRAME frame;
  frame.id = BAMOCAR_RX_ID;
  frame.extended = false;
  frame.priority = 0;
  frame.rtr = 0;
  frame.length = 3;
  frame.data.bytes[0] = 0x3D;
  frame.data.bytes[1] = 0x30;
  frame.data.bytes[2] = intervalMs;

  Can0.sendFrame(frame);
  Serial.print("Sent: Request SPEED_ACTUAL cyclic (0x3D 0x30 0x");
  Serial.print(intervalMs, HEX);
  Serial.println(")");
}

void enableDrive() {
  CAN_FRAME frame;
  frame.id = BAMOCAR_RX_ID;
  frame.extended = false;
  frame.priority = 0;
  frame.rtr = 0;
  frame.length = 3;
  frame.data.bytes[0] = 0x51;
  frame.data.bytes[1] = 0x00;
  frame.data.bytes[2] = 0x00;

  Can0.sendFrame(frame);
  Serial.println("Sent: Enable drive (0x51 0x00 0x00)");
}

void disableDrive() {
  CAN_FRAME frame;
  frame.id = BAMOCAR_RX_ID;
  frame.extended = false;
  frame.priority = 0;
  frame.rtr = 0;
  frame.length = 3;
  frame.data.bytes[0] = 0x51;
  frame.data.bytes[1] = 0x04;
  frame.data.bytes[2] = 0x00;

  Can0.sendFrame(frame);
  Serial.println("Sent: Disable drive (0x51 0x04 0x00)");
}

void sendTorqueCommand(int16_t torqueValue) {
  CAN_FRAME frame;
  frame.id = BAMOCAR_RX_ID;
  frame.extended = false;
  frame.priority = 0;
  frame.rtr = 0;
  frame.length = 3;
  frame.data.bytes[0] = 0x90;  // TORQUE_CMD register
  frame.data.bytes[1] = static_cast<uint8_t>(torqueValue & 0xFF);        // Low byte (little-endian)
  frame.data.bytes[2] = static_cast<uint8_t>((torqueValue >> 8) & 0xFF); // High byte

  Can0.sendFrame(frame);
  Serial.print("Sent torque command: ");
  Serial.println(torqueValue);
}

void updateTorqueFromPot() {
  const int potValue = analogRead(TORQUE_INPUT_PIN);  // 0-4095
  const float accelPercent = (static_cast<float>(potValue) / 4095.0f) * MAX_ACCEL_PERCENT;
  const float torquePercentOf150 = accelPercent / 150.0f;  // 150% corresponds to TORQUE_MAX

  int32_t scaledTorque = static_cast<int32_t>(TORQUE_MAX * torquePercentOf150);
  if (scaledTorque > TORQUE_MAX) {
    scaledTorque = TORQUE_MAX;
  } else if (scaledTorque < 0) {
    scaledTorque = 0;
  }

  currentTorqueCommand = static_cast<int16_t>(scaledTorque);

  Serial.print("Pot: ");
  Serial.print(potValue);
  Serial.print(" → ");
  Serial.print(accelPercent, 1);
  Serial.print("% accel → Torque command ");
  Serial.println(currentTorqueCommand);
}

void readCanMessages() {
  CAN_FRAME incoming;
  while (Can0.available() > 0) {
    Can0.read(incoming);

    Serial.print("RX ID: 0x");
    Serial.print(incoming.id, HEX);
    Serial.print("  Data: ");
    for (uint8_t i = 0; i < incoming.length; i++) {
      if (incoming.data.bytes[i] < 0x10) {
        Serial.print('0');
      }
      Serial.print(incoming.data.bytes[i], HEX);
      Serial.print(' ');
    }
    Serial.println();

    if (incoming.id == BAMOCAR_TX_ID && incoming.length >= 3 && incoming.data.bytes[0] == 0x40) {
      const uint16_t status = incoming.data.bytes[1] | (incoming.data.bytes[2] << 8);
      Serial.print("→ Drive STATUS: 0x");
      Serial.println(status, HEX);
    }
  }
}