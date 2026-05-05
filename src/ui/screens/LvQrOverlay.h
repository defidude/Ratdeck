#pragma once

#include "ui/UIManager.h"

// Fullscreen QR overlay — encodes lxma://<hash>:<pubkey> for Columba/Sideband.
class LvQrOverlay {
public:
    void create();
    void show(const String& destHashHex, const String& publicKeyHex);
    void hide();
    bool isVisible() const { return _visible; }
    bool handleKey(const KeyEvent& event);

private:
    lv_obj_t* _overlay = nullptr;
    lv_obj_t* _qr = nullptr;
    lv_obj_t* _lblAddr = nullptr;
    bool _visible = false;
};
