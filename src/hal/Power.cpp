#include "Power.h"
#include "hal/Display.h"

// Forward declaration — display instance provided externally
extern Display display;

void Power::enablePeripherals() {
    // CRITICAL: GPIO 10 must be HIGH to enable all T-Deck Plus peripherals
    pinMode(BOARD_POWER_PIN, OUTPUT);
    digitalWrite(BOARD_POWER_PIN, HIGH);
    delay(10);  // Allow peripherals to stabilize
}

void Power::begin() {
    _lastActivity = millis();
    _state = ACTIVE;

    // Configure battery ADC
    pinMode(BAT_ADC_PIN, INPUT);
    analogReadResolution(12);

    Serial.println("[POWER] Power manager initialized");
}

float Power::batteryVoltage() const {
    // T-Deck Plus: voltage divider on GPIO 4
    int raw = analogRead(BAT_ADC_PIN);
    // Voltage divider: 2x ratio, 3.3V reference, 12-bit ADC
    return (raw / 4095.0f) * 3.3f * 2.0f;
}

int Power::batteryPercent() const {
    float v = batteryVoltage();
    // LiPo voltage curve approximation
    if (v >= 4.2f) return 100;
    if (v <= 3.0f) return 0;
    return (int)((v - 3.0f) / 1.2f * 100.0f);
}

void Power::activity() {
    _lastActivity = millis();
    if (_state != ACTIVE) {
        setState(ACTIVE);
    }
}

void Power::setBrightness(uint8_t brightness) {
    _fullBrightness = brightness;
    if (_state == ACTIVE) {
        display.setBrightness(_fullBrightness);
    }
}

void Power::loop() {
    unsigned long elapsed = millis() - _lastActivity;

    switch (_state) {
        case ACTIVE:
            if (_offTimeout > 0 && elapsed >= _offTimeout) {
                setState(SCREEN_OFF);
            } else if (_dimTimeout > 0 && elapsed >= _dimTimeout) {
                setState(DIMMED);
            }
            break;

        case DIMMED:
            if (_offTimeout > 0 && elapsed >= _offTimeout) {
                setState(SCREEN_OFF);
            }
            break;

        case SCREEN_OFF:
            break;
    }
}

void Power::setState(State newState) {
    if (newState == _state) return;
    _state = newState;

    switch (_state) {
        case ACTIVE:
            display.wakeup();
            display.setBrightness(_fullBrightness);
            break;
        case DIMMED:
            display.setBrightness(DIM_BRIGHTNESS);
            break;
        case SCREEN_OFF:
            display.setBrightness(0);
            display.sleep();
            break;
    }
}
