# Ratputer — Build & Flash Reference

## Prerequisites

- **Python 3.12+** (for PlatformIO and esptool)
- **PlatformIO Core** (CLI): `pip install platformio`
- **esptool** (usually installed with PlatformIO, or `pip install esptool`)
- **Git**

No USB drivers needed on macOS or Linux — the ESP32-S3's USB-Serial/JTAG interface is natively supported.

> **Note**: PlatformIO may not be on your PATH after pip install. Use `python3 -m platformio` if `pio` is not found. This applies to all `pio` commands throughout this document.

## Build

```bash
python3 -m platformio run -e ratputer_915
```

Output binary: `.pio/build/ratputer_915/firmware.bin`

First build downloads all dependencies automatically (M5Unified, M5GFX, M5Cardputer, microReticulum, Crypto, ArduinoJson).

## Flash

### Via PlatformIO (simple)

```bash
python3 -m platformio run -e ratputer_915 -t upload --upload-port /dev/cu.usbmodem*
```

### Via esptool (more reliable)

PlatformIO defaults to 921600 baud which sometimes fails. esptool at 460800 is more reliable:

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* --baud 460800 \
    --before default-reset --after hard-reset \
    write-flash -z 0x10000 .pio/build/ratputer_915/firmware.bin
```

### Creating a Merged Binary

A merged binary includes bootloader + partition table + app in one file for clean flashing:

```bash
python3 -m esptool --chip esp32s3 merge-bin \
    --output ratputer_merged.bin \
    --flash-mode dio --flash-size 8MB \
    0x0    ~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/bin/bootloader_dio_80m.bin \
    0x8000 .pio/build/ratputer_915/partitions.bin \
    0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
    0x10000 .pio/build/ratputer_915/firmware.bin

python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* --baud 460800 \
    --before default-reset --after hard-reset \
    write-flash 0x0 ratputer_merged.bin
```

## Serial Monitor

```bash
python3 -m platformio device monitor -b 115200
```

Or with any serial terminal at 115200 baud.

### Serial WIPE Command

Within the first 500ms of boot, send `WIPE` over serial to wipe the SD card's `/ratputer/` directory. Useful for factory reset of stored messages, contacts, and config.

## USB Port Identification

The ESP32-S3 on Cardputer Adv uses USB-Serial/JTAG (not a separate UART chip):

| State | Port Name | Notes |
|-------|-----------|-------|
| Firmware mode | `/dev/cu.usbmodem<ID>` | Normal operation, serial + flashing |
| Bootloader mode | `/dev/cu.usbmodem5101` | Hold G0 during boot, esptool only |

The firmware-mode port name includes the chip's unique ID (e.g., `/dev/cu.usbmodem3C0F02E81B4C1`). Use `/dev/cu.usbmodem*` glob to match either.

## Build Flags

From `platformio.ini`:

| Flag | Purpose |
|------|---------|
| `-fexceptions` | Enable C++ exceptions (required by microReticulum) |
| `-DRATPUTER=1` | Main feature flag — guards all Ratputer-specific code |
| `-DARDUINO_USB_CDC_ON_BOOT=1` | USB CDC serial on boot (USBMode=default) |
| `-DARDUINO_USB_MODE=1` | USB mode 1 = USB-Serial/JTAG (not native CDC) |
| `-DRNS_USE_FS` | microReticulum: use filesystem for persistence |
| `-DRNS_PERSIST_PATHS` | microReticulum: persist transport paths to flash |
| `-DMSGPACK_USE_BOOST=OFF` | Disable Boost dependency in MsgPack |

`build_unflags = -fno-exceptions` removes the Arduino default no-exceptions flag.

## Partition Table

`partitions_8MB_ota.csv` — 8MB flash layout with OTA support:

| Name | Type | Offset | Size | Purpose |
|------|------|--------|------|---------|
| nvs | data/nvs | 0x9000 | 20 KB | Non-volatile storage (boot counter, WiFi creds) |
| otadata | data/ota | 0xE000 | 8 KB | OTA boot selection |
| app0 | app/ota_0 | 0x10000 | 3 MB | Primary firmware |
| app1 | app/ota_1 | 0x310000 | 3 MB | OTA update slot (reserved) |
| littlefs | data/spiffs | 0x610000 | 1.875 MB | LittleFS — identity, config, messages, paths |
| coredump | data/coredump | 0x7F0000 | 64 KB | ESP-IDF core dump on crash |

## CI/CD

GitHub Actions workflow (`.github/workflows/build.yml`):

- **Build**: Triggers on push to `main` and PRs. Runs `pio run`, uploads `firmware.bin` as artifact.
- **Release**: Triggers on `v*` tags. Builds firmware and creates a GitHub Release with the binary attached.

## Dependencies

All managed by PlatformIO's `lib_deps`:

| Library | Source | Purpose |
|---------|--------|---------|
| microReticulum | github.com/attermann/microReticulum | Reticulum protocol (C++) |
| Crypto | github.com/attermann/Crypto | Ed25519, X25519, AES, SHA256 |
| ArduinoJson | bblanchon/ArduinoJson ^7.4.2 | Config serialization |
| M5Unified | m5stack/M5Unified | Hardware abstraction |
| M5GFX | m5stack/M5GFX | Display + canvas rendering |
| M5Cardputer | m5stack/M5Cardputer | Keyboard (TCA8418) |

## Erasing Flash

To completely erase the ESP32-S3 flash (useful if LittleFS is corrupted or you want a clean start):

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* erase-flash
```

After erasing, you must reflash the firmware. LittleFS will auto-format on first boot, and a new identity will be generated.

## Common Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `A]Fatal error occurred: Could not open port` | Device not connected or wrong port | Check USB cable, try `/dev/cu.usbmodem*` glob |
| `Timed out waiting for packet header` | Baud rate too high for USB-Serial/JTAG | Use `--baud 460800` with esptool |
| `No such option: --upload-port` | Old PlatformIO version | `pip install -U platformio` |
| `ImportError: No module named platformio` | PlatformIO not installed for this Python | `pip install platformio` or use the correct `python3` |
| `pio: command not found` | PlatformIO not on PATH | Use `python3 -m platformio` instead |
| `Error: Bootloader binary size ... exceeds` | Partition mismatch | Ensure `partitions_8MB_ota.csv` is present in repo root |

## macOS vs Linux Ports

| OS | Firmware Port | Bootloader Port |
|----|---------------|-----------------|
| macOS | `/dev/cu.usbmodem<chipID>` | `/dev/cu.usbmodem5101` |
| Linux | `/dev/ttyACM0` (typical) | `/dev/ttyACM0` |

On Linux, you may need to add your user to the `dialout` group: `sudo usermod -aG dialout $USER` (then log out and back in).
