#include "LvTimezoneScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "fonts/fonts.h"
#include <Arduino.h>

void LvTimezoneScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);

    // Title
    lv_obj_t* title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(title, "SELECT TIMEZONE");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Subtitle
    lv_obj_t* sub = lv_label_create(parent);
    lv_obj_set_style_text_font(sub, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(sub, "Choose your nearest city:");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 30);

    // Build roller options string: "City  UTC+X\nCity  UTC-Y\n..."
    static char opts[640];
    int pos = 0;
    for (int i = 0; i < TIMEZONE_COUNT; i++) {
        if (i > 0 && pos < (int)sizeof(opts) - 1) opts[pos++] = '\n';
        pos += snprintf(opts + pos, sizeof(opts) - pos, "%-14s UTC%+d",
            TIMEZONE_TABLE[i].label, TIMEZONE_TABLE[i].baseOffset);
    }

    // Card container — green-bordered panel holding the roller
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, Theme::CONTENT_W - 20);
    lv_obj_set_style_bg_color(card, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(Theme::PRIMARY), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_pad_left(card, 6, 0);
    lv_obj_set_style_pad_right(card, 6, 0);
    lv_obj_set_style_pad_top(card, 0, 0);
    lv_obj_set_style_pad_bottom(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(card, true, 0);
    lv_obj_align_to(card, sub, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    // Roller inside the card — borderless, fills the card
    _roller = lv_roller_create(card);
    lv_roller_set_options(_roller, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(_roller, 7);
    lv_obj_set_width(_roller, lv_pct(100));
    lv_obj_center(_roller);
    lv_roller_set_selected(_roller, _selectedIdx, LV_ANIM_OFF);

    // Style the roller — no border, highlight goes edge-to-edge within card
    lv_obj_add_style(_roller, LvTheme::styleRoller(), LV_PART_MAIN);
    lv_obj_set_style_text_font(_roller, &lv_font_ratdeck_14, LV_PART_MAIN);
    // Selected row highlight
    lv_obj_set_style_bg_color(_roller, lv_color_hex(Theme::BG_HOVER), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(_roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(_roller, lv_color_hex(Theme::PRIMARY), LV_PART_SELECTED);
    lv_obj_set_style_text_font(_roller, &lv_font_ratdeck_14, LV_PART_SELECTED);

    // Size the card to fit the roller
    lv_obj_update_layout(parent);
    lv_obj_set_height(card, lv_obj_get_height(_roller));

    // Add to focus group for trackball navigation
    lv_group_add_obj(LvInput::group(), _roller);

    // Hint — positioned below roller
    lv_obj_t* hint = lv_label_create(parent);
    lv_obj_set_style_text_font(hint, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(hint, "[Enter] Select");
    lv_obj_align_to(hint, card, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
}

bool LvTimezoneScreen::handleKey(const KeyEvent& event) {
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (millis() - _enterTime < ENTER_GUARD_MS) return true;
        _selectedIdx = lv_roller_get_selected(_roller);
        if (_doneCb) _doneCb(_selectedIdx);
        return true;
    }
    // Trackball maps to PREV/NEXT (focus group nav), but roller needs
    // explicit selection changes since it's the only focusable widget
    if (event.up) {
        uint16_t sel = lv_roller_get_selected(_roller);
        if (sel > 0) lv_roller_set_selected(_roller, sel - 1, LV_ANIM_ON);
        return true;
    }
    if (event.down) {
        uint16_t sel = lv_roller_get_selected(_roller);
        if (sel < TIMEZONE_COUNT - 1) lv_roller_set_selected(_roller, sel + 1, LV_ANIM_ON);
        return true;
    }
    return true;  // Consume all keys on this screen
}
