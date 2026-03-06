#include "HelpOverlay.h"
#include "ui/Theme.h"
#include "hal/Display.h"

void HelpOverlay::draw(LGFX_TDeck& gfx) {
    if (!_visible) return;

    // Semi-transparent overlay box
    int bx = 20, by = Theme::CONTENT_Y + 10;
    int bw = Theme::CONTENT_W - 40;
    int bh = Theme::CONTENT_H - 20;

    gfx.fillRect(bx, by, bw, bh, Theme::BG);
    gfx.drawRect(bx, by, bw, bh, Theme::ACCENT);

    gfx.setTextSize(1);
    int x = bx + 8;
    int y = by + 8;
    int lineH = 12;

    // Title
    gfx.setTextColor(Theme::ACCENT, Theme::BG);
    gfx.setCursor(x, y);
    gfx.print("HOTKEYS");
    y += lineH + 4;

    const char* lines[] = {
        "Ctrl+H  This help",
        "Ctrl+M  Messages",
        "Ctrl+N  New message",
        "Ctrl+S  Settings",
        "Ctrl+A  Force announce",
        "Ctrl+D  Diagnostics (serial)",
        "Ctrl+T  Radio test TX",
        "Ctrl+R  RSSI monitor",
        ",  /    Cycle tabs",
        ";  .    Scroll up/down",
        "Esc     Back",
    };

    gfx.setTextColor(Theme::PRIMARY, Theme::BG);
    for (const char* line : lines) {
        gfx.setCursor(x, y);
        gfx.print(line);
        y += lineH;
    }
}

bool HelpOverlay::handleKey(const KeyEvent& event) {
    _visible = false;
    return true;
}
