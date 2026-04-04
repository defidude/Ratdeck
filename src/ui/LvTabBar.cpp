#include "LvTabBar.h"
#include "Theme.h"
#include "LvTheme.h"
#include <cstdio>
#include "fonts/fonts.h"

constexpr const char* LvTabBar::TAB_NAMES[TAB_COUNT];

static void tab_click_cb(lv_event_t* e) {
    LvTabBar* bar = (LvTabBar*)lv_event_get_user_data(e);
    lv_obj_t* target = lv_event_get_target(e);
    for (int i = 0; i < LvTabBar::TAB_COUNT; i++) {
        lv_obj_t* parent = lv_obj_get_parent(target);
        if (target == lv_obj_get_child(bar->obj(), i) ||
            (parent && parent == lv_obj_get_child(bar->obj(), i))) {
            bar->setActiveTab(i);
            break;
        }
    }
}

void LvTabBar::create(lv_obj_t* parent) {
    _bar = lv_obj_create(parent);
    lv_obj_set_size(_bar, Theme::SCREEN_W, Theme::TAB_BAR_H);
    lv_obj_align(_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_bar, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_bar, lv_color_hex(Theme::DIVIDER), 0);
    lv_obj_set_style_border_width(_bar, 1, 0);
    lv_obj_set_style_border_side(_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(_bar, 0, 0);
    lv_obj_set_style_radius(_bar, 0, 0);
    lv_obj_clear_flag(_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const lv_font_t* font = &lv_font_ratdeck_12;

    for (int i = 0; i < TAB_COUNT; i++) {
        _tabs[i] = lv_label_create(_bar);
        lv_obj_set_style_text_font(_tabs[i], font, 0);
        lv_label_set_recolor(_tabs[i], true);
        lv_label_set_text(_tabs[i], TAB_NAMES[i]);
        lv_obj_add_flag(_tabs[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(_tabs[i], tab_click_cb, LV_EVENT_CLICKED, this);
    }

    refreshTabs();
}

void LvTabBar::setActiveTab(int tab) {
    if (tab < 0 || tab >= TAB_COUNT) return;
    _activeTab = tab;
    refreshTabs();
    if (_tabCb) _tabCb(tab);
}

void LvTabBar::cycleTab(int direction) {
    int next = (_activeTab + direction + TAB_COUNT) % TAB_COUNT;
    setActiveTab(next);
}

void LvTabBar::setUnreadCount(int tab, int count) {
    if (tab < 0 || tab >= TAB_COUNT) return;
    if (_unread[tab] == count) return;  // No change
    _unread[tab] = count;
    refreshTab(tab);
}

void LvTabBar::refreshTab(int idx) {
    if (idx < 0 || idx >= TAB_COUNT || !_tabs[idx]) return;
    bool active = (idx == _activeTab);
    lv_obj_set_style_text_color(_tabs[idx],
        lv_color_hex(active ? Theme::TAB_ACTIVE : Theme::TAB_INACTIVE), 0);

    char buf[48];
    if (_unread[idx] > 0) {
        // Unread count in badge color using LVGL recolor
        snprintf(buf, sizeof(buf), "%s #%06X %d#",
            TAB_NAMES[idx], (unsigned int)Theme::BADGE_BG, _unread[idx]);
    } else {
        snprintf(buf, sizeof(buf), "%s", TAB_NAMES[idx]);
    }
    lv_label_set_text(_tabs[idx], buf);
}

void LvTabBar::refreshTabs() {
    for (int i = 0; i < TAB_COUNT; i++) {
        refreshTab(i);
    }
}
