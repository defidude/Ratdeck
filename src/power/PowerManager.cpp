#include "PowerManager.h"
#include "hal/Display.h"

void PowerManager::begin(Display* display) {
    _display = display;
    _lastActivity = millis();
    _state = ACTIVE;
    if (_display) _display->setBrightness(_fullBrightness);
}

void PowerManager::activity() {
    _lastActivity = millis();
    if (_state != ACTIVE) {
        setState(ACTIVE);
    }
}

void PowerManager::loop() {
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
            // Stay off until activity()
            break;
    }
}

void PowerManager::setState(State newState) {
    if (newState == _state) return;
    _state = newState;

    if (!_display) return;

    switch (_state) {
        case ACTIVE:
            _display->setBrightness(_fullBrightness);
            break;
        case DIMMED:
            _display->setBrightness(DIM_BRIGHTNESS);
            break;
        case SCREEN_OFF:
            _display->setBrightness(0);
            break;
    }
}
