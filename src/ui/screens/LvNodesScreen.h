#pragma once

#include "ui/UIManager.h"
#include <functional>
#include <string>
#include <vector>

class AnnounceManager;
class UserConfig;

class LvNodesScreen : public LvScreen {
public:
    using NodeSelectedCallback = std::function<void(const std::string& peerHex)>;

    void createUI(lv_obj_t* parent) override;
    void refreshUI() override;
    void onEnter() override;
    bool handleKey(const KeyEvent& event) override;

    void setAnnounceManager(AnnounceManager* am) { _am = am; }
    void setNodeSelectedCallback(NodeSelectedCallback cb) { _onSelect = cb; }
    void setUIManager(class UIManager* ui) { _ui = ui; }
    void setUserConfig(UserConfig* cfg) { _cfg = cfg; }
    bool handleLongPress() override;

    const char* title() const override { return "Nodes"; }

private:
    void updateSortOrder();
    void syncVisibleRows();
    void scrollToSelected();
    int getNodeIdxForEntry(int entry) const;

    // Action modal helpers
    enum class NodeAction { BROWSE, ACTION_MENU, NICKNAME_INPUT };
    void showActionMenu(int nodeIdx);
    void hideOverlay();
    void showNicknameInput();
    void updateMenuSelection();
    void updateNicknameDisplay();

    AnnounceManager* _am = nullptr;
    class UIManager* _ui = nullptr;
    UserConfig* _cfg = nullptr;
    NodeSelectedCallback _onSelect;
    bool _confirmDelete = false;

    // Action modal state
    NodeAction _actionState = NodeAction::BROWSE;
    int _menuIdx = 0;
    int _actionNodeIdx = -1;
    String _nicknameText;

    // Overlay widgets
    lv_obj_t* _overlay = nullptr;
    lv_obj_t* _menuLabels[3] = {};
    lv_obj_t* _menuBtns[3] = {};
    lv_obj_t* _nicknameBox = nullptr;
    lv_obj_t* _nicknameLbl = nullptr;
    lv_obj_t* _nicknameHint = nullptr;
    int _lastNodeCount = -1;
    int _lastContactCount = -1;
    int _selectedIdx = 0;
    int _totalEntries = 0;        // Total displayable entries (contacts + headers + online)

    // Sorted index vectors (into _am->nodes())
    std::vector<int> _sortedContactIndices;
    std::vector<int> _sortedOnlineIndices;
    bool _dataChanged = false;

    unsigned long _lastRebuild = 0;
    static constexpr unsigned long REBUILD_INTERVAL_MS = 5000;

    // Widget pool — fixed set of pre-allocated row widgets
    static constexpr int ROW_POOL_SIZE = 14;
    lv_obj_t* _poolRows[ROW_POOL_SIZE] = {};
    lv_obj_t* _poolNameLabels[ROW_POOL_SIZE] = {};
    lv_obj_t* _poolInfoLabels[ROW_POOL_SIZE] = {};
    int _viewportStart = 0;       // First visible sorted index

    lv_obj_t* _list = nullptr;
    lv_obj_t* _lblEmpty = nullptr;
};
