#include "LvTheme.h"
#include "Theme.h"
#include <Arduino.h>
#include "fonts/fonts.h"

namespace LvTheme {

// Existing styles (16)
static lv_style_t s_screen;
static lv_style_t s_label;
static lv_style_t s_labelMuted;
static lv_style_t s_labelAccent;
static lv_style_t s_btn;
static lv_style_t s_btnPressed;
static lv_style_t s_bar;
static lv_style_t s_barIndicator;
static lv_style_t s_switch;
static lv_style_t s_switchChecked;
static lv_style_t s_textarea;
static lv_style_t s_list;
static lv_style_t s_listBtn;
static lv_style_t s_listBtnFocused;
static lv_style_t s_dropdown;
static lv_style_t s_slider;

// New styles (6)
static lv_style_t s_btnFocused;
static lv_style_t s_textareaFocused;
static lv_style_t s_sectionHeader;
static lv_style_t s_modal;
static lv_style_t s_scrollbar;
static lv_style_t s_roller;

void init(lv_disp_t* disp) {
    // Screen background (LV_USE_THEME_DEFAULT is disabled in lv_conf.h,
    // so there's no default blue focus outline to worry about)
    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, lv_color_hex(Theme::BG));
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_text_color(&s_screen, lv_color_hex(Theme::TEXT_PRIMARY));
    lv_style_set_text_font(&s_screen, &lv_font_ratdeck_14);

    // Labels
    lv_style_init(&s_label);
    lv_style_set_text_color(&s_label, lv_color_hex(Theme::TEXT_PRIMARY));

    lv_style_init(&s_labelMuted);
    lv_style_set_text_color(&s_labelMuted, lv_color_hex(Theme::TEXT_SECONDARY));

    lv_style_init(&s_labelAccent);
    lv_style_set_text_color(&s_labelAccent, lv_color_hex(Theme::ACCENT));

    // Buttons
    lv_style_init(&s_btn);
    lv_style_set_bg_color(&s_btn, lv_color_hex(Theme::BG_ELEVATED));
    lv_style_set_bg_opa(&s_btn, LV_OPA_COVER);
    lv_style_set_border_color(&s_btn, lv_color_hex(Theme::BORDER));
    lv_style_set_border_width(&s_btn, 1);
    lv_style_set_text_color(&s_btn, lv_color_hex(Theme::TEXT_PRIMARY));
    lv_style_set_radius(&s_btn, 4);
    lv_style_set_pad_all(&s_btn, 8);

    lv_style_init(&s_btnPressed);
    lv_style_set_bg_color(&s_btnPressed, lv_color_hex(Theme::PRIMARY_SUBTLE));
    lv_style_set_border_color(&s_btnPressed, lv_color_hex(Theme::PRIMARY));
    lv_style_set_border_width(&s_btnPressed, 1);
    lv_style_set_text_color(&s_btnPressed, lv_color_hex(Theme::ACCENT));

    lv_style_init(&s_btnFocused);
    lv_style_set_border_color(&s_btnFocused, lv_color_hex(Theme::PRIMARY));
    lv_style_set_border_width(&s_btnFocused, 2);

    // Bar
    lv_style_init(&s_bar);
    lv_style_set_bg_color(&s_bar, lv_color_hex(Theme::BORDER));
    lv_style_set_bg_opa(&s_bar, LV_OPA_COVER);
    lv_style_set_radius(&s_bar, 3);

    lv_style_init(&s_barIndicator);
    lv_style_set_bg_color(&s_barIndicator, lv_color_hex(Theme::PRIMARY));
    lv_style_set_bg_opa(&s_barIndicator, LV_OPA_COVER);
    lv_style_set_radius(&s_barIndicator, 3);

    // Switch (LV_USE_SWITCH currently 0; kept for future lv_menu rewrite)
    lv_style_init(&s_switch);
    lv_style_set_bg_color(&s_switch, lv_color_hex(Theme::BORDER));
    lv_style_set_bg_opa(&s_switch, LV_OPA_COVER);
    lv_style_set_radius(&s_switch, LV_RADIUS_CIRCLE);

    lv_style_init(&s_switchChecked);
    lv_style_set_bg_color(&s_switchChecked, lv_color_hex(Theme::PRIMARY));

    // Textarea
    lv_style_init(&s_textarea);
    lv_style_set_bg_color(&s_textarea, lv_color_hex(Theme::BG_ELEVATED));
    lv_style_set_bg_opa(&s_textarea, LV_OPA_COVER);
    lv_style_set_border_color(&s_textarea, lv_color_hex(Theme::BORDER));
    lv_style_set_border_width(&s_textarea, 1);
    lv_style_set_text_color(&s_textarea, lv_color_hex(Theme::TEXT_PRIMARY));
    lv_style_set_radius(&s_textarea, 4);
    lv_style_set_pad_all(&s_textarea, 6);

    lv_style_init(&s_textareaFocused);
    lv_style_set_border_color(&s_textareaFocused, lv_color_hex(Theme::PRIMARY));
    lv_style_set_border_width(&s_textareaFocused, 1);

    // List container
    lv_style_init(&s_list);
    lv_style_set_bg_color(&s_list, lv_color_hex(Theme::BG));
    lv_style_set_bg_opa(&s_list, LV_OPA_COVER);
    lv_style_set_pad_all(&s_list, 0);
    lv_style_set_pad_row(&s_list, 0);
    lv_style_set_border_width(&s_list, 0);

    // List items
    lv_style_init(&s_listBtn);
    lv_style_set_bg_color(&s_listBtn, lv_color_hex(Theme::BG));
    lv_style_set_bg_opa(&s_listBtn, LV_OPA_COVER);
    lv_style_set_text_color(&s_listBtn, lv_color_hex(Theme::TEXT_PRIMARY));
    lv_style_set_border_color(&s_listBtn, lv_color_hex(Theme::DIVIDER));
    lv_style_set_border_width(&s_listBtn, 1);
    lv_style_set_border_side(&s_listBtn, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_all(&s_listBtn, 6);
    lv_style_set_pad_left(&s_listBtn, 8);
    lv_style_set_radius(&s_listBtn, 0);

    // List item focused — left-bar indicator + hover background
    lv_style_init(&s_listBtnFocused);
    lv_style_set_bg_color(&s_listBtnFocused, lv_color_hex(Theme::BG_HOVER));
    lv_style_set_bg_opa(&s_listBtnFocused, LV_OPA_COVER);
    lv_style_set_text_color(&s_listBtnFocused, lv_color_hex(Theme::TEXT_PRIMARY));
    lv_style_set_border_color(&s_listBtnFocused, lv_color_hex(Theme::PRIMARY));
    lv_style_set_border_width(&s_listBtnFocused, 2);
    lv_style_set_border_side(&s_listBtnFocused, LV_BORDER_SIDE_LEFT);

    // Dropdown
    lv_style_init(&s_dropdown);
    lv_style_set_bg_color(&s_dropdown, lv_color_hex(Theme::BG_ELEVATED));
    lv_style_set_bg_opa(&s_dropdown, LV_OPA_COVER);
    lv_style_set_border_color(&s_dropdown, lv_color_hex(Theme::BORDER));
    lv_style_set_border_width(&s_dropdown, 1);
    lv_style_set_text_color(&s_dropdown, lv_color_hex(Theme::TEXT_PRIMARY));
    lv_style_set_radius(&s_dropdown, 4);
    lv_style_set_pad_all(&s_dropdown, 4);

    // Slider
    lv_style_init(&s_slider);
    lv_style_set_bg_color(&s_slider, lv_color_hex(Theme::BORDER));
    lv_style_set_bg_opa(&s_slider, LV_OPA_COVER);
    lv_style_set_radius(&s_slider, 3);

    // Section header (for list section dividers like "Contacts (3)")
    lv_style_init(&s_sectionHeader);
    lv_style_set_bg_opa(&s_sectionHeader, LV_OPA_TRANSP);
    lv_style_set_text_color(&s_sectionHeader, lv_color_hex(Theme::TEXT_SECONDARY));
    lv_style_set_text_font(&s_sectionHeader, &lv_font_ratdeck_10);
    lv_style_set_text_letter_space(&s_sectionHeader, 1);
    lv_style_set_border_color(&s_sectionHeader, lv_color_hex(Theme::DIVIDER));
    lv_style_set_border_width(&s_sectionHeader, 1);
    lv_style_set_border_side(&s_sectionHeader, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_top(&s_sectionHeader, 8);
    lv_style_set_pad_bottom(&s_sectionHeader, 4);
    lv_style_set_pad_left(&s_sectionHeader, 8);

    // Modal overlay
    lv_style_init(&s_modal);
    lv_style_set_bg_color(&s_modal, lv_color_hex(Theme::BG_SURFACE));
    lv_style_set_bg_opa(&s_modal, LV_OPA_COVER);
    lv_style_set_border_color(&s_modal, lv_color_hex(Theme::BORDER));
    lv_style_set_border_width(&s_modal, 1);
    lv_style_set_radius(&s_modal, 6);
    lv_style_set_pad_all(&s_modal, 12);
    lv_style_set_pad_row(&s_modal, 4);
    lv_style_set_shadow_width(&s_modal, 8);
    lv_style_set_shadow_color(&s_modal, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&s_modal, LV_OPA_50);

    // Scrollbar
    lv_style_init(&s_scrollbar);
    lv_style_set_bg_color(&s_scrollbar, lv_color_hex(Theme::TEXT_MUTED));
    lv_style_set_bg_opa(&s_scrollbar, LV_OPA_40);
    lv_style_set_radius(&s_scrollbar, LV_RADIUS_CIRCLE);
    lv_style_set_width(&s_scrollbar, 3);
    lv_style_set_pad_right(&s_scrollbar, 2);

    // Roller — no border so selected row highlight extends edge-to-edge
    lv_style_init(&s_roller);
    lv_style_set_bg_color(&s_roller, lv_color_hex(Theme::BG));
    lv_style_set_bg_opa(&s_roller, LV_OPA_COVER);
    lv_style_set_text_color(&s_roller, lv_color_hex(Theme::TEXT_SECONDARY));
    lv_style_set_text_font(&s_roller, &lv_font_ratdeck_14);
    lv_style_set_border_width(&s_roller, 0);

    // Apply screen style to default theme
    lv_obj_add_style(lv_scr_act(), &s_screen, 0);

    Serial.println("[LVGL] Ratspeak theme initialized");
}

// Existing accessors (16)
lv_style_t* styleScreen()           { return &s_screen; }
lv_style_t* styleLabel()            { return &s_label; }
lv_style_t* styleLabelMuted()       { return &s_labelMuted; }
lv_style_t* styleLabelAccent()      { return &s_labelAccent; }
lv_style_t* styleBtn()              { return &s_btn; }
lv_style_t* styleBtnPressed()       { return &s_btnPressed; }
lv_style_t* styleBar()              { return &s_bar; }
lv_style_t* styleBarIndicator()     { return &s_barIndicator; }
lv_style_t* styleSwitch()           { return &s_switch; }
lv_style_t* styleSwitchChecked()    { return &s_switchChecked; }
lv_style_t* styleTextarea()         { return &s_textarea; }
lv_style_t* styleList()             { return &s_list; }
lv_style_t* styleListBtn()          { return &s_listBtn; }
lv_style_t* styleListBtnFocused()   { return &s_listBtnFocused; }
lv_style_t* styleDropdown()         { return &s_dropdown; }
lv_style_t* styleSlider()           { return &s_slider; }

// New accessors (6)
lv_style_t* styleBtnFocused()       { return &s_btnFocused; }
lv_style_t* styleTextareaFocused()  { return &s_textareaFocused; }
lv_style_t* styleSectionHeader()    { return &s_sectionHeader; }
lv_style_t* styleModal()            { return &s_modal; }
lv_style_t* styleScrollbar()        { return &s_scrollbar; }
lv_style_t* styleRoller()           { return &s_roller; }

}  // namespace LvTheme
