#include "GPS.h"
#include "config/BoardConfig.h"

bool GPS::begin() {
    // GPS is deprioritized — stub only
    // UBlox MIA-M10Q on UART: TX=43, RX=44, 115200 baud
    Serial.println("[GPS] Disabled (deprioritized)");
    return false;
}

void GPS::loop() {
    // No-op
}
