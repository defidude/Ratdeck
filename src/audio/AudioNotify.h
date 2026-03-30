#pragma once

#include <Arduino.h>

class AudioNotify {
public:
    void begin();
    void end();

    // Notification sounds
    void playMessage();
    void playAnnounce(bool update);
    void playError();
    void playBoot();        // Sci-fi boot sequence

    // Settings
    void setBootEnabled(bool enabled) { _boot_enabled = enabled; }
    void setMsgEnabled(bool enabled) { _msg_enabled = enabled; }
    void setAnnounceEnabled(bool enabled) { _announce_enabled = enabled; }
    void setErrorEnabled(bool enabled) { _error_enabled = enabled; }
    void setVolume(uint8_t vol) { _volume = vol; }
    uint8_t volume() const { return _volume; }

private:
    void writeTone(uint16_t freq, uint16_t durationMs);
    void writeSilence(uint16_t durationMs);

    bool _boot_enabled = true;
    bool _msg_enabled = true;
    bool _error_enabled = true;
    bool _announce_enabled = true;
    bool _i2sReady = false;
    uint8_t _volume = 80;  // 0-100
};
