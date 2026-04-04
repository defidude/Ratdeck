#pragma once

#include <lvgl.h>
#include "LvStatusBar.h"
#include "LvTabBar.h"
#include "hal/Keyboard.h"

// LVGL screen base class
class LvScreen {
public:
    virtual ~LvScreen() = default;
    virtual void createUI(lv_obj_t* parent) = 0;
    virtual void destroyUI();
    virtual void refreshUI() {}
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual bool handleKey(const KeyEvent& event) { return false; }
    virtual bool handleLongPress() { return false; }
    virtual const char* title() const = 0;

    lv_obj_t* screen() const { return _screen; }

protected:
    lv_obj_t* _screen = nullptr;
};

class UIManager {
public:
    void begin();

    // Screen management
    void setScreen(LvScreen* screen);
    LvScreen* getScreen() { return _currentLvScreen; }

    // Component access
    LvStatusBar& lvStatusBar() { return _lvStatusBar; }
    LvTabBar& lvTabBar() { return _lvTabBar; }

    // Update data (called periodically)
    void update();

    // Force full redraw
    void forceRedraw();

    // Handle key event — routes to current screen
    bool handleKey(const KeyEvent& event);
    bool handleLongPress();

    // Boot mode — hides status bar and tab bar
    void setBootMode(bool boot);
    bool isBootMode() const { return _bootMode; }

    // LVGL content area parent (between status bar and tab bar)
    lv_obj_t* contentParent() { return _lvContent; }

private:
    // LVGL components
    LvStatusBar _lvStatusBar;
    LvTabBar _lvTabBar;
    LvScreen* _currentLvScreen = nullptr;
    lv_obj_t* _lvContent = nullptr;

    bool _bootMode = false;
};
