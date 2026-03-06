#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include <functional>

// Sideband-compatible BLE GATT service
// Implements KISS-framed serial interface over BLE for Sideband app pairing.
// The Sideband app expects an RNode-compatible BLE serial profile using
// Nordic UART Service (NUS) UUIDs.
class BLESideband : public NimBLECharacteristicCallbacks {
public:
    using PacketCallback = std::function<void(const uint8_t* data, size_t len)>;

    bool begin(NimBLEServer* existingServer = nullptr);
    void loop();
    void stop();

    bool isConnected() const { return _connected; }

    // Send a packet (will be KISS-framed)
    void sendPacket(const uint8_t* data, size_t len);

    // Callback for received packets (after KISS deframing)
    void setPacketCallback(PacketCallback cb) { _packetCb = cb; }

    // Connection events (called by BLEInterface which owns server callbacks)
    void notifyConnect();
    void notifyDisconnect();

    // NimBLE characteristic callback
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;

private:
    void processRxByte(uint8_t b);

    NimBLEServer* _pServer = nullptr;
    NimBLEService* _pService = nullptr;
    NimBLECharacteristic* _pTxChar = nullptr;
    NimBLECharacteristic* _pRxChar = nullptr;
    bool _ownServer = false;
    bool _connected = false;

    // KISS framing
    std::vector<uint8_t> _rxFrame;
    bool _rxInFrame = false;
    bool _rxEscaped = false;

    // Queued incoming packets
    std::vector<std::vector<uint8_t>> _incomingPackets;
    PacketCallback _packetCb;

    // KISS constants
    static constexpr uint8_t KISS_FEND  = 0xC0;
    static constexpr uint8_t KISS_FESC  = 0xDB;
    static constexpr uint8_t KISS_TFEND = 0xDC;
    static constexpr uint8_t KISS_TFESC = 0xDD;
    static constexpr uint8_t KISS_CMD_DATA = 0x00;

    // Nordic UART Service UUIDs (compatible with Sideband/RNode BLE)
    static constexpr const char* NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
};
