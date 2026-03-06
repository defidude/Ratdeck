#include "HotkeyManager.h"

void HotkeyManager::registerHotkey(char key, const char* name, HotkeyCallback callback) {
    _hotkeys[tolower(key)] = {name, callback};
}

bool HotkeyManager::process(const KeyEvent& event) {
    // Ctrl+key hotkeys (always active regardless of input mode)
    if (event.ctrl && event.character != 0) {
        char key = tolower(event.character);
        auto it = _hotkeys.find(key);
        if (it != _hotkeys.end()) {
            Serial.printf("[HOTKEY] Ctrl+%c -> %s\n", key, it->second.name);
            if (it->second.callback) {
                it->second.callback();
            }
            return true;
        }
    }

    // Tab cycling is handled in main loop (after screen gets a chance to consume)
    return false;
}
