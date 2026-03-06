# Ratdeck

Standalone [Reticulum](https://reticulum.network/) transport node + [LXMF](https://github.com/markqvist/LXMF) encrypted messenger, built for the [LilyGo T-Deck Plus](https://www.lilygo.cc/products/t-deck-plus) with integrated SX1262 LoRa radio.

Not an RNode. Not a gateway. A fully self-contained mesh node with a keyboard, touchscreen, trackball, and LoRa radio — no host computer required.

## What This Does

Ratdeck turns a T-Deck Plus into a Reticulum mesh node that can:

- **Send and receive encrypted messages** over LoRa (LXMF protocol, Ed25519 signatures)
- **Discover other nodes** automatically via Reticulum announces
- **Bridge LoRa to WiFi** so desktop Reticulum instances can reach the mesh
- **Connect to remote Reticulum nodes** over TCP (e.g., `rns.beleth.net`)
- **Store messages and contacts** on flash and SD card with automatic backup
- **Configure everything on-device** — no config files, no host tools

The device runs [microReticulum](https://github.com/attermann/microReticulum) (a C++ port of the Reticulum stack) directly on the ESP32-S3, with a register-level SX1262 LoRa driver.

## Features

| Category | Details |
|----------|---------|
| **Networking** | Reticulum transport node, path discovery, announce propagation, auto-announce every 5 min |
| **Messaging** | LXMF encrypted messages, Ed25519 signatures, delivery tracking, per-conversation storage |
| **LoRa Radio** | SX1262 at 915 MHz, configurable SF (5-12), BW (7.8-500 kHz), CR (4/5-4/8), TX power (2-22 dBm) |
| **WiFi** | AP mode (TCP server on :4242) or STA mode (TCP client to remote nodes) — not concurrent |
| **BLE** | NimBLE Sideband interface for Reticulum over Bluetooth |
| **Storage** | Dual-backend: LittleFS (7.8 MB on flash) + FAT32 microSD, atomic writes, identity backup |
| **Display** | 320x240 IPS TFT via LovyanGFX, signal green on black, double-buffered sprite rendering |
| **Input** | Full QWERTY keyboard (ESP32-C3 I2C), GT911 capacitive touchscreen, optical trackball |
| **Audio** | I2S codec, notification sounds for messages, announces, errors, boot chime |
| **GPS** | UBlox MIA-M10Q GNSS (pins defined, v1.1) |
| **Power** | Screen dim/off/wake on input, configurable timeouts, battery % in status bar |
| **Reliability** | Boot loop recovery (NVS counter, forces WiFi OFF after 3 failures) |
| **Diagnostics** | Ctrl+D full dump, Ctrl+T radio test packet, Ctrl+R 5-second RSSI monitor |

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| **Board** | LilyGo T-Deck Plus | ESP32-S3, 16MB flash, PSRAM, 320x240 IPS TFT, QWERTY keyboard, trackball, touchscreen |
| **Radio** | Integrated SX1262 | 915 MHz ISM, TCXO 1.8V, DIO2 RF switch, shared SPI bus |
| **Storage** | microSD card | Optional but recommended. FAT32, any size |
| **USB** | USB-C | USB-Serial/JTAG. Port: `/dev/cu.usbmodem*` (macOS), `/dev/ttyACM*` (Linux) |

See [docs/PINMAP.md](docs/PINMAP.md) for the full GPIO pin map.

## Prerequisites

| Requirement | Version | Install |
|-------------|---------|---------|
| **Python** | 3.12+ | [python.org](https://www.python.org/downloads/) or your package manager |
| **PlatformIO Core** | 6.x | `pip install platformio` |
| **Git** | any | Your package manager |
| **USB driver** | — | None needed on macOS/Linux (ESP32-S3 USB-Serial/JTAG is built-in) |

PlatformIO automatically downloads the ESP32-S3 toolchain, Arduino framework, and all library dependencies on first build.

## Build and Flash

```bash
# Clone
git clone https://github.com/defidude/Ratdeck.git
cd Ratdeck

# Build (first build takes ~2 min to download toolchain + deps)
python3 -m platformio run -e ratdeck_915

# Flash (plug in T-Deck Plus via USB-C)
python3 -m platformio run -e ratdeck_915 -t upload --upload-port /dev/cu.usbmodem*
```

> If `pio` is not on your PATH after install, use `python3 -m platformio` everywhere.

### Alternative: esptool

PlatformIO's default baud sometimes fails over USB-Serial/JTAG. esptool at 460800 is more reliable:

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* --baud 460800 \
    --before default-reset --after hard-reset \
    write_flash -z 0x10000 .pio/build/ratdeck_915/firmware.bin
```

See [docs/BUILDING.md](docs/BUILDING.md) for merged binaries, build flags, partition table, and CI/CD details.

## First Boot

1. Plug in or power on the T-Deck Plus
2. Boot animation with progress bar (~3 seconds)
3. SX1262 radio initializes at 915 MHz
4. SD card checked, `/ratdeck/` directories auto-created
5. Reticulum identity generated (Ed25519 keypair, persisted to flash + SD)
6. WiFi AP starts: `ratdeck-XXXX` (password: `ratspeak`)
7. Initial announce broadcast to the mesh
8. Home screen: identity hash, transport status, radio info, uptime

## UI Layout

```
+------------------------------------------+
| [87%] [Ratspeak.org] [LoRa]             |  Status bar: battery, transport mode, radio
+------------------------------------------+
|                                          |
|            CONTENT AREA                  |  Screens: Home, Messages, Nodes, Map, Settings
|            320 x 240                     |
|                                          |
+------------------------------------------+
| [Home] [Msgs] [Nodes] [Map] [Setup]     |  Tab bar with unread badges
+------------------------------------------+
```

**Theme**: Signal green (#00FF41) on black. 320x240 pixels. LovyanGFX double-buffered sprite rendering.

## Keyboard and Hotkeys

### Hotkeys (Ctrl+key)

| Shortcut | Action |
|----------|--------|
| Ctrl+H | Toggle help overlay (shows all hotkeys on screen) |
| Ctrl+M | Jump to Messages tab |
| Ctrl+N | Compose new message |
| Ctrl+S | Jump to Settings tab |
| Ctrl+A | Force announce to network (immediate) |
| Ctrl+D | Dump full diagnostics to serial |
| Ctrl+T | Send radio test packet |
| Ctrl+R | RSSI monitor (5 seconds continuous sampling) |

### Navigation

| Key | Action |
|-----|--------|
| Trackball | Scroll / navigate |
| Touch | Tap to select UI elements |
| Enter | Select / confirm / send message |
| Esc | Back / cancel |
| Backspace | Delete character in text input |

## WiFi and Networking

Three WiFi modes, configured in Settings:

### OFF Mode

No WiFi. Saves power and heap. LoRa-only operation.

### AP Mode (default)

Creates a WiFi hotspot named `ratdeck-XXXX` (password: `ratspeak`). Runs a TCP server on port 4242 with HDLC framing.

**Bridge to desktop Reticulum**: Connect your laptop to the `ratdeck-XXXX` network, then add to your Reticulum config (`~/.reticulum/config`):

```ini
[[ratdeck]]
  type = TCPClientInterface
  target_host = 192.168.4.1
  target_port = 4242
```

Now your desktop `rnsd` can reach the LoRa mesh through Ratdeck.

### STA Mode

Connects to an existing WiFi network. Establishes outbound TCP connections to configured Reticulum endpoints.

**Setup**:
1. Ctrl+S, WiFi, Mode: **STA**
2. Enter your WiFi SSID and password
3. Add TCP endpoints: e.g., `rns.beleth.net` port `4242`, auto-connect enabled
4. Save — Ratdeck connects to your WiFi, then opens TCP links to the configured hosts

## LoRa Radio

### Default Configuration

| Parameter | Default | Range |
|-----------|---------|-------|
| Frequency | 915 MHz | Hardware-dependent |
| Spreading Factor | SF9 | SF5-SF12 |
| Bandwidth | 125 kHz | 7.8 kHz - 500 kHz |
| Coding Rate | 4/5 | 4/5 - 4/8 |
| TX Power | 22 dBm | 2-22 dBm |
| Preamble | 18 symbols | Configurable |
| Sync Word | 0x1424 | Reticulum standard |
| Max Packet | 255 bytes | SX1262 hardware limit |

All parameters configurable via Settings. Changes take effect immediately and persist across reboots.

## SD Card

Optional microSD card (FAT32, any size). Provides backup storage and extended capacity beyond the LittleFS partition.

### Directory Structure (auto-created on first boot)

```
/ratdeck/
  config/
    user.json           Runtime settings (radio, WiFi, display, audio)
  messages/
    <peer_hex>/         Per-conversation message history (JSON)
  contacts/             Discovered Reticulum nodes
  identity/
    identity.key        Ed25519 keypair backup
```

## Dependencies

All automatically managed by PlatformIO — no manual installation needed:

| Library | Version | Purpose |
|---------|---------|---------|
| [microReticulum](https://github.com/attermann/microReticulum) | git HEAD | Reticulum protocol stack (C++ port) |
| [Crypto](https://github.com/attermann/Crypto) | git HEAD | Ed25519, X25519, AES-128, SHA-256, HMAC |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | ^7.4.2 | JSON serialization for config and message storage |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | ^1.1.16 | Display driver — SPI, DMA, sprite double-buffering |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | ^2.1 | BLE stack for Sideband interface |

### Build Toolchain

| Component | Version | Notes |
|-----------|---------|-------|
| PlatformIO | espressif32@6.7.0 | ESP-IDF + Arduino framework |
| Board | esp32-s3-devkitc-1 | Generic ESP32-S3, 16MB flash, PSRAM |
| Arduino Core | ESP32 Arduino 2.x | C++17, exceptions enabled |

## Flash Memory Layout

16MB flash, partitioned for OTA support:

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvs | 0x9000 | 20 KB | Boot counter, WiFi credentials |
| otadata | 0xE000 | 8 KB | OTA boot selection |
| app0 | 0x10000 | 4 MB | Active firmware |
| app1 | 0x410000 | 4 MB | OTA update slot (reserved) |
| littlefs | 0x810000 | 7.8 MB | Identity, config, messages, transport paths |
| coredump | 0xFF0000 | 64 KB | ESP-IDF crash dump |

## Project Structure

```
Ratdeck/
  src/
    main.cpp                 Entry point: setup() + main loop
    config/                  BoardConfig.h (pins), Config.h (compile-time), UserConfig.* (runtime JSON)
    radio/                   SX1262.* (register-level driver), RadioConstants.h
    hal/                     Display (LovyanGFX), TouchInput (GT911), Trackball, Keyboard, Power, GPS, Audio
    input/                   InputManager (unified input), HotkeyManager (Ctrl+key dispatch)
    ui/                      UIManager, StatusBar, TabBar, Theme
      screens/               Boot, Home, Messages, MessageView, Nodes, Map, Settings, HelpOverlay
    reticulum/               ReticulumManager, AnnounceManager, LXMFManager, LXMFMessage
    transport/               LoRaInterface, WiFiInterface, TCPClientInterface, BLEInterface, BLESideband
    storage/                 FlashStore (LittleFS), SDStore (FAT32), MessageStore (dual)
    power/                   PowerManager (dim/off/wake)
    audio/                   AudioNotify (boot, message, announce, error sounds)
  docs/                      BUILDING, PINMAP, TROUBLESHOOTING, DEVELOPMENT, ARCHITECTURE, QUICKSTART, HOTKEYS
  platformio.ini             Build configuration (single env: ratdeck_915)
  partitions_16MB.csv        Flash partition table
  .github/workflows/         CI: build on push, release merged binary on tag
```

## Documentation

| Document | Contents |
|----------|----------|
| [Quick Start](docs/QUICKSTART.md) | First build, first boot, navigation, WiFi setup, SD card |
| [Building](docs/BUILDING.md) | Build commands, flashing, merged binaries, CI/CD, build flags |
| [Pin Map](docs/PINMAP.md) | Full GPIO assignments for all peripherals |
| [Hotkeys](docs/HOTKEYS.md) | Complete keyboard reference |
| [Architecture](docs/ARCHITECTURE.md) | Layer diagram, directory tree, design decisions |
| [Development](docs/DEVELOPMENT.md) | How to add screens, hotkeys, settings, transports |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Radio, build, boot loop, storage, WiFi issues |

## Current Status

**v1.0.0** — Working on hardware.

| Subsystem | Status |
|-----------|--------|
| LoRa radio | Working — TX/RX verified |
| WiFi AP | Working — TCP server, HDLC framing, bridges to desktop rnsd |
| WiFi STA + TCP | Working — connects to remote Reticulum nodes |
| LXMF messaging | Working — send/receive/store with Ed25519 signatures |
| Node discovery | Working — automatic announce processing |
| SD card storage | Working — dual-backend with atomic writes |
| Settings | Working — full on-device configuration |
| Touchscreen | Working — GT911 capacitive touch |
| Trackball | Working — optical navigation |
| BLE Sideband | Working — NimBLE interface |
| GPS | Pins defined — v1.1 |
| OTA updates | Partition reserved — not implemented |

## License

GPL-3.0
