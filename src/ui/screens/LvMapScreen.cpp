#include "LvMapScreen.h"
#include "ui/Theme.h"
#include "fonts/fonts.h"

void LvMapScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(lbl, "Map\n\nComing soon\n\nNode topology view\nwill appear here");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
}
