#include "SettingsScreen.h"
#include "ui/Theme.h"
#include "hal/Display.h"
#include "config/Config.h"
#include "config/UserConfig.h"
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "radio/SX1262.h"
#include "audio/AudioNotify.h"
#include "hal/Power.h"
#include "transport/WiFiInterface.h"
#include "reticulum/ReticulumManager.h"
#include <Arduino.h>
#include <esp_system.h>

// Radio presets matching Ratspeak/Ratputer
struct RadioPreset {
    const char* name;
    uint8_t sf;
    uint32_t bw;
    uint8_t cr;
    int8_t txPower;
    long preamble;
};

static const RadioPreset PRESETS[] = {
    {"Balanced",   9,  250000, 5,  14, 18},   // Good range/speed balance (~2.4 Kbps)
    {"Long Range", 12, 125000, 8,  17, 18},   // Max range, slow (~300 bps)
    {"Fast",       7,  500000, 5,  10, 18},   // Max speed, short range (~21 Kbps)
};
static constexpr int NUM_PRESETS = 3;

bool SettingsScreen::isEditable(int idx) const {
    if (idx < 0 || idx >= (int)_items.size()) return false;
    auto t = _items[idx].type;
    return t == SettingType::INTEGER || t == SettingType::TOGGLE
        || t == SettingType::ENUM_CHOICE || t == SettingType::ACTION;
}

void SettingsScreen::skipToNextEditable(int dir) {
    int n = (int)_items.size();
    if (n == 0) return;
    int start = _selectedIdx;
    for (int i = 0; i < n; i++) {
        _selectedIdx += dir;
        if (_selectedIdx < 0) _selectedIdx = 0;
        if (_selectedIdx >= n) _selectedIdx = n - 1;
        if (isEditable(_selectedIdx)) return;
        if (_selectedIdx == 0 && dir < 0) return;
        if (_selectedIdx == n - 1 && dir > 0) return;
    }
    _selectedIdx = start;
}

int SettingsScreen::detectPreset() const {
    if (!_cfg) return -1;
    auto& s = _cfg->settings();
    for (int i = 0; i < NUM_PRESETS; i++) {
        if (s.loraSF == PRESETS[i].sf && s.loraBW == PRESETS[i].bw
            && s.loraCR == PRESETS[i].cr && s.loraTxPower == PRESETS[i].txPower) {
            return i;
        }
    }
    return -1; // Custom
}

void SettingsScreen::applyPreset(int presetIdx) {
    if (!_cfg || presetIdx < 0 || presetIdx >= NUM_PRESETS) return;
    auto& s = _cfg->settings();
    const auto& p = PRESETS[presetIdx];
    s.loraSF = p.sf;
    s.loraBW = p.bw;
    s.loraCR = p.cr;
    s.loraTxPower = p.txPower;
}

void SettingsScreen::buildItems() {
    _items.clear();
    if (!_cfg) return;
    auto& s = _cfg->settings();

    // --- Device ---
    _items.push_back({"-- Device --", SettingType::HEADER, nullptr, nullptr, nullptr});
    _items.push_back({"Version", SettingType::READONLY, nullptr, nullptr,
        [](int) { return String(RATDECK_VERSION_STRING); }});
    _items.push_back({"Identity", SettingType::READONLY, nullptr, nullptr,
        [this](int) { return _identityHash.substring(0, 16); }});

    // --- Display ---
    _items.push_back({"-- Display --", SettingType::HEADER, nullptr, nullptr, nullptr});
    _items.push_back({"Brightness", SettingType::INTEGER,
        [&s]() { return s.brightness; },
        [&s](int v) { s.brightness = v; },
        [](int v) { return String(v); },
        16, 255, 16});
    _items.push_back({"Dim Timeout", SettingType::INTEGER,
        [&s]() { return s.screenDimTimeout; },
        [&s](int v) { s.screenDimTimeout = v; },
        [](int v) { return String(v) + "s"; },
        5, 300, 5});
    _items.push_back({"Off Timeout", SettingType::INTEGER,
        [&s]() { return s.screenOffTimeout; },
        [&s](int v) { s.screenOffTimeout = v; },
        [](int v) { return String(v) + "s"; },
        10, 600, 10});

    // --- Radio ---
    _items.push_back({"-- Radio --", SettingType::HEADER, nullptr, nullptr, nullptr});

    // Radio Preset selector
    {
        SettingItem presetItem;
        presetItem.label = "Preset";
        presetItem.type = SettingType::ENUM_CHOICE;
        presetItem.getter = [this]() {
            int p = detectPreset();
            return (p >= 0) ? p : NUM_PRESETS; // NUM_PRESETS = "Custom"
        };
        presetItem.setter = [this](int v) {
            if (v >= 0 && v < NUM_PRESETS) {
                applyPreset(v);
                // Getter lambdas capture &s by reference — they read
                // live values from _cfg->settings(). No rebuild needed.
            }
            // "Custom" — no-op, leave individual settings as-is
        };
        presetItem.formatter = nullptr;
        presetItem.minVal = 0;
        presetItem.maxVal = NUM_PRESETS; // 0=Balanced, 1=Long Range, 2=Fast, 3=Custom
        presetItem.step = 1;
        presetItem.enumLabels = {"Balanced", "Long Range", "Fast", "Custom"};
        _items.push_back(presetItem);
    }

    _items.push_back({"TX Power", SettingType::INTEGER,
        [&s]() { return s.loraTxPower; },
        [&s](int v) { s.loraTxPower = v; },
        [](int v) { return String(v) + " dBm"; },
        -9, 22, 1});
    _items.push_back({"Spread Factor", SettingType::INTEGER,
        [&s]() { return s.loraSF; },
        [&s](int v) { s.loraSF = v; },
        [](int v) { return String("SF") + String(v); },
        5, 12, 1});
    _items.push_back({"Bandwidth", SettingType::ENUM_CHOICE,
        [&s]() {
            if (s.loraBW <= 62500)  return 0;
            if (s.loraBW <= 125000) return 1;
            if (s.loraBW <= 250000) return 2;
            return 3; // 500k
        },
        [&s](int v) {
            static const uint32_t bws[] = {62500, 125000, 250000, 500000};
            s.loraBW = bws[constrain(v, 0, 3)];
        },
        nullptr, 0, 3, 1, {"62.5k", "125k", "250k", "500k"}});
    _items.push_back({"Coding Rate", SettingType::INTEGER,
        [&s]() { return s.loraCR; },
        [&s](int v) { s.loraCR = v; },
        [](int v) { return String("4/") + String(v); },
        5, 8, 1});

    // --- Actions ---
    _items.push_back({"-- Actions --", SettingType::HEADER, nullptr, nullptr, nullptr});
    {
        SettingItem announceItem;
        announceItem.label = "Send Announce";
        announceItem.type = SettingType::ACTION;
        announceItem.formatter = [](int) { return String("[Press Enter]"); };
        announceItem.action = [this]() {
            if (_rns) {
                _rns->announce();
                if (_ui) {
                    _ui->statusBar().flashAnnounce();
                    _ui->statusBar().showToast("Announce sent!");
                }
                Serial.println("[SETTINGS] Manual announce sent");
            } else {
                if (_ui) _ui->statusBar().showToast("RNS not ready");
            }
        };
        _items.push_back(announceItem);
    }
    {
        SettingItem initSD;
        initSD.label = "Init SD Card";
        initSD.type = SettingType::ACTION;
        initSD.formatter = [this](int) {
            return (_sd && _sd->isReady()) ? String("[Press Enter]") : String("No Card");
        };
        initSD.action = [this]() {
            if (!_sd || !_sd->isReady()) {
                if (_ui) _ui->statusBar().showToast("No SD card!", 1200);
                return;
            }
            if (_ui) _ui->statusBar().showToast("Initializing SD...", 2000);
            bool ok = _sd->formatForRatputer();
            if (_ui) _ui->statusBar().showToast(ok ? "SD initialized!" : "SD init failed!", 1500);
            Serial.printf("[SETTINGS] SD init: %s\n", ok ? "OK" : "FAILED");
        };
        _items.push_back(initSD);
    }
    {
        SettingItem wipeSD;
        wipeSD.label = "Wipe SD Data";
        wipeSD.type = SettingType::ACTION;
        wipeSD.formatter = [this](int) {
            return (_sd && _sd->isReady()) ? String("[Press Enter]") : String("No Card");
        };
        wipeSD.action = [this]() {
            if (!_sd || !_sd->isReady()) {
                if (_ui) _ui->statusBar().showToast("No SD card!", 1200);
                return;
            }
            if (_ui) _ui->statusBar().showToast("Wiping SD data...", 2000);
            bool ok = _sd->wipeRatputer();
            if (_ui) _ui->statusBar().showToast(ok ? "SD wiped & reinit!" : "Wipe failed!", 1500);
            Serial.printf("[SETTINGS] SD wipe: %s\n", ok ? "OK" : "FAILED");
        };
        _items.push_back(wipeSD);
    }

    // --- Audio ---
    _items.push_back({"-- Audio --", SettingType::HEADER, nullptr, nullptr, nullptr});
    _items.push_back({"Audio Enable", SettingType::TOGGLE,
        [&s]() { return s.audioEnabled ? 1 : 0; },
        [&s](int v) { s.audioEnabled = (v != 0); },
        [](int v) { return v ? String("ON") : String("OFF"); }});
    _items.push_back({"Volume", SettingType::INTEGER,
        [&s]() { return s.audioVolume; },
        [&s](int v) { s.audioVolume = v; },
        [](int v) { return String(v) + "%"; },
        0, 100, 10});

    // --- Input ---
    _items.push_back({"-- Input --", SettingType::HEADER, nullptr, nullptr, nullptr});
    _items.push_back({"Trackball Speed", SettingType::INTEGER,
        [&s]() { return s.trackballSpeed; },
        [&s](int v) { s.trackballSpeed = v; },
        [](int v) { return String(v); },
        1, 5, 1});

    // --- Network ---
    _items.push_back({"-- Network --", SettingType::HEADER, nullptr, nullptr, nullptr});
    _items.push_back({"WiFi Mode", SettingType::ENUM_CHOICE,
        [&s]() { return (int)s.wifiMode; },
        [&s](int v) { s.wifiMode = (RatWiFiMode)v; },
        nullptr, 0, 2, 1, {"OFF", "AP", "STA"}});
    _items.push_back({"BLE", SettingType::TOGGLE,
        [&s]() { return s.bleEnabled ? 1 : 0; },
        [&s](int v) { s.bleEnabled = (v != 0); },
        [](int v) { return v ? String("ON") : String("OFF"); }});

    // --- System (readonly) ---
    _items.push_back({"-- System --", SettingType::HEADER, nullptr, nullptr, nullptr});
    _items.push_back({"Free Heap", SettingType::READONLY, nullptr, nullptr,
        [](int) { return String((unsigned long)(ESP.getFreeHeap() / 1024)) + " KB"; }});
    _items.push_back({"Free PSRAM", SettingType::READONLY, nullptr, nullptr,
        [](int) { return String((unsigned long)(ESP.getFreePsram() / 1024)) + " KB"; }});
    _items.push_back({"Flash", SettingType::READONLY, nullptr, nullptr,
        [this](int) { return _flash && _flash->exists("/ratputer") ? String("Mounted") : String("Error"); }});
    _items.push_back({"SD Card", SettingType::READONLY, nullptr, nullptr,
        [this](int) { return _sd && _sd->isReady() ? String("Ready") : String("Not Found"); }});
}

void SettingsScreen::onEnter() {
    buildItems();
    _selectedIdx = 0;
    _scrollOffset = 0;
    _editing = false;
    // Skip to first editable item
    if (!isEditable(_selectedIdx)) {
        skipToNextEditable(1);
    }
    markDirty();
}

void SettingsScreen::draw(LGFX_TDeck& gfx) {
    gfx.setTextSize(1);

    int lineH = 14;
    int visibleLines = Theme::CONTENT_H / lineH;
    int valX = 160;

    // Adjust scroll so selected item is visible
    if (_selectedIdx < _scrollOffset) _scrollOffset = _selectedIdx;
    if (_selectedIdx >= _scrollOffset + visibleLines) _scrollOffset = _selectedIdx - visibleLines + 1;

    for (int i = _scrollOffset; i < (int)_items.size(); i++) {
        int row = i - _scrollOffset;
        int y = Theme::CONTENT_Y + row * lineH;
        if (y + lineH > Theme::SCREEN_H - Theme::TAB_BAR_H) break;

        const auto& item = _items[i];
        bool selected = (i == _selectedIdx);

        // Selection highlight for editable items
        if (selected && isEditable(i)) {
            gfx.fillRect(0, y, Theme::SCREEN_W, lineH, Theme::SELECTION_BG);
        }

        uint32_t bgCol = (selected && isEditable(i)) ? Theme::SELECTION_BG : Theme::BG;

        if (item.type == SettingType::HEADER) {
            // Header row — accent color
            gfx.setTextColor(Theme::ACCENT, Theme::BG);
            gfx.setCursor(4, y + 3);
            gfx.print(item.label);
        } else if (item.type == SettingType::ACTION) {
            // Action button
            gfx.setTextColor(selected ? Theme::ACCENT : Theme::PRIMARY, bgCol);
            gfx.setCursor(4, y + 3);
            gfx.print(item.label);
            // Show hint on right
            if (item.formatter) {
                String hint = item.formatter(0);
                gfx.setTextColor(Theme::MUTED, bgCol);
                gfx.setCursor(valX, y + 3);
                gfx.print(hint.c_str());
            }
        } else {
            // Label
            gfx.setTextColor(Theme::SECONDARY, bgCol);
            gfx.setCursor(4, y + 3);
            gfx.print(item.label);

            // Value
            String valStr;
            if (_editing && selected) {
                // Show edit value
                if (item.type == SettingType::ENUM_CHOICE && !item.enumLabels.empty()) {
                    int idx = constrain(_editValue, 0, (int)item.enumLabels.size() - 1);
                    valStr = item.enumLabels[idx];
                } else if (item.formatter) {
                    valStr = item.formatter(_editValue);
                } else {
                    valStr = String(_editValue);
                }
                // Yellow value with arrows
                gfx.setTextColor(Theme::WARNING_CLR, bgCol);
                gfx.setCursor(valX - 12, y + 3);
                gfx.print("<");
                gfx.setCursor(valX, y + 3);
                gfx.print(valStr.c_str());
                int endX = valX + (int)valStr.length() * 6 + 4;
                gfx.setCursor(endX, y + 3);
                gfx.print(">");
            } else {
                // Normal display
                if (item.type == SettingType::READONLY) {
                    valStr = item.formatter ? item.formatter(0) : "";
                    gfx.setTextColor(Theme::MUTED, bgCol);
                } else if (item.type == SettingType::ENUM_CHOICE && !item.enumLabels.empty()) {
                    int idx = item.getter ? constrain(item.getter(), 0, (int)item.enumLabels.size() - 1) : 0;
                    valStr = item.enumLabels[idx];
                    gfx.setTextColor(Theme::PRIMARY, bgCol);
                } else {
                    int val = item.getter ? item.getter() : 0;
                    valStr = item.formatter ? item.formatter(val) : String(val);
                    gfx.setTextColor(Theme::PRIMARY, bgCol);
                }
                gfx.setCursor(valX, y + 3);
                gfx.print(valStr.c_str());
            }
        }
    }

    // Scroll indicator
    if ((int)_items.size() > visibleLines) {
        int barH = Theme::CONTENT_H;
        int thumbH = max(8, barH * visibleLines / (int)_items.size());
        int thumbY = Theme::CONTENT_Y + (barH - thumbH) * _scrollOffset / max(1, (int)_items.size() - visibleLines);
        gfx.fillRect(Theme::SCREEN_W - 2, Theme::CONTENT_Y, 2, barH, Theme::BORDER);
        gfx.fillRect(Theme::SCREEN_W - 2, thumbY, 2, thumbH, Theme::SECONDARY);
    }
}

bool SettingsScreen::handleKey(const KeyEvent& event) {
    if (_items.empty()) return false;

    if (_editing) {
        // Edit mode: left/right change value, enter confirms, backspace/del cancels
        auto& item = _items[_selectedIdx];

        if (event.left) {
            _editValue -= item.step;
            if (_editValue < item.minVal) _editValue = item.minVal;
            markDirty();
            return true;
        }
        if (event.right) {
            _editValue += item.step;
            if (_editValue > item.maxVal) _editValue = item.maxVal;
            markDirty();
            return true;
        }
        if (event.enter || event.character == '\n' || event.character == '\r') {
            // Confirm edit
            if (item.setter) item.setter(_editValue);
            _editing = false;
            applyAndSave();
            markDirty();
            return true;
        }
        if (event.del || event.character == 8) {
            // Cancel edit
            _editing = false;
            markDirty();
            return true;
        }
        return true;  // Consume all keys in edit mode
    }

    // Browse mode
    if (event.up) {
        int prev = _selectedIdx;
        skipToNextEditable(-1);
        if (_selectedIdx != prev) markDirty();
        return true;
    }
    if (event.down) {
        int prev = _selectedIdx;
        skipToNextEditable(1);
        if (_selectedIdx != prev) markDirty();
        return true;
    }
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (!isEditable(_selectedIdx)) return true;
        auto& item = _items[_selectedIdx];

        if (item.type == SettingType::ACTION) {
            // Execute action callback
            if (item.action) item.action();
            markDirty();
        } else if (item.type == SettingType::TOGGLE) {
            // Toggle immediately
            int val = item.getter ? item.getter() : 0;
            if (item.setter) item.setter(val ? 0 : 1);
            applyAndSave();
            markDirty();
        } else {
            // Enter edit mode
            _editing = true;
            _editValue = item.getter ? item.getter() : 0;
            markDirty();
        }
        return true;
    }
    return false;
}

void SettingsScreen::applyAndSave() {
    if (!_cfg) return;
    auto& s = _cfg->settings();

    // Apply to hardware immediately (regardless of save outcome)
    if (_power) {
        _power->setBrightness(s.brightness);
        _power->setDimTimeout(s.screenDimTimeout);
        _power->setOffTimeout(s.screenOffTimeout);
    }
    if (_radio && _radio->isRadioOnline()) {
        _radio->setTxPower(s.loraTxPower);
        _radio->setSpreadingFactor(s.loraSF);
        _radio->setSignalBandwidth(s.loraBW);
        _radio->setCodingRate4(s.loraCR);
        _radio->receive();
    }
    if (_audio) {
        _audio->setEnabled(s.audioEnabled);
        _audio->setVolume(s.audioVolume);
    }

    // Save to persistent storage
    bool saved = false;
    if (_saveCallback) {
        saved = _saveCallback();
    } else if (_sd && _flash) {
        saved = _cfg->save(*_sd, *_flash);
    } else if (_flash) {
        saved = _cfg->save(*_flash);
    }

    // Toast: differentiate between full save and apply-only
    if (_ui) {
        _ui->statusBar().showToast(saved ? "Settings saved" : "Applied (save failed)", 1200);
    }

    Serial.printf("[SETTINGS] Applied to hardware, save=%s\n", saved ? "OK" : "FAILED");
}
