#pragma once

// =============================================================================
// RatDeck — Compile-Time Configuration
// =============================================================================

#define RATDECK_VERSION_MAJOR  1
#define RATDECK_VERSION_MINOR  8
#define RATDECK_VERSION_PATCH  2
#define RATDECK_VERSION_STRING "1.8.2"

// --- Feature Flags ---
#define HAS_DISPLAY     true
#define HAS_KEYBOARD    true
#define HAS_TOUCH       true
#define HAS_TRACKBALL   true
#define HAS_LORA        true
#define HAS_WIFI        true
#define HAS_BLE         true
#define HAS_SD          true
#define HAS_AUDIO       true
#define HAS_GPS         true    // UBlox MIA-M10Q UART GPS

// --- WiFi Defaults ---
#define WIFI_AP_PORT        4242
#define WIFI_AP_PASSWORD    "ratspeak"

// --- Storage Paths ---
#define PATH_IDENTITY       "/identity/identity.key"
#define PATH_IDENTITY_BAK   "/identity/identity.key.bak"
#define PATH_PATHS          "/transport/paths.msgpack"
#define PATH_USER_CONFIG    "/config/user.json"
#define PATH_CONTACTS       "/contacts/"
#define PATH_MESSAGES       "/messages/"

// --- SD Card Paths ---
#define SD_PATH_CONFIG_DIR   "/ratdeck/config"
#define SD_PATH_USER_CONFIG  "/ratdeck/config/user.json"
#define SD_PATH_MESSAGES     "/ratdeck/messages/"
#define SD_PATH_CONTACTS     "/ratdeck/contacts/"
#define SD_PATH_IDENTITY     "/ratdeck/identity/identity.key"

// --- TCP Client ---
#define MAX_TCP_CONNECTIONS         4
#define TCP_DEFAULT_PORT            4242
#define TCP_RECONNECT_INTERVAL_MS   15000
#define TCP_CONNECT_TIMEOUT_MS      5000

// --- Announce Flood Defense ---
#define RATDECK_MAX_ANNOUNCES_PER_SEC 5     // Transport-level rate limit (before Ed25519 verify)

// --- Limits ---
#define RATDECK_MAX_NODES             100   // Endpoint device, not transport node
#define RATDECK_MAX_MESSAGES_PER_CONV 100
#define FLASH_MSG_CACHE_LIMIT         20
#define RATDECK_MAX_OUTQUEUE          20
#define PATH_PERSIST_INTERVAL_MS  60000

// --- Power Management ---
#define SCREEN_DIM_TIMEOUT_MS   30000
#define SCREEN_OFF_TIMEOUT_MS   60000
#define SCREEN_DIM_BRIGHTNESS   64

// --- Radio Regions ---
enum RadioRegion : uint8_t {
    REGION_AMERICAS  = 0,  // 915 MHz (902-928 ISM)
    REGION_EUROPE    = 1,  // 868 MHz (863-870)
    REGION_AUSTRALIA = 2,  // 915 MHz (915-928)
    REGION_ASIA      = 3,  // 923 MHz (AS923)
    REGION_COUNT     = 4
};

static constexpr uint32_t REGION_FREQ[REGION_COUNT] = {
    915000000, 868000000, 915000000, 923000000
};

static const char* const REGION_LABELS[REGION_COUNT] = {
    "Americas (915)", "Europe (868)", "Australia (915)", "Asia (923)"
};

// --- Serial Debug ---
#define SERIAL_BAUD  115200

// --- Shared Utilities (defined in main.cpp) ---
#include <Arduino.h>
#include <Bytes.h>
RNS::Bytes encodeAnnounceName(const String& name);
