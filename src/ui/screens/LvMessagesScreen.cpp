#include "LvMessagesScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "ui/UIManager.h"
#include "reticulum/LXMFManager.h"
#include "reticulum/AnnounceManager.h"
#include "storage/MessageStore.h"
#include <Arduino.h>
#include <time.h>
#include <algorithm>
#include "fonts/fonts.h"

void LvMessagesScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    _lblEmpty = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblEmpty, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_lblEmpty, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(_lblEmpty, "No conversations");
    lv_obj_center(_lblEmpty);

    _list = lv_obj_create(parent);
    lv_obj_set_size(_list, lv_pct(100), lv_pct(100));
    lv_obj_add_style(_list, LvTheme::styleList(), 0);
    lv_obj_set_layout(_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);

    _lastConvCount = -1;
    rebuildList();
}

void LvMessagesScreen::onEnter() {
    _lastConvCount = -1;
    _focusActive = false;
    rebuildList();
}

void LvMessagesScreen::refreshUI() {
    if (!_lxmf) return;
    int count = (int)_lxmf->conversations().size();
    int unread = _lxmf->unreadCount();
    if (count != _lastConvCount || unread != _lastUnreadTotal) {
        rebuildList();
    }
}

void LvMessagesScreen::rebuildList() {
    if (!_lxmf || !_list) return;

    const auto& convs = _lxmf->conversations();
    int count = (int)convs.size();
    _lastConvCount = count;
    _lastUnreadTotal = _lxmf->unreadCount();
    _sortedPeers.clear();
    _sortedConvs.clear();

    lv_obj_clean(_list);

    if (count == 0) {
        lv_obj_clear_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_list, LV_OBJ_FLAG_HIDDEN);

    // Build sorted conversation info
    _sortedConvs.reserve(count);
    for (int i = 0; i < count; i++) {
        ConvInfo ci;
        ci.peerHex = convs[i];
        auto* s = _lxmf->getConversationSummary(ci.peerHex);
        if (s) {
            ci.lastTs = s->lastTimestamp;
            ci.preview = s->lastPreview;
            ci.hasUnread = s->unreadCount > 0;
        }
        std::string peerName;
        if (_am) peerName = _am->lookupName(ci.peerHex);
        ci.displayName = !peerName.empty() ? peerName.substr(0, 15) : ci.peerHex.substr(0, 12);
        _sortedConvs.push_back(ci);
    }

    std::sort(_sortedConvs.begin(), _sortedConvs.end(), [](const ConvInfo& a, const ConvInfo& b) {
        return a.lastTs > b.lastTs;
    });

    for (auto& ci : _sortedConvs) _sortedPeers.push_back(ci.peerHex);

    // Build list items with focus group support
    const lv_font_t* nameFont = &lv_font_ratdeck_14;
    const lv_font_t* smallFont = &lv_font_ratdeck_12;

    for (int i = 0; i < count; i++) {
        const auto& ci = _sortedConvs[i];

        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, Theme::CONTENT_W, 46);
        lv_obj_add_style(row, LvTheme::styleListBtn(), 0);
        lv_obj_add_style(row, LvTheme::styleListBtnFocused(), LV_STATE_FOCUSED);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(Theme::BORDER), 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);

        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            auto* self = (LvMessagesScreen*)lv_event_get_user_data(e);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            if (idx < (int)self->_sortedPeers.size() && self->_onOpen) {
                self->_onOpen(self->_sortedPeers[idx]);
            }
        }, LV_EVENT_CLICKED, this);

        lv_group_add_obj(LvInput::group(), row);
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_ON);
        }, LV_EVENT_FOCUSED, nullptr);

        int leftPad = 14;

        // Unread dot
        if (ci.hasUnread) {
            lv_obj_t* dot = lv_obj_create(row);
            lv_obj_set_size(dot, 6, 6);
            lv_obj_set_style_radius(dot, 3, 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex(Theme::PRIMARY), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_set_style_pad_all(dot, 0, 0);
            lv_obj_set_pos(dot, 4, 7);
        }

        // Name (top-left, first line)
        lv_obj_t* nameLbl = lv_label_create(row);
        lv_obj_set_style_text_font(nameLbl, nameFont, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_hex(Theme::PRIMARY), 0);
        lv_label_set_text(nameLbl, ci.displayName.c_str());
        lv_obj_set_pos(nameLbl, leftPad, 1);

        // Time (top-right)
        if (ci.lastTs > 1700000000) {
            time_t t = (time_t)ci.lastTs;
            struct tm* tm = localtime(&t);
            if (tm) {
                char timeBuf[8];
                snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tm->tm_hour, tm->tm_min);
                lv_obj_t* timeLbl = lv_label_create(row);
                lv_obj_set_style_text_font(timeLbl, &lv_font_ratdeck_10, 0);
                lv_obj_set_style_text_color(timeLbl, lv_color_hex(Theme::TEXT_MUTED), 0);
                lv_label_set_text(timeLbl, timeBuf);
                lv_obj_align(timeLbl, LV_ALIGN_TOP_RIGHT, -4, 3);
            }
        }

        // Preview (second line, below name)
        if (!ci.preview.empty()) {
            lv_obj_t* prevLbl = lv_label_create(row);
            lv_obj_set_style_text_font(prevLbl, smallFont, 0);
            lv_obj_set_style_text_color(prevLbl, lv_color_hex(Theme::TEXT_MUTED), 0);
            lv_label_set_long_mode(prevLbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(prevLbl, Theme::CONTENT_W - leftPad - 8);
            lv_label_set_text(prevLbl, ci.preview.c_str());
            lv_obj_set_pos(prevLbl, leftPad, 20);
        }
    }

    // Clear auto-focus if user hasn't started navigating yet
    if (!_focusActive) {
        lv_obj_t* focused = lv_group_get_focused(LvInput::group());
        if (focused) lv_obj_clear_state(focused, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
    }
}

int LvMessagesScreen::getFocusedPeerIdx() const {
    lv_obj_t* focused = lv_group_get_focused(LvInput::group());
    if (!focused) return -1;
    return (int)(intptr_t)lv_obj_get_user_data(focused);
}

bool LvMessagesScreen::handleLongPress() {
    if (!_lxmf) return false;
    // Only consume long-press once the user has actively navigated into
    // the list. Otherwise let main.cpp blank the screen.
    if (!_focusActive) return false;
    int idx = getFocusedPeerIdx();
    if (idx < 0 || idx >= (int)_sortedPeers.size()) return false;
    _lpPeerIdx = idx;
    _lpState = LP_MENU;
    _menuIdx = 0;
    if (_ui) _ui->lvStatusBar().showToast("Up/Down: Add Friend | Delete | Cancel", 5000);
    return true;
}

bool LvMessagesScreen::handleKey(const KeyEvent& event) {
    if (!_lxmf) return false;

    if (!_focusActive && (event.up || event.down || event.enter)) {
        _focusActive = true;
        lv_obj_t* focused = lv_group_get_focused(LvInput::group());
        if (focused) lv_obj_add_state(focused, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
        return true;
    }

    // Long-press menu mode
    if (_lpState == LP_MENU) {
        if (event.up || event.down) {
            _menuIdx = (_menuIdx + (event.down ? 1 : -1) + 3) % 3;
            const char* labels[] = {">> Add Friend <<", ">> Delete Chat <<", ">> Cancel <<"};
            if (_ui) _ui->lvStatusBar().showToast(labels[_menuIdx], 5000);
            return true;
        }
        if (event.enter || event.character == '\n' || event.character == '\r') {
            if (_menuIdx == 0 && _lpPeerIdx < (int)_sortedPeers.size()) {
                const auto& peerHex = _sortedPeers[_lpPeerIdx];
                if (_am) {
                    const DiscoveredNode* existing = _am->findNodeByHex(peerHex);
                    if (existing && !existing->saved) {
                        auto& node = const_cast<DiscoveredNode&>(*existing);
                        node.saved = true;
                        _am->saveContacts();
                        if (_ui) _ui->lvStatusBar().showToast("Added to friends!", 1200);
                    } else if (!existing) {
                        _am->addManualContact(peerHex, "");
                        if (_ui) _ui->lvStatusBar().showToast("Added to friends!", 1200);
                    } else {
                        if (_ui) _ui->lvStatusBar().showToast("Already a friend", 1200);
                    }
                }
            } else if (_menuIdx == 1 && _lpPeerIdx < (int)_sortedPeers.size()) {
                _lpState = LP_CONFIRM_DELETE;
                if (_ui) _ui->lvStatusBar().showToast("Delete chat? Enter=Yes Esc=No", 5000);
                return true;
            } else {
                if (_ui) _ui->lvStatusBar().showToast("Cancelled", 800);
            }
            _lpState = LP_NONE;
            return true;
        }
        if (event.del || event.character == 8 || event.character == 0x1B) {
            _lpState = LP_NONE;
            if (_ui) _ui->lvStatusBar().showToast("Cancelled", 800);
            return true;
        }
        return true;
    }

    // Confirm delete mode
    if (_lpState == LP_CONFIRM_DELETE) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            if (_lpPeerIdx < (int)_sortedPeers.size()) {
                const auto& peerHex = _sortedPeers[_lpPeerIdx];
                _lxmf->markRead(peerHex);
                extern MessageStore messageStore;
                messageStore.deleteConversation(peerHex);
                messageStore.refreshConversations();
                if (_ui) {
                    _ui->lvStatusBar().showToast("Chat deleted", 1200);
                    _ui->lvTabBar().setUnreadCount(LvTabBar::TAB_MSGS, _lxmf->unreadCount());
                }
                _lastConvCount = -1;
                rebuildList();
            }
            _lpState = LP_NONE;
            return true;
        }
        _lpState = LP_NONE;
        if (_ui) _ui->lvStatusBar().showToast("Cancelled", 800);
        return true;
    }

    // Let LVGL focus group handle up/down/enter navigation
    return false;
}
