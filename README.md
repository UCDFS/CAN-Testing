# CAN-Testing

This repository contains PlatformIO project folders for testing the UCDFS CAN bus implementation.

## Overview

This repository serves as the source for PlatformIO projects involved in testing and developing CAN bus communication for UC Davis Formula SAE (UCDFS). The projects here are designed to work with various microcontrollers and CAN transceivers to validate CAN bus functionality.

## Projects

### PRO_M_MOCK_MC
CAN bus listener implementation using a SparkFun Pro Micro and HW-184 CAN transceiver board (MCP2515-based). This project is designed to monitor and decode CAN bus messages.

**Features:**
- Listens for CAN messages on the bus
- Displays message details (ID, DLC, data, ASCII interpretation)
- Supports interrupt-based and polling-based reception
- Configurable bitrates and crystal frequencies
- Message filtering capabilities

See [PRO_M_MOCK_MC/README.md](PRO_M_MOCK_MC/README.md) for detailed documentation.

### DUE_SEND_TORQUE
CAN message sender implementation using an Arduino Due. This project sends torque command frames and control messages for testing motor controller communication.

**Features:**
- Sends torque command frames
- Sends enable/disable sequences for motor controllers
- Configurable CAN bitrate (500 kbps default)
- Uses native CAN controller on Arduino Due

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (via VS Code extension or CLI)
- Appropriate hardware (SparkFun Pro Micro, Arduino Due, CAN transceivers)
- CAN bus setup with proper termination

### Building and Uploading

Navigate to the project directory you want to work with and use PlatformIO commands:

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

Or use the PlatformIO IDE interface in VS Code.

## Hardware Setup

Each project folder contains specific wiring diagrams and hardware requirements in its README. Make sure to:

1. Connect CAN transceivers properly (CANH, CANL, GND)
2. Ensure proper 120Ω termination on the CAN bus
3. Verify power supply connections
4. Check SPI connections for MCP2515-based transceivers

## Development

### Adding New Projects

When adding new CAN testing projects:

1. Create a new PlatformIO project folder
2. Add a descriptive README.md with hardware requirements and usage
3. Configure `platformio.ini` with appropriate settings
4. Document any special wiring or configuration requirements

### Testing

Each project should be tested with:
- Known CAN bus traffic
- Various bitrate configurations
- Different message types (standard/extended)
- Error handling scenarios

## Contributing

When contributing to this repository:

1. Test your changes with actual hardware
2. Update documentation as needed
3. Follow existing code style and structure
4. Add comments for complex CAN message handling

## Resources

- [CAN Bus Specification](https://en.wikipedia.org/wiki/CAN_bus)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [MCP2515 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf)
- [Arduino Due CAN](https://github.com/collin80/due_can)

## License

This project is open source. Use and modify as needed for UCDFS applications.
