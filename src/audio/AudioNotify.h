#pragma once

#include <Arduino.h>

class AudioNotify {
public:
    void begin();
    void end();

    // Notification sounds
    void playMessage();
    void playAnnounce();
    void playError();
    void playBoot();        // Sci-fi boot sequence
    void requestMessage();  // Defer short alert out of packet callbacks
    void loop();

    // Settings
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }
    void setVolume(uint8_t vol) { _volume = vol; }
    uint8_t volume() const { return _volume; }

private:
    void writeTone(uint16_t freq, uint16_t durationMs);
    void writeSilence(uint16_t durationMs);

    bool _enabled = true;
    bool _i2sReady = false;
    volatile bool _messagePending = false;
    uint8_t _volume = 80;  // 0-100
};
