#include <Arduino.h>
#include <due_can.h>
#include <can_common.h>

// CAN message structures
CAN_FRAME torqueFrame;
CAN_FRAME enableFrame;
CAN_FRAME lockFrame;

bool triggerEnableSequence = false;

void sendTorqueFrame() {
  Can0.sendFrame(torqueFrame);
  Serial.print("Sent torque frame: ");
  Serial.print(torqueFrame.data.bytes[0], HEX);
  Serial.print(" ");
  Serial.print(torqueFrame.data.bytes[1], HEX);
  Serial.print(" ");
  Serial.print(torqueFrame.data.bytes[2], HEX);
  Serial.print(" over CAN, ID:");
  Serial.println(torqueFrame.id, HEX);
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

  // Bytes 1 & 2: Set the data for 15% torque
  // Calculated value for 15% torque is 4914, which is 0x1332 in hex.
  // The data is sent in Little-Endian format (low byte first)[cite: 217, 331].
  torqueFrame.data.bytes[1] = 0x32; // Low byte of 0x1332
  torqueFrame.data.bytes[2] = 0x13; // High byte of 0x1332

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