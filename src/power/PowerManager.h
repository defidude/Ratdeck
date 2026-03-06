#pragma once

#include <Arduino.h>

class Display;

class PowerManager {
public:
    enum State { ACTIVE, DIMMED, SCREEN_OFF };

    void begin(Display* display);
    void loop();

    // Call on any user activity (keypress, touch, trackball)
    void activity();

    // Configuration (seconds)
    void setDimTimeout(uint16_t seconds) { _dimTimeout = seconds * 1000UL; }
    void setOffTimeout(uint16_t seconds) { _offTimeout = seconds * 1000UL; }
    void setBrightness(uint8_t brightness) { _fullBrightness = brightness; }

    State state() const { return _state; }
    bool isScreenOn() const { return _state != SCREEN_OFF; }

private:
    void setState(State newState);

    Display* _display = nullptr;
    State _state = ACTIVE;
    unsigned long _lastActivity = 0;
    unsigned long _dimTimeout = 30000;   // 30s
    unsigned long _offTimeout = 60000;   // 60s
    uint8_t _fullBrightness = 255;
    static constexpr uint8_t DIM_BRIGHTNESS = 64;
};
