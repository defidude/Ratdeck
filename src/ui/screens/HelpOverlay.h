#pragma once

#include "ui/UIManager.h"

class HelpOverlay : public Screen {
public:
    bool handleKey(const KeyEvent& event) override;

    void toggle() { _visible = !_visible; }
    bool isVisible() const { return _visible; }

    const char* title() const override { return "Help"; }
    void draw(LGFX_TDeck& gfx) override;

private:
    bool _visible = false;
};
