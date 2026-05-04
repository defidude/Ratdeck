#include "LvBootScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "fonts/fonts.h"

namespace {

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    uint32_t color, lv_coord_t width, lv_text_align_t align) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_width(lbl, width);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(lbl, align, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text);
    return lbl;
}

int progressToPercent(float progress) {
    if (progress < 0.0f) return 0;
    if (progress > 1.0f) return 100;
    return (int)(progress * 100.0f + 0.5f);
}

}  // namespace

void LvBootScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    _lblTitle = makeLabel(parent, "RATSPEAK", &lv_font_montserrat_16,
                          Theme::ACCENT, 240, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(_lblTitle, LV_ALIGN_CENTER, 0, -26);

    _bar = lv_bar_create(parent);
    lv_obj_set_size(_bar, 188, 8);
    lv_obj_align(_bar, LV_ALIGN_CENTER, 0, 14);
    lv_bar_set_range(_bar, 0, 100);
    lv_bar_set_value(_bar, 0, LV_ANIM_OFF);
    lv_obj_add_style(_bar, LvTheme::styleBar(), LV_PART_MAIN);
    lv_obj_add_style(_bar, LvTheme::styleBarIndicator(), LV_PART_INDICATOR);
}

void LvBootScreen::setProgress(float progress, const char* status) {
    (void)status;
    int percent = progressToPercent(progress);
    if (_bar) {
        lv_bar_set_value(_bar, percent, LV_ANIM_OFF);
    }
    // Force LVGL to flush during boot (before main loop)
    lv_timer_handler();
}
