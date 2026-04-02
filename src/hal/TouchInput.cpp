#include "TouchInput.h"

TouchInput* TouchInput::_instance = nullptr;

bool TouchInput::begin() {
    _instance = this;

    // GT911 INT pin
    pinMode(TOUCH_INT, INPUT);

    // Try GT911 at both possible addresses
    Wire.beginTransmission(TOUCH_I2C_ADDR_1);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Wire.beginTransmission(TOUCH_I2C_ADDR_2);
        err = Wire.endTransmission();
        if (err != 0) {
            Serial.println("[TOUCH] GT911 not found at 0x5D or 0x14");
            return false;
        }
        _i2cAddress = TOUCH_I2C_ADDR_2;
    } else {
        _i2cAddress = TOUCH_I2C_ADDR_1;
    }
    Serial.print("[TOUCH] GT911 found at 0x");
    Serial.println(_i2cAddress, HEX);

    Serial.println("[TOUCH] Touch input registered");
    return true;
}

void TouchInput::setBorders(int16_t x_min, int16_t y_min, int16_t x_max, int16_t y_max) {
    _x_min = x_min;
    _y_min = y_min;
    _x_max = x_max;
    _y_max = y_max;
}

void TouchInput::update() {
    readGT911();
}

bool TouchInput::readGT911() {
    // Read touch status register (0x814E)
    Wire.beginTransmission(_i2cAddress);
    Wire.write(0x81);
    Wire.write(0x4E);
    if (Wire.endTransmission() != 0) {
        _touched = false;
        return false;
    }

    Wire.requestFrom(_i2cAddress, (uint8_t)1);
    if (!Wire.available()) {
        _touched = false;
        return false;
    }

    uint8_t status = Wire.read();
    uint8_t touchCount = status & 0x0F;
    bool bufferReady = (status & 0x80) != 0;

    if (!bufferReady || touchCount == 0) {
        _touched = false;
        Wire.beginTransmission(_i2cAddress);
        Wire.write(0x81);
        Wire.write(0x4E);
        Wire.write(0x00);
        Wire.endTransmission();
        return false;
    }

    // Read first touch point (0x814F-0x8155)
    Wire.beginTransmission(_i2cAddress);
    Wire.write(0x81);
    Wire.write(0x4F);
    if (Wire.endTransmission() != 0) {
        _touched = false;
        return false;
    }

    Wire.requestFrom(_i2cAddress, (uint8_t)7);
    if (Wire.available() < 7) {
        _touched = false;
        return false;
    }

    uint8_t trackId = Wire.read();
    _y = Wire.read() | (Wire.read() << 8);
    _x = Wire.read() | (Wire.read() << 8);
    Wire.read() | (Wire.read() << 8); // size (unused)

    _y = TFT_HEIGHT - 1 - _y;
    _touched = true;

    // Remap
    _x = (_x - _x_min) * 320 / (_x_max - _x_min);
    _y = (_y - _y_min) * 240 / (_y_max - _y_min);

    if (_x < 0) _x = 0;
    if (_x > 319) _x = 319;
    if (_y < 0) _y = 0;
    if (_y > 239) _y = 239;

    // Clear buffer status
    Wire.beginTransmission(_i2cAddress);
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0x00);
    Wire.endTransmission();

    return true;
}
