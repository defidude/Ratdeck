#include "LvTimezoneScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "fonts/fonts.h"
#include <Arduino.h>

namespace {

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    uint32_t color, lv_coord_t width, lv_text_align_t align,
                    lv_label_long_mode_t mode = LV_LABEL_LONG_WRAP) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_width(lbl, width);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(lbl, align, 0);
    lv_label_set_long_mode(lbl, mode);
    lv_label_set_text(lbl, text);
    return lbl;
}

void stylePanel(lv_obj_t* panel) {
    lv_obj_set_style_bg_color(panel, lv_color_hex(Theme::BG_ELEVATED), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(Theme::BORDER_ACTIVE), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 4, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_set_style_clip_corner(panel, true, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

int clampTimezoneIndex(int idx) {
    if (idx < 0 || idx >= TIMEZONE_COUNT) return 6;
    return idx;
}

lv_obj_t* makeActionButton(lv_obj_t* parent, const char* text, lv_coord_t x,
                           lv_coord_t y, lv_coord_t w, lv_coord_t h,
                           bool primary) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_style(btn, LvTheme::styleBtn(), 0);
    lv_obj_add_style(btn, LvTheme::styleBtnFocused(), LV_STATE_FOCUSED);
    lv_obj_add_style(btn, LvTheme::styleBtnPressed(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn,
        lv_color_hex(primary ? Theme::PRIMARY_SUBTLE : Theme::BG_ELEVATED), 0);
    lv_obj_set_style_border_color(btn,
        lv_color_hex(primary ? Theme::PRIMARY : Theme::BORDER), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(lbl,
        lv_color_hex(primary ? Theme::ACCENT : Theme::TEXT_PRIMARY), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, w - 10);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

}  // namespace

void LvTimezoneScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    _roller = nullptr;
    _upButton = nullptr;
    _downButton = nullptr;
    _doneButton = nullptr;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    _selectedIdx = clampTimezoneIndex(_selectedIdx);

    lv_obj_t* eyebrow = makeLabel(parent, "REGION SETUP", &lv_font_ratdeck_10,
                                  Theme::TEXT_SECONDARY, 220, LV_TEXT_ALIGN_CENTER,
                                  LV_LABEL_LONG_DOT);
    lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t* title = makeLabel(parent, "TIMEZONE", &lv_font_montserrat_16,
                                Theme::ACCENT, 260, LV_TEXT_ALIGN_CENTER,
                                LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* sub = makeLabel(parent, "Pick the nearest city for time and radio-region hints.",
                              &lv_font_ratdeck_12, Theme::TEXT_PRIMARY,
                              280, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 48);

    // Build roller options string: "City  UTC+X\nCity  UTC-Y\n..."
    static char opts[640];
    int pos = 0;
    for (int i = 0; i < TIMEZONE_COUNT; i++) {
        if (i > 0 && pos < (int)sizeof(opts) - 1) opts[pos++] = '\n';
        pos += snprintf(opts + pos, sizeof(opts) - pos, "%-13s UTC%+d",
            TIMEZONE_TABLE[i].label, TIMEZONE_TABLE[i].baseOffset);
    }

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 288, 104);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 76);
    stylePanel(card);

    _roller = lv_roller_create(card);
    lv_roller_set_options(_roller, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(_roller, 3);
    lv_obj_set_size(_roller, 278, 94);
    lv_obj_set_pos(_roller, 4, 4);
    lv_roller_set_selected(_roller, _selectedIdx, LV_ANIM_OFF);

    lv_obj_add_style(_roller, LvTheme::styleRoller(), LV_PART_MAIN);
    lv_obj_set_style_text_font(_roller, &lv_font_ratdeck_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(_roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_roller, lv_color_hex(Theme::BG_HOVER), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(_roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(_roller, lv_color_hex(Theme::ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_text_font(_roller, &lv_font_ratdeck_14, LV_PART_SELECTED);
    lv_obj_set_style_text_align(_roller, LV_TEXT_ALIGN_CENTER, LV_PART_SELECTED);

    // Add to focus group for trackball navigation
    lv_group_add_obj(LvInput::group(), _roller);
    lv_group_focus_obj(_roller);

    _upButton = makeActionButton(parent, "UP", 22, 194, 74, 34, false);
    lv_obj_add_event_cb(_upButton, [](lv_event_t* e) {
        auto* self = static_cast<LvTimezoneScreen*>(lv_event_get_user_data(e));
        if (!self) return;
        self->stepSelection(-1);
    }, LV_EVENT_CLICKED, this);

    _doneButton = makeActionButton(parent, "DONE", 106, 194, 108, 34, true);
    lv_obj_add_event_cb(_doneButton, [](lv_event_t* e) {
        auto* self = static_cast<LvTimezoneScreen*>(lv_event_get_user_data(e));
        if (!self) return;
        self->submit(false);
    }, LV_EVENT_CLICKED, this);

    _downButton = makeActionButton(parent, "DOWN", 224, 194, 74, 34, false);
    lv_obj_add_event_cb(_downButton, [](lv_event_t* e) {
        auto* self = static_cast<LvTimezoneScreen*>(lv_event_get_user_data(e));
        if (!self) return;
        self->stepSelection(1);
    }, LV_EVENT_CLICKED, this);
}

void LvTimezoneScreen::stepSelection(int delta) {
    if (!_roller) return;

    int sel = lv_roller_get_selected(_roller);
    sel += delta;
    if (sel < 0) sel = 0;
    if (sel >= TIMEZONE_COUNT) sel = TIMEZONE_COUNT - 1;
    lv_roller_set_selected(_roller, sel, LV_ANIM_ON);
}

void LvTimezoneScreen::submit(bool enforceEnterGuard) {
    if (!_roller) return;
    if (enforceEnterGuard && millis() - _enterTime < ENTER_GUARD_MS) return;

    _selectedIdx = lv_roller_get_selected(_roller);
    if (_doneCb) _doneCb(_selectedIdx);
}

bool LvTimezoneScreen::handleKey(const KeyEvent& event) {
    if (!_roller) return true;
    if (event.enter || event.character == '\n' || event.character == '\r') {
        submit(true);
        return true;
    }
    // Trackball maps to PREV/NEXT (focus group nav), but roller needs
    // explicit selection changes since it's the only focusable widget
    if (event.up) {
        stepSelection(-1);
        return true;
    }
    if (event.down) {
        stepSelection(1);
        return true;
    }
    return true;  // Consume all keys on this screen
}
