#include "LvBootScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "config/Config.h"
#include "fonts/fonts.h"

void LvBootScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Title: "RATDECK"
    _lblTitle = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_lblTitle, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(_lblTitle, "RATDECK");
    lv_obj_align(_lblTitle, LV_ALIGN_TOP_MID, 0, 60);

    // Version
    _lblVersion = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblVersion, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(_lblVersion, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    char ver[32];
    snprintf(ver, sizeof(ver), "v%s", RATDECK_VERSION_STRING);
    lv_label_set_text(_lblVersion, ver);
    lv_obj_align(_lblVersion, LV_ALIGN_TOP_MID, 0, 82);

    // Progress bar
    _bar = lv_bar_create(parent);
    lv_obj_set_size(_bar, 200, 10);
    lv_obj_align(_bar, LV_ALIGN_TOP_MID, 0, 105);
    lv_bar_set_range(_bar, 0, 100);
    lv_bar_set_value(_bar, 0, LV_ANIM_OFF);
    lv_obj_add_style(_bar, LvTheme::styleBar(), LV_PART_MAIN);
    lv_obj_add_style(_bar, LvTheme::styleBarIndicator(), LV_PART_INDICATOR);

    // Status text
    _lblStatus = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblStatus, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(_lblStatus, "Starting...");
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_MID, 0, 128);
}

void LvBootScreen::setProgress(float progress, const char* status) {
    if (_bar) {
        lv_bar_set_value(_bar, (int)(progress * 100), LV_ANIM_OFF);
    }
    if (_lblStatus) {
        lv_label_set_text(_lblStatus, status);
    }
    // Force LVGL to flush during boot (before main loop)
    lv_timer_handler();
}
