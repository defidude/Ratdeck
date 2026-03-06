#include "BootScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"
#include "hal/Display.h"

void BootScreen::draw(LGFX_TDeck& gfx) {
    int cx = Theme::SCREEN_W / 2;

    // Title
    gfx.setTextSize(2);
    gfx.setTextColor(Theme::PRIMARY, Theme::BG);
    const char* title = "RATDECK";
    int tw = strlen(title) * 12;  // 12px per char at size 2
    gfx.setCursor(cx - tw / 2, 70);
    gfx.print(title);

    // Version
    gfx.setTextSize(1);
    gfx.setTextColor(Theme::SECONDARY, Theme::BG);
    char ver[32];
    snprintf(ver, sizeof(ver), "v%s", RATDECK_VERSION_STRING);
    int vw = strlen(ver) * 6;
    gfx.setCursor(cx - vw / 2, 95);
    gfx.print(ver);

    // Progress bar background
    int barX = cx - 100;
    int barY = 120;
    int barW = 200;
    int barH = 10;
    gfx.fillRect(barX, barY, barW, barH, Theme::BORDER);

    // Progress bar fill
    int fillW = (int)(barW * _progress);
    if (fillW > 0) {
        gfx.fillRect(barX, barY, fillW, barH, Theme::PRIMARY);
    }

    // Status text
    gfx.setTextSize(1);
    gfx.setTextColor(Theme::SECONDARY, Theme::BG);
    int sw = strlen(_status) * 6;
    gfx.setCursor(cx - sw / 2, 145);
    gfx.print(_status);
}

void BootScreen::setProgress(float progress, const char* status) {
    _progress = progress;
    strncpy(_status, status, sizeof(_status) - 1);
    _status[sizeof(_status) - 1] = '\0';
    markDirty();

    // Force immediate render during boot (no main loop running yet)
    // Caller (main.cpp) must call ui.render() after this
}
