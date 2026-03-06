# Ratputer — Developer Guide

## Project Overview

Ratputer is a **standalone Reticulum transport node** with LXMF messaging, built for the M5Stack Cardputer Adv with Cap LoRa-1262 radio. It is **NOT an RNode** — it does not speak KISS protocol. It runs its own Reticulum stack (microReticulum) directly on the device.

Key characteristics:
- Standalone operation — no host computer required
- LoRa transport with 1-byte header framing (RNode-compatible on-air format)
- WiFi transport — AP mode (TCP server) or STA mode (TCP client)
- LXMF encrypted messaging with Ed25519 signatures
- Cyberpunk terminal UI with tabbed navigation
- JSON-based runtime configuration with SD card + flash dual-backend

## Source Tree

```
src/
├── main.cpp                    Setup (24-step init) + main loop (10 steps, 20 FPS)
├── config/
│   ├── BoardConfig.h           GPIO pins, SPI config, hardware constants
│   ├── Config.h                Compile-time: version, feature flags, storage paths, limits
│   └── UserConfig.*            Runtime settings: dual-backend JSON (SD primary, flash fallback)
├── radio/
│   ├── SX1262.*                Full SX1262 driver (register-level, extracted from RNode CE)
│   └── RadioConstants.h        SX1262 register addresses and command bytes
├── input/
│   ├── Keyboard.*              M5Cardputer TCA8418 keyboard wrapper, key event generation
│   └── HotkeyManager.*        Ctrl+key dispatch table, tab cycle callback
├── ui/
│   ├── Theme.h                 Signal green (#00FF41) on black, layout metrics
│   ├── UIManager.*             Canvas rendering loop, screen stack, boot/normal modes
│   ├── Screen.h                Abstract base: handleKey(), render(), update()
│   ├── StatusBar.*             Battery %, transport mode, LoRa indicator, announce flash
│   ├── TabBar.*                Home/Msgs/Nodes/Setup tabs with unread badges
│   ├── screens/
│   │   ├── BootScreen.*        Animated boot with progress bar
│   │   ├── HomeScreen.*        Identity hash, transport status, radio info, uptime
│   │   ├── MessagesScreen.*    Conversation list with unread counts
│   │   ├── MessageView.*       Single conversation view with text input
│   │   ├── NodesScreen.*       Discovered Reticulum nodes with RSSI/SNR
│   │   ├── SettingsScreen.*    Radio, WiFi, Display, Audio, About, Factory Reset
│   │   └── HelpOverlay.*      Hotkey reference overlay (Ctrl+H toggle)
│   ├── widgets/
│   │   ├── ScrollList.*        Scrollable list with selection highlight
│   │   ├── TextInput.*         Single-line text input with cursor
│   │   └── ProgressBar.*      Boot progress and general-purpose bars
│   └── assets/
│       └── BootLogo.h          Embedded boot screen graphic
├── reticulum/
│   ├── ReticulumManager.*      microReticulum lifecycle, identity, announce, transport loop
│   ├── AnnounceManager.*       Node discovery, contact persistence (SD + flash)
│   ├── LXMFManager.*           LXMF send/receive, outgoing queue, delivery tracking
│   └── LXMFMessage.*           Wire format: source(16) + msgpack + sig(64)
├── transport/
│   ├── LoRaInterface.*         SX1262 ↔ Reticulum bridge (InterfaceImpl), 1-byte header
│   ├── WiFiInterface.*         WiFi AP, TCP server on port 4242, HDLC framing
│   ├── TCPClientInterface.*    WiFi STA, TCP client to remote endpoints, HDLC framing
│   └── BLEStub.*              BLE advertising placeholder (disabled, v1.1)
├── storage/
│   ├── FlashStore.*            LittleFS wrapper with atomic writes (.tmp→verify→.bak→rename)
│   ├── SDStore.*               SD card (FAT32) with atomic writes, wipe, directory management
│   └── MessageStore.*          Per-conversation message storage (dual: flash + SD backup)
├── power/
│   └── PowerManager.*          Screen dim/off/wake state machine, brightness control
└── audio/
    └── AudioNotify.*           Notification sounds (boot, message, announce, error)
```

## Configuration System

### Compile-Time (`Config.h`)

Feature flags (`HAS_LORA`, `HAS_WIFI`, etc.), storage paths, protocol limits, power defaults. Changed only by editing source and recompiling.

### Runtime (`UserConfig`)

JSON-based settings persisted to storage. Schema defined by `UserSettings` struct in `UserConfig.h`:

```
{
  "loraFrequency": 915000000,
  "loraSF": 7,
  "loraBW": 500000,
  "loraCR": 5,
  "loraTxPower": 10,
  "wifiMode": 1,           // 0=OFF, 1=AP, 2=STA
  "wifiAPSSID": "ratputer-XXXX",
  "wifiAPPassword": "ratspeak",
  "wifiSTASSID": "",
  "wifiSTAPassword": "",
  "tcpConnections": [{"host": "rns.beleth.net", "port": 4242, "autoConnect": true}],
  "screenDimTimeout": 30,
  "screenOffTimeout": 60,
  "brightness": 255,
  "audioEnabled": true,
  "audioVolume": 80,
  "displayName": ""
}
```

**Dual-backend persistence**: `UserConfig::load(SDStore&, FlashStore&)` reads from SD first (`/ratputer/config/user.json`), falls back to flash (`/config/user.json`). `save()` writes to both.

## Transport Architecture

### InterfaceImpl Pattern

All transport interfaces inherit from `RNS::InterfaceImpl`:
- `start()` / `stop()` — lifecycle
- `send_outgoing(const RNS::Bytes& data)` — transmit
- `loop()` — poll for incoming data, call `receive_incoming()` to push up to Reticulum

### HDLC Framing (WiFi + TCP)

TCP connections use HDLC-like byte framing:
- `0x7E` — frame delimiter (start/end)
- `0x7D` — escape byte
- `0x20` — XOR mask for escaped bytes

Any `0x7E` or `0x7D` in payload is escaped as `0x7D (byte ^ 0x20)`.

### LoRa 1-Byte Header

Every LoRa packet has a 1-byte header prepended:
- Upper nibble: random sequence number (for future split-packet tracking)
- Lower nibble: flags (`0x01` = split, not currently implemented)

This matches the RNode on-air format, so Ratputer packets are structurally compatible with RNodes on the same frequency/modulation.

## Reticulum Integration

### microReticulum Library

C++ port of the Python Reticulum stack. Provides `Identity`, `Destination`, `Transport`, `Packet`, and `Link` classes.

Key integration points in `ReticulumManager`:
- `RNS::Reticulum::start()` — initialize the stack
- `RNS::Transport::register_interface()` — add LoRa, WiFi, TCP interfaces
- `RNS::Transport::register_announce_handler()` — node discovery callback
- `RNS::Reticulum::loop()` — process incoming/outgoing in main loop

### Identity Persistence

Device identity (Ed25519 keypair) is stored at `/identity/identity.key` in LittleFS with a backup copy on SD at `/ratputer/identity/identity.key`. If flash identity is lost (e.g., LittleFS format), the SD backup is restored automatically.

### Path Persistence

Transport paths are serialized to `/transport/paths.msgpack` in LittleFS periodically (every 60 seconds, configurable via `PATH_PERSIST_INTERVAL_MS`).

## LXMF Protocol

Wire format for direct LoRa delivery:

```
source_hash(16 bytes) + msgpack([timestamp, content, title, fields]) + signature(64 bytes)
```

- `source_hash` — 16-byte truncated SHA-256 of sender's public key
- MsgPack array: `[double timestamp, string content, string title, map fields]`
- `signature` — Ed25519 signature over `source_hash + msgpack_content`

Messages under the MDU (Maximum Data Unit, ~254 bytes for LoRa) are sent as single direct packets. Larger messages would require link-based transfer (not yet implemented).

Messages are stored as JSON per-conversation in both flash (`/messages/<peer_hex>/`) and SD (`/ratputer/messages/<peer_hex>/`).

## Storage Architecture

### FlashStore (LittleFS)

Primary storage for all persistent data. 1.875 MB partition at offset 0x610000.

**Atomic write pattern**: Write to `.tmp` → verify read-back → rename existing to `.bak` → rename `.tmp` to final path. Prevents corruption on power loss.

### SDStore (FAT32)

Secondary/backup storage on microSD card. Shares HSPI bus with LoRa radio.

Directory structure:
```
/ratputer/
├── config/
│   └── user.json        Runtime settings backup
├── messages/
│   └── <peer_hex>/      Per-conversation message history
├── contacts/            Discovered node info
└── identity/
    └── identity.key     Identity key backup
```

### MessageStore (Dual Backend)

Wraps FlashStore and SDStore to provide unified message access. Writes go to both backends; reads prefer SD (larger capacity), fall back to flash.

## WiFi State Machine

Three modes, selected in Settings:

```
RAT_WIFI_OFF (0) ──→ No WiFi, saves power + heap
RAT_WIFI_AP  (1) ──→ Creates AP "ratputer-XXXX", TCP server on :4242
RAT_WIFI_STA (2) ──→ Connects to configured network, TCP client connections
```

In STA mode, WiFi connection is non-blocking. TCP client interfaces are created on first successful connection and auto-reconnect if WiFi drops.

Boot loop recovery forces WiFi to OFF if 3 consecutive boots fail.

## How To: Add a New Screen

1. Create `src/ui/screens/MyScreen.h` and `MyScreen.cpp`
2. Inherit from `Screen` — implement `handleKey()`, `render()`, optionally `update()`
3. In `render()`, use `m5canvas` to draw within the content area (y: 14 to 119)
4. Add a global instance in `main.cpp`
5. Wire it up: either add to `tabScreens[]` array or navigate to it from a hotkey/callback

## How To: Add a New Hotkey

1. In `main.cpp`, create a callback function: `void onHotkeyX() { ... }`
2. Register in `setup()`: `hotkeys.registerHotkey('x', "Description", onHotkeyX);`
3. Update `docs/HOTKEYS.md` and the help overlay text in `HelpOverlay.cpp`

## How To: Add a Settings Submenu

1. In `SettingsScreen.h`, add an enum value to the menu state
2. In `SettingsScreen.cpp`, add menu item text and handler
3. Add `render*()` and `handleKey*()` methods for the new submenu
4. Use `userConfig->save(sdStore, flash)` to persist changes

## How To: Add a New Transport Interface

1. Create a class inheriting from `RNS::InterfaceImpl`
2. Implement `start()`, `stop()`, `loop()`, `send_outgoing()`
3. In `loop()`, call `receive_incoming(data)` when data arrives
4. In `main.cpp`, construct the impl, wrap in `RNS::Interface`, register with `RNS::Transport`
5. Store the `RNS::Interface` wrapper in a persistent container (e.g., `std::list`) — Transport holds references

## Initialization Sequence

`setup()` runs these steps in order:

1. M5Cardputer.begin() — display, keyboard, battery ADC
2. UI init + boot screen
3. Keyboard init
4. Register hotkeys (Ctrl+H/M/N/S/A/D/T/R)
5. Mount LittleFS (FlashStore)
6. Boot loop detection (NVS counter)
7. Radio init — SX1262 begin, configure modulation, enter RX
8. SD card init (shares HSPI, must be after radio)
9. Serial WIPE window (500ms)
10. Reticulum init — identity load/generate, transport start
11. MessageStore init (dual backend)
12. LXMF init + message callback
13. AnnounceManager init + contact load
14. Register announce handler with Transport
15. Load UserConfig (SD → flash fallback)
16. Boot loop recovery check (force WiFi OFF if triggered)
17. Apply saved radio settings
18. WiFi start (AP, STA, or OFF based on config)
19. BLE skip (disabled)
20. Power manager init + apply saved brightness/timeouts
21. Audio init + apply saved volume
22. Boot complete — switch to Home screen
23. Initial announce broadcast
24. Clear boot loop counter (NVS reset to 0)

## Main Loop

Runs at 20 FPS (50ms interval):

1. `M5Cardputer.update()` — poll M5 hardware
2. Keyboard poll → hotkey dispatch → screen key handler → tab cycling
3. `rns.loop()` — Reticulum transport + radio RX processing
4. Auto-announce (every 5 minutes)
5. `lxmf.loop()` — outgoing message queue
6. WiFi STA connection handler + TCP client creation
7. `wifiImpl->loop()` — WiFi transport (AP server accepts, processes clients)
8. TCP client loops — reconnection, frame processing
9. `power.loop()` — dim/off state machine
10. Canvas render (if screen is on)

## Memory Budget

The ESP32-S3 has 512 KB SRAM. Typical free heap at runtime:

| State | Free Heap | Notes |
|-------|-----------|-------|
| Boot complete (WiFi OFF) | ~170 KB | Baseline |
| Boot complete (WiFi AP) | ~150 KB | WiFi stack + TCP server |
| Boot complete (WiFi STA) | ~140 KB | WiFi stack + TCP clients |
| With BLE enabled | -50 KB | BLE disabled in v1.0 to save this |

Key consumers:
- microReticulum transport tables: ~20–40 KB (scales with paths/links)
- M5Canvas sprite buffer: 240×135×2 = 64.8 KB (RGB565 double-buffer)
- ArduinoJson documents: ~4 KB per config parse
- SX1262 TX/RX buffers: 255 bytes each
- TCP RX buffer: 600 bytes per connection

Monitor with `Ctrl+D` → `Free heap` or `ESP.getFreeHeap()` in code.

## Debugging Tips

### Serial output

All subsystems log with `[TAG]` prefixes. Connect at 115200 baud. Key tags: `[BOOT]`, `[RADIO]`, `[LORA_IF]`, `[WIFI]`, `[LXMF]`, `[SD]`.

### Radio debugging

- `Ctrl+D` dumps all SX1262 registers — compare sync word, IQ polarity, LNA, and OCP with a known-working device
- `Ctrl+T` sends a test packet and reads back the FIFO — confirms the TX path end-to-end
- `Ctrl+R` samples RSSI for 5 seconds — if readings stay at -110 to -120 dBm while another device transmits, the RX front-end isn't receiving RF
- `DevErrors: 0x0040` = PLL lock failure → check TCXO voltage (must be 3.0V / 0x06)

### Crash debugging

ESP-IDF stores a core dump in the `coredump` partition (64 KB at 0x7F0000). To read it:

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* read-flash 0x7F0000 0x10000 coredump.bin
python3 -m esp_coredump info_corefile -t raw -c coredump.bin .pio/build/ratputer_915/firmware.elf
```

### Common crash causes

| Crash | Cause | Fix |
|-------|-------|-----|
| `LoadProhibited` at transport loop | Dangling `Interface&` reference | Store `RNS::Interface` in `std::list` (not vector, not local scope) |
| `Stack overflow` in task | Deep call chain in ISR or recursive render | Increase stack size or reduce nesting |
| `Guru Meditation` on WiFi init | Heap exhaustion | Disable BLE, reduce TCP connections, check for leaks |
| Boot loop (3+ failures) | WiFi or TCP init crash | Boot loop recovery auto-disables WiFi; fix root cause in Settings |

## Compile-Time Limits

These are defined in `Config.h` and can be adjusted:

| Constant | Default | Purpose |
|----------|---------|---------|
| `RATPUTER_MAX_NODES` | 50 | Max discovered nodes in AnnounceManager |
| `RATPUTER_MAX_MESSAGES_PER_CONV` | 100 | Max messages stored per conversation |
| `FLASH_MSG_CACHE_LIMIT` | 20 | Keep only N most recent messages per conv in flash (SD has full history) |
| `RATPUTER_MAX_OUTQUEUE` | 20 | Max pending outgoing LXMF messages |
| `MAX_TCP_CONNECTIONS` | 4 | Max simultaneous TCP client connections |
| `TCP_RECONNECT_INTERVAL_MS` | 10000 | Retry interval for dropped TCP connections |
| `TCP_CONNECT_TIMEOUT_MS` | 5000 | Timeout for TCP connect() |
| `PATH_PERSIST_INTERVAL_MS` | 60000 | How often transport paths are saved to flash |
| `SCREEN_DIM_TIMEOUT_MS` | 30000 | Default screen dim timeout |
| `SCREEN_OFF_TIMEOUT_MS` | 60000 | Default screen off timeout |
| `ANNOUNCE_INTERVAL_MS` | 300000 | Auto-announce period (5 minutes, defined in main.cpp) |
