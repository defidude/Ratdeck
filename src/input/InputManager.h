#pragma once

#include "hal/Keyboard.h"
#include "hal/Trackball.h"
#include "hal/TouchInput.h"

class InputManager {
public:
    void begin(Keyboard* kb, Trackball* tb, TouchInput* touch);
    void update();

    // Keyboard events
    bool hasKeyEvent() const { return _hasKey; }
    const KeyEvent& getKeyEvent() const { return _keyEvent; }

    // Any activity (for power wake)
    bool hadActivity() const { return _activity; }

private:
    Keyboard* _kb = nullptr;
    Trackball* _tb = nullptr;
    TouchInput* _touch = nullptr;

    bool _hasKey = false;
    KeyEvent _keyEvent;
    bool _activity = false;

    // Trackball navigation state
    int8_t _tbAccumX = 0;
    int8_t _tbAccumY = 0;
    unsigned long _lastTbNavTime = 0;
    static constexpr int8_t TB_NAV_THRESHOLD = 3;
    static constexpr unsigned long TB_NAV_RATE_MS = 200;
};
