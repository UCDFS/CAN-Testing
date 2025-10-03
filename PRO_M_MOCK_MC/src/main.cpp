#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

// Pin definitions for SparkFun Pro Micro
#define CAN_CS_PIN    10    // Chip Select pin
#define CAN_INT_PIN   2     // Interrupt pin

// Create MCP_CAN object
MCP_CAN CAN(CAN_CS_PIN);

// Variables for CAN message handling
unsigned long rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

// Enhanced statistics and filtering
unsigned long messageCount = 0;
unsigned long errorCount = 0;
unsigned long lastStatsTime = 0;
unsigned long lastMessageTime = 0;
const unsigned long STATS_INTERVAL = 10000; // Print stats every 10 seconds
const unsigned long MESSAGE_TIMEOUT = 5000;  // Detect silence after 5 seconds

// Message filtering and analysis
struct MessageStats {
  unsigned long id;
  unsigned long count;
  unsigned long lastSeen;
};

MessageStats knownMessages[16]; // Track up to 16 different message IDs
int knownMessageCount = 0;

// Interrupt flag
volatile bool messageReceived = false;

// Function prototypes
void printCANMessage(unsigned long id, unsigned char dlc, unsigned char *data);
void setupCANFilters();
void printStatistics();
void updateMessageStats(unsigned long id);
void interpretMessage(unsigned long id, unsigned char dlc, unsigned char *data);
void canISR();

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("=================================");
  Serial.println("  Enhanced CAN Bus Listener v2.0");
  Serial.println("  SparkFun Pro Micro + MCP2515");
  Serial.println("=================================");
  
  // Setup interrupt pin
  pinMode(CAN_INT_PIN, INPUT);
  
  Serial.print("Initializing MCP2515...");
  
  // Try different configurations if first fails
  byte initResult = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  if (initResult != CAN_OK) {
    Serial.println(" FAILED with 8MHz crystal!");
    Serial.print("Trying 16MHz crystal...");
    initResult = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ);
  }
  
  if (initResult == CAN_OK) {
    Serial.println(" SUCCESS!");
  } else {
    Serial.println(" FAILED!");
    Serial.println("Error code: " + String(initResult));
    Serial.println("Check wiring, crystal frequency, and connections.");
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
    }
  }
  
  // Set to normal mode
  CAN.setMode(MCP_NORMAL);
  
  // Enable interrupt
  if (digitalPinToInterrupt(CAN_INT_PIN) != NOT_AN_INTERRUPT) {
    attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), canISR, FALLING);
    Serial.println("Interrupt enabled on pin " + String(CAN_INT_PIN));
  } else {
    Serial.println("Warning: Interrupt not available, using polling mode");
  }
  
  // Setup filters (optional)
  setupCANFilters();
  
  Serial.println("\nConfiguration:");
  Serial.println("- Bitrate: 500 kbps");
  Serial.println("- Crystal: Auto-detected");
  Serial.println("- CS Pin: " + String(CAN_CS_PIN));
  Serial.println("- INT Pin: " + String(CAN_INT_PIN));
  
  Serial.println("\nListening for CAN messages...");
  Serial.println("Time(ms) | ID    | DLC | Data                | ASCII    | Interpretation");
  Serial.println("---------|-------|-----|---------------------|----------|---------------");
  
  lastStatsTime = millis();
  lastMessageTime = millis();
}

void loop() {
  // Check for messages (interrupt-driven or polling)
  if (messageReceived || (CAN_MSGAVAIL == CAN.checkReceive())) {
    messageReceived = false;
    
    byte readResult = CAN.readMsgBuf(&rxId, &len, rxBuf);
    if (readResult == CAN_OK) {
      messageCount++;
      lastMessageTime = millis();
      printCANMessage(rxId, len, rxBuf);
      updateMessageStats(rxId);
      interpretMessage(rxId, len, rxBuf);
    } else {
      errorCount++;
      Serial.println("Error reading CAN message: " + String(readResult));
    }
  }

  
  // Detect bus silence
  if (millis() - lastMessageTime > MESSAGE_TIMEOUT && messageCount > 0) {
    static bool silenceReported = false;
    if (!silenceReported) {
      Serial.println("\n*** CAN Bus appears silent ***");
      silenceReported = true;
    }
  }
  
  delay(1);
}

void canISR() {
  messageReceived = true;
}

void printCANMessage(unsigned long id, unsigned char dlc, unsigned char *data) {
  // Print timestamp with better formatting
  unsigned long timestamp = millis();
  Serial.print(String(timestamp));
  
  // Pad timestamp to 8 characters
  int tsLen = String(timestamp).length();
  for (int i = tsLen; i < 8; i++) {
    Serial.print(" ");
  }
  Serial.print(" | ");
  
  // Print CAN ID with proper formatting
  Serial.print("0x");
  if (id < 0x100) Serial.print("0");
  if (id < 0x10) Serial.print("0");
  Serial.print(id, HEX);
  Serial.print(" | ");
  
  // Print Data Length Code
  Serial.print(dlc);
  Serial.print("   | ");
  
  // Print hex data with consistent spacing
  String dataHex = "";
  for (int i = 0; i < 8; i++) {
    if (i < dlc) {
      if (data[i] < 0x10) dataHex += "0";
      dataHex += String(data[i], HEX);
    } else {
      dataHex += "  "; // Empty bytes
    }
    if (i < 7) dataHex += " ";
  }
  dataHex.toUpperCase();
  Serial.print(dataHex);
  Serial.print(" | ");

}

// ...existing code...
void interpretMessage(unsigned long id, unsigned char dlc, unsigned char *data) {
  if (dlc == 0) {
    Serial.println("Remote frame");
    return;
  }

  if (id == 0x201) {
    if (dlc < 3) {
      Serial.println("Motor Controller Command (0x201) -> payload too short");
      return;
    }

    uint8_t regId = data[0];
    switch (regId) {
      case 0x90: {
        uint16_t rawTorque = (uint16_t)data[1] | ((uint16_t)data[2] << 8); // little-endian
        float torquePercent = (rawTorque / 32768.0f) * 100.0f;
        Serial.print("Motor Torque Request (Reg 0x90) -> raw: 0x");
        if (rawTorque < 0x1000) Serial.print("0");
        Serial.print(rawTorque, HEX);
        Serial.print(" (");
        Serial.print(rawTorque);
        Serial.print(") ≈ ");
        Serial.print(torquePercent, 2);
        Serial.println("%");
        break;
      }
      default: {
        Serial.print("Motor Controller Command (Reg 0x");
        Serial.print(regId, HEX);
        Serial.print(") -> data:");
        for (int i = 1; i < dlc; i++) {
          Serial.print(" 0x");
          if (data[i] < 0x10) Serial.print("0");
          Serial.print(data[i], HEX);
        }
        Serial.println();
        break;
      }
    }
    return;
  }
}

void updateMessageStats(unsigned long id) {
  // Find existing entry or create new one
  for (int i = 0; i < knownMessageCount; i++) {
    if (knownMessages[i].id == id) {
      knownMessages[i].count++;
      knownMessages[i].lastSeen = millis();
      return;
    }
  }
  
  // Add new message ID if we have space
  if (knownMessageCount < 16) {
    knownMessages[knownMessageCount].id = id;
    knownMessages[knownMessageCount].count = 1;
    knownMessages[knownMessageCount].lastSeen = millis();
    knownMessageCount++;
  }
}

void printStatistics() {
  Serial.println("\n" + String("=").substring(0, 50));
  Serial.println("STATISTICS - Uptime: " + String(millis()/1000) + "s");
  Serial.println("Total messages: " + String(messageCount));
  Serial.println("Errors: " + String(errorCount));
  Serial.println("Message rate: " + String((float)messageCount / (millis()/1000.0), 2) + " msg/s");
  
  Serial.println("\nKnown Message IDs:");
  for (int i = 0; i < knownMessageCount; i++) {
    Serial.print("  0x");
    if (knownMessages[i].id < 0x100) Serial.print("0");
    if (knownMessages[i].id < 0x10) Serial.print("0");
    Serial.print(knownMessages[i].id, HEX);
    Serial.print(": " + String(knownMessages[i].count) + " msgs");
    Serial.println(" (last: " + String((millis() - knownMessages[i].lastSeen)/1000) + "s ago)");
  }
  Serial.println(String("=").substring(0, 50) + "\n");
}

void setupCANFilters() {
  // Uncomment to enable filtering for specific message ranges
  /*
  // Accept messages in range 0x470-0x47F
  CAN.init_Mask(0, 0, 0x7F0);        // Mask: ignore lower 4 bits
  CAN.init_Filt(0, 0, 0x470);        // Filter: accept 0x470-0x47F
  
  // Accept specific high-priority messages
  CAN.init_Mask(1, 0, 0x7FF);        // Mask: exact match
  CAN.init_Filt(1, 0, 0x001);        // Emergency messages
  CAN.init_Filt(2, 0, 0x002);        // System status
  
  Serial.println("CAN filters enabled: 0x470-0x47F, 0x001, 0x002");
  */
}