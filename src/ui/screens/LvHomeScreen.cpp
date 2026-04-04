#include "LvHomeScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "reticulum/ReticulumManager.h"
#include "reticulum/LXMFManager.h"
#include "reticulum/AnnounceManager.h"
#include "radio/SX1262.h"
#include "config/UserConfig.h"
#include "transport/TCPClientInterface.h"
#include <Arduino.h>
#include <WiFi.h>
#include "fonts/fonts.h"

void LvHomeScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Name — large, accent color
    _lblName = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblName, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_lblName, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(_lblName, "...");

    // ID hash — smaller, muted
    _lblId = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblId, &lv_font_ratdeck_10, 0);
    lv_obj_set_style_text_color(_lblId, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(_lblId, "ID: ...");

    // Transport indicators row
    _lblStatus = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblStatus, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(_lblStatus, "Status: ...");
    lv_obj_set_style_pad_top(_lblStatus, 4, 0);

    // Online nodes
    _lblNodes = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblNodes, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(_lblNodes, lv_color_hex(Theme::TEXT_SECONDARY), 0);
    lv_label_set_text(_lblNodes, "Online Nodes: ...");

    // Announce button — compact, understated
    _btnAnnounce = lv_btn_create(parent);
    lv_obj_set_size(_btnAnnounce, 90, 24);
    lv_obj_add_style(_btnAnnounce, LvTheme::styleBtn(), 0);
    lv_obj_add_style(_btnAnnounce, LvTheme::styleBtnFocused(), LV_STATE_FOCUSED);
    lv_obj_add_style(_btnAnnounce, LvTheme::styleBtnPressed(), LV_STATE_PRESSED);

    lv_obj_t* btnLbl = lv_label_create(_btnAnnounce);
    lv_obj_set_style_text_font(btnLbl, &lv_font_ratdeck_10, 0);
    lv_label_set_text(btnLbl, "Announce");
    lv_obj_center(btnLbl);

    lv_group_add_obj(LvInput::group(), _btnAnnounce);
    lv_obj_add_event_cb(_btnAnnounce, [](lv_event_t* e) {
        auto* self = (LvHomeScreen*)lv_event_get_user_data(e);
        if (self->_announceCb) self->_announceCb();
    }, LV_EVENT_CLICKED, this);

    // Force refresh on new UI
    _lastUptime = ULONG_MAX;
    _lastHeap = UINT32_MAX;
    refreshUI();
}

void LvHomeScreen::onEnter() {
    _lastUptime = ULONG_MAX;
    _lastHeap = UINT32_MAX;
    refreshUI();
}

void LvHomeScreen::refreshUI() {
    if (!_lblName) return;

    unsigned long upMins = millis() / 60000;
    uint32_t heap = ESP.getFreeHeap() / 1024;
    if (upMins == _lastUptime && heap == _lastHeap) return;
    _lastUptime = upMins;
    _lastHeap = heap;

    // Name
    if (_cfg && !_cfg->settings().displayName.isEmpty()) {
        lv_label_set_text(_lblName, _cfg->settings().displayName.c_str());
    } else if (_rns) {
        String dh = _rns->destinationHashHex();
        String fallback = "Ratspeak.org-" + dh.substring(0, 3);
        lv_label_set_text(_lblName, fallback.c_str());
    } else {
        lv_label_set_text(_lblName, "---");
    }

    // ID (LXMF destination hash, 12 chars)
    if (_rns) {
        String dh = _rns->destinationHashHex();
        if (dh.length() > 12) dh = dh.substring(0, 12);
        lv_label_set_text_fmt(_lblId, "LXMF ID: %s", dh.c_str());
    } else {
        lv_label_set_text(_lblId, "LXMF ID: ---");
    }

    // Status with transport indicators
    bool loraUp = _radioOnline && _radio && _radio->isRadioOnline();
    bool tcpUp = false;
    if (_tcpClients) {
        for (auto* tcp : *_tcpClients) {
            if (tcp && tcp->isConnected()) { tcpUp = true; break; }
        }
    }
    bool wifiUp = WiFi.status() == WL_CONNECTED;

    if (loraUp && tcpUp) {
        lv_label_set_text(_lblStatus, "LoRa + TCP");
        lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::SUCCESS), 0);
    } else if (loraUp && wifiUp) {
        lv_label_set_text(_lblStatus, "LoRa + WiFi");
        lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::SUCCESS), 0);
    } else if (loraUp) {
        lv_label_set_text(_lblStatus, "LoRa");
        lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::SUCCESS), 0);
    } else if (tcpUp) {
        lv_label_set_text(_lblStatus, "TCP");
        lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::SUCCESS), 0);
    } else {
        lv_label_set_text(_lblStatus, "Offline");
        lv_obj_set_style_text_color(_lblStatus, lv_color_hex(Theme::ERROR_CLR), 0);
    }

    // Online Nodes (30 min window)
    if (_am) {
        int online = _am->nodesOnlineSince(1800000);
        lv_label_set_text_fmt(_lblNodes, "Online Nodes: %d", online);
    } else {
        lv_label_set_text(_lblNodes, "Online Nodes: 0");
    }
}

bool LvHomeScreen::handleKey(const KeyEvent& event) {
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (_announceCb) _announceCb();
        return true;
    }
    return false;
}
