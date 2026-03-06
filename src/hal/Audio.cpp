#include "Audio.h"
#include "config/BoardConfig.h"

bool Audio::begin() {
    // TODO: Initialize ES7210 codec via I2S
    // Pins: I2S_WS=5, I2S_DOUT=6, I2S_BCK=7, I2S_DIN=14, I2S_SCK=47, I2S_MCLK=48
    Serial.println("[AUDIO] ES7210 init (stub)");
    return true;
}

void Audio::setVolume(uint8_t vol) {
    _volume = vol;
}
