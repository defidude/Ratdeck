# Ratputer — Architecture

## Overview

Ratputer is a standalone Reticulum transport node with LXMF messaging, built for the M5Stack Cardputer Adv with Cap LoRa-1262 radio module.

## Layer Diagram

```
┌─────────────────────────────────────┐
│         UI Layer (M5Canvas)         │
│  Screens: Home, Msgs, Nodes, Setup  │
│  Widgets: ScrollList, TextInput     │
│  StatusBar, TabBar, HelpOverlay     │
├─────────────────────────────────────┤
│       Application Layer             │
│  LXMFManager  AnnounceManager       │
│  UserConfig   AudioNotify           │
│  PowerManager MessageStore          │
├─────────────────────────────────────┤
│       Reticulum Layer               │
│  ReticulumManager (microReticulum)  │
│  Identity, Destination, Transport   │
├─────────────────────────────────────┤
│       Transport Layer               │
│  LoRaInterface  WiFiInterface       │
│  TCPClientInterface  BLEStub        │
├─────────────────────────────────────┤
│       Storage Layer                 │
│  FlashStore (LittleFS)              │
│  SDStore (FAT32 microSD)            │
├─────────────────────────────────────┤
│       Hardware Layer                │
│  SX1262 Radio   M5Cardputer         │
│  LittleFS       ESP32-S3            │
└─────────────────────────────────────┘
```

## Directory Structure

```
src/
├── main.cpp              Main entry point (setup + loop)
├── config/
│   ├── BoardConfig.h     Pin definitions, hardware constants
│   ├── Config.h          Compile-time settings, feature flags, paths
│   └── UserConfig.*      Runtime settings (JSON, dual SD+flash backend)
├── radio/
│   ├── SX1262.*          SX1262 LoRa driver (register-level)
│   └── RadioConstants.h  Register definitions
├── input/
│   ├── Keyboard.*        M5Cardputer keyboard wrapper
│   └── HotkeyManager.*   Ctrl+key dispatch
├── ui/
│   ├── Theme.h           Color palette, layout metrics
│   ├── UIManager.*       Canvas rendering, screen stack
│   ├── Screen.h          Abstract base class
│   ├── StatusBar.*       Top bar (battery, transport, LoRa)
│   ├── TabBar.*          Bottom tab navigation
│   ├── screens/          Per-tab screen implementations
│   ├── widgets/          Reusable UI components
│   └── assets/           Boot logo
├── reticulum/
│   ├── ReticulumManager.*  microReticulum integration
│   ├── AnnounceManager.*   Node discovery, contact persistence
│   ├── LXMFManager.*       LXMF messaging protocol
│   └── LXMFMessage.*       Message format (MsgPack wire, JSON storage)
├── transport/
│   ├── LoRaInterface.*   SX1262 ↔ Reticulum bridge (1-byte header)
│   ├── WiFiInterface.*   WiFi AP transport, TCP server on :4242
│   ├── TCPClientInterface.*  WiFi STA transport, TCP client to remote nodes
│   └── BLEStub.*         BLE advertising placeholder (disabled)
├── storage/
│   ├── FlashStore.*      LittleFS with atomic writes
│   ├── SDStore.*         SD card (FAT32) with atomic writes + wipe
│   └── MessageStore.*    Per-conversation storage (dual: flash + SD)
├── power/
│   └── PowerManager.*    Screen dim/off/wake
└── audio/
    └── AudioNotify.*     Notification sounds
```

## Data Flow

### Incoming LoRa Packet

```
SX1262 IRQ (DIO1) → SX1262::receive() reads FIFO
    → LoRaInterface::loop() strips 1-byte header
        → RNS::InterfaceImpl::receive_incoming()
            → RNS::Transport processes packet
                ├── Announce → AnnounceManager callback → UI update
                ├── LXMF data → LXMFManager → MessageStore (flash + SD) → UI notification
                └── Path/link → Transport table update → persist to flash
```

### Outgoing LXMF Message

```
User types message → MessageView → LXMFManager::send()
    → Pack: source_hash(16) + msgpack([ts, content, title, fields]) + Ed25519 sig(64)
        → RNS::Packet → RNS::Transport selects interface
            ├── LoRaInterface → prepend 1-byte header → SX1262::beginPacket/endPacket
            └── WiFi/TCPClient → HDLC frame (0x7E delimit, 0x7D escape) → TCP socket
```

### Config Save

```
SettingsScreen → UserConfig::save(sd, flash)
    ├── serialize to JSON string
    ├── SDStore::writeAtomic("/ratputer/config/user.json")  → .tmp → verify → .bak → rename
    └── FlashStore::writeAtomic("/config/user.json")        → .tmp → verify → .bak → rename
```

## Key Design Decisions

### Radio Driver
Extracted from RNode_Firmware_CE, stripped of multi-interface and CSMA/CA. Custom SPI (HSPI) with TCXO 3.0V configuration. IRQ stale latch fix applied to prevent DCD lockup after first TX.

### Display
Double-buffered M5Canvas sprite (240×135 RGB565). All rendering goes through UIManager which handles status bar, content area clipping, tab bar, and overlay.

### Reticulum Integration
microReticulum C++ library with LittleFS-backed filesystem. Device runs as a Transport Node with LoRa and WiFi/TCP interfaces registered.

### LXMF Messages
Wire format: `source_hash(16) + msgpack([timestamp, content, title, fields]) + signature(64)`. Direct packet delivery for messages under MDU. Stored as JSON per-conversation in flash and SD.

### WiFi Transport
Three separate modes (not concurrent):
- **OFF**: No WiFi — saves power and ~20KB heap
- **AP**: Creates hotspot, TCP server on port 4242 with HDLC framing (0x7E delimiters, 0x7D escape)
- **STA**: Connects to existing network, TCP client connections to configured endpoints

AP+STA concurrent mode was removed — it consumed too much heap and caused instability.

### TCP Client Transport
Outbound TCP connections to remote Reticulum nodes. Created on first WiFi STA connection, auto-reconnect on disconnect. Uses same HDLC framing as WiFi AP.

### Dual-Backend Storage
FlashStore (LittleFS) is the primary store. SDStore (FAT32 microSD) provides backup and extended capacity. Both use atomic writes (.tmp → verify → .bak → rename) to prevent corruption on power loss. UserConfig, MessageStore, and AnnounceManager write to both backends.

### Boot Loop Recovery
NVS counter tracks consecutive boot failures. After 3 failures, WiFi is forced OFF on next boot (WiFi init is the most common crash source). Counter resets to 0 at end of successful setup().

### Transport Reference Stability
`RNS::Transport::_interfaces` stores `Interface&` references (not copies). All `RNS::Interface` wrappers must outlive the transport — stored in `std::list` (not vector, which would invalidate references on reallocation).

### Power Management
Three states: Active → Dimmed (25% brightness) → Screen Off. Wakes on any keypress. Configurable timeouts via Settings.
