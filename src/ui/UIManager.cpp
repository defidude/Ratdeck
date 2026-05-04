#include "UIManager.h"
#include "Theme.h"
#include "LvTheme.h"

// --- LvScreen base ---

void LvScreen::destroyUI() {
    // Content is owned by UIManager's _lvContent — just clear our pointers.
    // The UIManager calls lv_obj_clean(_lvContent) before creating the next screen.
    _screen = nullptr;
}

// --- UIManager ---

void UIManager::begin() {
    // Initialize LVGL theme and create persistent UI structure
    lv_obj_t* scr = lv_scr_act();
    LvTheme::init(lv_disp_get_default());

    // Apply screen background style
    lv_obj_add_style(scr, LvTheme::styleScreen(), 0);

    // Create LVGL status bar (top)
    _lvStatusBar.create(scr);

    // Create LVGL tab bar (bottom)
    _lvTabBar.create(scr);

    // Create content area between status bar and tab bar
    _lvContent = lv_obj_create(scr);
    lv_obj_set_pos(_lvContent, 0, Theme::STATUS_BAR_H);
    lv_obj_set_size(_lvContent, Theme::CONTENT_W, Theme::CONTENT_H);
    lv_obj_set_style_bg_color(_lvContent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_lvContent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_lvContent, 0, 0);
    lv_obj_set_style_pad_all(_lvContent, 0, 0);
    lv_obj_set_style_radius(_lvContent, 0, 0);

    Serial.println("[UI] LVGL UI structure created");
}

void UIManager::setScreen(LvScreen* screen) {
    if (_currentLvScreen == screen) return;

    // Transition from previous LVGL screen
    if (_currentLvScreen) {
        _currentLvScreen->onExit();
        _currentLvScreen->destroyUI();
    }

    _currentLvScreen = screen;

    // Show LVGL layers
    if (!_bootMode) {
        lv_obj_clear_flag(_lvStatusBar.obj(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lvTabBar.obj(), LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(_lvContent, LV_OBJ_FLAG_HIDDEN);

    if (_currentLvScreen) {
        // Clean content area
        lv_obj_clean(_lvContent);
        _currentLvScreen->createUI(_lvContent);
        _currentLvScreen->onEnter();
    }
}

void UIManager::setBootMode(bool boot) {
    _bootMode = boot;
    if (boot) {
        lv_obj_add_flag(_lvStatusBar.obj(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_lvTabBar.obj(), LV_OBJ_FLAG_HIDDEN);
        // In boot mode, content area is full screen
        lv_obj_set_pos(_lvContent, 0, 0);
        lv_obj_set_size(_lvContent, Theme::SCREEN_W, Theme::SCREEN_H);
    } else {
        lv_obj_clear_flag(_lvStatusBar.obj(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_lvTabBar.obj(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(_lvContent, 0, Theme::STATUS_BAR_H);
        lv_obj_set_size(_lvContent, Theme::CONTENT_W, Theme::CONTENT_H);
    }
}

void UIManager::update() {
    _lvStatusBar.update();
    if (_currentLvScreen) _currentLvScreen->refreshUI();
}

void UIManager::forceRedraw() {
    lv_obj_invalidate(lv_scr_act());
}

bool UIManager::handleKey(const KeyEvent& event) {
    if (_currentLvScreen) {
        return _currentLvScreen->handleKey(event);
    }
    return false;
}

bool UIManager::handleLongPress() {
    if (_currentLvScreen) {
        return _currentLvScreen->handleLongPress();
    }
    return false;
}
