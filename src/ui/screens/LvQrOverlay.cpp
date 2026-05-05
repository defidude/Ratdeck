#include "LvQrOverlay.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include <lvgl.h>
#include "fonts/fonts.h"

void LvQrOverlay::create() {
    if (_overlay) return;
    _overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_overlay, Theme::SCREEN_W, Theme::SCREEN_H);
    lv_obj_align(_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_overlay, 0, 0);
    lv_obj_set_style_pad_all(_overlay, 6, 0);
    lv_obj_set_style_pad_row(_overlay, 4, 0);
    lv_obj_set_layout(_overlay, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_overlay);
    lv_obj_set_style_text_font(title, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(title, "Scan to add me");

    // 180px; dark-on-light for standard scanner contrast on 320x240.
    _qr = lv_qrcode_create(_overlay,
                           180,
                           lv_color_hex(0x000000),
                           lv_color_hex(0xFFFFFF));

    _lblAddr = lv_label_create(_overlay);
    lv_obj_set_style_text_font(_lblAddr, &lv_font_ratdeck_10, 0);
    lv_obj_set_style_text_color(_lblAddr, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_long_mode(_lblAddr, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_lblAddr, Theme::SCREEN_W - 12);
    lv_obj_set_style_text_align(_lblAddr, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_lblAddr, "");

    lv_obj_t* hint = lv_label_create(_overlay);
    lv_obj_set_style_text_font(hint, &lv_font_ratdeck_10, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(hint, "Any key to close");

    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void LvQrOverlay::show(const String& destHashHex, const String& publicKeyHex) {
    if (!_overlay) create();

    // lxma://<hash>:<pubkey> — Columba/Sideband format; pubkey skips
    // the PENDING_IDENTITY round-trip on the scanning end.
    String payload = "lxma://" + destHashHex;
    if (publicKeyHex.length() > 0) {
        payload += ":" + publicKeyHex;
    }

    if (_qr) {
        if (lv_qrcode_update(_qr, payload.c_str(), payload.length()) != LV_RES_OK) {
            return;
        }
    }
    if (_lblAddr) {
        lv_label_set_text(_lblAddr, destHashHex.c_str());
    }

    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    _visible = true;
}

void LvQrOverlay::hide() {
    if (_overlay) lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
    _visible = false;
}

bool LvQrOverlay::handleKey(const KeyEvent& event) {
    if (!_visible) return false;
    (void)event;
    hide();
    return true;
}
