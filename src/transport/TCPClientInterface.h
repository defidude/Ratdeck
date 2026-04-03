#pragma once

#include <Interface.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <vector>

class TCPClientInterface : public RNS::InterfaceImpl {
public:
    TCPClientInterface(const char* host, uint16_t port, const char* name);
    virtual ~TCPClientInterface();

    bool start() override;
    void stop() override;
    void loop() override;

    virtual inline std::string toString() const override {
        return "TCPClient[" + _name + "]";
    }

    bool isConnected() { return _client.connected(); }
    const String& host() const { return _host; }
    uint16_t port() const { return _port; }

protected:
    void send_outgoing(const RNS::Bytes& data) override;

private:
    void tryConnect();
    void sendFrame(const uint8_t* data, size_t len);
    int readFrame();

    WiFiClient _client;
    String _host;
    uint16_t _port;
    unsigned long _lastAttempt = 0;
    unsigned long _lastRxTime = 0;
    uint8_t* _rxBuffer = nullptr;
    uint8_t* _txBuffer = nullptr;    // PSRAM-allocated send frame buffer
    uint8_t* _wrapBuffer = nullptr;  // PSRAM-allocated packet rewrite buffer
    static constexpr size_t RX_BUFFER_SIZE = 2048;
    static constexpr size_t TX_BUFFER_SIZE = RX_BUFFER_SIZE * 2 + 2;

    // Hub transport_id for Header2 wrapping (learned from incoming Header2 packets)
    uint8_t _hubTransportId[16] = {};
    bool _hubTransportIdKnown = false;

    // Telemetry counters
    unsigned long _hubRxCount = 0;
    unsigned long _txDropCount = 0;

    // Pending announces: buffered until hub transport_id is learned
    std::vector<RNS::Bytes> _pendingAnnounces;

    // Persistent HDLC frame reassembly state (survives across loop() calls)
    bool _inFrame = false;
    bool _escaped = false;
    size_t _rxPos = 0;

    static constexpr uint8_t FRAME_START = 0x7E;
    static constexpr uint8_t FRAME_ESC   = 0x7D;
    static constexpr uint8_t FRAME_XOR   = 0x20;
    static constexpr unsigned long TCP_KEEPALIVE_TIMEOUT_MS = 300000; // 5 min
    static constexpr unsigned long TCP_LOOP_BUDGET_MS = 25;

public:
    unsigned long lastRxTime() const { return _lastRxTime; }
    unsigned long hubRxCount() const { return _hubRxCount; }
};
