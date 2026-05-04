#include <Arduino.h>
#include "config/Config.h"

#if HAS_BLE

#include "BLESideband.h"

bool BLESideband::begin(NimBLEServer* existingServer) {
    if (!_packetsMutex) _packetsMutex = xSemaphoreCreateMutex();
    if (existingServer) {
        _pServer = existingServer;
        _ownServer = false;
    } else {
        NimBLEDevice::init("Ratdeck");
        _pServer = NimBLEDevice::createServer();
        _ownServer = true;
    }

    // Create Nordic UART Service (don't set server callbacks — BLEInterface owns them)
    _pService = _pServer->createService(NUS_SERVICE_UUID);

    // TX: Ratdeck -> Sideband (NOTIFY)
    _pTxChar = _pService->createCharacteristic(
        NUS_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // RX: Sideband -> Ratdeck (WRITE)
    _pRxChar = _pService->createCharacteristic(
        NUS_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _pRxChar->setCallbacks(this);

    _pService->start();

    // Only start server + advertising if we own it
    if (_ownServer) {
        _pServer->start();
        NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
        pAdv->addServiceUUID(NUS_SERVICE_UUID);
        pAdv->setName("Ratdeck");
        pAdv->start();
    } else {
        // Add NUS UUID to existing advertising
        NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
        pAdv->addServiceUUID(NUS_SERVICE_UUID);
    }

    Serial.println("[BLE] Sideband NUS service started");
    return true;
}

void BLESideband::stop() {
    _connected = false;
    if (_ownServer) {
        NimBLEDevice::deinit(true);
    }
}

void BLESideband::notifyConnect() {
    _connected = true;
}

void BLESideband::notifyDisconnect() {
    _connected = false;
}

void BLESideband::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    NimBLEAttValue val = pCharacteristic->getValue();
    const uint8_t* data = val.data();
    size_t len = val.size();

    for (size_t i = 0; i < len; i++) {
        processRxByte(data[i]);
    }
}

void BLESideband::processRxByte(uint8_t b) {
    if (b == KISS_FEND) {
        if (_rxInFrame && !_rxFrame.empty()) {
            // First byte is KISS command; 0x00 = data frame
            if (_rxFrame[0] == KISS_CMD_DATA && _rxFrame.size() > 1) {
                std::vector<uint8_t> pkt(_rxFrame.begin() + 1, _rxFrame.end());
                if (_packetsMutex && xSemaphoreTake(_packetsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    if (_incomingPackets.size() < 12) _incomingPackets.push_back(std::move(pkt));
                    xSemaphoreGive(_packetsMutex);
                }
            }
            _rxFrame.clear();
        }
        _rxInFrame = true;
        _rxFrame.clear();
        return;
    }

    if (!_rxInFrame) return;

    // Handle KISS escape sequences
    if (b == KISS_FESC) {
        _rxEscaped = true;
        return;
    }
    if (_rxEscaped) {
        _rxEscaped = false;
        if (b == KISS_TFEND) b = KISS_FEND;
        else if (b == KISS_TFESC) b = KISS_FESC;
    }

    if (_rxFrame.size() < 600) {
        _rxFrame.push_back(b);
    }
}

void BLESideband::loop() {
    std::vector<std::vector<uint8_t>> localPackets;
    if (_packetsMutex && xSemaphoreTake(_packetsMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        localPackets.swap(_incomingPackets);
        xSemaphoreGive(_packetsMutex);
    }
    for (auto& pkt : localPackets) {
        if (_packetCb && !pkt.empty()) {
            _packetCb(pkt.data(), pkt.size());
        }
    }
}

void BLESideband::sendPacket(const uint8_t* data, size_t len) {
    if (!_connected || !_pTxChar) return;

    // KISS-frame: FEND + CMD_DATA + escaped_data + FEND
    std::vector<uint8_t> frame;
    frame.reserve(len * 2 + 3);
    frame.push_back(KISS_FEND);
    frame.push_back(KISS_CMD_DATA);

    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == KISS_FEND) {
            frame.push_back(KISS_FESC);
            frame.push_back(KISS_TFEND);
        } else if (b == KISS_FESC) {
            frame.push_back(KISS_FESC);
            frame.push_back(KISS_TFESC);
        } else {
            frame.push_back(b);
        }
    }
    frame.push_back(KISS_FEND);

    // Send in MTU-sized chunks
    uint16_t mtu = NimBLEDevice::getMTU() - 3;
    if (mtu < 20) mtu = 20;

    for (size_t offset = 0; offset < frame.size(); offset += mtu) {
        size_t chunk = std::min((size_t)mtu, frame.size() - offset);
        _pTxChar->notify(frame.data() + offset, chunk);
    }
}

#endif
