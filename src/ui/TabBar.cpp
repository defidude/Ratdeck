#include "TabBar.h"
#include "Theme.h"
#include "hal/Display.h"
#include <Arduino.h>
#include <cstdio>

constexpr const char* TabBar::TAB_NAMES[5];

void TabBar::draw(LGFX_TDeck& gfx) {
    int y = Theme::SCREEN_H - Theme::TAB_BAR_H;

    // Clear tab bar area
    gfx.fillRect(0, y, Theme::SCREEN_W, Theme::TAB_BAR_H, Theme::BG);

    // Top border
    gfx.drawFastHLine(0, y, Theme::SCREEN_W, Theme::BORDER);

    gfx.setTextSize(1);

    for (int i = 0; i < 5; i++) {
        int tx = i * Theme::TAB_W;

        // Active tab indicator
        if (i == _activeTab) {
            gfx.drawFastHLine(tx + 2, y, Theme::TAB_W - 4, Theme::ACCENT);
        }

        // Tab label
        uint32_t col = (i == _activeTab) ? Theme::TAB_ACTIVE : Theme::TAB_INACTIVE;
        gfx.setTextColor(col, Theme::BG);

        char label[24];
        if (_unread[i] > 0) {
            snprintf(label, sizeof(label), "%s(%d)", TAB_NAMES[i], _unread[i]);
        } else {
            snprintf(label, sizeof(label), "%s", TAB_NAMES[i]);
        }

        // Center text in tab
        int textW = strlen(label) * 6;  // 6px per char at textSize 1
        int textX = tx + (Theme::TAB_W - textW) / 2;
        gfx.setCursor(textX, y + 4);
        gfx.print(label);
    }
}

void TabBar::setActiveTab(int tab) {
    if (tab >= 0 && tab < 5) {
        _activeTab = tab;
        markDirty();
    }
}

void TabBar::cycleTab(int direction) {
    _activeTab = (_activeTab + direction + 5) % 5;
    markDirty();
}

void TabBar::setUnreadCount(int tab, int count) {
    if (tab >= 0 && tab < 5) {
        _unread[tab] = count;
        markDirty();
    }
}
