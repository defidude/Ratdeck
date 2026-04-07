#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config/BoardConfig.h"

// Input modes
enum class InputMode {
    Navigation,  // Arrow-like movement, hotkeys active
    TextInput    // Character entry, Esc exits to Navigation
};

// Simplified key event for consumers
struct KeyEvent {
    char character;
    bool ctrl;
    bool shift;
    bool fn;
    bool alt;
    bool opt;
    bool enter;
    bool del;
    bool tab;
    bool space;
    // Directional arrows (from trackball)
    bool up;
    bool down;
    bool left;
    bool right;
};

class Keyboard {
public:
    bool begin();
    void update();

    // Mode control
    InputMode getMode() const { return _mode; }
    void setMode(InputMode mode) { _mode = mode; }

    // State queries
    bool hasEvent() const { return _hasEvent; }
    const KeyEvent& getEvent() const { return _event; }

    // Backlight control
    // NOTES:
    // - Backlight control has been added to the ESP32-C3 F/W on 2024-12-25,
    //   there's no way to detect if the installed F/W supports it
    //   (no I2C reads except for key states, I2C writes are always ACK'ed).
    // - Backlight toggle via <Alt>+<B> is implemented in the ESP32-C3 F/W,
    //   we can't track the actual backlight ON/OFF state.
    // - The ESP32-C3 F/W uses 2 brightness settings, one for <Alt>+<B> (which doesn't
    //   change the current brightness), the other one for the current brightness.
    //   The range for the 1st one is limited to [31, 255], we use it for the 2nd one, too.
    bool setBacklightBrightness(uint8_t percent); // doesn't change the current brightness
    bool backlightOn();
    bool backlightOff();

private:
    uint8_t readKey(uint8_t* modOut);
    static bool setBrightness(uint8_t pwm);

    InputMode _mode = InputMode::Navigation;
    KeyEvent _event = {};
    bool _hasEvent = false;
    uint8_t _lastKey = 0;
    bool _altHeld = false;           // Software Alt tracking
    unsigned long _altPressTime = 0; // When Alt was detected
    uint8_t _backlightBrightness = 255; // [31, 255]

    static Keyboard* _instance;
    static int _debugCount;          // Log first N keypresses
};
