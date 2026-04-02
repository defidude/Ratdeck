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
    void setBorders(int16_t x_min, int16_t y_min, int16_t x_max, int16_t y_max);

private:
    bool readGT911();

    uint8_t _i2cAddress = 0;

    bool _touched = false;
    int16_t _x = 0;
    int16_t _y = 0;
    int16_t _x_min = 10;
    int16_t _y_min = 8;
    int16_t _x_max = 313;
    int16_t _y_max = 243;

    static TouchInput* _instance;
};
