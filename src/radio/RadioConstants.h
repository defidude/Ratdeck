#pragma once

// =============================================================================
// SX1262 Register Addresses, Opcodes, and Constants
// Direct port from Ratputer (extracted from RNode_Firmware_CE)
// =============================================================================

#include <cmath>

// --- Opcodes ---
#define OP_RF_FREQ_6X               0x86
#define OP_SLEEP_6X                 0x84
#define OP_STANDBY_6X               0x80
#define OP_TX_6X                    0x83
#define OP_RX_6X                    0x82
#define OP_PA_CONFIG_6X             0x95
#define OP_SET_IRQ_FLAGS_6X         0x08
#define OP_CLEAR_IRQ_STATUS_6X      0x02
#define OP_GET_IRQ_STATUS_6X        0x12
#define OP_RX_BUFFER_STATUS_6X      0x13
#define OP_PACKET_STATUS_6X         0x14
#define OP_CURRENT_RSSI_6X          0x15
#define OP_MODULATION_PARAMS_6X     0x8B
#define OP_PACKET_PARAMS_6X         0x8C
#define OP_STATUS_6X                0xC0
#define OP_TX_PARAMS_6X             0x8E
#define OP_PACKET_TYPE_6X           0x8A
#define OP_BUFFER_BASE_ADDR_6X      0x8F
#define OP_READ_REGISTER_6X         0x1D
#define OP_WRITE_REGISTER_6X        0x0D
#define OP_DIO3_TCXO_CTRL_6X        0x97
#define OP_DIO2_RF_CTRL_6X          0x9D
#define OP_CAD_PARAMS               0x88
#define OP_CALIBRATE_6X             0x89
#define OP_RX_TX_FALLBACK_MODE_6X   0x93
#define OP_REGULATOR_MODE_6X        0x96
#define OP_CALIBRATE_IMAGE_6X       0x98
#define OP_GET_DEVICE_ERRORS_6X     0x17
#define OP_CLEAR_DEVICE_ERRORS_6X   0x07

// --- FIFO ---
#define OP_FIFO_WRITE_6X            0x0E
#define OP_FIFO_READ_6X             0x1E

// --- Calibration ---
#define MASK_CALIBRATE_ALL          0x7F

// --- IRQ Masks ---
#define IRQ_TX_DONE_MASK_6X             0x01
#define IRQ_RX_DONE_MASK_6X             0x02
#define IRQ_PREAMBLE_DET_MASK_6X        0x04
#define IRQ_HEADER_DET_MASK_6X          0x10
#define IRQ_PAYLOAD_CRC_ERROR_MASK_6X   0x40
#define IRQ_ALL_MASK_6X                 0b0100001111111111

// --- Register Addresses ---
#define REG_OCP_6X                  0x08E7
#define REG_LNA_6X                  0x08AC
#define REG_SYNC_WORD_MSB_6X        0x0740
#define REG_SYNC_WORD_LSB_6X        0x0741
#define REG_PAYLOAD_LENGTH_6X       0x0702
#define REG_RANDOM_GEN_6X           0x0819
#define REG_IQ_POLARITY_6X          0x0736
#define REG_TX_CLAMP_CONFIG_6X      0x08D8

// --- Modes ---
#define MODE_LONG_RANGE_MODE_6X     0x01
#define MODE_STDBY_RC_6X            0x00
#define MODE_STDBY_XOSC_6X          0x01
#define MODE_FALLBACK_STDBY_RC_6X   0x20
#define MODE_FALLBACK_STDBY_XOSC_6X 0x30
#define MODE_IMPLICIT_HEADER        0x01
#define MODE_EXPLICIT_HEADER        0x00

// --- TCXO Voltage Settings ---
#define MODE_TCXO_3_3V_6X           0x07
#define MODE_TCXO_3_0V_6X           0x06
#define MODE_TCXO_2_7V_6X           0x05
#define MODE_TCXO_2_4V_6X           0x04
#define MODE_TCXO_2_2V_6X           0x03
#define MODE_TCXO_1_8V_6X           0x02
#define MODE_TCXO_1_7V_6X           0x01
#define MODE_TCXO_1_6V_6X           0x00

// --- Sync Word ---
#define SYNC_WORD_6X                0x1424

// --- OCP ---
#define OCP_TUNED                   0x38

// --- Frequency Calculation ---
#define XTAL_FREQ_6X    (double)32000000
#define FREQ_DIV_6X     (double)pow(2.0, 25.0)
#define FREQ_STEP_6X    (double)(XTAL_FREQ_6X / FREQ_DIV_6X)

// --- TX Timeout Multiplier ---
#define MODEM_TIMEOUT_MULT          1.5

// --- LoRa PHY Constants ---
#define PHY_HEADER_LORA_SYMBOLS     8
#define PHY_CRC_LORA_BITS           16
#define LORA_PREAMBLE_SYMBOLS_MIN   18
