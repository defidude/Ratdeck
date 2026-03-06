# Ratputer — Hotkey Reference

All hotkeys use **Ctrl+key** combinations.

| Shortcut | Action |
|----------|--------|
| Ctrl+H | Toggle help overlay |
| Ctrl+M | Jump to Messages tab |
| Ctrl+N | New message |
| Ctrl+S | Jump to Settings tab |
| Ctrl+A | Force announce to network |
| Ctrl+D | Dump diagnostics to serial |
| Ctrl+T | Send radio test packet (FIFO verification) |
| Ctrl+R | RSSI monitor (5-second continuous sampling) |

## Navigation

These keys match the physical arrow key positions on the Cardputer Adv keyboard:

| Key | Action |
|-----|--------|
| `;` (semicolon) | Scroll up / previous item |
| `.` (period) | Scroll down / next item |
| `,` (comma) | Previous tab |
| `/` (slash) | Next tab |
| Enter | Select / confirm / send |
| Esc | Back / cancel |

## Text Input

When a text input field is active:
- Type normally to enter characters
- **Backspace** to delete
- **Enter** to submit
- **Esc** to cancel
- Double-tap **Aa** for caps lock

## Tabs

| Tab | Contents |
|-----|----------|
| Home | Identity, transport status, radio info, uptime |
| Msgs | Conversation list with unread badges |
| Nodes | Discovered Reticulum nodes |
| Setup | Settings, about, factory reset |

## Serial Diagnostics

**Ctrl+D** prints to serial (115200 baud):
- Identity hash, transport status, path/link counts
- Radio parameters (freq, SF, BW, CR, TX power, preamble)
- SX1262 register dump (sync word, IQ, LNA, OCP, TX clamp)
- Device errors, current RSSI
- Free heap, flash usage, uptime

**Ctrl+T** sends a test packet with header `0xA0` and payload `RATPUTER_TEST_1234567890`, then reads back the FIFO buffer to verify the TX path.

**Ctrl+R** samples RSSI continuously for 5 seconds, printing each reading. Transmit from another device during sampling to verify the RX front-end is hearing RF.

## Settings Submenus (Ctrl+S)

| Menu Item | Contents |
|-----------|----------|
| Radio | Frequency (Hz), spreading factor (5–12), bandwidth (7.8k–500k), coding rate (4/5–4/8), TX power (2–22 dBm) |
| WiFi | Mode (OFF/AP/STA), AP SSID + password, STA SSID + password, TCP endpoints list |
| Display | Brightness (0–255), dim timeout (seconds), off timeout (seconds) |
| Audio | Enable/disable notifications, volume (0–100%) |
| About | Firmware version, Reticulum identity hash, uptime, free heap, flash usage |
| Factory Reset | Clears config from flash + SD, reboots with defaults (identity and messages preserved) |
