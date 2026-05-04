#include "LvDataCleanScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
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

lv_obj_t* makeChoice(lv_obj_t* parent, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, 126, 34);
    lv_obj_set_style_bg_color(box, lv_color_hex(Theme::BG_ELEVATED), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(Theme::BORDER), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    return box;
}

void styleChoice(lv_obj_t* box, bool selected, bool destructive, bool armed) {
    if (!box) return;
    uint32_t border = selected ? (destructive && armed ? Theme::ERROR_CLR : Theme::PRIMARY) : Theme::BORDER;
    uint32_t bg = selected ? Theme::PRIMARY_SUBTLE : Theme::BG_ELEVATED;
    if (destructive && armed) bg = 0x2A0E0E;
    lv_obj_set_style_bg_color(box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(box, selected ? 2 : 1, 0);
}

}  // namespace

void LvDataCleanScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    _yesBox = nullptr;
    _noBox = nullptr;
    _yesLabel = nullptr;
    _noLabel = nullptr;
    _hintLabel = nullptr;
    _confirmLabel = nullptr;
    _statusLabel = nullptr;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    _selectedYes = false;
    _confirmWipe = false;

    lv_obj_t* eyebrow = makeLabel(parent, "STORAGE CHECK", &lv_font_ratdeck_10,
                                  Theme::TEXT_SECONDARY, 220, LV_TEXT_ALIGN_CENTER,
                                  LV_LABEL_LONG_DOT);
    lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* title = makeLabel(parent, "SD DATA FOUND", &lv_font_montserrat_16,
                                Theme::ACCENT, 260, LV_TEXT_ALIGN_CENTER,
                                LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t* msg = makeLabel(parent,
        "Ratdeck found older data on the SD card.",
        &lv_font_ratdeck_12, Theme::TEXT_PRIMARY, 268, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_t* prompt = makeLabel(parent,
        "Keeping data is safest. Erase only for a clean setup.",
        &lv_font_ratdeck_12, Theme::TEXT_SECONDARY, 276, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 82);

    _noBox = makeChoice(parent, 28, 122);
    _noLabel = makeLabel(_noBox, "KEEP DATA", &lv_font_ratdeck_12,
                         Theme::TEXT_PRIMARY, 112, LV_TEXT_ALIGN_CENTER,
                         LV_LABEL_LONG_DOT);
    lv_obj_center(_noLabel);
    lv_obj_add_event_cb(_noBox, [](lv_event_t* e) {
        auto* self = static_cast<LvDataCleanScreen*>(lv_event_get_user_data(e));
        if (!self) return;
        self->_selectedYes = false;
        self->_confirmWipe = false;
        self->updateSelection();
        if (self->_doneCb) self->_doneCb(false);
    }, LV_EVENT_CLICKED, this);

    _yesBox = makeChoice(parent, 166, 122);
    _yesLabel = makeLabel(_yesBox, "ERASE", &lv_font_ratdeck_12,
                          Theme::TEXT_PRIMARY, 112, LV_TEXT_ALIGN_CENTER,
                          LV_LABEL_LONG_DOT);
    lv_obj_center(_yesLabel);
    lv_obj_add_event_cb(_yesBox, [](lv_event_t* e) {
        auto* self = static_cast<LvDataCleanScreen*>(lv_event_get_user_data(e));
        if (!self) return;
        self->_selectedYes = true;
        if (!self->_confirmWipe) {
            self->_confirmWipe = true;
            self->updateSelection();
            return;
        }
        if (self->_doneCb) self->_doneCb(true);
    }, LV_EVENT_CLICKED, this);

    _confirmLabel = makeLabel(parent, "", &lv_font_ratdeck_12,
                              Theme::WARNING_CLR, 280, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(_confirmLabel, LV_ALIGN_TOP_MID, 0, 164);

    _hintLabel = makeLabel(parent, "Left/Right choose  Enter continues",
                           &lv_font_ratdeck_12, Theme::ACCENT,
                           286, LV_TEXT_ALIGN_CENTER, LV_LABEL_LONG_DOT);
    lv_obj_align(_hintLabel, LV_ALIGN_TOP_MID, 0, 188);

    _statusLabel = makeLabel(parent, "", &lv_font_ratdeck_12,
                             Theme::PRIMARY, 286, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(_statusLabel, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_add_flag(_statusLabel, LV_OBJ_FLAG_HIDDEN);
    updateSelection();

    lv_obj_t* ver = makeLabel(parent, "", &lv_font_ratdeck_10,
                              Theme::TEXT_MUTED, 120, LV_TEXT_ALIGN_CENTER,
                              LV_LABEL_LONG_DOT);
    char verBuf[32];
    snprintf(verBuf, sizeof(verBuf), "v%s", RATDECK_VERSION_STRING);
    lv_label_set_text(ver, verBuf);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void LvDataCleanScreen::updateSelection() {
    styleChoice(_noBox, !_selectedYes, false, false);
    styleChoice(_yesBox, _selectedYes, true, _confirmWipe);

    if (_selectedYes) {
        lv_obj_set_style_text_color(_yesLabel,
            lv_color_hex(_confirmWipe ? Theme::ERROR_CLR : Theme::WARNING_CLR), 0);
        lv_obj_set_style_text_color(_noLabel, lv_color_hex(Theme::TEXT_MUTED), 0);
        if (_confirmLabel) {
            lv_label_set_text(_confirmLabel,
                _confirmWipe ? "Erase armed. Press Enter again to wipe." :
                               "Erase will remove old SD data.");
        }
        if (_hintLabel) {
            lv_label_set_text(_hintLabel,
                _confirmWipe ? "Enter confirms erase  Left cancels" :
                               "Enter arms erase  Left cancels");
        }
    } else {
        lv_obj_set_style_text_color(_yesLabel, lv_color_hex(Theme::TEXT_MUTED), 0);
        lv_obj_set_style_text_color(_noLabel, lv_color_hex(Theme::ACCENT), 0);
        if (_confirmLabel) lv_label_set_text(_confirmLabel, "Recommended: keep existing data.");
        if (_hintLabel) lv_label_set_text(_hintLabel, "Enter keeps data  Right selects erase");
    }
}

void LvDataCleanScreen::showStatus(const char* msg) {
    if (_yesBox) lv_obj_add_flag(_yesBox, LV_OBJ_FLAG_HIDDEN);
    if (_noBox) lv_obj_add_flag(_noBox, LV_OBJ_FLAG_HIDDEN);
    if (_yesLabel) lv_obj_add_flag(_yesLabel, LV_OBJ_FLAG_HIDDEN);
    if (_noLabel) lv_obj_add_flag(_noLabel, LV_OBJ_FLAG_HIDDEN);
    if (_hintLabel) lv_obj_add_flag(_hintLabel, LV_OBJ_FLAG_HIDDEN);
    if (_confirmLabel) lv_obj_add_flag(_confirmLabel, LV_OBJ_FLAG_HIDDEN);
    if (_statusLabel) {
        lv_label_set_text(_statusLabel, msg ? msg : "");
        lv_obj_clear_flag(_statusLabel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_handler();
}

bool LvDataCleanScreen::handleKey(const KeyEvent& event) {
    if (event.left || event.up) {
        _selectedYes = false;
        _confirmWipe = false;
        updateSelection();
        return true;
    }
    if (event.right || event.down) {
        _selectedYes = true;
        _confirmWipe = false;
        updateSelection();
        return true;
    }
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (_selectedYes && !_confirmWipe) {
            _confirmWipe = true;
            updateSelection();
            return true;
        }
        if (_doneCb) _doneCb(_selectedYes);
        return true;
    }
    return true;  // Consume all keys
}
