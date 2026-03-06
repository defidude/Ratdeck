#include "InputManager.h"

void InputManager::begin(Keyboard* kb, Trackball* tb, TouchInput* touch) {
    _kb = kb;
    _tb = tb;
    _touch = touch;
}

void InputManager::update() {
    _hasKey = false;
    _activity = false;

    // Poll keyboard
    if (_kb) {
        _kb->update();
        if (_kb->hasEvent()) {
            _keyEvent = _kb->getEvent();
            _hasKey = true;
            _activity = true;
        }
    }

    // Poll trackball — convert deltas to nav KeyEvents
    if (_tb) {
        _tb->update();
        if (_tb->hadMovement() || _tb->wasClicked()) {
            _activity = true;
        }

        // Generate nav events from trackball when no keyboard key was pressed
        if (!_hasKey) {
            unsigned long now = millis();

            // Accumulate deltas, clamp to ±20
            _tbAccumX += _tb->lastDeltaX();
            _tbAccumY += _tb->lastDeltaY();
            if (_tbAccumX > 20) _tbAccumX = 20;
            if (_tbAccumX < -20) _tbAccumX = -20;
            if (_tbAccumY > 20) _tbAccumY = 20;
            if (_tbAccumY < -20) _tbAccumY = -20;

            if (now - _lastTbNavTime >= TB_NAV_RATE_MS) {
                // Click → enter
                if (_tb->wasClicked()) {
                    _keyEvent = {};
                    _keyEvent.enter = true;
                    _hasKey = true;
                    _lastTbNavTime = now;
                    _tbAccumX = 0;
                    _tbAccumY = 0;
                }
                // Pick dominant axis — whichever accumulated more wins
                else {
                    int8_t absX = _tbAccumX < 0 ? -_tbAccumX : _tbAccumX;
                    int8_t absY = _tbAccumY < 0 ? -_tbAccumY : _tbAccumY;
                    bool yDominant = absY >= absX;

                    if (yDominant && absY >= TB_NAV_THRESHOLD) {
                        _keyEvent = {};
                        if (_tbAccumY < 0) _keyEvent.up = true;
                        else               _keyEvent.down = true;
                        _hasKey = true;
                        _lastTbNavTime = now;
                        _tbAccumX = 0;
                        _tbAccumY = 0;
                    }
                    else if (!yDominant && absX >= TB_NAV_THRESHOLD) {
                        _keyEvent = {};
                        if (_tbAccumX < 0) _keyEvent.left = true;
                        else               _keyEvent.right = true;
                        _hasKey = true;
                        _lastTbNavTime = now;
                        _tbAccumX = 0;
                        _tbAccumY = 0;
                    }
                }
            }
        }
    }

    // Touch is polled by LVGL indev read callback — just check for activity
    if (_touch && _touch->isTouched()) {
        _activity = true;
    }
}
