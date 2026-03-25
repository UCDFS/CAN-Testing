#include "bamocar.h"
#include "bamocar_registers.h"
#include "logging.h"

void sendCAN(const CAN_message_t &msg) {
  Can1.write(msg);
  logCANFrame(msg, "TX");
}

void requestStatusCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_STATUS;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestErrorsCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_ERROR_WORD;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestStatusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_STATUS;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void requestSpeedCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_SPEED_ACTUAL;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestCurrentCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_CURRENT_ACTUAL;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestTempsCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_TEMP_MOTOR;  // motor temp
  msg.buf[2] = interval_ms;
  sendCAN(msg);
  msg.buf[1] = REG_TEMP_INVERTER;  // inverter (IGBT) temp
  sendCAN(msg);
}

void requestDCBusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TRANSMIT_REQUEST;
  msg.buf[1] = REG_DC_BUS_VOLTAGE;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void clearErrors() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_CLEAR_ERRORS;
  msg.buf[1] = 0x00;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void configureCanTimeout(uint16_t ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_CAN_TIMEOUT;
  msg.buf[1] = ms & 0xFF;
  msg.buf[2] = (ms >> 8) & 0xFF;
  sendCAN(msg);
}

void enableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_DRIVE_COMMAND;
  msg.buf[1] = 0x04;
  msg.buf[2] = 0x00;
  sendCAN(msg);
  delay(100);
  msg.buf[1] = 0x00;
  sendCAN(msg);
}

void disableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_DRIVE_COMMAND;
  msg.buf[1] = 0x04;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void sendTorqueCommand(int16_t torqueValue) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = REG_TORQUE_COMMAND;
  msg.buf[1] = torqueValue & 0xFF;
  msg.buf[2] = (torqueValue >> 8) & 0xFF;
  sendCAN(msg);
}

// ---------- CAN RX ----------
void readCanMessages() {
  CAN_message_t msg;
  while (Can1.read(msg)) {
    logCANFrame(msg, "RX");

    if (msg.id == BAMOCAR_TX_ID && msg.len >= 3) {
      uint8_t reg = msg.buf[0];

      if (reg == REG_STATUS) { // STATUS register
        bamocarOnline = true;
        statusWord = msg.buf[1] | (msg.buf[2] << 8);
      }

      else if (reg == REG_SPEED_ACTUAL) { // RPM feedback (signed, normalised to NMAX)
        rpmFeedback = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == REG_CURRENT_ACTUAL) { // I_ACT actual current (signed, normalised to IMAX)
        actualCurrent = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == REG_TEMP_MOTOR) { // motor temperature (°C)
        motorTemp = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == REG_TEMP_INVERTER) { // inverter (IGBT) temperature (°C)
        inverterTemp = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == REG_DC_BUS_VOLTAGE) { // DC bus voltage (UDC = raw / 31.5848, per BAMOCAR FAQ)
        dcBusVoltage = (msg.buf[1] | (msg.buf[2] << 8)) / 31.5848f;
      }

      else if (reg == REG_ERROR_WORD) { // Error register
        bamocarErrorWord = msg.buf[1] | (msg.buf[2] << 8);
      }
    }
  }
}
