#include "StatusBar.h"
#include "Theme.h"
#include "hal/Display.h"
#include <Arduino.h>

void StatusBar::update() {
    bool wasFlashing = (_announceFlashEnd > 0);
    if (wasFlashing && millis() >= _announceFlashEnd) {
        _announceFlashEnd = 0;
        markDirty();
    }
    if (_toastEnd > 0 && millis() >= _toastEnd) {
        _toastEnd = 0;
        _toastMsg.clear();
        markDirty();
    }
}

void StatusBar::draw(LGFX_TDeck& gfx) {
    // Toast notification replaces status bar temporarily
    if (_toastEnd > 0 && millis() < _toastEnd) {
        gfx.fillRect(0, 0, Theme::SCREEN_W, Theme::STATUS_BAR_H, Theme::ACCENT);
        gfx.setTextSize(1);
        gfx.setTextColor(Theme::BG, Theme::ACCENT);
        // Center the toast text
        int textW = (int)_toastMsg.length() * 6;
        int tx = (Theme::SCREEN_W - textW) / 2;
        gfx.setCursor(tx, 3);
        gfx.print(_toastMsg.c_str());
        gfx.drawFastHLine(0, Theme::STATUS_BAR_H - 1, Theme::SCREEN_W, Theme::BORDER);
        return;
    }

    // Clear status bar area
    gfx.fillRect(0, 0, Theme::SCREEN_W, Theme::STATUS_BAR_H, Theme::BG);
    gfx.setTextSize(1);

    int x = 2;

    // LoRa indicator
    bool flashing = _announceFlashEnd > 0 && millis() < _announceFlashEnd;
    if (_loraOnline) {
        gfx.setTextColor(flashing ? Theme::ACCENT : Theme::PRIMARY, Theme::BG);
        gfx.setCursor(x, 3);
        gfx.print(flashing ? "LoRa TX" : "LoRa OK");
    } else {
        gfx.setTextColor(Theme::MUTED, Theme::BG);
        gfx.setCursor(x, 3);
        gfx.print("LoRa --");
    }
    x += 50;

    // BLE indicator
    if (_bleActive) {
        gfx.setTextColor(Theme::ACCENT, Theme::BG);
        gfx.setCursor(x, 3);
        gfx.print("BLE");
        x += 30;
    }

    // WiFi indicator
    if (_wifiActive) {
        gfx.setTextColor(Theme::PRIMARY, Theme::BG);
        gfx.setCursor(x, 3);
        gfx.print("WiFi");
    }

    // Battery (right side)
    if (_battPct >= 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", _battPct);
        uint32_t col = Theme::PRIMARY;
        if (_battPct <= 15) col = Theme::ERROR_CLR;
        else if (_battPct <= 30) col = Theme::WARNING_CLR;
        gfx.setTextColor(col, Theme::BG);
        gfx.setCursor(Theme::SCREEN_W - 60, 3);
        gfx.print(buf);
    }

    // Uptime (far right) — minutes only to avoid per-second redraws
    unsigned long mins = millis() / 60000;
    char timeBuf[16];
    if (mins >= 60) {
        snprintf(timeBuf, sizeof(timeBuf), "%luh%lum", mins / 60, mins % 60);
    } else {
        snprintf(timeBuf, sizeof(timeBuf), "%lum", mins);
    }
    gfx.setTextColor(Theme::SECONDARY, Theme::BG);
    gfx.setCursor(Theme::SCREEN_W - 36, 3);
    gfx.print(timeBuf);

    // Bottom border line
    gfx.drawFastHLine(0, Theme::STATUS_BAR_H - 1, Theme::SCREEN_W, Theme::BORDER);
}

void StatusBar::flashAnnounce() {
    _announceFlashEnd = millis() + 1000;
    markDirty();
}

void StatusBar::showToast(const char* msg, uint32_t durationMs) {
    _toastMsg = msg;
    _toastEnd = millis() + durationMs;
    markDirty();
}
