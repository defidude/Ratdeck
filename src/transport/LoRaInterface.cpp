#include "LoRaInterface.h"
#include "config/BoardConfig.h"
#include <algorithm>

// RNode on-air framing constants (from RNode_Firmware Framing.h / Config.h)
// Every LoRa packet has a 1-byte header: upper nibble = random sequence, lower nibble = flags
#define RNODE_HEADER_L      1
#define RNODE_FLAG_SPLIT    0x01
#define RNODE_NIBBLE_SEQ    0xF0
#define RNODE_SINGLE_MTU    (MAX_PACKET_SIZE - RNODE_HEADER_L)  // 254 bytes payload per frame

LoRaInterface::LoRaInterface(SX1262* radio, const char* name)
    : RNS::InterfaceImpl(name), _radio(radio)
{
    _IN = true;
    _OUT = true;
    _bitrate = 2000;
    // Reticulum MTU (500 bytes) — split-packet framing allows up to 2x254 = 508 bytes
    _HW_MTU = RNS::Type::Reticulum::MTU;
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
    Serial.println("[LORA_IF] Interface started (split-packet enabled, MTU=500)");
    return true;
}

void LoRaInterface::stop() {
    _online = false;
    Serial.println("[LORA_IF] Interface stopped");
}

void LoRaInterface::send_outgoing(const RNS::Bytes& data) {
    if (!_online || !_radio) return;

    // Reject packets exceeding Reticulum MTU (500 bytes)
    if (data.size() > RNS::Type::Reticulum::MTU) {
        Serial.printf("[LORA_IF] TX DROPPED: exceeds Reticulum MTU (%d > %d)\n",
            (int)data.size(), (int)RNS::Type::Reticulum::MTU);
        return;
    }

    if (_txPending || _splitTxPending) {
        if ((int)_txQueue.size() < TX_QUEUE_MAX) {
            _txQueue.push_back(data);
            Serial.printf("[LORA_IF] TX queued (%d in queue)\n", (int)_txQueue.size());
        } else {
            Serial.println("[LORA_IF] TX queue full, dropping oldest");
            _txQueue.pop_front();
            _txQueue.push_back(data);
        }
        return;
    }

    transmitNow(data);
}

void LoRaInterface::transmitNow(const RNS::Bytes& data) {
    uint8_t header = (uint8_t)(random(256)) & RNODE_NIBBLE_SEQ;
    bool needsSplit = (data.size() > RNODE_SINGLE_MTU);

    if (needsSplit) {
        header |= RNODE_FLAG_SPLIT;
        // First frame: header + first 254 bytes of payload
        size_t firstLen = RNODE_SINGLE_MTU;

        Serial.printf("[LORA_IF] TX SPLIT: %d bytes in 2 frames (seq=0x%02X)\n",
            (int)data.size(), header & RNODE_NIBBLE_SEQ);

        _radio->beginPacket();
        _radio->write(header);
        _radio->write(data.data(), firstLen);
        _radio->endPacket(true);

        // Save remaining data for second frame
        _splitTxPending = true;
        _splitTxRemaining = RNS::Bytes(data.data() + firstLen, data.size() - firstLen);
        _splitTxHeader = header;

        Serial.printf("[LORA_IF] TX SPLIT frame 1: %d+1 bytes (remaining: %d)\n",
            (int)firstLen, (int)_splitTxRemaining.size());
    } else {
        // Single frame: fits in one LoRa packet
        _radio->beginPacket();
        _radio->write(header);
        _radio->write(data.data(), data.size());
        _radio->endPacket(true);

        Serial.printf("[LORA_IF] TX %d+1 bytes (hdr=0x%02X)\n", (int)data.size(), header);
    }

    _txPending = true;
    _txData = data;
    InterfaceImpl::handle_outgoing(data);

    // Track airtime
    size_t airBytes = needsSplit ? (RNODE_SINGLE_MTU + RNODE_HEADER_L) : (data.size() + RNODE_HEADER_L);
    float airtimeMs = _radio->getAirtime(airBytes);
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
}

void LoRaInterface::loop() {
    if (!_online || !_radio) return;

    // Handle async TX completion
    if (_txPending) {
        if (!_radio->isTxBusy()) {
            _txPending = false;

            // If split TX pending, send the second frame immediately
            if (_splitTxPending) {
                _splitTxPending = false;

                Serial.printf("[LORA_IF] TX SPLIT frame 2: %d+1 bytes\n",
                    (int)_splitTxRemaining.size());

                _radio->beginPacket();
                _radio->write(_splitTxHeader);
                _radio->write(_splitTxRemaining.data(), _splitTxRemaining.size());
                _radio->endPacket(true);

                _txPending = true;
                _splitTxRemaining = RNS::Bytes();

                // Track airtime for second frame
                float airtimeMs = _radio->getAirtime(_splitTxRemaining.size() + RNODE_HEADER_L);
                _airtimeAccumMs += airtimeMs;
                return;
            }

            _txData = RNS::Bytes();

            if (!_txQueue.empty()) {
                RNS::Bytes next = _txQueue.front();
                _txQueue.pop_front();
                transmitNow(next);
            } else {
                _radio->receive();
            }
        }
        return;
    }

    // Split RX timeout: discard stale partial packets
    if (_splitRxPending && (millis() - _splitRxTimestamp > SPLIT_RX_TIMEOUT_MS)) {
        Serial.println("[LORA_IF] RX SPLIT timeout, discarding partial");
        _splitRxPending = false;
        _splitRxBuffer = RNS::Bytes();
    }

    // Periodic RX debug
    static unsigned long lastRxDebug = 0;
    if (millis() - lastRxDebug > 30000) {
        lastRxDebug = millis();
        int rssi = _radio->currentRssi();
        uint8_t status = _radio->getStatus();
        uint8_t chipMode = (status >> 4) & 0x07;
        Serial.printf("[LORA_IF] RX: RSSI=%d dBm, status=0x%02X(mode=%d)\n",
            rssi, status, chipMode);
    }

    if (!_radio->packetAvailable) return;
    _radio->packetAvailable = false;

    int packetSize = _radio->parsePacket();
    if (packetSize <= RNODE_HEADER_L) {
        if (packetSize > 0) {
            Serial.printf("[LORA_IF] RX runt packet (%d bytes), discarding\n", packetSize);
        }
        _radio->receive();
        return;
    }

    uint8_t raw[MAX_PACKET_SIZE];
    memcpy(raw, _radio->packetBuffer(), packetSize);

    uint8_t header = raw[0];
    int payloadSize = packetSize - RNODE_HEADER_L;
    uint8_t seq = header & RNODE_NIBBLE_SEQ;
    bool isSplit = (header & RNODE_FLAG_SPLIT) != 0;

    if (isSplit) {
        // Split packet handling
        if (!_splitRxPending) {
            // First frame of a split packet
            _splitRxPending = true;
            _splitRxSeq = seq;
            _splitRxBuffer = RNS::Bytes(raw + RNODE_HEADER_L, payloadSize);
            _splitRxTimestamp = millis();

            Serial.printf("[LORA_IF] RX SPLIT frame 1: %d bytes (seq=0x%02X), RSSI=%d, SNR=%.1f\n",
                payloadSize, seq, _radio->packetRssi(), _radio->packetSnr());
            _radio->receive();
            return;
        } else if (seq == _splitRxSeq) {
            // Second frame matches — reassemble
            Serial.printf("[LORA_IF] RX SPLIT frame 2: %d bytes (seq=0x%02X), RSSI=%d, SNR=%.1f\n",
                payloadSize, seq, _radio->packetRssi(), _radio->packetSnr());

            _splitRxBuffer.append(raw + RNODE_HEADER_L, payloadSize);
            int totalSize = _splitRxBuffer.size();
            _splitRxPending = false;

            Serial.printf("[LORA_IF] RX SPLIT reassembled: %d bytes total\n", totalSize);

            InterfaceImpl::handle_incoming(_splitRxBuffer);
            _splitRxBuffer = RNS::Bytes();

            if (!_txPending) {
                _radio->receive();
            }
            return;
        } else {
            // Sequence mismatch — discard old, start new
            Serial.printf("[LORA_IF] RX SPLIT seq mismatch (had 0x%02X, got 0x%02X), restarting\n",
                _splitRxSeq, seq);
            _splitRxSeq = seq;
            _splitRxBuffer = RNS::Bytes(raw + RNODE_HEADER_L, payloadSize);
            _splitRxTimestamp = millis();
            _radio->receive();
            return;
        }
    }

    // Non-split packet — if we were waiting for split frame 2, discard the partial
    if (_splitRxPending) {
        Serial.println("[LORA_IF] RX non-split while waiting for split frame 2, discarding partial");
        _splitRxPending = false;
        _splitRxBuffer = RNS::Bytes();
    }

    Serial.printf("[LORA_IF] RX %d bytes (hdr=0x%02X, payload=%d), RSSI=%d, SNR=%.1f\n",
                  packetSize, header, payloadSize,
                  _radio->packetRssi(), _radio->packetSnr());

    RNS::Bytes buf(payloadSize);
    memcpy(buf.writable(payloadSize), raw + RNODE_HEADER_L, payloadSize);
    InterfaceImpl::handle_incoming(buf);

    if (!_txPending) {
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
