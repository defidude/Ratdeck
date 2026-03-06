#pragma once

#include <Arduino.h>
#include "config/BoardConfig.h"

class Power {
public:
    // Enable peripheral power (GPIO 10 HIGH) — call first in setup
    static void enablePeripherals();

    void begin();
    void loop();

    // Call on any user activity (keypress, touch, trackball)
    void activity();

    // Battery
    float batteryVoltage() const;
    int batteryPercent() const;

    // Backlight
    void setBrightness(uint8_t brightness);
    void setDimTimeout(uint16_t seconds) { _dimTimeout = seconds * 1000UL; }
    void setOffTimeout(uint16_t seconds) { _offTimeout = seconds * 1000UL; }

    enum State { ACTIVE, DIMMED, SCREEN_OFF };
    State state() const { return _state; }
    bool isScreenOn() const { return _state != SCREEN_OFF; }

private:
    void setState(State newState);

    State _state = ACTIVE;
    unsigned long _lastActivity = 0;
    unsigned long _dimTimeout = 30000;
    unsigned long _offTimeout = 60000;
    uint8_t _fullBrightness = 255;
    static constexpr uint8_t DIM_BRIGHTNESS = 64;
};
