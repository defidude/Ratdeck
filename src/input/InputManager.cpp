#include "InputManager.h"
#include "config/BoardConfig.h"

void InputManager::begin(Keyboard* kb, Trackball* tb, TouchInput* touch) {
    _kb = kb;
    _tb = tb;
    _touch = touch;
}

void InputManager::update() {
    _hasKey = false;
    _activity = false;
    _strongActivity = false;
    _longPress = false;

    // Poll keyboard
    if (_kb) {
        _kb->update();
        if (_kb->hasEvent()) {
            _keyEvent = _kb->getEvent();
            _hasKey = true;
            _activity = true;
            _strongActivity = true;
        }
    }

    // Poll trackball — convert deltas to nav KeyEvents
    if (_tb) {
        _tb->update();
        if (_tb->hadMovement()) {
            _activity = true;  // Movement is weak — only wakes from dim
        }

        // Generate nav events from trackball movement (click handled below via GPIO).
        // Skip entirely when screen is off so a backpacked device doesn't accumulate
        // phantom up/down/left/right keypresses or wake from movement.
        bool screenOn = !_powerMgr || _powerMgr->isScreenOn();
        if (!_hasKey && screenOn) {
            unsigned long now = millis();

            // Accumulate deltas, clamp to ±20
            _tbAccumX += _tb->lastDeltaX();
            _tbAccumY += _tb->lastDeltaY();
            if (_tbAccumX > 20) _tbAccumX = 20;
            if (_tbAccumX < -20) _tbAccumX = -20;
            if (_tbAccumY > 20) _tbAccumY = 20;
            if (_tbAccumY < -20) _tbAccumY = -20;

            if (now - _lastTbNavTime >= TB_NAV_RATE_MS) {
                int8_t absX = _tbAccumX < 0 ? -_tbAccumX : _tbAccumX;
                int8_t absY = _tbAccumY < 0 ? -_tbAccumY : _tbAccumY;
                bool yDominant = absY >= absX;

                if (yDominant && absY >= TB_NAV_THRESHOLD) {
                    _keyEvent = {};
                    if (_tbAccumY < 0) _keyEvent.up = true;
                    else               _keyEvent.down = true;
                    _hasKey = true;
                    _activity = true;
                    _strongActivity = true;
                    _lastTbNavTime = now;
                    _tbAccumX = 0;
                    _tbAccumY = 0;
                }
                else if (!yDominant && absX >= TB_NAV_THRESHOLD) {
                    _keyEvent = {};
                    if (_tbAccumX < 0) _keyEvent.left = true;
                    else               _keyEvent.right = true;
                    _hasKey = true;
                    _activity = true;
                    _strongActivity = true;
                    _lastTbNavTime = now;
                    _tbAccumX = 0;
                    _tbAccumY = 0;
                }
            }
        }

        // Click / long-press detection via GPIO (deferred click with debounce)
        // Short click fires on button RELEASE; long press fires after hold threshold
        // Debounce: require GPIO HIGH for CLICK_DEBOUNCE_MS before accepting release
        bool clickDown = (digitalRead(TBALL_CLICK) == LOW);

        if (clickDown) {
            _lastClickDownMs = millis();  // Track last time GPIO was LOW
            if (!_clickPending) {
                // Button just went down — start tracking, don't fire yet
                _clickPending = true;
                _longPressFired = false;
                _clickStartMs = millis();
                // Capture screen state BEFORE activity wakes it, so long-press
                // doesn't blank a freshly-woken screen (wake-then-blank ping-pong)
                _clickFromScreenOn = _powerMgr ? _powerMgr->isScreenOn() : true;
                _activity = true;
                _strongActivity = true;  // Click wakes from screen off
            } else if (!_longPressFired && millis() - _clickStartMs >= LONG_PRESS_MS) {
                // Long press threshold reached — only emit if click started screen-on
                if (_clickFromScreenOn) {
                    _longPress = true;
                }
                _longPressFired = true;
                _hasKey = false;  // Suppress any concurrent events
                _activity = true;
                _strongActivity = true;
            }
        } else if (_clickPending) {
            // GPIO is HIGH — only accept release after debounce period
            if (millis() - _lastClickDownMs >= CLICK_DEBOUNCE_MS) {
                _clickPending = false;
                // Suppress wake-click: if the press began with the screen off,
                // the click's job was just to wake the device, not to confirm.
                if (!_longPressFired && !_hasKey && _clickFromScreenOn) {
                    _keyEvent = {};
                    _keyEvent.enter = true;
                    _keyEvent.character = '\n';
                    _hasKey = true;
                    _activity = true;
                    _strongActivity = true;
                    _lastTbNavTime = millis();
                    _tbAccumX = 0;
                    _tbAccumY = 0;
                }
                _longPressFired = false;
            }
            // If GPIO was LOW too recently, ignore — likely bounce
        }
    }

    // Touch activity check — throttled to ~50Hz.
    // Suppressed while screen is off (pocket-carry safety: prevents
    // accidental wakes from pressure on the touch panel).
    if (_touch && (!_powerMgr || _powerMgr->isScreenOn())) {
        unsigned long now = millis();
        if (now - _lastTouchPoll >= TOUCH_POLL_MS) {
            _lastTouchPoll = now;
            _touch->update();
            if (_touch->isTouched()) {
                _activity = true;
                _strongActivity = true;
            }
        }
    }
}
