#pragma once

#include <Arduino.h>

// ES7210 I2S audio codec driver for T-Deck Plus
// Stub — full I2S implementation in Phase 6
class Audio {
public:
    bool begin();
    void setVolume(uint8_t vol);
    uint8_t volume() const { return _volume; }

private:
    uint8_t _volume = 80;
};
