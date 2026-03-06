#include "TouchInput.h"

TouchInput* TouchInput::_instance = nullptr;

bool TouchInput::begin() {
    _instance = this;

    // GT911 INT pin
    pinMode(TOUCH_INT, INPUT);

    // Try GT911 at both possible addresses
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Wire.beginTransmission(0x14);
        err = Wire.endTransmission();
        if (err != 0) {
            Serial.println("[TOUCH] GT911 not found at 0x5D or 0x14");
            return false;
        }
        Serial.println("[TOUCH] GT911 found at 0x14");
    } else {
        Serial.println("[TOUCH] GT911 found at 0x5D");
    }

    Serial.println("[TOUCH] Touch input registered");
    return true;
}

void TouchInput::update() {
    readGT911();
}

bool TouchInput::readGT911() {
    // Read touch status register (0x814E)
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x81);
    Wire.write(0x4E);
    if (Wire.endTransmission() != 0) {
        _touched = false;
        return false;
    }

    Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) {
        _touched = false;
        return false;
    }

    uint8_t status = Wire.read();
    uint8_t touchCount = status & 0x0F;
    bool bufferReady = (status & 0x80) != 0;

    if (!bufferReady || touchCount == 0) {
        _touched = false;
        Wire.beginTransmission(TOUCH_I2C_ADDR);
        Wire.write(0x81);
        Wire.write(0x4E);
        Wire.write(0x00);
        Wire.endTransmission();
        return false;
    }

    // Read first touch point (0x8150-0x8157)
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x81);
    Wire.write(0x50);
    if (Wire.endTransmission() != 0) {
        _touched = false;
        return false;
    }

    Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6) {
        _touched = false;
        return false;
    }

    Wire.read(); // track ID
    _x = Wire.read() | (Wire.read() << 8);
    _y = Wire.read() | (Wire.read() << 8);
    Wire.read(); // size (unused)

    _touched = true;

    // Clear buffer status
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0x00);
    Wire.endTransmission();

    return true;
}
