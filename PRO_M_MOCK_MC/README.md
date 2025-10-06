# CAN Bus Listener - SparkFun Pro Micro + HW-184

This project implements a CAN bus listener using a SparkFun Pro Micro (Arduino-compatible) and an HW-184 CAN transceiver board (MCP2515-based).

## Hardware Requirements

- SparkFun Pro Micro (5V/16MHz or 3.3V/8MHz)
- HW-184 CAN Transceiver Board (MCP2515 + TJA1050)
- CAN bus with active nodes
- Jumper wires
- Breadboard (optional)

## Wiring Diagram

Connect the HW-184 to the SparkFun Pro Micro as follows:

| HW-184 Pin | Pro Micro Pin | Description         |
| ---------- | ------------- | ------------------- |
| VCC        | VCC (5V/3.3V) | Power supply        |
| GND        | GND           | Ground              |
| CS         | 10            | Chip Select (SPI)   |
| SO (MISO)  | 14            | Master In Slave Out |
| SI (MOSI)  | 16            | Master Out Slave In |
| SCK        | 15            | Serial Clock        |
| INT        | 2 (optional)  | Interrupt pin       |

**CAN Bus Connections:**

- Connect CANH and CANL to your CAN bus network
- Ensure proper CAN bus termination (120Ω resistors at both ends)

## Configuration

### Crystal Frequency

The HW-184 typically comes with an 8MHz crystal. If your board has a different crystal frequency, update the `MCP_CRYSTAL` setting in `include/can_config.h`:

```cpp
#define MCP_CRYSTAL     MCP_8MHZ       // Change to MCP_16MHZ or MCP_20MHZ if needed
```

### CAN Bitrate

The default bitrate is 500 kbps. Common automotive CAN networks use:

- 500 kbps (most common)
- 250 kbps
- 125 kbps

Update in `include/can_config.h`:

```cpp
#define CAN_BITRATE     CAN_500KBPS    // Change as needed
```

### Pin Configuration

If you need to use different pins, update them in `include/can_config.h`:

```cpp
#define CAN_CS_PIN      10             // Chip Select
#define CAN_INT_PIN     2              // Interrupt (optional)
```

## Usage

1. **Build and Upload:**

   ```bash
   pio run --target upload
   ```

2. **Monitor Serial Output:**

   ```bash
   pio device monitor
   ```

3. **Expected Output:**

   ```
   =================================
     Enhanced CAN Bus Listener v2.0
     SparkFun Pro Micro + MCP2515
   =================================
   Initializing MCP2515... SUCCESS!

   Configuration:
   - Bitrate: 500 kbps
   - Crystal: Auto-detected
   - CS Pin: 10

   Listening for CAN messages...
   timestamp,id,dlc,data0,data1,data2,data3,data4,data5,data6,data7,interpretation
   12345,0x123,8,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
   12367,0x456,4,0xFF,0xAA,0xBB,0xCC,,,,,
   ```

4. **Graphical Monitor (Optional):**

   A lightweight Tkinter-based viewer is provided in `tools/can_gui.py`. It consumes the CSV stream from the board and maintains one row per CAN ID, refreshing the hex payload in-place when updates arrive.

   Install dependency:

   ```bash
   python3 -m pip install pyserial
   ```

   Run the viewer (replace the serial port with your device):

   ```bash
   python3 tools/can_gui.py /dev/tty.usbmodem101 --baud 115200
   ```

## Troubleshooting

### No Messages Received

1. Check wiring connections
2. Verify CAN bus is active (other nodes transmitting)
3. Check bitrate matches your CAN network
4. Ensure crystal frequency setting is correct
5. Verify CAN bus termination

### Initialization Failed

1. Check SPI connections (CS, MOSI, MISO, SCK)
2. Verify power supply connections
3. Check crystal frequency setting
4. Ensure HW-184 is not damaged

### Garbled Messages

1. Wrong bitrate setting
2. Incorrect crystal frequency
3. Poor connections or noise

## Advanced Features

### Message Filtering

Uncomment the filter setup in `main.cpp` to only receive specific CAN IDs:

```cpp
// In setup() function:
setupCANFilters(mcp2515);

// In setupCANFilters() function:
mcp.setFilter(RXF0, false, 0x123);  // Only accept ID 0x123
mcp.setFilterMask(MASK0, false, 0x7FF);
```

### Interrupt-Based Reception

For higher performance, you can use interrupt-based message reception by connecting the INT pin and implementing an interrupt service routine.

## Library Dependencies

- `coryjfowler/mcp_can@^1.5.1` - MCP_CAN library for MCP2515 CAN controller

## Alternative Bitrates and Crystals

The MCP_CAN library supports various combinations:

**Bitrates:**

- `CAN_125KBPS` - 125 kbps
- `CAN_250KBPS` - 250 kbps
- `CAN_500KBPS` - 500 kbps (default)
- `CAN_1000KBPS` - 1000 kbps (1 Mbps)

**Crystal Frequencies:**

- `MCP_8MHZ` - 8 MHz crystal (default for HW-184)
- `MCP_16MHZ` - 16 MHz crystal
- `MCP_20MHZ` - 20 MHz crystal

To change these settings, modify the `CAN.begin()` call in `main.cpp`:

```cpp
CAN.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ)  // Example: 250kbps with 8MHz crystal
```

## License

This project is open source. Use and modify as needed for your applications.
