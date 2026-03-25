#include "bamocar.h"
#include "logging.h"

void sendCAN(const CAN_message_t &msg) {
  Can1.write(msg);
  logCANFrame(msg, "TX");
}

void requestStatusCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x40;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestStatusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x40;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void requestSpeedCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x30;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestCurrentCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x20;
  msg.buf[2] = interval_ms;
  sendCAN(msg);
}

void requestTempsCyclic(uint8_t interval_ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0x0E;  // motor temp
  msg.buf[2] = interval_ms;
  sendCAN(msg);
  msg.buf[1] = 0x0F;  // inverter (IGBT) temp
  sendCAN(msg);
}

void requestDCBusOnce() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x3D;
  msg.buf[1] = 0xEB;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void clearErrors() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x8E;
  msg.buf[1] = 0x00;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void configureCanTimeout(uint16_t ms) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0xD0;
  msg.buf[1] = ms & 0xFF;
  msg.buf[2] = (ms >> 8) & 0xFF;
  sendCAN(msg);
}

void enableDrive() {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x51;
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
  msg.buf[0] = 0x51;
  msg.buf[1] = 0x04;
  msg.buf[2] = 0x00;
  sendCAN(msg);
}

void sendTorqueCommand(int16_t torqueValue) {
  CAN_message_t msg = {0};
  msg.id = BAMOCAR_RX_ID;
  msg.len = 3;
  msg.buf[0] = 0x90;
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

      if (reg == 0x40) { // STATUS register
        bamocarOnline = true;
        statusWord = msg.buf[1] | (msg.buf[2] << 8);
      }

      else if (reg == 0x30) { // RPM feedback (signed, normalised to NMAX)
        rpmFeedback = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == 0x20) { // I_ACT actual current (signed, normalised to IMAX)
        actualCurrent = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == 0x0E) { // motor temperature (°C)
        motorTemp = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == 0x0F) { // inverter (IGBT) temperature (°C)
        inverterTemp = (int16_t)(msg.buf[1] | (msg.buf[2] << 8));
      }

      else if (reg == 0xEB) { // DC bus voltage (UDC = raw / 31.5848, per BAMOCAR FAQ)
        dcBusVoltage = (msg.buf[1] | (msg.buf[2] << 8)) / 31.5848f;
      }
    }
  }
}
