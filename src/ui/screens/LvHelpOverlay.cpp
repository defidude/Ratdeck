#include "LvHelpOverlay.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include <lvgl.h>
#include "fonts/fonts.h"

void LvHelpOverlay::create() {
    _overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_overlay, Theme::CONTENT_W - 40, Theme::CONTENT_H - 20);
    lv_obj_center(_overlay);
    lv_obj_add_style(_overlay, LvTheme::styleModal(), 0);
    lv_obj_set_style_pad_row(_overlay, 3, 0);
    lv_obj_set_layout(_overlay, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_overlay, LV_FLEX_FLOW_COLUMN);

    const lv_font_t* font = &lv_font_ratdeck_12;

    auto mkLabel = [&](const char* text, uint32_t color) {
        lv_obj_t* lbl = lv_label_create(_overlay);
        lv_obj_set_style_text_font(lbl, font, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
        lv_label_set_text(lbl, text);
    };

    mkLabel("HOTKEYS", Theme::ACCENT);
    mkLabel("Ctrl+H  Help", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+M  Messages", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+N  New Message", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+S  Settings", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+A  Announce", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+D  Diagnostics", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+T  Radio Test", Theme::TEXT_PRIMARY);
    mkLabel("Ctrl+R  RSSI Monitor", Theme::TEXT_PRIMARY);
    mkLabel(",  /    Prev/Next Tab", Theme::TEXT_PRIMARY);
    mkLabel("Esc     Back", Theme::TEXT_PRIMARY);
    mkLabel("", Theme::TEXT_MUTED);
    mkLabel("Any key to close", Theme::TEXT_MUTED);

    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void LvHelpOverlay::show() {
    if (!_overlay) create();
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    _visible = true;
}

void LvHelpOverlay::hide() {
    if (_overlay) lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    _visible = false;
}

void LvHelpOverlay::toggle() {
    if (_visible) hide(); else show();
}

bool LvHelpOverlay::handleKey(const KeyEvent& event) {
    if (_visible) {
        hide();
        return true;
    }
    return false;
}
