#pragma once

#include <Arduino.h>
#include "config/BoardConfig.h"

class Trackball {
public:
    bool begin();

    // Poll GPIO state changes
    void update();

    // Cursor position
    int16_t cursorX() const { return _cursorX; }
    int16_t cursorY() const { return _cursorY; }
    bool isClicked() const { return _clicked; }
    bool hadMovement() const { return _hadMovement; }
    bool wasClicked() const { return _clicked; }

    // Raw deltas from last update (before speed multiply)
    int8_t lastDeltaX() const { return _lastDX; }
    int8_t lastDeltaY() const { return _lastDY; }

    // Speed multiplier (1-5)
    void setSpeed(uint8_t speed) { _speed = constrain(speed, 1, 5); }

private:
    static void IRAM_ATTR isrUp();
    static void IRAM_ATTR isrDown();
    static void IRAM_ATTR isrLeft();
    static void IRAM_ATTR isrRight();
    static void IRAM_ATTR isrClick();

    int16_t _cursorX = TFT_WIDTH / 2;
    int16_t _cursorY = TFT_HEIGHT / 2;
    bool _clicked = false;
    bool _hadMovement = false;
    int8_t _lastDX = 0;
    int8_t _lastDY = 0;
    uint8_t _speed = 3;

    static volatile int8_t _deltaX;
    static volatile int8_t _deltaY;
    static volatile bool _clickFlag;
    static Trackball* _instance;
};
