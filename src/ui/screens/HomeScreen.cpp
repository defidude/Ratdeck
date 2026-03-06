#include "HomeScreen.h"
#include "ui/Theme.h"
#include "hal/Display.h"
#include "reticulum/ReticulumManager.h"
#include "radio/SX1262.h"
#include "config/UserConfig.h"
#include <Arduino.h>
#include <esp_system.h>

void HomeScreen::update() {
    // Only redraw when minute changes or heap changes significantly
    unsigned long upMins = millis() / 60000;
    uint32_t heap = ESP.getFreeHeap() / 1024;
    if (upMins != _lastUptime || heap != _lastHeap) {
        _lastUptime = upMins;
        _lastHeap = heap;
        markDirty();
    }
}

void HomeScreen::draw(LGFX_TDeck& gfx) {
    int x = 4;
    int y = Theme::CONTENT_Y + 4;
    int lineH = 12;

    gfx.setTextSize(1);

    auto drawLine = [&](uint32_t col, const char* fmt, ...) {
        char buf[80];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        gfx.setTextColor(col, Theme::BG);
        gfx.setCursor(x, y);
        gfx.print(buf);
        y += lineH;
    };

    if (_rns) {
        drawLine(Theme::PRIMARY, "ID: %s", _rns->identityHash().c_str());
        drawLine(Theme::PRIMARY, "Transport: %s",
            _rns->isTransportActive() ? "ACTIVE" : "OFFLINE");
        drawLine(Theme::PRIMARY, "Paths: %d  Links: %d",
            (int)_rns->pathCount(), (int)_rns->linkCount());
    } else {
        drawLine(Theme::MUTED, "Identity: ---");
        drawLine(Theme::MUTED, "Transport: OFFLINE");
        drawLine(Theme::MUTED, "Paths: 0  Links: 0");
    }

    if (_radio && _radio->isRadioOnline()) {
        drawLine(Theme::PRIMARY, "LoRa: SF%d BW%luk %ddBm",
            _radio->getSpreadingFactor(),
            (unsigned long)(_radio->getSignalBandwidth() / 1000),
            _radio->getTxPower());
    } else {
        drawLine(Theme::ERROR_CLR, "Radio: OFFLINE");
    }

    drawLine(Theme::PRIMARY, "Heap: %lukB free",
        (unsigned long)(ESP.getFreeHeap() / 1024));

    drawLine(Theme::PRIMARY, "PSRAM: %lukB free",
        (unsigned long)(ESP.getFreePsram() / 1024));

    unsigned long mins = millis() / 60000;
    if (mins >= 60) {
        drawLine(Theme::PRIMARY, "Uptime: %luh %lum", mins / 60, mins % 60);
    } else {
        drawLine(Theme::PRIMARY, "Uptime: %lum", mins);
    }
}
