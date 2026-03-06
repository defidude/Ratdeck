#pragma once

#include <Arduino.h>
#include <functional>
#include <map>
#include "hal/Keyboard.h"

class HotkeyManager {
public:
    using HotkeyCallback = std::function<void()>;

    // Register a Ctrl+key hotkey
    void registerHotkey(char key, const char* name, HotkeyCallback callback);

    // Register left/right arrow actions (non-Ctrl)
    void setTabCycleCallback(std::function<void(int direction)> cb) { _tabCycleCb = cb; }

    // Process a key event. Returns true if consumed by hotkey.
    bool process(const KeyEvent& event);

private:
    struct HotkeyEntry {
        const char* name;
        HotkeyCallback callback;
    };

    std::map<char, HotkeyEntry> _hotkeys;
    std::function<void(int direction)> _tabCycleCb;
};
