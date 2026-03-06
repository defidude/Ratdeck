#pragma once

#include "ui/UIManager.h"
#include <functional>
#include <string>

class AnnounceManager;

class NodesScreen : public Screen {
public:
    using NodeSelectedCallback = std::function<void(const std::string& peerHex)>;

    void update() override;
    void onEnter() override;
    bool handleKey(const KeyEvent& event) override;

    void setAnnounceManager(AnnounceManager* am) { _am = am; }
    void setNodeSelectedCallback(NodeSelectedCallback cb) { _onSelect = cb; }

    const char* title() const override { return "Nodes"; }
    void draw(LGFX_TDeck& gfx) override;

private:
    AnnounceManager* _am = nullptr;
    NodeSelectedCallback _onSelect;
    int _lastNodeCount = -1;
    int _selectedIdx = 0;
};
