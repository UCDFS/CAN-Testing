#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

// Pin definitions for SparkFun Pro Micro
#define CAN_CS_PIN    10    // Chip Select pin
#define CAN_INT_PIN   2     // Interrupt pin (optional)

// Create MCP_CAN object
MCP_CAN CAN(CAN_CS_PIN);

// Variables for CAN message handling
unsigned long rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

// Statistics and configuration
unsigned long messageCount = 0;
unsigned long lastStatsTime = 0;
unsigned long lastHeartbeat = 0;
const unsigned long STATS_INTERVAL = 10000;    // Print stats every 10 seconds
const unsigned long HEARTBEAT_INTERVAL = 5000; // Send heartbeat every 5 seconds

// Enable/disable heartbeat transmission (set to false for listen-only mode)
const bool SEND_HEARTBEAT = false;
const unsigned long HEARTBEAT_ID = 0x7DF;  // OBD2 functional addressing

// Function prototypes
void printCANMessage(unsigned long id, unsigned char dlc, unsigned char *data);
void setupCANFilters();
void sendHeartbeat();
void printCANStatistics();
String formatCANId(unsigned long id);
String formatDataBytes(unsigned char *data, unsigned char len);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println(F("========================================="));
  Serial.println(F("  Advanced CAN Bus Listener"));
  Serial.println(F("  SparkFun Pro Micro + HW-184 MCP2515"));
  Serial.println(F("========================================="));
  
  // Initialize MCP2515
  Serial.print(F("Initializing MCP2515..."));
  
  if(CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println(F(" SUCCESS!"));
  } else {
    Serial.println(F(" FAILED!"));
    Serial.println(F("Check your wiring and settings."));
    Serial.println(F("Common issues:"));
    Serial.println(F("- Incorrect CS pin connection"));
    Serial.println(F("- Wrong crystal frequency setting"));
    Serial.println(F("- Poor power supply"));
    while (1) delay(1000);
  }
  
  // Set to normal mode
  CAN.setMode(MCP_NORMAL);
  
  // Optional: Setup message filters (uncomment to enable)
  // setupCANFilters();
  
  Serial.println(F("\nConfiguration:"));
  Serial.println(F("- Bitrate: 500 kbps"));
  Serial.println(F("- Crystal: 8 MHz"));
  Serial.println(F("- CS Pin: 10"));
  Serial.print(F("- Heartbeat: "));
  Serial.println(SEND_HEARTBEAT ? F("Enabled") : F("Disabled (Listen-only)"));
  
  Serial.println(F("\nListening for CAN messages..."));
  Serial.println(F("Time(ms)   | ID      | DLC | Data                | ASCII    | Info"));
  Serial.println(F("-----------|---------|-----|---------------------|----------|--------"));
  
  lastStatsTime = millis();
  lastHeartbeat = millis();
}

void loop() {
  // Check for received messages
  if(CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&rxId, &len, rxBuf);
    messageCount++;
    printCANMessage(rxId, len, rxBuf);
  }
  
  // Send heartbeat if enabled
  if (SEND_HEARTBEAT && (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL)) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
  
  // Print periodic statistics
  if (millis() - lastStatsTime >= STATS_INTERVAL) {
    printCANStatistics();
    lastStatsTime = millis();
  }
  
  delay(1);
}

void printCANMessage(unsigned long id, unsigned char dlc, unsigned char *data) {
  // Print timestamp (truncated to fit)
  unsigned long timestamp = millis();
  Serial.print(timestamp);
  
  // Pad timestamp to 10 characters
  String timeStr = String(timestamp);
  for (int i = timeStr.length(); i < 10; i++) {
    Serial.print(" ");
  }
  Serial.print(" | ");
  
  // Print CAN ID with proper formatting
  Serial.print(formatCANId(id));
  Serial.print(" | ");
  
  // Print Data Length Code
  Serial.print(dlc);
  Serial.print("   | ");
  
  // Print data bytes
  Serial.print(formatDataBytes(data, dlc));
  Serial.print(" | ");
  
  // Print ASCII representation (8 chars max)
  for (int i = 0; i < dlc && i < 8; i++) {
    if (data[i] >= 32 && data[i] <= 126) {
      Serial.print((char)data[i]);
    } else {
      Serial.print(".");
    }
  }
  // Pad ASCII to 8 characters
  for (int i = dlc; i < 8; i++) {
    Serial.print(" ");
  }
  Serial.print(" | ");
  
  // Add message type information
  if (id >= 0x7E0 && id <= 0x7E7) {
    Serial.print("OBD2-Req");
  } else if (id >= 0x7E8 && id <= 0x7EF) {
    Serial.print("OBD2-Resp");
  } else if (id == 0x7DF) {
    Serial.print("OBD2-Func");
  } else if (id < 0x100) {
    Serial.print("Std-ID");
  } else {
    Serial.print("Ext-ID");
  }
  
  Serial.println();
}

String formatCANId(unsigned long id) {
  String result = "0x";
  if (id < 0x100) result += "0";
  if (id < 0x10) result += "0";
  result += String(id, HEX);
  result.toUpperCase();
  
  // Pad to 7 characters total
  while (result.length() < 7) {
    result += " ";
  }
  return result;
}

String formatDataBytes(unsigned char *data, unsigned char len) {
  String result = "";
  for (int i = 0; i < len && i < 8; i++) {
    if (data[i] < 0x10) result += "0";
    result += String(data[i], HEX);
    if (i < len - 1) result += " ";
  }
  result.toUpperCase();
  
  // Pad to 19 characters
  while (result.length() < 19) {
    result += " ";
  }
  return result;
}

void sendHeartbeat() {
  unsigned char heartbeatData[8] = {0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  
  byte sndStat = CAN.sendMsgBuf(HEARTBEAT_ID, 0, 8, heartbeatData);
  
  if(sndStat == CAN_OK) {
    Serial.println(F("\n>>> Heartbeat sent <<<"));
  } else {
    Serial.println(F("\n>>> Heartbeat failed <<<"));
  }
}

void printCANStatistics() {
  Serial.println();
  Serial.println(F("=== STATISTICS ==="));
  Serial.print(F("Messages received: "));
  Serial.println(messageCount);
  Serial.print(F("Uptime: "));
  Serial.print(millis() / 1000);
  Serial.println(F(" seconds"));
  Serial.print(F("Rate: "));
  if (messageCount > 0 && millis() > 1000) {
    float rate = (float)messageCount / (millis() / 1000.0);
    Serial.print(rate, 2);
    Serial.println(F(" msg/sec"));
  } else {
    Serial.println(F("0.00 msg/sec"));
  }
  Serial.println(F("=================="));
  Serial.println();
}

void setupCANFilters() {
  // Example: Filter for OBD2 messages only
  // This will only receive messages with IDs 0x7E0-0x7EF and 0x7DF
  
  /*
  // Set up mask and filters
  CAN.init_Mask(0, 0, 0x7F0);        // Mask for RXB0
  CAN.init_Filt(0, 0, 0x7E0);        // Filter 0: OBD2 requests
  CAN.init_Filt(1, 0, 0x7E8);        // Filter 1: OBD2 responses
  
  CAN.init_Mask(1, 0, 0x7FF);        // Mask for RXB1  
  CAN.init_Filt(2, 0, 0x7DF);        // Filter 2: OBD2 functional
  CAN.init_Filt(3, 0, 0x7E0);        // Filter 3: OBD2 requests
  CAN.init_Filt(4, 0, 0x7E8);        // Filter 4: OBD2 responses
  CAN.init_Filt(5, 0, 0x7EF);        // Filter 5: OBD2 responses
  
  Serial.println(F("CAN filters configured for OBD2 messages"));
  */
}