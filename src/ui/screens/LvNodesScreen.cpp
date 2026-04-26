#include "LvNodesScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "ui/UIManager.h"
#include "reticulum/AnnounceManager.h"
#include "config/UserConfig.h"
#include <Arduino.h>
#include <algorithm>
#include "fonts/fonts.h"

void LvNodesScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    _lblEmpty = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblEmpty, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_lblEmpty, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(_lblEmpty, "No nodes discovered");
    lv_obj_center(_lblEmpty);

    _list = lv_obj_create(parent);
    lv_obj_set_size(_list, lv_pct(100), lv_pct(100));
    lv_obj_add_style(_list, LvTheme::styleList(), 0);
    lv_obj_set_layout(_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);

    rebuildList();

    // --- Action modal overlay (on top layer, centered) ---
    _overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_overlay, 200, 110);
    lv_obj_set_pos(_overlay, (Theme::SCREEN_W - 200) / 2, Theme::STATUS_BAR_H + (Theme::CONTENT_H - 110) / 2);
    lv_obj_add_style(_overlay, LvTheme::styleModal(), 0);
    lv_obj_set_style_pad_row(_overlay, 4, 0);
    lv_obj_set_layout(_overlay, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);

    const char* menuText[] = {"Add Contact", "Message", "Back"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_obj_create(_overlay);
        lv_obj_set_size(btn, 176, 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(Theme::BG_SURFACE), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto* self = (LvNodesScreen*)lv_event_get_user_data(e);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            self->_menuIdx = idx;
            KeyEvent tap = {};
            tap.enter = true;
            self->handleKey(tap);
        }, LV_EVENT_CLICKED, this);

        _menuLabels[i] = lv_label_create(btn);
        lv_obj_set_style_text_font(_menuLabels[i], &lv_font_ratdeck_14, 0);
        lv_obj_set_style_text_color(_menuLabels[i], lv_color_hex(Theme::PRIMARY), 0);
        lv_label_set_text(_menuLabels[i], menuText[i]);
        lv_obj_center(_menuLabels[i]);

        _menuBtns[i] = btn;
    }

    // Nickname input widgets
    _nicknameBox = lv_obj_create(_overlay);
    lv_obj_set_size(_nicknameBox, 176, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(_nicknameBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_nicknameBox, 0, 0);
    lv_obj_set_style_pad_all(_nicknameBox, 0, 0);
    lv_obj_set_style_pad_row(_nicknameBox, 2, 0);
    lv_obj_set_layout(_nicknameBox, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_nicknameBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_nicknameBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_nicknameBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_nicknameBox, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* nickTitle = lv_label_create(_nicknameBox);
    lv_obj_set_style_text_font(nickTitle, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(nickTitle, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(nickTitle, "Enter nickname:");

    _nicknameLbl = lv_label_create(_nicknameBox);
    lv_obj_set_style_text_font(_nicknameLbl, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_nicknameLbl, lv_color_hex(Theme::PRIMARY), 0);
    lv_label_set_text(_nicknameLbl, "_");

    _nicknameHint = lv_label_create(_nicknameBox);
    lv_obj_set_style_text_font(_nicknameHint, &lv_font_ratdeck_10, 0);
    lv_obj_set_style_text_color(_nicknameHint, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(_nicknameHint, "Enter=Save  Esc=Cancel");
}

void LvNodesScreen::destroyUI() {
    if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
    for (int i = 0; i < 3; i++) { _menuBtns[i] = nullptr; _menuLabels[i] = nullptr; }
    _nicknameBox = nullptr; _nicknameLbl = nullptr; _nicknameHint = nullptr;
    _list = nullptr; _lblEmpty = nullptr;
    LvScreen::destroyUI();
}

void LvNodesScreen::onEnter() {
    _lastNodeCount = -1;
    _lastContactCount = -1;
    _focusActive = false;
    rebuildList();
}

void LvNodesScreen::refreshUI() {
    if (!_am) return;
    unsigned long now = millis();
    if (now - _lastRebuild < REBUILD_INTERVAL_MS) return;
    int contacts = 0;
    for (const auto& n : _am->nodes()) { if (n.saved) contacts++; }
    int countDelta = abs(_am->nodeCount() - _lastNodeCount);
    int contactDelta = abs(contacts - _lastContactCount);
    if (countDelta > 0 || contactDelta > 0) {
        _lastRebuild = now;
        if (countDelta > 3 || contactDelta > 0) {
            rebuildList();
        }
    }
}

void LvNodesScreen::rebuildList() {
    if (!_am || !_list) return;
    // Preserve scroll position across rebuilds
    lv_coord_t scrollY = lv_obj_get_scroll_y(_list);
    lv_obj_clean(_list);
    _sortedContactIndices.clear();
    _sortedOnlineIndices.clear();

    const auto& nodes = _am->nodes();
    int count = (int)nodes.size();
    _lastNodeCount = count;

    for (int i = 0; i < count; i++) {
        if (nodes[i].saved) _sortedContactIndices.push_back(i);
        else _sortedOnlineIndices.push_back(i);
    }
    _lastContactCount = (int)_sortedContactIndices.size();

    std::sort(_sortedOnlineIndices.begin(), _sortedOnlineIndices.end(), [&nodes](int a, int b) {
        return nodes[a].lastSeen > nodes[b].lastSeen;
    });

    if (_sortedContactIndices.empty() && _sortedOnlineIndices.empty()) {
        lv_obj_clear_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_list, LV_OBJ_FLAG_HIDDEN);

    bool devMode = _cfg && _cfg->settings().devMode;

    auto addHeader = [&](const char* text) {
        lv_obj_t* hdr = lv_obj_create(_list);
        lv_obj_set_size(hdr, Theme::CONTENT_W, 22);
        lv_obj_add_style(hdr, LvTheme::styleSectionHeader(), 0);
        lv_obj_set_style_radius(hdr, 0, 0);
        lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl = lv_label_create(hdr);
        lv_obj_set_style_text_font(lbl, &lv_font_ratdeck_10, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(Theme::ACCENT), 0);
        lv_label_set_text(lbl, text);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);
    };

    auto addNodeRow = [&](int nodeIdx) {
        const auto& node = nodes[nodeIdx];

        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, Theme::CONTENT_W, 26);
        lv_obj_add_style(row, LvTheme::styleListBtn(), 0);
        lv_obj_add_style(row, LvTheme::styleListBtnFocused(), LV_STATE_FOCUSED);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row, (void*)(intptr_t)nodeIdx);

        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            auto* self = (LvNodesScreen*)lv_event_get_user_data(e);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            if (idx >= 0 && idx < (int)self->_am->nodes().size()) {
                self->showActionMenu(idx);
            }
        }, LV_EVENT_CLICKED, this);

        lv_group_add_obj(LvInput::group(), row);
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_ON);
        }, LV_EVENT_FOCUSED, nullptr);

        // Name + hash
        std::string truncName = node.name.substr(0, devMode ? 12 : 15);
        std::string displayHash = node.hash.toHex().substr(0, 12);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s [%s]", truncName.c_str(), displayHash.c_str());
        lv_obj_t* nameLbl = lv_label_create(row);
        lv_obj_set_style_text_font(nameLbl, &lv_font_ratdeck_12, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_hex(
            node.saved ? Theme::ACCENT : Theme::PRIMARY), 0);
        lv_label_set_text(nameLbl, buf);
        lv_obj_align(nameLbl, LV_ALIGN_LEFT_MID, 8, 0);

        // Info (hops + age)
        unsigned long ageSec = (millis() - node.lastSeen) / 1000;
        char infoBuf[32];
        if (node.hops > 0 && node.hops < 128) {
            if (ageSec < 60) snprintf(infoBuf, sizeof(infoBuf), "%dhop %lus", node.hops, ageSec);
            else snprintf(infoBuf, sizeof(infoBuf), "%dhop %lum", node.hops, ageSec / 60);
        } else {
            if (ageSec < 60) snprintf(infoBuf, sizeof(infoBuf), "%lus", ageSec);
            else snprintf(infoBuf, sizeof(infoBuf), "%lum", ageSec / 60);
        }
        if (devMode && node.rssi != 0) {
            char rssiBuf[16];
            snprintf(rssiBuf, sizeof(rssiBuf), " %ddB", node.rssi);
            strncat(infoBuf, rssiBuf, sizeof(infoBuf) - strlen(infoBuf) - 1);
        }
        lv_obj_t* infoLbl = lv_label_create(row);
        lv_obj_set_style_text_font(infoLbl, &lv_font_ratdeck_10, 0);
        lv_obj_set_style_text_color(infoLbl, lv_color_hex(Theme::TEXT_SECONDARY), 0);
        lv_label_set_text(infoLbl, infoBuf);
        lv_obj_align(infoLbl, LV_ALIGN_RIGHT_MID, -4, 0);
    };

    // Build list: Contacts section, then Online section
    if (!_sortedContactIndices.empty()) {
        char hdrBuf[32];
        snprintf(hdrBuf, sizeof(hdrBuf), "Contacts (%d)", (int)_sortedContactIndices.size());
        addHeader(hdrBuf);
        for (int idx : _sortedContactIndices) addNodeRow(idx);
    }

    {
        char hdrBuf[32];
        snprintf(hdrBuf, sizeof(hdrBuf), "Online (%d)", (int)_sortedOnlineIndices.size());
        addHeader(hdrBuf);
        for (int idx : _sortedOnlineIndices) addNodeRow(idx);
    }

    // Restore scroll position so the list doesn't jump to top on refresh
    if (scrollY > 0) {
        lv_obj_update_layout(_list);
        lv_obj_scroll_to_y(_list, scrollY, LV_ANIM_OFF);
    }

    // Clear auto-focus if user hasn't started navigating yet
    if (!_focusActive) {
        lv_obj_t* focused = lv_group_get_focused(LvInput::group());
        if (focused) lv_obj_clear_state(focused, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
    }
}

int LvNodesScreen::getFocusedNodeIdx() const {
    lv_obj_t* focused = lv_group_get_focused(LvInput::group());
    if (!focused) return -1;
    return (int)(intptr_t)lv_obj_get_user_data(focused);
}

// --- Action modal helpers ---

void LvNodesScreen::showActionMenu(int nodeIdx) {
    _actionNodeIdx = nodeIdx;
    _menuIdx = 0;
    _actionState = NodeAction::ACTION_MENU;
    _nicknameText = "";
    if (_overlay) {
        if (_am && nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
            bool isSaved = _am->nodes()[nodeIdx].saved;
            lv_label_set_text(_menuLabels[0], isSaved ? "Edit Name" : "Add Contact");
        }
        for (int i = 0; i < 3; i++) lv_obj_clear_flag(_menuBtns[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_nicknameBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
        updateMenuSelection();
    }
}

void LvNodesScreen::hideOverlay() {
    _actionState = NodeAction::BROWSE;
    _actionNodeIdx = -1;
    _nicknameText = "";
    if (_overlay) lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void LvNodesScreen::showNicknameInput() {
    _actionState = NodeAction::NICKNAME_INPUT;
    if (_am && _actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size()) {
        _nicknameText = String(_am->nodes()[_actionNodeIdx].name.c_str());
    }
    for (int i = 0; i < 3; i++) lv_obj_add_flag(_menuBtns[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_nicknameBox, LV_OBJ_FLAG_HIDDEN);
    updateNicknameDisplay();
}

void LvNodesScreen::updateMenuSelection() {
    for (int i = 0; i < 3; i++) {
        bool sel = (i == _menuIdx);
        lv_obj_set_style_text_color(_menuLabels[i], lv_color_hex(
            sel ? Theme::TEXT_PRIMARY : Theme::TEXT_SECONDARY), 0);
        lv_obj_set_style_bg_color(_menuBtns[i], lv_color_hex(
            sel ? Theme::BG_HOVER : Theme::BG_SURFACE), 0);
        lv_obj_set_style_bg_opa(_menuBtns[i], LV_OPA_COVER, 0);
    }
}

void LvNodesScreen::updateNicknameDisplay() {
    if (_nicknameLbl) {
        String display = _nicknameText + "_";
        lv_label_set_text(_nicknameLbl, display.c_str());
    }
}

bool LvNodesScreen::handleLongPress() {
    if (!_am) return false;
    // Only consume long-press once the user has actively navigated into
    // the list. Otherwise let main.cpp blank the screen.
    if (!_focusActive) return false;
    int nodeIdx = getFocusedNodeIdx();
    if (nodeIdx < 0 || nodeIdx >= (int)_am->nodes().size()) return false;
    const auto& node = _am->nodes()[nodeIdx];
    if (node.saved) {
        _confirmDelete = true;
        _actionNodeIdx = nodeIdx;
        if (_ui) _ui->lvStatusBar().showToast("Remove friend? Enter=Yes Esc=No", 5000);
    } else {
        showActionMenu(nodeIdx);
    }
    return true;
}

bool LvNodesScreen::handleKey(const KeyEvent& event) {
    if (!_am) return false;

    // --- Focus activation guard (only in browse mode) ---
    if (_actionState == NodeAction::BROWSE && !_confirmDelete &&
        !_focusActive && (event.up || event.down || event.enter)) {
        _focusActive = true;
        lv_obj_t* focused = lv_group_get_focused(LvInput::group());
        if (focused) lv_obj_add_state(focused, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
        return true;
    }

    // --- Nickname input mode ---
    if (_actionState == NodeAction::NICKNAME_INPUT) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            if (_actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size()) {
                auto& node = const_cast<DiscoveredNode&>(_am->nodes()[_actionNodeIdx]);
                String finalName = _nicknameText;
                finalName.trim();
                if (finalName.isEmpty()) {
                    if (!node.name.empty()) finalName = String(node.name.c_str());
                    else finalName = String(node.hash.toHex().substr(0, 12).c_str());
                }
                node.name = finalName.c_str();
                node.saved = true;
                _am->saveContacts();
                if (_ui) _ui->lvStatusBar().showToast("Contact saved!", 1200);
                hideOverlay();
                rebuildList();
            } else {
                hideOverlay();
            }
            return true;
        }
        if (event.character == 0x1B) { hideOverlay(); return true; }
        if (event.character == '\b' || event.character == 0x7F) {
            if (_nicknameText.length() > 0) _nicknameText.remove(_nicknameText.length() - 1);
            updateNicknameDisplay();
            return true;
        }
        if (event.character >= 0x20 && event.character <= 0x7E && _nicknameText.length() < 16) {
            _nicknameText += (char)event.character;
            updateNicknameDisplay();
            return true;
        }
        return true;
    }

    // --- Action menu mode ---
    if (_actionState == NodeAction::ACTION_MENU) {
        if (event.up) {
            if (_menuIdx > 0) { _menuIdx--; updateMenuSelection(); }
            return true;
        }
        if (event.down) {
            if (_menuIdx < 2) { _menuIdx++; updateMenuSelection(); }
            return true;
        }
        if (event.enter || event.character == '\n' || event.character == '\r') {
            switch (_menuIdx) {
                case 0:
                    showNicknameInput();
                    break;
                case 1:
                    if (_actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size() && _onSelect) {
                        std::string hex = _am->nodes()[_actionNodeIdx].hash.toHex();
                        hideOverlay();
                        _onSelect(hex);
                    } else {
                        hideOverlay();
                    }
                    break;
                case 2:
                    hideOverlay();
                    break;
            }
            return true;
        }
        if (event.character == 0x1B) { hideOverlay(); return true; }
        return true;
    }

    // --- Confirm delete mode ---
    if (_confirmDelete) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            if (_actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size()) {
                _am->deleteContact(_actionNodeIdx);
                if (_ui) _ui->lvStatusBar().showToast("Contact deleted", 1200);
                rebuildList();
            }
            _confirmDelete = false;
            return true;
        }
        _confirmDelete = false;
        if (_ui) _ui->lvStatusBar().showToast("Cancelled", 800);
        return true;
    }

    // 's' or 'S' to save/unsave contact
    if (event.character == 's' || event.character == 'S') {
        int nodeIdx = getFocusedNodeIdx();
        if (nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
            auto& node = const_cast<DiscoveredNode&>(_am->nodes()[nodeIdx]);
            node.saved = !node.saved;
            if (node.saved) _am->saveContacts();
            rebuildList();
        }
        return true;
    }

    // Let LVGL focus group handle up/down/enter navigation
    return false;
}
