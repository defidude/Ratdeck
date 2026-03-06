#pragma once

#include "ui/UIManager.h"
#include <string>
#include <vector>
#include <functional>

class UserConfig;
class FlashStore;
class SDStore;
class SX1262;
class AudioNotify;
class Power;
class WiFiInterface;
class TCPClientInterface;
class ReticulumManager;

enum class SettingType : uint8_t {
    HEADER,
    READONLY,
    INTEGER,
    TOGGLE,
    ENUM_CHOICE,
    ACTION       // Button — triggers callback on Enter
};

struct SettingItem {
    const char* label;
    SettingType type;
    std::function<int()> getter;
    std::function<void(int)> setter;
    std::function<String(int)> formatter;
    int minVal = 0;
    int maxVal = 1;
    int step = 1;
    // For ENUM_CHOICE: list of option labels
    std::vector<const char*> enumLabels;
    // For ACTION: callback on Enter
    std::function<void()> action;
};

class SettingsScreen : public Screen {
public:
    void onEnter() override;
    bool handleKey(const KeyEvent& event) override;

    void setUserConfig(UserConfig* cfg) { _cfg = cfg; }
    void setFlashStore(FlashStore* fs) { _flash = fs; }
    void setSDStore(SDStore* sd) { _sd = sd; }
    void setRadio(SX1262* radio) { _radio = radio; }
    void setAudio(AudioNotify* audio) { _audio = audio; }
    void setPower(Power* power) { _power = power; }
    void setWiFi(WiFiInterface* wifi) { _wifi = wifi; }
    void setTCPClients(std::vector<TCPClientInterface*>* tcp) { _tcp = tcp; }
    void setRNS(ReticulumManager* rns) { _rns = rns; }
    void setUIManager(UIManager* ui) { _ui = ui; }
    void setIdentityHash(const String& hash) { _identityHash = hash; }
    void setSaveCallback(std::function<bool()> cb) { _saveCallback = cb; }

    const char* title() const override { return "Settings"; }
    void draw(LGFX_TDeck& gfx) override;

private:
    void buildItems();
    void applyAndSave();
    void applyPreset(int presetIdx);
    int detectPreset() const;
    void skipToNextEditable(int dir);
    bool isEditable(int idx) const;

    UserConfig* _cfg = nullptr;
    FlashStore* _flash = nullptr;
    SDStore* _sd = nullptr;
    SX1262* _radio = nullptr;
    AudioNotify* _audio = nullptr;
    Power* _power = nullptr;
    WiFiInterface* _wifi = nullptr;
    std::vector<TCPClientInterface*>* _tcp = nullptr;
    ReticulumManager* _rns = nullptr;
    UIManager* _ui = nullptr;
    String _identityHash;

    std::function<bool()> _saveCallback;

    std::vector<SettingItem> _items;
    int _selectedIdx = 0;
    int _scrollOffset = 0;
    bool _editing = false;
    int _editValue = 0;
};
