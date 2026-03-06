# Ratputer — Hardware Pin Map

M5Stack Cardputer Adv (ESP32-S3) + Cap LoRa-1262

All pin definitions are in `src/config/BoardConfig.h`.

## Bus Overview

```
ESP32-S3
   ├── HSPI (shared bus) ──┬── SX1262 LoRa (CS=5)
   │    SCK=40              └── SD Card (CS=12)
   │    MISO=39
   │    MOSI=14
   │
   ├── I2C ── TCA8418 Keyboard (SDA=8, SCL=9, INT=11)
   │
   ├── UART (reserved) ── GNSS module (RX=15, TX=13)
   │
   ├── USB-Serial/JTAG ── USB-C port (firmware + debug)
   │
   └── M5Unified managed ──┬── ST7789V2 Display (SPI)
                            ├── ES8311 Audio Codec (I2S)
                            └── Battery ADC
```

## SX1262 LoRa Radio

Uses **HSPI** (custom SPI bus, not the default VSPI):

| Signal | GPIO | Notes |
|--------|------|-------|
| SCK | 40 | SPI clock |
| MISO | 39 | SPI data out (radio → ESP) |
| MOSI | 14 | SPI data in (ESP → radio) |
| CS | 5 | Chip select (active low) |
| IRQ | 4 | DIO1 interrupt (falling edge) |
| RST | 3 | Reset (active low, 100μs pulse) |
| BUSY | 6 | Poll before SPI transactions |
| RXEN | -1 | Not connected (DIO2 used as RF switch) |
| TXEN | -1 | Not connected |

**Radio configuration:**
- TCXO voltage: 3.0V (`MODE_TCXO_3_0V_6X` = 0x06)
- DIO2 as RF switch: enabled
- SPI clock: 8 MHz

## SD Card

| Signal | GPIO | Notes |
|--------|------|-------|
| CS | 12 | Separate from LoRa CS (5) |

Shares HSPI bus with LoRa radio (SCK=40, MISO=39, MOSI=14). Only one device active at a time — SD must be initialized **after** radio.

## Keyboard (TCA8418)

| Signal | GPIO | Notes |
|--------|------|-------|
| SDA | 8 | I2C data |
| SCL | 9 | I2C clock |
| INT | 11 | Active low, falling edge |

Managed by the M5Cardputer library. The TCA8418 is a dedicated keyboard controller IC with built-in key matrix scanning and FIFO.

## GNSS (Reserved — v1.1)

| Signal | GPIO | Notes |
|--------|------|-------|
| RX | 15 | GPS TX → ESP RX |
| TX | 13 | ESP TX → GPS RX |

UART at 115200 baud. Pins defined but no code path yet.

## Display

**ST7789V2** — 240×135 pixels, RGB565, SPI interface.

Fully managed by M5Unified. No GPIO definitions needed in firmware — the M5Unified library auto-configures display pins based on board detection.

## Audio

**ES8311** codec + **NS4150B** amplifier.

Fully managed by M5Unified. No GPIO definitions needed.

## Battery

ADC via M5Unified. 1750mAh LiPo, TP4057 charger IC.

## SPI Bus Sharing

The HSPI bus is shared between the SX1262 radio (CS=5) and the SD card (CS=12). Key constraints:

1. **Initialize radio first** — SD card init must come after `radio.begin()` since the SPI bus is configured during radio init
2. **One active at a time** — pull CS high on the inactive device before talking to the other
3. **Radio has priority** — if a packet arrives during SD access, there may be a brief delay before the ISR fires

## Hardware Constants

| Constant | Value | Notes |
|----------|-------|-------|
| `MAX_PACKET_SIZE` | 255 | SX1262 maximum single packet |
| `SPI_FREQUENCY` | 8 MHz | SPI clock for SX1262 |
| `DISPLAY_WIDTH` | 240 | Pixels |
| `DISPLAY_HEIGHT` | 135 | Pixels |
| `GPS_BAUD` | 115200 | GNSS UART speed |
