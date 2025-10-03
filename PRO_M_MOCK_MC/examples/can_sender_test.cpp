#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

// Pin definitions for SparkFun Pro Micro
#define CAN_CS_PIN    10    // Chip Select pin
#define CAN_INT_PIN   2     // Interrupt pin (optional)

// Create MCP_CAN object
MCP_CAN CAN(CAN_CS_PIN);

// Test message data
unsigned char testData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
unsigned long testId = 0x123;

unsigned long messageCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("=================================");
  Serial.println("  CAN Bus Sender Test");
  Serial.println("  HW-184 MCP2515 CAN Controller");
  Serial.println("=================================");
  
  // Initialize MCP2515
  Serial.print("Initializing MCP2515...");
  
  if(CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println(" SUCCESS!");
  } else {
    Serial.println(" FAILED!");
    Serial.println("Check your wiring and settings.");
    while (1) delay(1000);
  }
  
  // Set to normal mode to start transmitting
  CAN.setMode(MCP_NORMAL);
  
  Serial.println("Sending test CAN messages every 2 seconds...");
  Serial.println("ID: 0x123, Data: 01 02 03 04 05 06 07 08");
  Serial.println();
}

void loop() {
  // Send test message
  byte sndStat = CAN.sendMsgBuf(testId, 0, 8, testData);
  
  if(sndStat == CAN_OK) {
    messageCount++;
    Serial.println("Message " + String(messageCount) + " sent successfully!");
    
    // Update test data for next message
    for(int i = 0; i < 8; i++) {
      testData[i]++;
    }
  } else {
    Serial.println("Error sending message");
  }
  
  delay(2000);  // Send every 2 seconds
}