#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

#include <mcp_can.h>

// Pin Configuration for SparkFun Pro Micro
#define CAN_CS_PIN      10             // Chip Select (CS)
#define CAN_INT_PIN     2              // Interrupt pin (optional for polling mode)

// CAN Bus Configuration
// Common bitrates: CAN_125KBPS, CAN_250KBPS, CAN_500KBPS, CAN_1000KBPS
// Common crystal frequencies: MCP_8MHZ, MCP_16MHZ, MCP_20MHZ

// Message filtering (optional)
// Uncomment and modify these if you want to filter specific CAN IDs
// #define FILTER_ID_1     0x123
// #define FILTER_ID_2     0x456
// #define MASK_ID         0x7FF          // Standard CAN ID mask

// Function prototypes
void printCANMessage(unsigned long id, unsigned char dlc, unsigned char *data);
void setupCANFilters();

#endif