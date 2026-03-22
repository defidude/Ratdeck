#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config/BoardConfig.h"

class TouchInput {
public:
    bool begin();

    // Raw touch state
    bool isTouched() const { return _touched; }
    int16_t x() const { return _x; }
    int16_t y() const { return _y; }

    void update();

private:
    bool readGT911();

    uint8_t _i2cAddress = 0;

    bool _touched = false;
    int16_t _x = 0;
    int16_t _y = 0;

    static TouchInput* _instance;
};
