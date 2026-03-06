#pragma once

#include <Interface.h>
#include <NimBLEDevice.h>
#include <vector>

// BLE transport interface for Reticulum packet relay
// NimBLE GATT server with RX/TX characteristics, HDLC framing
class BLEInterface : public RNS::InterfaceImpl,
                     public NimBLEServerCallbacks,
                     public NimBLECharacteristicCallbacks {
public:
    BLEInterface(const char* name = "BLEInterface");
    virtual ~BLEInterface();

    bool start() override;
    void stop() override;
    void loop() override;

    virtual inline std::string toString() const override {
        return "BLEInterface[" + _name + "]";
    }

    bool isActive() const { return _active; }
    bool isClientConnected() const { return _connected; }

    // Get server for sharing with BLESideband (only valid after start())
    NimBLEServer* getServer() { return _pServer; }

    // Set companion Sideband service for connection event forwarding
    void setSideband(class BLESideband* sb) { _sideband = sb; }

    // Inject a packet from external source (e.g. Sideband) into Reticulum
    void injectIncoming(const RNS::Bytes& data);

    // NimBLE server callbacks
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;

    // NimBLE characteristic callbacks
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;

protected:
    void send_outgoing(const RNS::Bytes& data) override;

private:
    // HDLC framing (same as WiFi/TCP interfaces)
    void sendFrame(const uint8_t* data, size_t len);
    void processRxByte(uint8_t b);

    NimBLEServer* _pServer = nullptr;
    NimBLEService* _pService = nullptr;
    NimBLECharacteristic* _pTxChar = nullptr;
    NimBLECharacteristic* _pRxChar = nullptr;

    bool _active = false;
    bool _connected = false;

    // HDLC rx state
    std::vector<uint8_t> _rxFrame;
    bool _rxEscape = false;
    bool _rxActive = false;

    // Queued incoming frames (written by BLE callback, consumed by loop)
    std::vector<std::vector<uint8_t>> _incomingFrames;

    class BLESideband* _sideband = nullptr;

    static constexpr uint8_t FRAME_START = 0x7E;
    static constexpr uint8_t FRAME_ESC   = 0x7D;
    static constexpr uint8_t FRAME_XOR   = 0x20;

    // Custom UUIDs for Ratdeck BLE transport
    static constexpr const char* SERVICE_UUID = "e2f0a5b1-c3d4-4e56-8f90-1a2b3c4d5e6f";
    static constexpr const char* TX_CHAR_UUID = "e2f0a5b2-c3d4-4e56-8f90-1a2b3c4d5e6f";
    static constexpr const char* RX_CHAR_UUID = "e2f0a5b3-c3d4-4e56-8f90-1a2b3c4d5e6f";
};
