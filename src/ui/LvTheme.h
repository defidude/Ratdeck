#pragma once

#include <lvgl.h>

// Ratdeck LVGL theme: compact field-console styles for LVGL 8.3.
namespace LvTheme {

void init(lv_disp_t* disp);

// Style accessors - existing (16)
lv_style_t* styleScreen();
lv_style_t* styleLabel();
lv_style_t* styleLabelMuted();
lv_style_t* styleLabelAccent();
lv_style_t* styleBtn();
lv_style_t* styleBtnPressed();
lv_style_t* styleBar();
lv_style_t* styleBarIndicator();
lv_style_t* styleSwitch();
lv_style_t* styleSwitchChecked();
lv_style_t* styleTextarea();
lv_style_t* styleList();
lv_style_t* styleListBtn();
lv_style_t* styleListBtnFocused();
lv_style_t* styleDropdown();
lv_style_t* styleSlider();

// Style accessors - new (6)
lv_style_t* styleBtnFocused();
lv_style_t* styleTextareaFocused();
lv_style_t* styleSectionHeader();
lv_style_t* styleModal();
lv_style_t* styleScrollbar();
lv_style_t* styleRoller();

// Shared shell styles
lv_style_t* styleStatusBar();
lv_style_t* styleStatusToast();
lv_style_t* styleTabBar();
lv_style_t* styleTabCell();
lv_style_t* styleTabCellActive();
lv_style_t* styleBadge();

}  // namespace LvTheme
