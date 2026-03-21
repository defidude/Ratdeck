#include "LoRaInterface.h"
#include "config/BoardConfig.h"
#include <algorithm>

// RNode on-air framing constants (from RNode_Firmware_CE Framing.h / Config.h)
// Every LoRa packet has a 1-byte header: upper nibble = random sequence, lower nibble = flags
#define RNODE_HEADER_L      1
#define RNODE_FLAG_SPLIT    0x01
#define RNODE_NIBBLE_SEQ    0xF0

LoRaInterface::LoRaInterface(SX1262* radio, const char* name)
    : RNS::InterfaceImpl(name), _radio(radio)
{
    _IN = true;
    _OUT = true;
    _bitrate = 2000;        // Approximate for SF8/125kHz
    _HW_MTU = MAX_PACKET_SIZE - RNODE_HEADER_L;  // 254 bytes payload (1 byte reserved for RNode header)
}

LoRaInterface::~LoRaInterface() {
    stop();
}

bool LoRaInterface::start() {
    if (!_radio || !_radio->isRadioOnline()) {
        Serial.println("[LORA_IF] Radio not available");
        _online = false;
        return false;
    }
    _online = true;
    _radio->receive();
    Serial.println("[LORA_IF] Interface started");
    return true;
}

void LoRaInterface::stop() {
    _online = false;
    Serial.println("[LORA_IF] Interface stopped");
}

void LoRaInterface::send_outgoing(const RNS::Bytes& data) {
    if (!_online || !_radio) return;

    if (_txPending) {
        Serial.println("[LORA_IF] TX busy, dropping packet");
        return;
    }

    // Reject packets that exceed the LoRa MTU (255 bytes including 1-byte header).
    // Sending truncated packets corrupts encryption (HMAC failures on receiver).
    if (data.size() + RNODE_HEADER_L > MAX_PACKET_SIZE) {
        Serial.printf("[LORA_IF] TX DROPPED: packet too large (%d + %d = %d > %d)\n",
            (int)data.size(), RNODE_HEADER_L,
            (int)(data.size() + RNODE_HEADER_L), MAX_PACKET_SIZE);
        return;
    }

    // Build RNode-compatible 1-byte header:
    //   Upper nibble: random sequence number (for split-packet tracking)
    //   Lower nibble: flags (FLAG_SPLIT=0x01 if packet won't fit in single frame)
    uint8_t header = (uint8_t)(random(256)) & RNODE_NIBBLE_SEQ;  // Random upper nibble, flags=0

    Serial.printf("[LORA_IF] TX: sending %d bytes, radio: SF%d BW%lu CR%d preamble=%ld freq=%lu txp=%d\n",
        data.size(),
        _radio->getSpreadingFactor(),
        (unsigned long)_radio->getSignalBandwidth(),
        _radio->getCodingRate4(),
        _radio->getPreambleLength(),
        (unsigned long)_radio->getFrequency(),
        _radio->getTxPower());

    _radio->beginPacket();
    _radio->write(header);                        // 1-byte RNode header
    _radio->write(data.data(), data.size());      // Reticulum packet payload
    _radio->endPacket(true);                      // Async: start TX and return immediately

    _txPending = true;
    _txData = data;
    InterfaceImpl::handle_outgoing(data);
    Serial.printf("[LORA_IF] TX %d+1 bytes queued (hdr=0x%02X)\n", data.size(), header);

    // Track airtime
    float airtimeMs = _radio->getAirtime(data.size() + RNODE_HEADER_L);
    unsigned long txNow = millis();
    if (txNow - _airtimeWindowStart >= AIRTIME_WINDOW_MS) {
        _airtimeAccumMs = 0;
        _airtimeWindowStart = txNow;
    } else {
        float elapsed = (float)(txNow - _airtimeWindowStart);
        float remaining = 1.0f - (elapsed / AIRTIME_WINDOW_MS);
        if (remaining < 0) remaining = 0;
        _airtimeAccumMs *= remaining;
        _airtimeWindowStart = txNow;
    }
    _airtimeAccumMs += airtimeMs;
    Serial.printf("[LORA_IF] TX airtime: %.1fms (util=%.1f%%)\n", airtimeMs, airtimeUtilization() * 100);
}

void LoRaInterface::loop() {
    if (!_online || !_radio) return;

    // Handle async TX completion
    if (_txPending) {
        if (!_radio->isTxBusy()) {
            _txPending = false;
            _txData = RNS::Bytes();
            _radio->receive();
        }
        return;  // Don't process RX while TX is active
    }

    // Periodic RX debug: dump RSSI + chip status every 30 seconds
    static unsigned long lastRxDebug = 0;
    if (millis() - lastRxDebug > 30000) {
        lastRxDebug = millis();
        int rssi = _radio->currentRssi();
        uint8_t status = _radio->getStatus();
        uint8_t chipMode = (status >> 4) & 0x07;
        Serial.printf("[LORA_IF] RX: RSSI=%d dBm, status=0x%02X(mode=%d)\n",
            rssi, status, chipMode);
    }

    // Only check for packets when DIO1 interrupt signals one is available
    if (!_radio->packetAvailable) return;
    _radio->packetAvailable = false;

    int packetSize = _radio->parsePacket();
    if (packetSize > RNODE_HEADER_L) {
        // parsePacket() already read the FIFO into packetBuffer() — copy from there
        // (avoid calling readBytes() which would re-read the FIFO via read())
        uint8_t raw[MAX_PACKET_SIZE];
        memcpy(raw, _radio->packetBuffer(), packetSize);

        // Strip the 1-byte RNode header, pass only the Reticulum payload
        uint8_t header = raw[0];
        int payloadSize = packetSize - RNODE_HEADER_L;

        Serial.printf("[LORA_IF] RX %d bytes (hdr=0x%02X, payload=%d), RSSI=%d, SNR=%.1f\n",
                      packetSize, header, payloadSize,
                      _radio->packetRssi(), _radio->packetSnr());

        // Hex dump first 32 bytes for debugging interop
        Serial.printf("[LORA_IF] RX hex: ");
        for (int i = 0; i < packetSize && i < 32; i++) Serial.printf("%02X ", raw[i]);
        Serial.println();

        RNS::Bytes buf(payloadSize);
        memcpy(buf.writable(payloadSize), raw + RNODE_HEADER_L, payloadSize);
        InterfaceImpl::handle_incoming(buf);

        // Re-enter RX — but only if handle_incoming didn't trigger a TX.
        // handle_incoming() can synchronously call send_outgoing() (for link
        // proofs, path responses), which starts an async TX via endPacket(true).
        // Calling receive() here would abort that TX (clears IRQ flags + enters RX).
        if (!_txPending) {
            _radio->receive();
        }
    } else if (packetSize > 0) {
        // Packet too small (only header, no payload) — discard
        Serial.printf("[LORA_IF] RX runt packet (%d bytes), discarding\n", packetSize);
        _radio->receive();
    }
}

float LoRaInterface::airtimeUtilization() const {
    if (_airtimeAccumMs <= 0) return 0;
    unsigned long elapsed = millis() - _airtimeWindowStart;
    if (elapsed == 0) elapsed = 1;
    float windowMs = std::min((float)elapsed, (float)AIRTIME_WINDOW_MS);
    return _airtimeAccumMs / windowMs;
}
