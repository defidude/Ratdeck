#include "Display.h"

bool Display::begin() {
    _gfx.init();
    _gfx.setRotation(1);  // Landscape: 320x240
    _gfx.setBrightness(128);
    _gfx.fillScreen(TFT_BLACK);

    Serial.printf("[DISPLAY] Initialized: %dx%d (rotation=1, LovyanGFX direct)\n",
                  _gfx.width(), _gfx.height());

    // Quick visual test: draw colored rectangles directly
    _gfx.fillRect(0, 0, 107, 120, TFT_RED);
    _gfx.fillRect(107, 0, 107, 120, TFT_GREEN);
    _gfx.fillRect(214, 0, 106, 120, TFT_BLUE);
    _gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    _gfx.setTextSize(2);
    _gfx.setCursor(60, 140);
    _gfx.print("RATDECK DISPLAY TEST");
    delay(1500);
    _gfx.fillScreen(TFT_BLACK);

    return true;
}

void Display::setBrightness(uint8_t level) {
    _gfx.setBrightness(level);
}

void Display::sleep() {
    _gfx.sleep();
}

void Display::wakeup() {
    _gfx.wakeup();
}
