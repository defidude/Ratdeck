// =============================================================================
// Ratdeck — Main Entry Point
// LilyGo T-Deck Plus: LovyanGFX Direct UI + microReticulum + LXMF Messaging
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_netif.h>
#include <lvgl.h>

#include "config/BoardConfig.h"
#include "config/Config.h"
#include "hal/Display.h"
#include "hal/TouchInput.h"
#include "hal/Trackball.h"
#include "hal/Keyboard.h"
#include "hal/Power.h"
#if HAS_GPS
#include "hal/GPSManager.h"
#endif
#include "radio/SX1262.h"
#include "input/InputManager.h"
#include "input/HotkeyManager.h"
#include "ui/UIManager.h"
#include "ui/LvTabBar.h"
#include "ui/LvInput.h"
#include "ui/screens/LvBootScreen.h"
#include "ui/screens/LvHomeScreen.h"
#include "ui/screens/LvNodesScreen.h"
#include "ui/screens/LvMessagesScreen.h"
#include "ui/screens/LvMessageView.h"
#include "ui/screens/LvContactsScreen.h"
#include "ui/screens/LvSettingsScreen.h"
#include "ui/screens/LvHelpOverlay.h"
// Map screen removed
#include "ui/screens/LvNameInputScreen.h"
#include "ui/screens/LvTimezoneScreen.h"
#include "ui/screens/LvDataCleanScreen.h"
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "storage/MessageStore.h"
#include "reticulum/ReticulumManager.h"
#include "reticulum/AnnounceManager.h"
#include "reticulum/LXMFManager.h"
#include "reticulum/IdentityManager.h"
#include "transport/LoRaInterface.h"
#include "transport/WiFiInterface.h"
#include <WiFiMulti.h>
#include "transport/TCPClientInterface.h"
#include "transport/AutoInterfaceWrapper.h"
#if HAS_BLE
#include "transport/BLEInterface.h"
#include "transport/BLESideband.h"
#endif
#include "config/UserConfig.h"
#include "audio/AudioNotify.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <atomic>
#include <list>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>

// --- Hardware ---
// Single shared SPI bus for display, LoRa, and SD card
// IMPORTANT: On ESP32-S3, Arduino FSPI=0 maps to SPI2 hardware.
// Do NOT use SPI2_HOST (IDF constant = 1) — Arduino treats index 1 as HSPI/SPI3!
SPIClass sharedSPI(FSPI);

SX1262 radio(&sharedSPI,
    LORA_CS, SPI_SCK, SPI_MOSI, SPI_MISO,
    LORA_RST, LORA_IRQ, LORA_BUSY, LORA_RXEN,
    LORA_HAS_TCXO, LORA_DIO2_AS_RF_SWITCH);

Display display;
TouchInput touch;
Trackball trackball;
Keyboard keyboard;

// --- Subsystems ---
InputManager inputManager;
HotkeyManager hotkeys;
UIManager ui;
FlashStore flash;
SDStore sdStore;
MessageStore messageStore;
ReticulumManager rns;
AnnounceManager* announceManager = nullptr;
RNS::HAnnounceHandler announceHandler;
LXMFManager lxmf;
WiFiInterface* wifiImpl = nullptr;
RNS::Interface wifiIface({RNS::Type::NONE});
std::vector<TCPClientInterface*> tcpClients;
std::list<RNS::Interface> tcpIfaces;  // Must persist — Transport stores references (list: no realloc)
std::list<TCPClientInterface*> retiredTcpClients;
bool tcpReloadRequested = false;
#if HAS_BLE
BLEInterface bleInterface;
BLESideband bleSideband;
#endif
UserConfig userConfig;
Power powerMgr;
AudioNotify audio;
IdentityManager identityMgr;
#if HAS_GPS
GPSManager gps;
#endif

// --- LVGL Screens ---
LvBootScreen lvBootScreen;
LvHomeScreen lvHomeScreen;
LvNodesScreen lvNodesScreen;
LvMessagesScreen lvMessagesScreen;
LvContactsScreen lvContactsScreen;
LvMessageView lvMessageView;
LvSettingsScreen lvSettingsScreen;
LvHelpOverlay lvHelpOverlay;
// LvMapScreen removed
LvNameInputScreen lvNameInputScreen;
LvTimezoneScreen lvTimezoneScreen;
LvDataCleanScreen lvDataCleanScreen;

// Tab-screen mapping (4 tabs) — LVGL versions
LvScreen* lvTabScreens[LvTabBar::TAB_COUNT] = {};

// --- State ---
bool radioOnline = false;
bool bootComplete = false;
bool bootLoopRecovery = false;
bool sdHadExistingData = false;
bool wifiSTAStarted = false;
WiFiMulti wifiMulti;
bool wifiSTAConnected = false;
unsigned long lastAutoAnnounce = 0;

// STA reconnects are scheduled from WiFi events and fired from loop().
std::atomic<bool> wifiNeedsReconnect{false};
std::atomic<unsigned long> wifiReconnectAt{0};
std::atomic<uint8_t> wifiReconnectAttempt{0};
constexpr unsigned long WIFI_BACKOFF_MS[4] = {5000, 15000, 60000, 300000};
constexpr unsigned long WIFI_NETIF_SETTLE_MS = 1500;

static void scheduleWiFiReconnect() {
    uint8_t attempt = wifiReconnectAttempt.load();
    uint8_t idx = attempt < 4 ? attempt : 3;
    unsigned long backoff = WIFI_BACKOFF_MS[idx];
    if (backoff < WIFI_NETIF_SETTLE_MS) backoff = WIFI_NETIF_SETTLE_MS;
    wifiReconnectAt.store(millis() + backoff);
    wifiNeedsReconnect.store(true);
    if (attempt < 4) wifiReconnectAttempt.store(attempt + 1);
}

static void onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            // Our own disconnect() below can emit another disconnect event.
            if (wifiNeedsReconnect.load()) break;
            scheduleWiFiReconnect();
            // Drop the netif and clear stale AP info before the next connect.
            WiFi.disconnect(false, true);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            wifiNeedsReconnect.store(false);
            wifiReconnectAttempt.store(0);
            break;
        default:
            break;
    }
}

unsigned long lastStatusUpdate = 0;
constexpr unsigned long STATUS_UPDATE_MS = 1000;                // 1 Hz status bar update
unsigned long lastHeartbeat = 0;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 5000;
unsigned long loopCycleStart = 0;
unsigned long maxLoopTime = 0;
unsigned long lastLvglTime = 0;
constexpr unsigned long LVGL_INTERVAL_MS = 33;          // ~30 FPS
constexpr unsigned long TCP_GLOBAL_BUDGET_MS = 35;      // Max cumulative TCP time per loop

AutoInterfaceWrapper autoIface;
bool autoIfaceDeferredStart = false;
unsigned long autoIfaceDeferredAt = 0;
unsigned long lastAutoIfaceLinkCheck = 0;

// LXMF diagnostic counters (reset each heartbeat)
static uint32_t diagTcpSkipEvents = 0;

// =============================================================================
// Timezone helper — returns POSIX TZ string for current config
// =============================================================================

static const char* currentPosixTZ() {
    uint8_t idx = userConfig.settings().timezoneIdx;
    if (idx < TIMEZONE_COUNT) return TIMEZONE_TABLE[idx].posixTZ;
    return "EST5EDT,M3.2.0,M11.1.0";  // Fallback
}

// =============================================================================
// Announce with display name (MessagePack-encoded app_data)
// =============================================================================

// LXMF announce app_data:
//   [display_name(bin), stamp_cost(nil|uint), supported_functionality(array)]
// Always emit fixarray(3) so Python LXMF doesn't default auto_compress=True for
// our destinations. stamp_cost=nil means no inbound stamp is required. Empty
// supported_functionality list = we do NOT support SF_COMPRESSION (bz2).
RNS::Bytes encodeAnnounceName(const String& name) {
    size_t nameLen = name.length();
    if (nameLen > 31) nameLen = 31;
    uint8_t buf[5 + 31];
    size_t i = 0;
    buf[i++] = 0x93;                   // fixarray(3)
    buf[i++] = 0xC4;                   // bin 8
    buf[i++] = (uint8_t)nameLen;
    if (nameLen) { memcpy(buf + i, name.c_str(), nameLen); i += nameLen; }
    buf[i++] = 0xC0;                   // stamp_cost = nil (no stamp required)
    buf[i++] = 0x90;                   // empty fixarray (no SF_* supported)
    return RNS::Bytes(buf, i);
}

static bool hasUsableAnnounceTransport() {
    if (!rns.isTransportActive()) return false;
    if (radioOnline) return true;
    if (wifiImpl && wifiImpl->isAPActive() && wifiImpl->getClientCount() > 0) return true;
    for (auto* tcp : tcpClients) {
        if (tcp && tcp->isConnected()) return true;
    }
#if HAS_BLE
    if (bleInterface.isClientConnected()) return true;
#endif
    if (autoIface.isOnline() && autoIface.peerCount() > 0) return true;
    return false;
}

static bool announceWithName(bool silent = false) {
    if (!hasUsableAnnounceTransport()) {
        if (!silent) ui.lvStatusBar().showToast("No active transport", 1500);
        Serial.println("[ANNOUNCE-TX] skipped: no active transport");
        return false;
    }
    RNS::Bytes appData = encodeAnnounceName(userConfig.settings().displayName);
    Serial.printf("[ANNOUNCE-TX] name=\"%s\" appData=%d bytes silent=%s\n",
        userConfig.settings().displayName.c_str(), (int)appData.size(),
        silent ? "yes" : "no");
    rns.announce(appData);
    if (!silent) {
        ui.lvStatusBar().flashAnnounce();
        ui.lvStatusBar().showToast("Announce sent!");
    }
    return true;
}

static void manualAnnounce() {
    if (announceWithName()) Serial.println("[ANNOUNCE] Manual announce sent");
}

// =============================================================================
// TCP client management — stop old clients, create new from config
// =============================================================================

static void drainRetiredTCPClients() {
    for (auto it = retiredTcpClients.begin(); it != retiredTcpClients.end(); ) {
        TCPClientInterface* tcp = *it;
        if (!tcp || tcp->canDestroy()) {
            if (tcp) delete tcp;
            it = retiredTcpClients.erase(it);
        } else {
            ++it;
        }
    }
}

static void retireTCPClient(TCPClientInterface* tcp) {
    if (!tcp) return;
    tcp->stop();
    if (tcp->canDestroy()) {
        delete tcp;
    } else {
        retiredTcpClients.push_back(tcp);
    }
}

static void reloadTCPClients() {
    // Stop and deregister existing clients
    for (auto& iface : tcpIfaces) {
        RNS::Transport::deregister_interface(iface);
    }
    for (auto* tcp : tcpClients) {
        retireTCPClient(tcp);
    }
    tcpClients.clear();
    tcpIfaces.clear();
    drainRetiredTCPClients();

    // Create new clients from current config
    if (WiFi.status() == WL_CONNECTED) {
        for (auto& ep : userConfig.settings().tcpConnections) {
            if (ep.autoConnect && !ep.host.isEmpty()) {
                char name[32];
                snprintf(name, sizeof(name), "TCP.%s", ep.host.c_str());
                auto* tcp = new TCPClientInterface(ep.host.c_str(), ep.port, name);
                tcpIfaces.emplace_back(tcp);
                tcpIfaces.back().mode(RNS::Type::Interface::MODE_FULL);
                RNS::Transport::register_interface(tcpIfaces.back());
                tcp->start();
                tcpClients.push_back(tcp);
                Serial.printf("[TCP] Created client: %s:%d (registered with Transport, mode=FULL)\n", ep.host.c_str(), ep.port);
                Serial.printf("[TCP] Total interfaces registered: %d\n", (int)RNS::Transport::get_interfaces().size());
            }
        }
    }

    if (tcpClients.empty()) {
        Serial.println("[TCP] No active TCP connections");
    }
}

static void requestTCPClientsReload() {
    tcpReloadRequested = true;
}

// =============================================================================
// Hotkey callbacks
// =============================================================================

void onHotkeyHelp() {
    lvHelpOverlay.toggle();
}
void onHotkeyMessages() {
    ui.lvTabBar().setActiveTab(LvTabBar::TAB_MSGS);
    ui.setScreen(&lvMessagesScreen);
}
void onHotkeyNewMsg() {
    bool hasContacts = false;
    if (announceManager) {
        for (const auto& node : announceManager->nodes()) {
            if (node.saved) { hasContacts = true; break; }
        }
    }
    if (hasContacts) {
        ui.lvTabBar().setActiveTab(LvTabBar::TAB_CONTACTS);
        ui.setScreen(&lvContactsScreen);
    } else {
        ui.lvTabBar().setActiveTab(LvTabBar::TAB_NODES);
        ui.setScreen(&lvNodesScreen);
        ui.lvStatusBar().showToast("Pick a peer to message", 1200);
    }
}
void onHotkeySettings() {
    ui.lvTabBar().setActiveTab(LvTabBar::TAB_SETTINGS);
    ui.setScreen(&lvSettingsScreen);
}
void onHotkeyAnnounce() {
    manualAnnounce();
}
void onHotkeyAutoIface() {
    Serial.println("=== AUTOIFACE DUMP ===");
    Serial.printf("Enabled in settings : %s\n",
        userConfig.settings().autoIfaceEnabled ? "YES" : "no");
    Serial.printf("Online              : %s\n", autoIface.isOnline() ? "YES" : "no");
    if (autoIface.isOnline()) {
        Serial.printf("Multicast address   : %s\n", autoIface.multicastAddress().c_str());
        Serial.printf("Link-local          : %s\n", WiFi.localIPv6().toString().c_str());
        Serial.printf("Peers               : %u\n", (unsigned)autoIface.peerCount());
    }
    Serial.printf("Deferred-start armed: %s (elapsed=%lums)\n",
        autoIfaceDeferredStart ? "YES" : "no",
        autoIfaceDeferredStart ? (millis() - autoIfaceDeferredAt) : 0UL);
    Serial.println("======================");
}
void onHotkeyDiag() {
    Serial.println("=== DIAGNOSTIC DUMP ===");
    Serial.printf("Device: Ratdeck T-Deck Plus\n");
    Serial.printf("Identity: %s\n", rns.identityHash().c_str());
    Serial.printf("Transport: %s\n", rns.isTransportActive() ? "ACTIVE" : "OFFLINE");
    Serial.printf("Paths: %d  Links: %d\n", (int)rns.pathCount(), (int)rns.linkCount());
    Serial.printf("Radio: %s\n", radioOnline ? "ONLINE" : "OFFLINE");
    if (radioOnline) {
        Serial.printf("Freq: %lu Hz  SF: %d  BW: %lu  CR: 4/%d  TXP: %d dBm\n",
                      (unsigned long)radio.getFrequency(),
                      radio.getSpreadingFactor(),
                      (unsigned long)radio.getSignalBandwidth(),
                      radio.getCodingRate4(),
                      radio.getTxPower());
        Serial.printf("Preamble: %ld symbols\n", radio.getPreambleLength());
        uint16_t devErr = radio.getDeviceErrors();
        uint8_t status = radio.getStatus();
        Serial.printf("DevErrors: 0x%04X  Status: 0x%02X (mode=%d cmd=%d)\n",
            devErr, status, (status >> 4) & 0x07, (status >> 1) & 0x07);
        if (devErr & 0x40) Serial.println("  *** PLL LOCK FAILED ***");
        Serial.printf("Current RSSI: %d dBm\n", radio.currentRssi());
    }
    Serial.printf("Free heap: %lu bytes  PSRAM: %lu bytes\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
    Serial.printf("Uptime: %lu s\n", millis() / 1000);
    Serial.println("=======================");
}

// RSSI monitor — non-blocking state machine (sampled in main loop)
volatile bool rssiMonitorActive = false;
unsigned long rssiMonitorStart = 0;
unsigned long rssiLastSample = 0;
int rssiMinVal = 0, rssiMaxVal = -200, rssiSampleCount = 0;

void onHotkeyRssiMonitor() {
    if (!radioOnline) { Serial.println("[RSSI] Radio offline"); return; }
    if (rssiMonitorActive) {
        // Already running — cancel
        rssiMonitorActive = false;
        Serial.printf("[RSSI] Stopped: %d samples, min=%d max=%d dBm\n",
                      rssiSampleCount, rssiMinVal, rssiMaxVal);
        return;
    }
    Serial.println("[RSSI] Sampling for 5 seconds (non-blocking)...");
    rssiMonitorActive = true;
    rssiMonitorStart = millis();
    rssiLastSample = 0;
    rssiMinVal = 0;
    rssiMaxVal = -200;
    rssiSampleCount = 0;
}

void onHotkeyRadioTest() {
    Serial.println("[TEST] Sending raw test packet...");
    uint8_t header = 0xA0;
    const char* testPayload = "RATDECK_TEST_1234567890";
    radio.beginPacket();
    radio.write(header);
    radio.write((const uint8_t*)testPayload, strlen(testPayload));
    bool ok = radio.endPacket();
    Serial.printf("[TEST] TX %s (%d bytes)\n", ok ? "OK" : "FAILED", (int)(1 + strlen(testPayload)));
    radio.receive();
}

// =============================================================================
// Helper: render boot screen immediately
// =============================================================================
static void bootRender() {
    // LVGL boot screen calls lv_timer_handler() internally via setProgress()
    // Legacy render kept as fallback
}

// =============================================================================
// Setup — 26-step boot sequence
// =============================================================================

void setup() {
    // Step 1: Power pin — CRITICAL: enables all T-Deck Plus peripherals
    Power::enablePeripherals();

    // Step 2: Serial
    Serial.begin(SERIAL_BAUD);
    delay(100);
    Serial.println();
    Serial.println("=================================");
    Serial.printf("  Ratdeck v%s\n", RATDECK_VERSION_STRING);
    Serial.println("  LilyGo T-Deck Plus");
    Serial.println("=================================");

    esp_reset_reason_t reason = esp_reset_reason();
    const char* reasonStr = "UNKNOWN";
    switch (reason) {
        case ESP_RST_POWERON:   reasonStr = "POWER_ON"; break;
        case ESP_RST_SW:        reasonStr = "SOFTWARE"; break;
        case ESP_RST_PANIC:     reasonStr = "PANIC"; break;
        case ESP_RST_INT_WDT:   reasonStr = "INT_WDT"; break;
        case ESP_RST_TASK_WDT:  reasonStr = "TASK_WDT"; break;
        case ESP_RST_WDT:       reasonStr = "WDT"; break;
        case ESP_RST_BROWNOUT:  reasonStr = "BROWNOUT"; break;
        case ESP_RST_DEEPSLEEP: reasonStr = "DEEP_SLEEP"; break;
        default: break;
    }
    Serial.printf("[BOOT] Reset: %s (%d)\n", reasonStr, (int)reason);
    Serial.printf("[BOOT] Heap: %lu  PSRAM: %lu\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getPsramSize());
    if (!psramFound() || heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < 1024 * 1024) {
        Serial.printf("[BOOT] FATAL: PSRAM unavailable or too fragmented (largest=%lu)\n",
                      (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        while (true) delay(1000);
    }

    // Step 3: Initialize I2C bus (shared by keyboard + touchscreen)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(20);

    // Step 3.5: Initialize shared SPI bus
    sharedSPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    // Deassert all slave CS pins to prevent bus contention
    pinMode(LORA_CS, OUTPUT); digitalWrite(LORA_CS, HIGH);
    pinMode(SD_CS, OUTPUT);   digitalWrite(SD_CS, HIGH);

    // Step 4: Radio + SD init BEFORE display
    // Radio and SD must init while SPIClass exclusively owns SPI2_HOST.
    // LovyanGFX's init() later joins the bus via spi_bus_add_device().
    // This avoids any bus re-init dance that would invalidate device handles.
    Serial.println("[BOOT] Initializing radio...");
    if (radio.begin(LORA_DEFAULT_FREQ)) {
        radio.setSpreadingFactor(LORA_DEFAULT_SF);
        radio.setSignalBandwidth(LORA_DEFAULT_BW);
        radio.setCodingRate4(LORA_DEFAULT_CR);
        radio.setTxPower(LORA_DEFAULT_TX_POWER);
        radio.setPreambleLength(LORA_DEFAULT_PREAMBLE);
        radio.receive();
        radioOnline = true;
        Serial.println("[RADIO] SX1262 online at 915 MHz");
    } else {
        Serial.println("[RADIO] SX1262 not detected!");
    }

    // SD card init (shared SPI, right after radio)
    digitalWrite(LORA_CS, HIGH);
    delay(10);
    if (sdStore.begin(&sharedSPI, SD_CS)) {
        sdHadExistingData = sdStore.hasExistingData();
        sdStore.formatForRatdeck();
        Serial.println("[SD] Card ready");
    } else {
        Serial.println("[SD] Not detected");
    }

    // Verify radio SPI still works after SD init
    if (radioOnline) {
        uint8_t sw_msb = radio.readRegister(0x0740);
        uint8_t sw_lsb = radio.readRegister(0x0741);
        Serial.printf("[BOOT] Radio SPI pre-display: syncword=0x%02X%02X %s\n",
            sw_msb, sw_lsb, (sw_msb == 0xFF && sw_lsb == 0xFF) ? "DEAD!" : "OK");
    }

    // Step 5: Display HAL — LovyanGFX + ST7789V
    // LovyanGFX's Bus_SPI::init() calls spi_bus_initialize() which will
    // return ESP_ERR_INVALID_STATE (bus already owned by SPIClass) and
    // then spi_bus_add_device() to join the existing bus. Both LGFX and
    // SPIClass get valid device handles on the same SPI2_HOST bus.
    display.begin();
    Serial.println("[BOOT] Display initialized (LovyanGFX direct)");

    // Step 5.5: Initialize LVGL display driver
    if (!display.beginLVGL()) {
        display.gfx().fillScreen(TFT_BLACK);
        display.gfx().setTextColor(TFT_RED, TFT_BLACK);
        display.gfx().drawString("LVGL/PSRAM failed", 24, 106);
        display.setBrightness(160);
        while (true) delay(1000);
    }
    Serial.println("[BOOT] LVGL initialized");

    // Verify radio SPI survives display init
    if (radioOnline) {
        uint8_t sw_msb = radio.readRegister(0x0740);
        uint8_t sw_lsb = radio.readRegister(0x0741);
        Serial.printf("[BOOT] Radio SPI post-display: syncword=0x%02X%02X %s\n",
            sw_msb, sw_lsb, (sw_msb == 0xFF && sw_lsb == 0xFF) ? "DEAD!" : "OK");
    }

    // Step 6: UI manager (initializes both legacy and LVGL UI layers)
    ui.begin();
    ui.setBootMode(true);
    ui.setScreen(&lvBootScreen);
    ui.lvStatusBar().setLoRaOnline(radioOnline);
    lvBootScreen.setProgress(0.45f, radioOnline ? "Radio online" : "Radio FAILED");

    // Display::begin() left the backlight at 0 to hide an unpainted
    // framebuffer; the setProgress() above has now flushed the boot screen.
    // powerMgr at step 24 overrides with the user's configured value.
    display.setBrightness(128);

    // Step 7: Touch HAL — GT911 I2C
    touch.begin();
    lvBootScreen.setProgress(0.50f, "Touch ready");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 8: Keyboard HAL — ESP32-C3 I2C
    keyboard.begin();
    lvBootScreen.setProgress(0.52f, "Keyboard ready");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 9: Trackball HAL — GPIO interrupts
    trackball.begin();
    lvBootScreen.setProgress(0.54f, "Trackball ready");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 10: Input manager
    inputManager.begin(&keyboard, &trackball, &touch);
    inputManager.setPowerMgr(&powerMgr);

    // Step 10.5: LVGL input drivers
    LvInput::init(&keyboard, &trackball, &touch);

    lvBootScreen.setProgress(0.55f, "Input ready");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 11: Register hotkeys
    hotkeys.registerHotkey('h', "Help", onHotkeyHelp);
    hotkeys.registerHotkey('m', "Messages", onHotkeyMessages);
    hotkeys.registerHotkey('n', "New Message", onHotkeyNewMsg);
    hotkeys.registerHotkey('s', "Settings", onHotkeySettings);
    hotkeys.registerHotkey('a', "Announce", onHotkeyAnnounce);
    hotkeys.registerHotkey('d', "Diagnostics", onHotkeyDiag);
    hotkeys.registerHotkey('i', "AutoIface dump", onHotkeyAutoIface);
    hotkeys.registerHotkey('t', "Radio Test", onHotkeyRadioTest);
    hotkeys.registerHotkey('r', "RSSI Monitor", onHotkeyRssiMonitor);
    hotkeys.setTabCycleCallback([](int dir) {
        ui.lvTabBar().cycleTab(dir);
        int tab = ui.lvTabBar().getActiveTab();
        if (lvTabScreens[tab]) ui.setScreen(lvTabScreens[tab]);
    });
    lvBootScreen.setProgress(0.58f, "Hotkeys registered");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 12: Mount LittleFS
    lvBootScreen.setProgress(0.60f, "Mounting flash...");
    // (LVGL boot renders via lv_timer_handler in setProgress)
    if (!flash.begin()) {
        Serial.println("[BOOT] Flash init failed; automatic formatting disabled");
        lvBootScreen.setProgress(0.62f, "Flash mount failed");
    } else {
        Serial.println("[BOOT] LittleFS mounted OK");
    }

    // Step 13: Boot loop detection (NVS)
    {
        Preferences prefs;
        if (prefs.begin("ratdeck", false)) {
            int bc = prefs.getInt("bootc", 0);
            prefs.putInt("bootc", bc + 1);
            prefs.end();
            if (bc >= 3) {
                Serial.printf("[BOOT] Boot loop detected (%d failures)\n", bc);
                bootLoopRecovery = true;
            }
        }
    }

    lvBootScreen.setProgress(0.64f, "Loading config...");
    userConfig.load(sdStore, flash);
    inputManager.setTrackballSpeed(userConfig.settings().trackballSpeed);

    lvBootScreen.setProgress(0.65f, "Starting Reticulum...");
    // (LVGL boot renders via lv_timer_handler in setProgress)
    rns.setSDStore(&sdStore);
    if (rns.begin(&radio, &flash, userConfig.settings().loraEnabled)) {
        Serial.printf("[BOOT] Identity: %s\n", rns.identityHash().c_str());
        lvBootScreen.setProgress(0.72f, "Reticulum active");
    } else {
        Serial.println("[BOOT] Reticulum init failed!");
        lvBootScreen.setProgress(0.72f, "RNS: FAILED");
    }
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 15.5: Identity manager
    identityMgr.begin(&flash, &sdStore);

    // Step 16: Message store
    lvBootScreen.setProgress(0.72f, "Starting messaging...");
    // (LVGL boot renders via lv_timer_handler in setProgress)
    messageStore.begin(&flash, &sdStore, userConfig.settings().sdStorageEnabled);

    // Step 17: LXMF init
    lxmf.begin(&rns, &messageStore);
    lxmf.setMessageCallback([](const LXMFMessage& msg) {
        Serial.printf("[LXMF] Message from %s\n", msg.sourceHash.toHex().substr(0, 8).c_str());
        ui.lvTabBar().setUnreadCount(LvTabBar::TAB_MSGS, lxmf.unreadCount());
        audio.requestMessage();
    });
    // Pre-cache unread counts so first tab switch to Messages is instant
    lxmf.unreadCount();
    lvBootScreen.setProgress(0.75f, "LXMF ready");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 18: Announce manager
    lvBootScreen.setProgress(0.78f, "Loading contacts...");
    // (LVGL boot renders via lv_timer_handler in setProgress)
    // Filter to lxmf.delivery so we don't capture every aspect (lxmf.propagation,
    // nomadnetwork.node, etc.) from the same peer as separate "doubled" entries.
    announceManager = new AnnounceManager("lxmf.delivery");
    announceManager->setStorage(&sdStore, &flash);
    announceManager->setLocalDestHash(rns.destination().hash());
    if (rns.loraInterface()) announceManager->setLoRaInterface(rns.loraInterface());
    announceManager->loadContacts();
    announceManager->loadNameCache();
    announceHandler = RNS::HAnnounceHandler(announceManager);
    RNS::Transport::register_announce_handler(announceHandler);

    // No default TCP hub.  Users opt in via Settings → TCP Server →
    // "Ratspeak Hub" (seeds rns.ratspeak.org) or "Custom" (host/port).

    // Sync display name between active identity slot and config.
    // The identity slot is the source of truth for the name.
    {
        String slotName;
        if (identityMgr.syncNameFromActive(slotName)) {
            if (!slotName.isEmpty()) {
                // Slot has a name — use it (overrides any stale config value)
                if (userConfig.settings().displayName != slotName) {
                    Serial.printf("[BOOT] Name from identity slot: '%s'\n", slotName.c_str());
                    userConfig.settings().displayName = slotName;
                    userConfig.save(sdStore, flash);
                }
            } else if (!userConfig.settings().displayName.isEmpty()) {
                // Slot has no name but config does — seed the slot (first boot migration)
                identityMgr.setDisplayName(identityMgr.activeIndex(),
                    userConfig.settings().displayName);
                Serial.printf("[BOOT] Seeded identity slot name: '%s'\n",
                    userConfig.settings().displayName.c_str());
            }
        }
    }

    // Step 20: Boot loop recovery
    if (bootLoopRecovery) {
        userConfig.settings().wifiMode = RAT_WIFI_OFF;
        Serial.println("[BOOT] WiFi forced OFF (boot loop recovery)");
    }
    lvBootScreen.setProgress(0.83f, "Config loaded");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 21: Apply radio config
    if (radioOnline && userConfig.settings().loraEnabled) {
        auto& s = userConfig.settings();
        radio.setFrequency(s.loraFrequency);
        radio.setSpreadingFactor(s.loraSF);
        radio.setSignalBandwidth(s.loraBW);
        radio.setCodingRate4(s.loraCR);
        radio.setTxPower(s.loraTxPower);
        radio.setPreambleLength(s.loraPreamble);
        radio.receive();
        Serial.printf("[BOOT] Radio: %lu Hz, SF%d, BW%lu, CR4/%d, %d dBm, pre=%ld\n",
                      (unsigned long)s.loraFrequency, s.loraSF,
                      (unsigned long)s.loraBW, s.loraCR, s.loraTxPower, s.loraPreamble);
        ui.lvStatusBar().setLoRaOnline(true);
    } else if (radioOnline) {
        radio.sleep();
        ui.lvStatusBar().setLoRaOnline(false);
        Serial.println("[BOOT] LoRa disabled by config");
    }
    lvBootScreen.setProgress(0.84f, "Radio configured");
    // (LVGL boot renders via lv_timer_handler in setProgress)

    // Step 22: WiFi start
    RatWiFiMode wifiMode = userConfig.settings().wifiMode;
    ui.lvStatusBar().setWiFiEnabled(wifiMode != RAT_WIFI_OFF);
    if (wifiMode == RAT_WIFI_AP) {
        lvBootScreen.setProgress(0.87f, "Starting WiFi AP...");
        // (LVGL boot renders via lv_timer_handler in setProgress)
        wifiImpl = new WiFiInterface("WiFi.AP");
        if (!userConfig.settings().wifiAPSSID.isEmpty()) {
            wifiImpl->setAPCredentials(
                userConfig.settings().wifiAPSSID.c_str(),
                userConfig.settings().wifiAPPassword.c_str());
        }
        wifiIface = wifiImpl;
        wifiIface.mode(RNS::Type::Interface::MODE_GATEWAY);
        RNS::Transport::register_interface(wifiIface);
        wifiImpl->start();
        ui.lvStatusBar().setWiFiActive(true);
    } else if (wifiMode == RAT_WIFI_STA) {
        lvBootScreen.setProgress(0.87f, "WiFi STA starting...");
        auto& settings = userConfig.settings();
        auto& nets = settings.wifiSTANetworks;
        size_t selectedSlot = settings.wifiSTASelected < WIFI_STA_MAX_NETWORKS ? settings.wifiSTASelected : 0;
        int registered = 0;
        if (selectedSlot < nets.size() && !nets[selectedSlot].ssid.isEmpty()) {
            const auto& n = nets[selectedSlot];
            wifiMulti.addAP(n.ssid.c_str(), n.password.c_str());
            registered++;
            Serial.printf("[WIFI] STA: using profile %u (%s)\n",
                          (unsigned)(selectedSlot + 1), n.ssid.c_str());
        } else {
            Serial.printf("[WIFI] STA: selected profile %u is empty\n",
                          (unsigned)(selectedSlot + 1));
        }
        // WiFi is enabled but not yet connected — indicator will be yellow
        if (registered > 0) {
            WiFi.mode(WIFI_STA);
            WiFi.onEvent(onWiFiEvent);
            // AutoInterface needs an IPv6 link-local address.  Must be enabled
            // BEFORE WiFi.begin() so SLAAC starts on STA association.
            if (userConfig.settings().autoIfaceEnabled) {
                WiFi.enableIpV6();
                Serial.println("[WIFI] IPv6 enabled (AutoInterface ON)");
            }
            uint8_t initialStatus = wifiMulti.run(5000);
            wifiSTAStarted = true;
            if (initialStatus != WL_CONNECTED && WiFi.status() != WL_CONNECTED &&
                !wifiNeedsReconnect.load()) {
                scheduleWiFiReconnect();
            }
            Serial.printf("[WIFI] STA: %d selected profile registered\n", registered);
        }
    } else {
        lvBootScreen.setProgress(0.87f, "WiFi disabled");
        // (LVGL boot renders via lv_timer_handler in setProgress)
    }

    // Step 23: BLE stays disabled in default builds.
    lvBootScreen.setProgress(0.90f, "Links ready");
    // (LVGL boot renders via lv_timer_handler in setProgress)
#if HAS_BLE
    ui.lvStatusBar().setBLEEnabled(userConfig.settings().bleEnabled);
    if (userConfig.settings().bleEnabled) {
        bleInterface.setSideband(&bleSideband);

        if (bleInterface.start()) {
            static RNS::Interface bleIface(&bleInterface);
            bleIface.mode(RNS::Type::Interface::MODE_GATEWAY);
            RNS::Transport::register_interface(bleIface);

            bleSideband.begin(bleInterface.getServer());
            bleSideband.setPacketCallback([](const uint8_t* data, size_t len) {
                RNS::Bytes pkt(data, len);
                bleInterface.injectIncoming(pkt);
            });

            ui.lvStatusBar().setBLEActive(true);
            Serial.println("[BLE] Transport + Sideband ready");
        }
    } else {
        Serial.println("[BLE] Disabled by config");
    }
#else
    ui.lvStatusBar().setBLEEnabled(false);
    ui.lvStatusBar().setBLEActive(false);
    Serial.println("[BLE] Disabled in default firmware build");
#endif

    // Step 24: Power manager
    lvBootScreen.setProgress(0.92f, "Power manager...");
    // (LVGL boot renders via lv_timer_handler in setProgress)
    powerMgr.begin();
    powerMgr.setDimTimeout(userConfig.settings().screenDimTimeout);
    powerMgr.setOffTimeout(userConfig.settings().screenOffTimeout);
    powerMgr.setBrightness(userConfig.settings().brightness);
    powerMgr.setKbBrightness(userConfig.settings().keyboardBrightness);
    powerMgr.setKbAutoOn(userConfig.settings().keyboardAutoOn);
    powerMgr.setKbAutoOff(userConfig.settings().keyboardAutoOff);

    // Step 24.5: GPS init
#if HAS_GPS
    if (userConfig.settings().gpsTimeEnabled) {
        lvBootScreen.setProgress(0.93f, "Starting GPS...");
        gps.setPosixTZ(currentPosixTZ());
        gps.setLocationEnabled(userConfig.settings().gpsLocationEnabled);
        gps.begin();
        Serial.println("[BOOT] GPS UART started (MIA-M10Q)");
    }
#endif

    // Step 25: Audio init
    lvBootScreen.setProgress(0.94f, "Audio...");
    // (LVGL boot renders via lv_timer_handler in setProgress)
    audio.setEnabled(userConfig.settings().audioEnabled);
    audio.setVolume(userConfig.settings().audioVolume);
    audio.begin();

    // Boot complete — transition to Home screen
    // Yield to LVGL instead of blocking delay
    lvBootScreen.setProgress(0.98f, "Ready");
    for (int i = 0; i < 6; i++) { lv_timer_handler(); delay(1); }
    lvBootScreen.setProgress(1.0f, "Ready");
    audio.playBoot();

    bootComplete = true;

    // Keep LVGL responsive during blocking radio operations (if screen is on)
    // Re-entrancy guard prevents nested lv_timer_handler() calls
    radio.setYieldCallback([]() {
        static bool inYield = false;
        if (inYield) return;
        inYield = true;
        if (powerMgr.isScreenOn()) {
            lv_timer_handler();
        }
        inYield = false;
    });

    // Wire up LVGL screen dependencies
    lvHomeScreen.setReticulumManager(&rns);
    lvHomeScreen.setRadio(&radio);
    lvHomeScreen.setUserConfig(&userConfig);
    lvHomeScreen.setLXMFManager(&lxmf);
    lvHomeScreen.setAnnounceManager(announceManager);
    lvHomeScreen.setRadioOnline(radioOnline);
    lvHomeScreen.setTCPClients(&tcpClients);
    lvHomeScreen.setAnnounceCallback([]() {
        manualAnnounce();
        Serial.println("[HOME] Announce triggered via Enter");
    });
    lvHomeScreen.setAudioToggleCallback([]() {
        userConfig.settings().audioEnabled = !userConfig.settings().audioEnabled;
        audio.setEnabled(userConfig.settings().audioEnabled);
        bool ok = userConfig.save(sdStore, flash);
        ui.lvStatusBar().showToast(userConfig.settings().audioEnabled ? "Audio ON" : "Audio OFF", 1000);
        Serial.printf("[AUDIO] Notifications %s (save %s)\n",
                      userConfig.settings().audioEnabled ? "ON" : "OFF",
                      ok ? "OK" : "FAILED");
    });
    lvHomeScreen.setLoraToggleCallback([]() {
        auto& s = userConfig.settings();
        s.loraEnabled = !s.loraEnabled;
        bool ok = userConfig.save(sdStore, flash);
        ui.lvStatusBar().showToast(
            ok ? "LoRa saved; reboot to apply" : "Save failed",
            ok ? 3000 : 2000);
        Serial.printf("[LORA] Saved %s (save %s, reboot required)\n",
                      s.loraEnabled ? "ON" : "OFF",
                      ok ? "OK" : "FAILED");
    });
    lvHomeScreen.setTCPToggleCallback([]() {
        auto& s = userConfig.settings();
        bool enabled = false;
        bool hasSavedRelay = false;
        for (const auto& ep : s.tcpConnections) {
            if (!ep.host.isEmpty()) hasSavedRelay = true;
            if (!ep.host.isEmpty() && ep.autoConnect) { enabled = true; break; }
        }
        if (enabled) {
            for (auto& ep : s.tcpConnections) ep.autoConnect = false;
        } else if (hasSavedRelay) {
            for (auto& ep : s.tcpConnections) {
                if (!ep.host.isEmpty()) ep.autoConnect = true;
            }
        } else {
            s.tcpConnections.clear();
            TCPEndpoint ep;
            ep.host = "rns.ratspeak.org";
            ep.port = TCP_DEFAULT_PORT;
            ep.autoConnect = true;
            s.tcpConnections.push_back(ep);
        }
        bool ok = userConfig.save(sdStore, flash);
        ui.lvStatusBar().showToast(
            ok ? "TCP relay saved; reboot to apply" : "Save failed",
            ok ? 3000 : 2000);
        Serial.printf("[TCP] Saved relay %s (save %s, reboot required)\n",
                      enabled ? "OFF" : "ON",
                      ok ? "OK" : "FAILED");
    });
    lvHomeScreen.setWiFiToggleCallback([]() {
        auto& s = userConfig.settings();
        if (s.wifiMode == RAT_WIFI_OFF) {
            RatWiFiMode restoreMode = s.wifiRestoreMode == RAT_WIFI_OFF ? RAT_WIFI_STA : s.wifiRestoreMode;
            if (restoreMode == RAT_WIFI_STA) {
                size_t slot = s.wifiSTASelected < s.wifiSTANetworks.size() ? s.wifiSTASelected : 0;
                if (slot >= s.wifiSTANetworks.size() || s.wifiSTANetworks[slot].ssid.isEmpty()) {
                    ui.lvStatusBar().showToast("Add WiFi in Settings", 2000);
                    return;
                }
            } else if (restoreMode != RAT_WIFI_AP) {
                ui.lvStatusBar().showToast("Add WiFi in Settings", 2000);
                return;
            }
            s.wifiMode = restoreMode;
        } else {
            s.wifiRestoreMode = s.wifiMode;
            s.wifiMode = RAT_WIFI_OFF;
        }
        bool ok = userConfig.save(sdStore, flash);
        ui.lvStatusBar().showToast(
            ok ? "WiFi saved; reboot to apply" : "Save failed",
            ok ? 3000 : 2000);
        Serial.printf("[WIFI] Saved mode %d (save %s, reboot required)\n",
                      (int)s.wifiMode, ok ? "OK" : "FAILED");
    });
#if HAS_GPS
    lvHomeScreen.setGPSToggleCallback([]() {
        auto& s = userConfig.settings();
        bool oldTime = s.gpsTimeEnabled;
        s.gpsTimeEnabled = !s.gpsTimeEnabled;
        bool ok = userConfig.save(sdStore, flash);
        if (!ok) {
            s.gpsTimeEnabled = oldTime;
            ui.lvStatusBar().showToast("Save failed", 2000);
            Serial.println("[GPS] Toggle save failed");
            return;
        }
        if (s.gpsTimeEnabled) {
            gps.setPosixTZ(currentPosixTZ());
            gps.setLocationEnabled(s.gpsLocationEnabled);
            gps.begin();
            ui.lvStatusBar().showToast("GPS ON", 1000);
            Serial.println("[GPS] Enabled via Home");
        } else {
            gps.stop();
            ui.lvStatusBar().setGPSFix(false);
            ui.lvStatusBar().showToast("GPS OFF", 1000);
            Serial.println("[GPS] Disabled via Home");
        }
    });
#else
    lvHomeScreen.setGPSToggleCallback([]() {
        ui.lvStatusBar().showToast("GPS unavailable", 1500);
    });
#endif
    lvHomeScreen.setPeersCallback([]() {
        ui.lvTabBar().setActiveTab(LvTabBar::TAB_NODES);
        ui.setScreen(&lvNodesScreen);
    });

    lvContactsScreen.setAnnounceManager(announceManager);
    lvContactsScreen.setUIManager(&ui);
    lvContactsScreen.setNodeSelectedCallback([](const std::string& peerHex) {
        lvMessageView.setPeerHex(peerHex);
        ui.lvTabBar().setActiveTab(LvTabBar::TAB_MSGS);
        ui.setScreen(&lvMessageView);
    });

    lvNodesScreen.setAnnounceManager(announceManager);
    lvNodesScreen.setUIManager(&ui);
    lvNodesScreen.setUserConfig(&userConfig);
    lvNodesScreen.setNodeSelectedCallback([](const std::string& peerHex) {
        lvMessageView.setPeerHex(peerHex);
        ui.lvTabBar().setActiveTab(LvTabBar::TAB_MSGS);
        ui.setScreen(&lvMessageView);
    });

    lvMessagesScreen.setLXMFManager(&lxmf);
    lvMessagesScreen.setAnnounceManager(announceManager);
    lvMessagesScreen.setUIManager(&ui);
    lvMessagesScreen.setOpenCallback([](const std::string& peerHex) {
        lvMessageView.setPeerHex(peerHex);
        ui.setScreen(&lvMessageView);
    });

    lvMessageView.setLXMFManager(&lxmf);
    lvMessageView.setAnnounceManager(announceManager);
    lvMessageView.setUIManager(&ui);
    lvMessageView.setBackCallback([]() {
        ui.setScreen(&lvMessagesScreen);
    });

    lvSettingsScreen.setUserConfig(&userConfig);
    lvSettingsScreen.setFlashStore(&flash);
    lvSettingsScreen.setSDStore(&sdStore);
    lvSettingsScreen.setRadio(&radio);
    lvSettingsScreen.setAudio(&audio);
    lvSettingsScreen.setPower(&powerMgr);
    lvSettingsScreen.setWiFi(wifiImpl);
    lvSettingsScreen.setTCPClients(&tcpClients);
    lvSettingsScreen.setRNS(&rns);
    lvSettingsScreen.setIdentityManager(&identityMgr);
    lvSettingsScreen.setUIManager(&ui);
    lvSettingsScreen.setIdentityHash(rns.destinationHashStr());
    lvSettingsScreen.setDestinationHash(rns.destinationHashHex());
    lvSettingsScreen.setSaveCallback([]() -> bool {
        inputManager.setTrackballSpeed(userConfig.settings().trackballSpeed);
        bool ok = userConfig.save(sdStore, flash);
        Serial.printf("[CONFIG] Save %s\n", ok ? "OK" : "FAILED");
        return ok;
    });
    lvSettingsScreen.setTCPChangeCallback([]() {
        Serial.println("[TCP] Settings changed, scheduling reload...");
        requestTCPClientsReload();
    });
#if HAS_GPS
    lvSettingsScreen.setGPSChangeCallback([](bool timeEnabled) {
        if (timeEnabled) {
            gps.setPosixTZ(currentPosixTZ());
            gps.setLocationEnabled(userConfig.settings().gpsLocationEnabled);
            gps.begin();
            Serial.println("[GPS] Time enabled via settings");
        } else {
            gps.stop();
            ui.lvStatusBar().setGPSFix(false);
            Serial.println("[GPS] Disabled via settings");
        }
    });
#endif

    // LVGL help overlay
    lvHelpOverlay.create();

    // Tab bar callbacks — LVGL
    lvTabScreens[LvTabBar::TAB_HOME]     = &lvHomeScreen;
    lvTabScreens[LvTabBar::TAB_CONTACTS] = &lvContactsScreen;
    lvTabScreens[LvTabBar::TAB_MSGS]     = &lvMessagesScreen;
    lvTabScreens[LvTabBar::TAB_NODES]    = &lvNodesScreen;
    lvTabScreens[LvTabBar::TAB_SETTINGS] = &lvSettingsScreen;

    ui.lvTabBar().setTabCallback([](int tab) {
        if (lvTabScreens[tab]) ui.setScreen(lvTabScreens[tab]);
    });

    // Data clean screen (first boot only — when SD has old data)
    lvDataCleanScreen.setDoneCallback([](bool wipe) {
        if (wipe) {
            Serial.println("[BOOT] User chose to wipe old data");
            lvDataCleanScreen.showStatus("Clearing old data...");
            sdStore.wipeRatdeck();
            if (announceManager) announceManager->clearAll();
            Serial.println("[BOOT] Old data cleared");
            lvDataCleanScreen.showStatus("Done! Rebooting...");
            delay(1500);
            ESP.restart();
        } else {
            Serial.println("[BOOT] User chose to keep old data");
            userConfig.settings().sdStorageEnabled = true;
            userConfig.save(sdStore, flash);
            lvDataCleanScreen.showStatus("SD storage enabled. Rebooting...");
            delay(1500);
            ESP.restart();
        }
    });

    // --- Boot flow helpers ---
    // Transition to home screen (shared by name input, timezone, and normal boot)
    auto goHome = []() {
        ui.setBootMode(false);
        ui.setScreen(&lvHomeScreen);
        ui.lvTabBar().setActiveTab(LvTabBar::TAB_HOME);
        Serial.println("[BOOT] Home ready; first auto announce after configured interval");
    };

    // Show timezone screen, then go home
    auto showTimezone = [goHome]() {
        if (!userConfig.settings().timezoneSet) {
            lvTimezoneScreen.setSelectedIndex(userConfig.settings().timezoneIdx);
            ui.setScreen(&lvTimezoneScreen);
            Serial.println("[BOOT] Showing timezone selection");
        } else {
            goHome();
        }
    };

    // Timezone screen done callback
    lvTimezoneScreen.setDoneCallback([goHome](int tzIdx) {
        userConfig.settings().timezoneIdx = (uint8_t)tzIdx;
        userConfig.settings().timezoneSet = true;
        bool saved = userConfig.save(sdStore, flash);
        if (!saved) {
            Serial.println("[BOOT] Timezone save failed; staying in setup");
            ui.lvStatusBar().showToast("Save failed; storage unavailable", 3000);
            return;
        }
        Serial.printf("[BOOT] Timezone set: %s (%s)\n",
            TIMEZONE_TABLE[tzIdx].label, TIMEZONE_TABLE[tzIdx].posixTZ);
        // Apply timezone immediately
        const char* tz = TIMEZONE_TABLE[tzIdx].posixTZ;
        setenv("TZ", tz, 1);
        tzset();
#if HAS_GPS
        if (userConfig.settings().gpsTimeEnabled) {
            gps.setPosixTZ(tz);
        }
#endif
        // Warn if timezone suggests a different radio region
        uint8_t tzRegion = TIMEZONE_TABLE[tzIdx].radioRegion;
        if (tzRegion != userConfig.settings().radioRegion) {
            char msg[64];
            snprintf(msg, sizeof(msg), "TZ suggests %s region", REGION_LABELS[tzRegion]);
            ui.lvStatusBar().showToast(msg, 3000);
            Serial.printf("[REGION] Timezone suggests %s, current is %s\n",
                REGION_LABELS[tzRegion], REGION_LABELS[userConfig.settings().radioRegion]);
        }
        goHome();
    });

    // Name input screen (first boot only — when no display name is set)
    lvNameInputScreen.setDoneCallback([showTimezone](const String& name) {
        String finalName = name;
        if (finalName.isEmpty()) {
            // Auto-generate: Ratspeak.org-xxx (first 3 chars of LXMF dest hash)
            String dh = rns.destinationHashHex();
            finalName = "Ratspeak.org-" + dh.substring(0, 3);
        }
        userConfig.settings().displayName = finalName;
        bool saved = userConfig.save(sdStore, flash);
        if (!saved) {
            Serial.println("[BOOT] Display name save failed; staying in setup");
            ui.lvStatusBar().showToast("Save failed; storage unavailable", 3000);
            return;
        }
        // Also save to active identity slot
        if (identityMgr.activeIndex() >= 0) {
            identityMgr.setDisplayName(identityMgr.activeIndex(), finalName);
        }
        Serial.printf("[BOOT] Display name set: '%s'\n", finalName.c_str());

        // Next step: timezone selection (or home if already set)
        showTimezone();
    });

    if (sdHadExistingData && !userConfig.settings().sdStorageEnabled) {
        ui.setScreen(&lvDataCleanScreen);
        Serial.println("[BOOT] Existing SD data found; waiting for user choice");
    } else if (userConfig.settings().displayName.isEmpty()) {
        // First boot — go to name input
        ui.setScreen(&lvNameInputScreen);
        Serial.println("[BOOT] Showing name input screen");
    } else if (!userConfig.settings().timezoneSet) {
        // Name set but timezone not — show timezone picker
        lvTimezoneScreen.setSelectedIndex(userConfig.settings().timezoneIdx);
        ui.setScreen(&lvTimezoneScreen);
        Serial.println("[BOOT] Showing timezone selection (name already set)");
    } else {
        // Everything configured — go straight to home
        goHome();
    }

    // Clear boot loop counter — we survived!
    {
        Preferences prefs;
        if (prefs.begin("ratdeck", false)) {
            prefs.putInt("bootc", 0);
            prefs.end();
        }
    }

    if (userConfig.settings().keyboardAutoOn) {
        // We are in ACTIVE power state here, switch keyboard backlight ON
        keyboard.backlightOn();
    }

    Serial.println("[BOOT] Ratdeck ready");
    Serial.printf("[BOOT] Summary: radio=%s flash=%s sd=%s\n",
                  radioOnline ? "ONLINE" : "OFFLINE",
                  flash.isReady() ? "OK" : "FAIL",
                  sdStore.isReady() ? "OK" : "FAIL");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    // 1. Input polling
    inputManager.update();
    if (inputManager.hadStrongActivity()) {
        powerMgr.activity();       // Keyboard/touch: wake from any state
    } else if (inputManager.hadActivity()) {
        powerMgr.weakActivity();   // Trackball: wake from dim only
    }

    // 2. Long-press dispatch — screen blanking is the default if no screen consumes it
    if (inputManager.hadLongPress()) {
        if (!ui.handleLongPress()) {
            powerMgr.forceScreenOff();
        }
    }

    // 3. Key event dispatch
    if (inputManager.hasKeyEvent()) {
        const KeyEvent& evt = inputManager.getKeyEvent();

        // Help overlay intercepts all keys when visible
        if (lvHelpOverlay.isVisible()) {
            lvHelpOverlay.handleKey(evt);
        }
        else {
            // Screen-local input owns the keyboard. This keeps message and
            // settings text entry from being preempted by global shortcuts.
            bool consumed = ui.handleKey(evt);
            if (!consumed) {
                bool hotkeyAllowed = !ui.isBootMode() || (evt.ctrl && evt.character == 'h');
                bool hotkeyConsumed = hotkeyAllowed && hotkeys.process(evt);
                if (!hotkeyConsumed) {

                    // Feed to LVGL input system only if the screen didn't consume it
                    LvInput::feedKey(evt);

                    // Tab cycling: ,=left /=right OR trackball left/right (only if screen didn't consume)
                    if (!evt.ctrl && !ui.isBootMode()) {
                        bool tabLeft  = (evt.character == ',') || evt.left;
                        bool tabRight = (evt.character == '/') || evt.right;
                        if (tabLeft) {
                            ui.lvTabBar().cycleTab(-1);
                            int tab = ui.lvTabBar().getActiveTab();
                            if (lvTabScreens[tab]) ui.setScreen(lvTabScreens[tab]);
                        }
                        if (tabRight) {
                            ui.lvTabBar().cycleTab(1);
                            int tab = ui.lvTabBar().getActiveTab();
                            if (lvTabScreens[tab]) ui.setScreen(lvTabScreens[tab]);
                        }
                    }
                }
            }
        }
    }

    // 3. LVGL timer handler — 30 FPS active, 5 FPS dimmed.
    // Bypass the throttle on input activity so a keypress/scroll renders this
    // iteration instead of waiting up to a full frame interval.
    {
        unsigned long now = millis();
        unsigned long lvglInterval = powerMgr.isDimmed() ? 200 : LVGL_INTERVAL_MS;
        bool inputBurst = inputManager.hadActivity();
        if (powerMgr.isScreenOn() && (inputBurst || now - lastLvglTime >= lvglInterval)) {
            lastLvglTime = now;
            lv_timer_handler();
        }
    }

    // 4. Reticulum loop (radio RX via LoRaInterface) — throttle to ~100Hz
    unsigned long rnsDuration = 0;
    {
        static unsigned long lastRNS = 0;
        unsigned long now = millis();
        if (now - lastRNS >= 10) {
            lastRNS = now;
            unsigned long rnsStart = millis();
            rns.loop();
            rnsDuration = millis() - rnsStart;
        }
    }

    // 4.5 Keep LVGL responsive after heavy RNS processing (announce floods)
    if (rnsDuration > LVGL_INTERVAL_MS && powerMgr.isScreenOn()) {
        lv_timer_handler();
    }

    // 5. Auto-announce every 30-360 minutes from boot. Manual announces do
    // not reset this schedule.
    const unsigned long announceInterval = (unsigned long)userConfig.settings().announceInterval * 60000; // m -> ms
    if (bootComplete && millis() - lastAutoAnnounce >= announceInterval) {
        lastAutoAnnounce = millis();
        if (rns.loraInterface() && rns.loraInterface()->airtimeUtilization() > LoRaInterface::AIRTIME_THROTTLE) {
            Serial.println("[AUTO] Skipping announce: LoRa airtime > 25%");
        } else {
            announceWithName(!powerMgr.isScreenOn());
            Serial.println("[AUTO] Periodic announce");
        }
    }

    // 6. LXMF outgoing queue + announce manager deferred saves
    lxmf.loop();
    if (announceManager) announceManager->loop();
    audio.loop();

    // 7. WiFi STA connection handler
    if (wifiSTAStarted) {
        if (wifiNeedsReconnect.load() && WiFi.status() != WL_CONNECTED &&
            (long)(millis() - wifiReconnectAt.load()) >= 0) {
            wifiNeedsReconnect.store(false);
            uint8_t attempt = wifiReconnectAttempt.load();
            Serial.printf("[WIFI] Reconnect attempt #%u\n", (unsigned)attempt);
            uint8_t result = wifiMulti.run(2000);
            if (result != WL_CONNECTED && WiFi.status() != WL_CONNECTED &&
                !wifiNeedsReconnect.load()) {
                scheduleWiFiReconnect();
            }
        }
        bool connected = (WiFi.status() == WL_CONNECTED);
        if (connected && !wifiSTAConnected) {
            wifiSTAConnected = true;
            ui.lvStatusBar().setWiFiActive(true);
            Serial.printf("[WIFI] STA connected: %s\n", WiFi.localIP().toString().c_str());

            // NTP time sync (DST-aware POSIX TZ string)
            {
                const char* tz = currentPosixTZ();
                configTzTime(tz, "pool.ntp.org", "time.nist.gov");
                Serial.printf("[NTP] Time sync started (TZ=%s)\n", tz);
            }

            // Recreate TCP clients on every WiFi connect (old clients may have stale sockets)
            reloadTCPClients();
            // Arm AutoInterface deferred-start; SLAAC needs ~1.5–10s to assign
            // a link-local IPv6 address, so we don't start the interface here.
            // Trigger link-local creation AFTER association (calling
            // esp_netif_create_ip6_linklocal pre-association is a no-op on
            // some Arduino-ESP32 versions).
            if (userConfig.settings().autoIfaceEnabled) {
                WiFi.enableIpV6();
                autoIfaceDeferredStart = true;
                autoIfaceDeferredAt = millis();
            }
        } else if (!connected && wifiSTAConnected) {
            wifiSTAConnected = false;
            ui.lvStatusBar().setWiFiActive(false);
            ui.lvStatusBar().setTCPConnected(false);
            // Stop and deregister TCP clients cleanly
            for (auto& iface : tcpIfaces) {
                RNS::Transport::deregister_interface(iface);
            }
            for (auto* tcp : tcpClients) {
                retireTCPClient(tcp);
            }
            tcpClients.clear();
            tcpIfaces.clear();
            Serial.println("[WIFI] STA disconnected, TCP interfaces deregistered");
            autoIface.stop();
            autoIfaceDeferredStart = false;
        }
    }

    // 7.6. AutoInterface deferred start — fire once SLAAC assigns a link-local
    // IPv6 address.  Arduino's IPv6Address::toString returns the expanded
    // form ("0000:0000:..." for unset; "fe80:0000:..." once SLAAC completes),
    // so check the prefix bytes directly: link-local is fe80::/10.
    if (autoIfaceDeferredStart) {
        unsigned long elapsed = millis() - autoIfaceDeferredAt;
        if (elapsed >= 1500) {
            IPv6Address ll = WiFi.localIPv6();
            bool isLinkLocal = (ll[0] == 0xfe) && ((ll[1] & 0xc0) == 0x80);
            if (isLinkLocal) {
                autoIfaceDeferredStart = false;
                esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                uint32_t scope = sta ? esp_netif_get_netif_impl_index(sta) : 1;
                autoIface.start(
                    userConfig.settings().autoIfaceGroupId.c_str(),
                    userConfig.settings().autoIfaceMaxPeers,
                    ll.toString(),
                    scope);
            } else if (elapsed >= 10000) {
                autoIfaceDeferredStart = false;
                Serial.println("[AUTOIFACE] SLAAC timeout — no link-local after 10s");
            }
        }
    }

    // 7.7. AutoInterface link-local rotation watch — covers SLAAC privacy
    // address rotation while STA stays associated.  notify_link_change()
    // is idempotent in the library, so polling here is cheap (string
    // compare, no socket churn) and only does real work on actual change.
    if (autoIface.isOnline() && wifiSTAConnected &&
        millis() - lastAutoIfaceLinkCheck >= 2000) {
        lastAutoIfaceLinkCheck = millis();
        IPv6Address ll = WiFi.localIPv6();
        bool isLinkLocal = (ll[0] == 0xfe) && ((ll[1] & 0xc0) == 0x80);
        if (isLinkLocal) {
            esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            uint32_t scope = sta ? esp_netif_get_netif_impl_index(sta) : 1;
            autoIface.notifyLinkChange(ll.toString(), scope);
        }
    }

    // 7.8. Deferred TCP reload from Settings. Avoid tearing down/recreating
    // Transport interfaces inside the LVGL key event path.
    if (tcpReloadRequested) {
        tcpReloadRequested = false;
        Serial.println("[TCP] Applying deferred settings reload...");
        reloadTCPClients();
        if (announceManager) announceManager->clearTransientNodes();
    }

    // 8. WiFi + TCP loops (with global budget) — skip only if RNS severely overloaded
    {
        drainRetiredTCPClients();
        bool skipTcp = (rnsDuration > 500);
        if (skipTcp) diagTcpSkipEvents++;
        if (!skipTcp && wifiImpl) wifiImpl->loop();
        if (!skipTcp) {
            unsigned long tcpBudgetStart = millis();
            for (auto* tcp : tcpClients) {
                if (millis() - tcpBudgetStart >= TCP_GLOBAL_BUDGET_MS) break;
                tcp->loop();
                yield();
            }
        }
        // AutoInterface always runs — its loop is non-blocking, capped at 4
        // packets per socket per call, time-gated for announces/peer-jobs.
        // Skipping it under TCP load causes peers to time out (22 s silence
        // window) when a TCP flood holds the loop above the skip threshold.
        autoIface.loop();
    }

    // 9. BLE loops
#if HAS_BLE
    bleInterface.loop();
    bleSideband.loop();
#endif

    // 9.5. GPS poll (non-blocking, reads available UART bytes)
#if HAS_GPS
    if (userConfig.settings().gpsTimeEnabled) {
        gps.loop();
    }
#endif

    // 10. Power management
    powerMgr.loop();

    // 11. Periodic status bar update (1 Hz) + render
    if (millis() - lastStatusUpdate >= STATUS_UPDATE_MS) {
        lastStatusUpdate = millis();
        if (powerMgr.isScreenOn()) {
            ui.lvStatusBar().setBatteryPercent(powerMgr.batteryPercent());
            // Update TCP connection indicator
            bool anyTcpUp = false;
            for (auto* tcp : tcpClients) {
                if (tcp && tcp->isConnected()) { anyTcpUp = true; break; }
            }
            ui.lvStatusBar().setTCPConnected(anyTcpUp);
            ui.lvStatusBar().setAutoIfacePeers(
                autoIface.isOnline() ? (int)autoIface.peerCount() : -1);
#if HAS_GPS
            if (userConfig.settings().gpsTimeEnabled) {
                ui.lvStatusBar().setGPSFix(gps.hasTimeFix());
            }
#endif
            // Update clock display (shows time from any valid source: GPS, NTP, etc.)
            ui.lvStatusBar().setUse24Hour(userConfig.settings().use24HourTime);
            ui.lvStatusBar().updateTime();
            ui.update();
        }
    }



    // 12.5. RSSI monitor (non-blocking, one sample per loop iteration)
    if (rssiMonitorActive && radioOnline) {
        unsigned long now = millis();
        if (now - rssiMonitorStart >= 5000) {
            rssiMonitorActive = false;
            Serial.printf("[RSSI] Done: %d samples, min=%d max=%d dBm\n",
                          rssiSampleCount, rssiMinVal, rssiMaxVal);
        } else if (now - rssiLastSample >= 100) {
            rssiLastSample = now;
            int rssi = radio.currentRssi();
            if (rssi < rssiMinVal) rssiMinVal = rssi;
            if (rssi > rssiMaxVal) rssiMaxVal = rssi;
            rssiSampleCount++;
            Serial.printf("[RSSI] %d dBm\n", rssi);
        }
    }

    // 13. Heartbeat for crash diagnosis
    {
        unsigned long cycleTime = millis() - loopCycleStart;
        if (cycleTime > maxLoopTime) maxLoopTime = cycleTime;

        if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
            lastHeartbeat = millis();
            Serial.printf("[HEART] heap=%lu psram=%lu min=%lu loop=%lums nodes=%d paths=%d links=%d lxmfQ=%d up=%lus radio=%s sd=%s flash=%s\n",
                          (unsigned long)ESP.getFreeHeap(),
                          (unsigned long)ESP.getFreePsram(),
                          (unsigned long)ESP.getMinFreeHeap(),
                          maxLoopTime,
                          announceManager ? announceManager->nodeCount() : 0,
                          (int)rns.pathCount(),
                          (int)rns.linkCount(),
                          lxmf.queuedCount(),
                          millis() / 1000,
                          radioOnline ? "ON" : "OFF",
                          sdStore.isReady() ? "OK" : "FAIL",
                          flash.isReady() ? "OK" : "FAIL");
            // Diagnostic: show registered transport interfaces and TCP connection status
            {
                auto& ifaces = RNS::Transport::get_interfaces();
                int tcpUp = 0;
                int tcpRx = 0;
                for (auto* tcp : tcpClients) {
                    if (tcp && tcp->isConnected()) tcpUp++;
                    if (tcp) tcpRx += tcp->hubRxCount();
                }
                Serial.printf("[HEART-DIAG] ifaces=%d tcp=%d/%d wifi=%s autoiface=%s peers=%u\n",
                    (int)ifaces.size(), tcpUp, (int)tcpClients.size(),
                    wifiSTAConnected ? "STA" : (wifiImpl ? "AP" : "OFF"),
                    autoIface.isOnline() ? "ON" : "off",
                    (unsigned)autoIface.peerCount());
                Serial.printf("[LXMF-DIAG] tcp_rx=%d tcp_skip=%lu ann_filt=%lu\n",
                    tcpRx, (unsigned long)diagTcpSkipEvents,
                    (unsigned long)rns.announceFilterCount());
                diagTcpSkipEvents = 0;
            }
#if HAS_GPS
            if (userConfig.settings().gpsTimeEnabled) {
                Serial.printf("[GPS] sats=%d timeFix=%s locFix=%s syncs=%lu chars=%lu\n",
                    gps.satellites(),
                    gps.hasTimeFix() ? "YES" : "NO",
                    gps.hasLocationFix() ? "YES" : "NO",
                    (unsigned long)gps.timeSyncCount(),
                    (unsigned long)gps.charsProcessed());
            }
#endif
            maxLoopTime = 0;
        }
    }
    loopCycleStart = millis();

    yield();
}
