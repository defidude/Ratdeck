#pragma once

#include <Interface.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <vector>

class WiFiInterface : public RNS::InterfaceImpl {
public:
    WiFiInterface(const char* name = "WiFiInterface");
    virtual ~WiFiInterface();

    virtual bool start() override;
    virtual void stop() override;
    virtual void loop() override;

    virtual inline std::string toString() const override {
        return "WiFiInterface[" + _name + "]";
    }

    // Full WiFi shutdown (AP + STA + radio off)
    void stopFull();

    // AP config
    void setAPCredentials(const char* ssid, const char* password);
    String getAPSSID() const { return _apSSID; }
    int getClientCount() const { return _clients.size(); }
    bool isAPActive() const { return _apActive; }

    // STA config (optional)
    void setSTACredentials(const char* ssid, const char* password);
    bool isSTAConnected() const;

    // WiFi scanner
    struct ScanResult {
        String ssid;
        int rssi;
        bool encrypted;
    };
    static std::vector<ScanResult> scanNetworks(int maxResults = 15);

protected:
    virtual void send_outgoing(const RNS::Bytes& data) override;

private:
    void startAP();
    void acceptClients();
    void readClients();
    void sendToClients(const uint8_t* data, size_t len);

    // HDLC-like framing for TCP stream
    void sendFrame(WiFiClient& client, const uint8_t* data, size_t len);
    int readFrame(WiFiClient& client, uint8_t* buffer, size_t maxLen);

    String _apSSID;
    String _apPassword;
    String _staSSID;
    String _staPassword;
    bool _apActive = false;

    WiFiServer _server;
    std::vector<WiFiClient> _clients;
    uint8_t _rxBuffer[600];

    static constexpr uint8_t FRAME_START = 0x7E;
    static constexpr uint8_t FRAME_ESC   = 0x7D;
    static constexpr uint8_t FRAME_XOR   = 0x20;
};
