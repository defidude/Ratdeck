#include "MapScreen.h"
#include "ui/Theme.h"
#include "hal/Display.h"

void MapScreen::draw(LGFX_TDeck& gfx) {
    gfx.setTextSize(1);
    gfx.setTextColor(Theme::MUTED, Theme::BG);

    const char* lines[] = {
        "Map",
        "",
        "Coming soon",
        "",
        "Node topology view",
        "will appear here"
    };

    int y = Theme::CONTENT_Y + Theme::CONTENT_H / 2 - 36;
    for (const char* line : lines) {
        int tw = strlen(line) * 6;
        gfx.setCursor(Theme::SCREEN_W / 2 - tw / 2, y);
        gfx.print(line);
        y += 12;
    }
}
