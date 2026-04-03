#include "LvMessagesScreen.h"
#include "ui/Theme.h"
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
    lv_obj_set_style_text_color(_lblEmpty, lv_color_hex(Theme::MUTED), 0);
    lv_label_set_text(_lblEmpty, "No conversations");
    lv_obj_center(_lblEmpty);

    _list = lv_obj_create(parent);
    lv_obj_set_size(_list, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(_list, 0, 0);
    lv_obj_set_flex_grow(_list, 1);
    lv_obj_set_style_bg_color(_list, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_pad_all(_list, 0, 0);
    lv_obj_set_style_pad_row(_list, 0, 0);
    lv_obj_set_style_radius(_list, 0, 0);
    lv_obj_set_layout(_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);

    // Pre-allocate row pool (following LvNodesScreen pattern)
    const lv_font_t* nameFont = &lv_font_ratdeck_14;
    const lv_font_t* smallFont = &lv_font_ratdeck_12;

    for (int i = 0; i < ROW_POOL_SIZE; i++) {
        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, Theme::CONTENT_W, 38);
        lv_obj_set_style_bg_color(row, lv_color_hex(Theme::BG), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(Theme::BORDER), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            auto* self = (LvMessagesScreen*)lv_event_get_user_data(e);
            lv_obj_t* target = lv_event_get_target(e);
            int poolIdx = (int)(intptr_t)lv_obj_get_user_data(target);
            int dataIdx = self->_viewportStart + poolIdx;
            if (dataIdx < (int)self->_sortedPeers.size() && self->_onOpen) {
                self->_selectedIdx = dataIdx;
                self->syncVisibleRows();
                self->_onOpen(self->_sortedPeers[dataIdx]);
            }
        }, LV_EVENT_CLICKED, this);

        // Unread dot
        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, 3, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(Theme::PRIMARY), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_set_pos(dot, 5, 8);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);

        // Name label (line 1)
        lv_obj_t* nameLbl = lv_label_create(row);
        lv_obj_set_style_text_font(nameLbl, nameFont, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_hex(Theme::PRIMARY), 0);
        lv_label_set_text(nameLbl, "");
        lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 14, 2);

        // Time label (line 1, right)
        lv_obj_t* timeLbl = lv_label_create(row);
        lv_obj_set_style_text_font(timeLbl, &lv_font_ratdeck_10, 0);
        lv_obj_set_style_text_color(timeLbl, lv_color_hex(Theme::MUTED), 0);
        lv_label_set_text(timeLbl, "");
        lv_obj_align(timeLbl, LV_ALIGN_TOP_RIGHT, -4, 4);

        // Preview label (line 2)
        lv_obj_t* prevLbl = lv_label_create(row);
        lv_obj_set_style_text_font(prevLbl, smallFont, 0);
        lv_obj_set_style_text_color(prevLbl, lv_color_hex(Theme::MUTED), 0);
        lv_label_set_text(prevLbl, "");
        lv_obj_align(prevLbl, LV_ALIGN_BOTTOM_LEFT, 14, -4);

        _poolRows[i] = row;
        _poolDots[i] = dot;
        _poolNameLabels[i] = nameLbl;
        _poolTimeLabels[i] = timeLbl;
        _poolPreviewLabels[i] = prevLbl;
    }

    _lastConvCount = -1;
    rebuildList();
}

void LvMessagesScreen::onEnter() {
    _lastConvCount = -1;
    _selectedIdx = 0;
    _viewportStart = 0;
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

// Update only the selection highlight via pool sync
void LvMessagesScreen::updateSelection(int oldIdx, int newIdx) {
    syncVisibleRows();
}

void LvMessagesScreen::rebuildList() {
    if (!_lxmf || !_list) return;

    const auto& convs = _lxmf->conversations();
    int count = (int)convs.size();
    _lastConvCount = count;
    _lastUnreadTotal = _lxmf->unreadCount();
    _sortedPeers.clear();
    _sortedConvs.clear();

    if (count == 0) {
        lv_obj_clear_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < ROW_POOL_SIZE; i++) lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
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
        // Resolve display name
        std::string peerName;
        if (_am) peerName = _am->lookupName(ci.peerHex);
        ci.displayName = !peerName.empty() ? peerName.substr(0, 15) : ci.peerHex.substr(0, 12);
        _sortedConvs.push_back(ci);
    }

    std::sort(_sortedConvs.begin(), _sortedConvs.end(), [](const ConvInfo& a, const ConvInfo& b) {
        return a.lastTs > b.lastTs;
    });

    for (auto& ci : _sortedConvs) _sortedPeers.push_back(ci.peerHex);

    if (_selectedIdx >= count) _selectedIdx = count - 1;
    if (_selectedIdx < 0) _selectedIdx = 0;

    syncVisibleRows();
}

void LvMessagesScreen::syncVisibleRows() {
    if (!_list) return;
    int count = (int)_sortedConvs.size();

    if (count == 0) {
        for (int i = 0; i < ROW_POOL_SIZE; i++) lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Compute viewport centered on selection
    int halfPool = ROW_POOL_SIZE / 2;
    _viewportStart = _selectedIdx - halfPool;
    if (_viewportStart < 0) _viewportStart = 0;
    if (_viewportStart + ROW_POOL_SIZE > count) {
        _viewportStart = count - ROW_POOL_SIZE;
        if (_viewportStart < 0) _viewportStart = 0;
    }

    for (int i = 0; i < ROW_POOL_SIZE; i++) {
        int convIdx = _viewportStart + i;
        if (convIdx >= count) {
            lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        const auto& ci = _sortedConvs[convIdx];
        bool isSelected = (convIdx == _selectedIdx);

        // Selection highlight
        lv_obj_set_style_bg_color(_poolRows[i], lv_color_hex(
            isSelected ? Theme::SELECTION_BG : Theme::BG), 0);

        // Unread dot
        if (ci.hasUnread) {
            lv_obj_clear_flag(_poolDots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_poolDots[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Name
        lv_label_set_text(_poolNameLabels[i], ci.displayName.c_str());

        // Time
        if (ci.lastTs > 1700000000) {
            time_t t = (time_t)ci.lastTs;
            struct tm* tm = localtime(&t);
            if (tm) {
                char timeBuf[8];
                snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tm->tm_hour, tm->tm_min);
                lv_label_set_text(_poolTimeLabels[i], timeBuf);
            } else {
                lv_label_set_text(_poolTimeLabels[i], "");
            }
        } else {
            lv_label_set_text(_poolTimeLabels[i], "");
        }

        // Preview
        lv_label_set_text(_poolPreviewLabels[i], ci.preview.c_str());
    }
}

bool LvMessagesScreen::handleLongPress() {
    if (!_lxmf) return false;
    int count = (int)_lxmf->conversations().size();
    if (count == 0 || _selectedIdx >= count) return false;
    _lpState = LP_MENU;
    _menuIdx = 0;
    if (_ui) _ui->lvStatusBar().showToast("Up/Down: Add Friend | Delete | Cancel", 5000);
    return true;
}

bool LvMessagesScreen::handleKey(const KeyEvent& event) {
    if (!_lxmf) return false;

    // Long-press menu mode
    if (_lpState == LP_MENU) {
        if (event.up || event.down) {
            _menuIdx = (_menuIdx + (event.down ? 1 : -1) + 3) % 3;
            const char* labels[] = {">> Add Friend <<", ">> Delete Chat <<", ">> Cancel <<"};
            if (_ui) _ui->lvStatusBar().showToast(labels[_menuIdx], 5000);
            return true;
        }
        if (event.enter || event.character == '\n' || event.character == '\r') {
            int count = (int)_lxmf->conversations().size();
            if (_menuIdx == 0 && _selectedIdx < (int)_sortedPeers.size()) {
                // Add friend
                const auto& peerHex = _sortedPeers[_selectedIdx];
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
            } else if (_menuIdx == 1 && _selectedIdx < (int)_sortedPeers.size()) {
                // Confirm delete
                _lpState = LP_CONFIRM_DELETE;
                if (_ui) _ui->lvStatusBar().showToast("Delete chat? Enter=Yes Esc=No", 5000);
                return true;
            } else {
                // Cancel
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
            if (_selectedIdx < (int)_sortedPeers.size()) {
                const auto& peerHex = _sortedPeers[_selectedIdx];
                _lxmf->markRead(peerHex);
                extern MessageStore messageStore;
                messageStore.deleteConversation(peerHex);
                messageStore.refreshConversations();
                if (_ui) {
                    _ui->lvStatusBar().showToast("Chat deleted", 1200);
                    _ui->lvTabBar().setUnreadCount(LvTabBar::TAB_MSGS, _lxmf->unreadCount());
                }
                _selectedIdx = 0;
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

    int count = (int)_lxmf->conversations().size();
    if (count == 0) return false;

    if (event.up) {
        if (_selectedIdx > 0) {
            int prev = _selectedIdx;
            _selectedIdx--;
            syncVisibleRows();
        }
        return true;
    }
    if (event.down) {
        if (_selectedIdx < count - 1) {
            int prev = _selectedIdx;
            _selectedIdx++;
            syncVisibleRows();
        }
        return true;
    }
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (_selectedIdx < (int)_sortedPeers.size() && _onOpen) {
            _onOpen(_sortedPeers[_selectedIdx]);
        }
        return true;
    }
    return false;
}
