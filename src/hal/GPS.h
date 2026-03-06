#pragma once

#include <Arduino.h>

// UBlox MIA-M10Q GPS driver for T-Deck Plus
// Stub — GPS is deprioritized per plan
class GPS {
public:
    bool begin();
    void loop();

    bool hasFix() const { return false; }
    double latitude() const { return 0; }
    double longitude() const { return 0; }
    int satellites() const { return 0; }

private:
    bool _enabled = false;
};
