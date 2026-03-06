#pragma once

#include "StatusBar.h"
#include "TabBar.h"
#include "hal/Keyboard.h"

class LGFX_TDeck;

class Screen {
public:
    virtual ~Screen() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update() {}
    virtual bool handleKey(const KeyEvent& event) { return false; }
    virtual const char* title() const = 0;
    virtual void draw(LGFX_TDeck& gfx) = 0;

    bool isDirty() const { return _dirty; }
    void markDirty() { _dirty = true; }
    void clearDirty() { _dirty = false; }

protected:
    bool _dirty = true;
};

class UIManager {
public:
    void begin(LGFX_TDeck* gfx);

    // Screen management
    void setScreen(Screen* screen);
    Screen* getScreen() { return _currentScreen; }

    // Component access
    StatusBar& statusBar() { return _statusBar; }
    TabBar& tabBar() { return _tabBar; }

    // Update data (called periodically)
    void update();

    // Render if dirty (called from loop)
    void render();

    // Force full redraw
    void forceRedraw();

    // Handle key event — routes to current screen
    bool handleKey(const KeyEvent& event);

    // Boot mode — hides status bar and tab bar
    void setBootMode(bool boot);
    bool isBootMode() const { return _bootMode; }

    // Overlay support
    void setOverlay(Screen* overlay);

private:
    LGFX_TDeck* _gfx = nullptr;
    StatusBar _statusBar;
    TabBar _tabBar;
    Screen* _currentScreen = nullptr;
    Screen* _overlay = nullptr;
    bool _bootMode = false;
};
