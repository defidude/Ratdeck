#pragma once

class LGFX_TDeck;

class TabBar {
public:
    enum Tab { TAB_HOME = 0, TAB_MSGS, TAB_NODES, TAB_MAP, TAB_SETUP };

    void setGfx(LGFX_TDeck* gfx) { _gfx = gfx; }
    void draw(LGFX_TDeck& gfx);

    void setActiveTab(int tab);
    int getActiveTab() const { return _activeTab; }
    void cycleTab(int direction);

    void setUnreadCount(int tab, int count);

    using TabCallback = void(*)(int tab);
    void setTabCallback(TabCallback cb) { _tabCb = cb; }

    bool isDirty() const { return _dirty; }
    void markDirty() { _dirty = true; }
    void clearDirty() { _dirty = false; }

    friend class UIManager;

private:
    LGFX_TDeck* _gfx = nullptr;
    bool _dirty = true;

    int _activeTab = TAB_HOME;
    int _unread[5] = {};
    TabCallback _tabCb = nullptr;

    static constexpr const char* TAB_NAMES[5] = {"Home", "Msgs", "Nodes", "Map", "Setup"};
};
