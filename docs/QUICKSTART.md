# Ratputer — Quick Start

## Hardware Required

- M5Stack Cardputer Adv (ESP32-S3, 8MB flash)
- Cap LoRa-1262 module (SX1262, 915 MHz)
- microSD card (optional, recommended)

## Build & Flash

```bash
# Install PlatformIO
pip install platformio

# Clone and build
git clone https://github.com/defidude/Ratputer.git
cd Ratputer
python3 -m platformio run -e ratputer_915

# Flash to device (use glob to match USB port)
python3 -m platformio run -e ratputer_915 -t upload --upload-port /dev/cu.usbmodem*
```

> **Note**: If `pio` is not on your PATH, use `python3 -m platformio` instead. See [BUILDING.md](BUILDING.md) for esptool flashing and merged binary instructions.

### USB Port

The Cardputer Adv uses USB-Serial/JTAG — the port appears as `/dev/cu.usbmodem<chipID>` in firmware mode. Use the `*` glob to match it.

## First Boot

1. Power on the Cardputer Adv
2. Boot animation plays with progress bar
3. Radio initializes at 915 MHz
4. SD card checked (auto-creates `/ratputer/` directories)
5. Reticulum transport node starts, identity generated
6. WiFi AP starts: `ratputer-XXXX` (password: `ratspeak`)
7. Home screen shows identity hash and status

## Navigation

Keys match the physical arrow positions on the Cardputer Adv keyboard:

- **`;`** / **`.`**: Scroll up/down in lists
- **`,`** / **`/`**: Cycle between tabs (left/right)
- **Enter**: Select/confirm
- **Esc**: Back/cancel
- **Ctrl+key**: Hotkeys (press Ctrl+H for help)

## Sending a Message

1. Wait for another node to appear in the **Nodes** tab
2. Press **Ctrl+M** to go to Messages
3. Select a conversation or use **Ctrl+N** for new
4. Type your message and press **Enter**

## WiFi Setup

Default: AP mode with SSID `ratputer-XXXX`.

### AP Mode (default)

Connect a laptop to the `ratputer-XXXX` WiFi network, then configure `rnsd` with a TCPClientInterface pointing at `192.168.4.1:4242`.

### STA Mode

To connect Ratputer to your WiFi network:

1. Press **Ctrl+S** → WiFi → Mode → STA
2. Enter your WiFi SSID and password
3. Save — device reconnects in STA mode
4. Add TCP endpoints (e.g., `rns.beleth.net:4242`) in WiFi → TCP Connections

### WiFi OFF

Select OFF in WiFi settings to disable WiFi entirely (saves power and ~20KB heap).

## SD Card

Insert a microSD card before powering on. The firmware auto-creates:

```
/ratputer/config/     Settings backup
/ratputer/messages/   Message archives
/ratputer/contacts/   Discovered nodes
/ratputer/identity/   Identity key backup
```

To wipe SD data: connect serial at 115200 baud, send `WIPE` within 500ms of boot.

## Serial Monitor

```bash
python3 -m platformio device monitor -b 115200
```

Useful serial hotkeys:
- **Ctrl+D** on device: dump full diagnostics
- **Ctrl+T** on device: send radio test packet
- **Ctrl+R** on device: 5-second RSSI sampling

## Settings

Press **Ctrl+S** to access settings:
- **Radio**: frequency, spreading factor, bandwidth, coding rate, TX power
- **WiFi**: mode (OFF/AP/STA), AP SSID + password, STA SSID + password, TCP endpoints
- **Display**: brightness (0–255), dim timeout, off timeout
- **Audio**: enable/disable, volume (0–100)
- **About**: version, identity hash, uptime, factory reset

Changes take effect immediately and persist to both flash and SD.

## Connecting Two Ratputers

Two Ratputers on the same LoRa settings will discover each other automatically:

1. Power on both devices
2. Wait ~30 seconds for announces to propagate
3. Check the **Nodes** tab — the other device should appear with its identity hash, RSSI, and SNR
4. Select the node → opens a conversation in Messages
5. Type a message and press Enter

Both devices must use the same frequency, spreading factor, bandwidth, and coding rate. The defaults (915 MHz, SF7, BW 500kHz, CR 4/5) work out of the box.

## Connecting to a Desktop Reticulum Instance

### Option A: AP Mode Bridge (Ratputer as hotspot)

1. Leave Ratputer in AP mode (default)
2. On your laptop, connect to `ratputer-XXXX` (password: `ratspeak`)
3. Add to `~/.reticulum/config`:
   ```ini
   [[ratputer]]
     type = TCPClientInterface
     target_host = 192.168.4.1
     target_port = 4242
   ```
4. Restart `rnsd` — your desktop is now on the LoRa mesh

### Option B: STA Mode (Ratputer joins your WiFi)

1. Switch Ratputer to STA mode (Ctrl+S → WiFi → Mode → STA)
2. Enter your WiFi SSID and password
3. On your laptop (same network), configure Ratputer as a TCP server interface in `~/.reticulum/config`:
   ```ini
   [[ratputer]]
     type = TCPServerInterface
     listen_ip = 0.0.0.0
     listen_port = 4242
   ```
4. On Ratputer, add a TCP endpoint pointing to your laptop's IP and port 4242
5. Both sides can now exchange Reticulum traffic over WiFi
