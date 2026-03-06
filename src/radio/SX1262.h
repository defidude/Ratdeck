#pragma once

// =============================================================================
// SX1262 LoRa Radio Driver — Direct port from Ratputer
// Pin assignments come from BoardConfig.h (T-Deck Plus pins)
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include "RadioConstants.h"
#include "config/BoardConfig.h"

class SX1262 {
public:
    SX1262(SPIClass* spi, int ss, int sclk, int mosi, int miso,
           int reset, int irq, int busy, int rxen = -1,
           bool tcxo = true, bool dio2_as_rf_switch = true);

    // --- Lifecycle ---
    bool begin(uint32_t frequency);
    void end();

    // --- TX ---
    int  beginPacket(int implicitHeader = 0);
    int  endPacket();
    size_t write(uint8_t byte);
    size_t write(const uint8_t* buffer, size_t size);

    // --- RX ---
    void receive(int size = 0);
    int  available();
    int  read();
    int  peek();
    int  parsePacket(int size = 0);
    void readBytes(uint8_t* buffer, size_t size);

    // --- Configuration ---
    void setFrequency(uint32_t frequency);
    uint32_t getFrequency();
    void setTxPower(int level);
    int8_t getTxPower();
    void setSpreadingFactor(int sf);
    uint8_t getSpreadingFactor();
    void setSignalBandwidth(uint32_t sbw);
    uint32_t getSignalBandwidth();
    void setCodingRate4(int denominator);
    uint8_t getCodingRate4();
    void setPreambleLength(long length);
    void enableCrc();
    void disableCrc();

    // --- Status ---
    int  currentRssi();
    int  packetRssi();
    float packetSnr();
    bool isRadioOnline() { return _radioOnline; }
    long getPreambleLength() const { return _preambleLength; }
    uint8_t readRegister(uint16_t address);
    uint16_t getDeviceErrors();
    void clearDeviceErrors();
    uint8_t getStatus();
    uint16_t getIrqFlags();

    // --- FIFO access ---
    void readBuffer(uint8_t* buffer, size_t size);
    const uint8_t* packetBuffer() const { return _packet; }

    // --- Interrupt-driven RX ---
    void onReceive(void(*callback)(int));

    // --- Power ---
    void standby();
    void sleep();

    // --- Misc ---
    uint8_t random();

private:
    bool preInit();
    void reset();
    void writeRegister(uint16_t address, uint8_t value);
    uint8_t singleTransfer(uint8_t opcode, uint16_t address, uint8_t value);
    void executeOpcode(uint8_t opcode, uint8_t* buffer, uint8_t size);
    void executeOpcodeRead(uint8_t opcode, uint8_t* buffer, uint8_t size);
    void writeBuffer(const uint8_t* buffer, size_t size);
    void waitOnBusy();

    void loraMode();
    void rxAntEnable();
    void calibrate();
    void calibrate_image(uint32_t frequency);
    void enableTCXO();
    void setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro);
    void setPacketParams(uint32_t preamble, uint8_t headermode, uint8_t length, uint8_t crc);
    void setSyncWord(uint16_t sw);
    void explicitHeaderMode();
    void implicitHeaderMode();
    void handleLowDataRate();

    float getAirtime(uint16_t written);

    void handleDio0Rise();
    bool getPacketValidity();
    static void IRAM_ATTR onDio0Rise();

    SPISettings _spiSettings;
    SPIClass*   _spiModem;
    int _ss, _sclk, _mosi, _miso;
    int _reset, _irq, _busy, _rxen;

    uint32_t _frequency = 0;
    uint8_t  _sf = 0;
    uint8_t  _bw = 0;
    uint8_t  _cr = 0;
    int8_t   _txp = 0;
    bool     _ldro = false;
    long     _preambleLength = 0;

    int  _packetIndex = 0;
    int  _implicitHeaderMode = 0;
    int  _payloadLength = 0;
    int  _crcMode = 0;
    int  _fifo_tx_addr_ptr = 0;
    int  _fifo_rx_addr_ptr = 0;

    bool _preinitDone = false;
    bool _radioOnline = false;
    bool _tcxo = false;
    bool _dio2_as_rf_switch = false;

    uint8_t _packet[MAX_PACKET_SIZE] = {};
    void (*_onReceive)(int) = nullptr;

    unsigned long _preambleDetectedAt = 0;
    long _loraPreambleTimeMs = 0;
    long _loraHeaderTimeMs = 0;
    float _loraSymbolTimeMs = 0;

    static SX1262* _instance;
};
