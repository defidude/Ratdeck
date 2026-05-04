#include "LvNameInputScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "config/Config.h"
#include "fonts/fonts.h"

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

lv_obj_t* makeLine(lv_obj_t* parent, lv_coord_t y) {
    lv_obj_t* line = lv_obj_create(parent);
    lv_obj_set_pos(line, 24, y);
    lv_obj_set_size(line, Theme::SCREEN_W - 48, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(Theme::DIVIDER), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

lv_obj_t* makeActionButton(lv_obj_t* parent, const char* text, lv_coord_t y) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 168, 34);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_add_style(btn, LvTheme::styleBtn(), 0);
    lv_obj_add_style(btn, LvTheme::styleBtnFocused(), LV_STATE_FOCUSED);
    lv_obj_add_style(btn, LvTheme::styleBtnPressed(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(Theme::PRIMARY_SUBTLE), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(Theme::PRIMARY), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, 150);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

}  // namespace

void LvNameInputScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    _textarea = nullptr;
    _doneButton = nullptr;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lv_obj_t* eyebrow = makeLabel(parent, "FIRST BOOT", &lv_font_ratdeck_10,
                                  Theme::TEXT_SECONDARY, 220, LV_TEXT_ALIGN_CENTER,
                                  LV_LABEL_LONG_DOT);
    lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 13);

    lv_obj_t* title = makeLabel(parent, "DISPLAY NAME", &lv_font_montserrat_16,
                                Theme::ACCENT, 260, LV_TEXT_ALIGN_CENTER,
                                LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    makeLine(parent, 58);

    lv_obj_t* prompt = makeLabel(parent,
        "Choose the name other operators see on announces.",
        &lv_font_ratdeck_12, Theme::TEXT_PRIMARY, 250, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 70);

    _textarea = lv_textarea_create(parent);
    lv_obj_set_size(_textarea, 252, 36);
    lv_obj_align(_textarea, LV_ALIGN_TOP_MID, 0, 108);
    lv_textarea_set_max_length(_textarea, MAX_NAME_LEN);
    lv_textarea_set_one_line(_textarea, true);
    lv_textarea_set_placeholder_text(_textarea, "Optional");
    lv_obj_add_style(_textarea, LvTheme::styleTextarea(), 0);
    lv_obj_add_style(_textarea, LvTheme::styleTextareaFocused(), LV_STATE_FOCUSED);
    lv_obj_set_style_text_font(_textarea, &lv_font_ratdeck_14, 0);
    lv_group_add_obj(LvInput::group(), _textarea);
    lv_group_focus_obj(_textarea);

    lv_obj_t* note = makeLabel(parent,
        "Leave blank to generate a Ratdeck name from this identity.",
        &lv_font_ratdeck_12, Theme::TEXT_SECONDARY, 260, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 154);

    _doneButton = makeActionButton(parent, "DONE", 184);
    lv_obj_add_event_cb(_doneButton, [](lv_event_t* e) {
        auto* self = static_cast<LvNameInputScreen*>(lv_event_get_user_data(e));
        if (!self) return;
        self->submit(false);
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* ver = makeLabel(parent, "", &lv_font_ratdeck_10,
                              Theme::TEXT_MUTED, 120, LV_TEXT_ALIGN_CENTER,
                              LV_LABEL_LONG_DOT);
    char verBuf[32];
    snprintf(verBuf, sizeof(verBuf), "v%s", RATDECK_VERSION_STRING);
    lv_label_set_text(ver, verBuf);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void LvNameInputScreen::submit(bool enforceEnterGuard) {
    if (!_textarea) return;
    if (enforceEnterGuard && millis() - _enterTime < ENTER_GUARD_MS) return;

    const char* text = lv_textarea_get_text(_textarea);
    if (_doneCb) {
        _doneCb(String(text && strlen(text) > 0 ? text : ""));
    }
}

bool LvNameInputScreen::handleKey(const KeyEvent& event) {
    if (!_textarea) return false;

    if (event.enter || event.character == '\n' || event.character == '\r') {
        submit(true);
        return true;
    }
    if (event.del || event.character == 8) {
        lv_textarea_del_char(_textarea);
        return true;
    }
    if (event.character >= 0x20 && event.character <= 0x7E) {
        char buf[2] = {event.character, 0};
        lv_textarea_add_text(_textarea, buf);
        return true;
    }
    return true;  // Consume all keys
}
