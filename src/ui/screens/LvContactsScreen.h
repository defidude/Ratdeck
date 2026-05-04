#pragma once

#include "ui/UIManager.h"
#include <functional>
#include <string>
#include <vector>

class AnnounceManager;

class LvContactsScreen : public LvScreen {
public:
    using NodeSelectedCallback = std::function<void(const std::string& peerHex)>;

    void createUI(lv_obj_t* parent) override;
    void refreshUI() override;
    void onEnter() override;
    bool handleKey(const KeyEvent& event) override;

    void setAnnounceManager(AnnounceManager* am) { _am = am; }
    void setNodeSelectedCallback(NodeSelectedCallback cb) { _onSelect = cb; }
    void setUIManager(class UIManager* ui) { _ui = ui; }
    bool handleLongPress() override;

    const char* title() const override { return "Contacts"; }

private:
    void rebuildList();

    AnnounceManager* _am = nullptr;
    class UIManager* _ui = nullptr;
    NodeSelectedCallback _onSelect;
    bool _confirmDelete = false;
    bool _focusActive = false;
    int _deleteIdx = -1;
    int _lastContactCount = -1;
    unsigned long _lastRebuild = 0;
    static constexpr unsigned long REBUILD_INTERVAL_MS = 30000;
    std::vector<int> _contactIndices;
    std::vector<std::vector<uint8_t>> _avatarBuffers;

    lv_obj_t* _list = nullptr;
    lv_obj_t* _emptyState = nullptr;
};
