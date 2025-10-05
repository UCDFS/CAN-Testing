#include <Arduino.h>
#include <due_can.h>
#include <can_common.h>

// Maximum allowed accelerator position as a percentage of full torque
const uint8_t MAX_ACCEL_POS = 50;
const int16_t TORQUE_FULL_SCALE = 32767;
const int16_t MAX_TORQUE_COMMAND = (int32_t)MAX_ACCEL_POS * TORQUE_FULL_SCALE / 100;

const uint8_t TORQUE_INPUT_PIN = A0;
const int ANALOG_CENTER = 512;       // Midpoint of 10-bit ADC range for zero torque
const int ANALOG_RANGE = 1023;       // Max reading from analogRead (0-1023)
const uint8_t ANALOG_DEADZONE = 8;   // Ignore small variations around the center

// CAN message structures
CAN_FRAME torqueFrame;
CAN_FRAME enableFrame;
CAN_FRAME lockFrame;

bool triggerEnableSequence = false;

void sendTorqueFrame() {
  int rawPot = analogRead(TORQUE_INPUT_PIN);
  int centered = rawPot - ANALOG_CENTER;

  if (abs(centered) <= ANALOG_DEADZONE) {
    centered = 0;
  }

  int32_t scaledCommand = (int32_t)centered * MAX_TORQUE_COMMAND / ANALOG_CENTER;

  if (scaledCommand > MAX_TORQUE_COMMAND) {
    scaledCommand = MAX_TORQUE_COMMAND;
  } else if (scaledCommand < -MAX_TORQUE_COMMAND) {
    scaledCommand = -MAX_TORQUE_COMMAND;
  }

  int16_t torqueCommand = (int16_t)scaledCommand;
  uint16_t commandBytes = static_cast<uint16_t>(torqueCommand);

  torqueFrame.data.bytes[1] = commandBytes & 0xFF;
  torqueFrame.data.bytes[2] = (commandBytes >> 8) & 0xFF;

  Can0.sendFrame(torqueFrame);
  Serial.print("Sent torque frame: ");
  Serial.print(torqueFrame.data.bytes[0], HEX);
  Serial.print(" ");
  Serial.print(torqueFrame.data.bytes[1], HEX);
  Serial.print(" ");
  Serial.print(torqueFrame.data.bytes[2], HEX);
  Serial.print(" over CAN, ID:");
  Serial.println(torqueFrame.id, HEX);
  Serial.print("Pot raw: ");
  Serial.print(rawPot);
  Serial.print(" -> torque command: ");
  Serial.print(torqueCommand);
  Serial.print(" (");
  Serial.print((float)torqueCommand * 100.0f / TORQUE_FULL_SCALE, 2);
  Serial.println("%)");
}

void sendLockFrame() {
  Can0.sendFrame(lockFrame);
  Serial.print("Sent lock frame: ");
  Serial.print(lockFrame.data.bytes[0], HEX);
  Serial.print(" ");
  Serial.print(lockFrame.data.bytes[1], HEX);
  Serial.print(" ");
  Serial.print(lockFrame.data.bytes[2], HEX);
  Serial.print(" over CAN, ID:");
  Serial.println(lockFrame.id, HEX);
}

void sendEnableFrame() {
  Can0.sendFrame(enableFrame);
  Serial.print("Sent enable frame: ");
  Serial.print(enableFrame.data.bytes[0], HEX);
  Serial.print(" ");
  Serial.print(enableFrame.data.bytes[1], HEX);
  Serial.print(" ");
  Serial.print(enableFrame.data.bytes[2], HEX);
  Serial.print(" over CAN, ID:");
  Serial.println(enableFrame.id, HEX);
}

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }

  pinMode(TORQUE_INPUT_PIN, INPUT);
  
  Serial.println("Arduino Due CAN Bus Example");
  Serial.println("Sending efficient 8-byte CAN message");
  
  // Initialize CAN0 at 500kbps to match your listener
  // CAN0 uses pins CANRX0 (pin 70) and CANTX0 (pin 71) on Arduino Due
  Can0.begin(CAN_BPS_500K);
  
  // Set up an efficient 8-byte CAN message
  torqueFrame.id = 0x201;
  torqueFrame.extended = false;
  torqueFrame.priority = 0;
  torqueFrame.rtr = 0;
  torqueFrame.length = 3;

  // Byte 0: Set the REGID for Torque Command Value
  // According to Example 5, the REGID is 0x90 
  torqueFrame.data.bytes[0] = 0x90;

  // Bytes 1 & 2: Runtime torque command (little-endian, signed 16-bit)
  torqueFrame.data.bytes[1] = 0x00; // Low byte initialized to zero torque
  torqueFrame.data.bytes[2] = 0x00; // High byte initialized to zero torque

  // Configure lock frame (MODE-BIT register 0x51, bit 2 set to 1)
  lockFrame.id = 0x201;
  lockFrame.extended = false;
  lockFrame.priority = 0;
  lockFrame.rtr = 0;
  lockFrame.length = 3;
  lockFrame.data.bytes[0] = 0x51;
  lockFrame.data.bytes[1] = 0x04; // Little-endian low byte with bit 2 set
  lockFrame.data.bytes[2] = 0x00; // Little-endian high byte

  // Configure enable frame (MODE-BIT register 0x51, bit 2 cleared to enable)
  enableFrame.id = 0x201;
  enableFrame.extended = false;
  enableFrame.priority = 0;
  enableFrame.rtr = 0;
  enableFrame.length = 3;
  enableFrame.data.bytes[0] = 0x51;
  enableFrame.data.bytes[1] = 0x00; // Little-endian low byte, bit 2 cleared
  enableFrame.data.bytes[2] = 0x00; // Little-endian high byte
  
  // // Pack 8 bytes efficiently: "CAN Test" uses all 8 bytes
  // const char* message = "CAN Test";
  // for (int i = 0; i < 8; i++) {
  //   canFrame.data.bytes[i] = message[i];
  // }
  
  Serial.println("CAN Bus initialized successfully");
  Serial.println("Ready to send messages...");
}

void loop() {
  while (Serial.available() > 0) {
    char incoming = Serial.read();
    if (incoming == 's' || incoming == 'S') {
      triggerEnableSequence = true;
    }
  }

  if (triggerEnableSequence) {
    Serial.println("Pause requested. Halting torque frame transmission for 2 seconds...");
    delay(2000);
    Serial.println("Sending lock frame (ENABLE OFF) to motor controller.");
    sendLockFrame();
    delay(100);
    Serial.println("Sending enable frame (NOT ENABLE OFF) to motor controller.");
    sendEnableFrame();
    Serial.println("Waiting 2 seconds before resuming torque transmission.");
    delay(2000);
    triggerEnableSequence = false;
    Serial.println("Resuming torque frame transmissions.");
  } else {
    sendTorqueFrame();
    delay(10);
  }
}