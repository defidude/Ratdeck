#include "LvDataCleanScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "config/Config.h"
#include "fonts/fonts.h"

void LvDataCleanScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);

    // Title: "RATSPEAK"
    lv_obj_t* title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(Theme::PRIMARY), 0);
    lv_label_set_text(title, "RATSPEAK");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Subtitle
    lv_obj_t* sub = lv_label_create(parent);
    lv_obj_set_style_text_font(sub, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(sub, "ratspeak.org");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 42);

    // Message
    lv_obj_t* msg = lv_label_create(parent);
    lv_obj_set_style_text_font(msg, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(msg, "Old data found on SD card.");
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 75);

    // Prompt
    lv_obj_t* prompt = lv_label_create(parent);
    lv_obj_set_style_text_font(prompt, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(prompt, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(prompt, "Remove old data and start fresh?");
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 95);

    // Yes label
    _yesLabel = lv_label_create(parent);
    lv_obj_set_style_text_font(_yesLabel, &lv_font_ratdeck_14, 0);
    lv_label_set_text(_yesLabel, "[ Yes ]");
    lv_obj_align(_yesLabel, LV_ALIGN_TOP_MID, -50, 135);

    // No label
    _noLabel = lv_label_create(parent);
    lv_obj_set_style_text_font(_noLabel, &lv_font_ratdeck_14, 0);
    lv_label_set_text(_noLabel, "[ No ]");
    lv_obj_align(_noLabel, LV_ALIGN_TOP_MID, 50, 135);

    updateSelection();

    // Hint
    _hintLabel = lv_label_create(parent);
    lv_obj_set_style_text_font(_hintLabel, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(_hintLabel, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(_hintLabel, "[</>] Select  [Enter] OK");
    lv_obj_align(_hintLabel, LV_ALIGN_TOP_MID, 0, 165);

    // Status label (hidden until showStatus is called)
    _statusLabel = lv_label_create(parent);
    lv_obj_set_style_text_font(_statusLabel, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(Theme::PRIMARY), 0);
    lv_label_set_text(_statusLabel, "");
    lv_obj_align(_statusLabel, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_add_flag(_statusLabel, LV_OBJ_FLAG_HIDDEN);

    // Version
    lv_obj_t* ver = lv_label_create(parent);
    lv_obj_set_style_text_font(ver, &lv_font_ratdeck_10, 0);
    lv_obj_set_style_text_color(ver, lv_color_hex(Theme::TEXT_MUTED), 0);
    char verBuf[32];
    snprintf(verBuf, sizeof(verBuf), "v%s", RATDECK_VERSION_STRING);
    lv_label_set_text(ver, verBuf);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void LvDataCleanScreen::updateSelection() {
    if (_selectedYes) {
        lv_obj_set_style_text_color(_yesLabel, lv_color_hex(Theme::ACCENT), 0);
        lv_obj_set_style_text_color(_noLabel, lv_color_hex(Theme::TEXT_MUTED), 0);
    } else {
        lv_obj_set_style_text_color(_yesLabel, lv_color_hex(Theme::TEXT_MUTED), 0);
        lv_obj_set_style_text_color(_noLabel, lv_color_hex(Theme::ACCENT), 0);
    }
}

void LvDataCleanScreen::showStatus(const char* msg) {
    if (_yesLabel) lv_obj_add_flag(_yesLabel, LV_OBJ_FLAG_HIDDEN);
    if (_noLabel) lv_obj_add_flag(_noLabel, LV_OBJ_FLAG_HIDDEN);
    if (_hintLabel) lv_obj_add_flag(_hintLabel, LV_OBJ_FLAG_HIDDEN);
    if (_statusLabel) {
        lv_label_set_text(_statusLabel, msg);
        lv_obj_clear_flag(_statusLabel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_handler();
}

bool LvDataCleanScreen::handleKey(const KeyEvent& event) {
    if (event.left) {
        _selectedYes = true;
        updateSelection();
        return true;
    }
    if (event.right) {
        _selectedYes = false;
        updateSelection();
        return true;
    }
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (_doneCb) _doneCb(_selectedYes);
        return true;
    }
    return true;  // Consume all keys
}
