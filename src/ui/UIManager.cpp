#include "UIManager.h"
#include "Theme.h"
#include "hal/Display.h"

void UIManager::begin(LGFX_TDeck* gfx) {
    _gfx = gfx;
    _statusBar.setGfx(gfx);
    _tabBar.setGfx(gfx);
}

void UIManager::setScreen(Screen* screen) {
    if (_currentScreen) {
        _currentScreen->onExit();
    }

    _currentScreen = screen;

    if (_currentScreen) {
        _currentScreen->onEnter();
        _currentScreen->markDirty();
    }

    forceRedraw();
}

void UIManager::setOverlay(Screen* overlay) {
    _overlay = overlay;
    if (_overlay) {
        _overlay->markDirty();
    }
    forceRedraw();
}

void UIManager::setBootMode(bool boot) {
    _bootMode = boot;
    forceRedraw();
}

void UIManager::update() {
    _statusBar.update();
    if (_currentScreen) _currentScreen->update();
}

void UIManager::render() {
    if (!_gfx) return;

    bool needStatusRedraw = _statusBar.isDirty();
    bool needTabRedraw = _tabBar.isDirty();
    bool needContentRedraw = (_currentScreen && _currentScreen->isDirty());
    bool needOverlayRedraw = (_overlay && _overlay->isDirty());

    if (!needStatusRedraw && !needTabRedraw && !needContentRedraw && !needOverlayRedraw)
        return;

    if (!_bootMode && needStatusRedraw) {
        _statusBar.draw(*_gfx);
        _statusBar.clearDirty();
    }

    if (needContentRedraw) {
        if (_bootMode) {
            // In boot mode, content area is full screen
            _gfx->setClipRect(0, 0, Theme::SCREEN_W, Theme::SCREEN_H);
        } else {
            _gfx->setClipRect(0, Theme::CONTENT_Y, Theme::CONTENT_W, Theme::CONTENT_H);
        }
        _gfx->fillRect(0, _bootMode ? 0 : Theme::CONTENT_Y,
                        Theme::CONTENT_W, _bootMode ? Theme::SCREEN_H : Theme::CONTENT_H,
                        Theme::BG);
        _currentScreen->draw(*_gfx);
        _currentScreen->clearDirty();
        _gfx->clearClipRect();
    }

    if (needOverlayRedraw && _overlay) {
        _gfx->setClipRect(0, Theme::CONTENT_Y, Theme::CONTENT_W, Theme::CONTENT_H);
        _overlay->draw(*_gfx);
        _overlay->clearDirty();
        _gfx->clearClipRect();
    }

    if (!_bootMode && needTabRedraw) {
        _tabBar.draw(*_gfx);
        _tabBar.clearDirty();
    }
}

void UIManager::forceRedraw() {
    _statusBar.markDirty();
    _tabBar.markDirty();
    if (_currentScreen) _currentScreen->markDirty();
    if (_overlay) _overlay->markDirty();
}

bool UIManager::handleKey(const KeyEvent& event) {
    if (_overlay) {
        return _overlay->handleKey(event);
    }
    if (_currentScreen) {
        return _currentScreen->handleKey(event);
    }
    return false;
}
