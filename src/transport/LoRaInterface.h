#pragma once

#include <Interface.h>
#include "radio/SX1262.h"
#include <deque>

class LoRaInterface : public RNS::InterfaceImpl {
public:
    LoRaInterface(SX1262* radio, const char* name = "LoRaInterface");
    virtual ~LoRaInterface();

    virtual bool start() override;
    virtual void stop() override;
    virtual void loop() override;

    virtual inline std::string toString() const override {
        return "LoRaInterface[" + _name + "]";
    }

    float airtimeUtilization() const;

    // Last received packet signal quality
    int lastRxRssi() const { return _lastRxRssi; }
    float lastRxSnr() const { return _lastRxSnr; }
    bool isOnline() const { return _online; }

protected:
    virtual void send_outgoing(const RNS::Bytes& data) override;

private:
    void transmitNow(const RNS::Bytes& data);

    SX1262* _radio;
    bool _txPending = false;
    RNS::Bytes _txData;

    // TX queue: buffer packets when radio is busy instead of dropping
    static constexpr int TX_QUEUE_MAX = 4;
    std::deque<RNS::Bytes> _txQueue;

    // Split-packet TX state: when a packet > 254 bytes, send in two LoRa frames
    bool _splitTxPending = false;
    RNS::Bytes _splitTxRemaining;
    uint8_t _splitTxHeader = 0;

    // Split-packet RX state: reassemble two LoRa frames into one Reticulum packet
    static constexpr unsigned long SPLIT_RX_TIMEOUT_MS = 5000;
    bool _splitRxPending = false;
    uint8_t _splitRxSeq = 0;
    RNS::Bytes _splitRxBuffer;
    unsigned long _splitRxTimestamp = 0;

    int _lastRxRssi = 0;
    float _lastRxSnr = 0;

    unsigned long _airtimeWindowStart = 0;
    float _airtimeAccumMs = 0;
    static constexpr unsigned long AIRTIME_WINDOW_MS = 60000;
public:
    static constexpr float AIRTIME_THROTTLE = 0.25f;
};
