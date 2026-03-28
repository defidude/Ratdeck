#include "BLEInterface.h"
#include "BLESideband.h"

BLEInterface::BLEInterface(const char* name)
    : RNS::InterfaceImpl(name)
{
    _IN = true;
    _OUT = true;
    _bitrate = 100000;
    _HW_MTU = 185;
}

BLEInterface::~BLEInterface() {
    stop();
}

bool BLEInterface::start() {
    _framesMutex = xSemaphoreCreateMutex();
    NimBLEDevice::init("Ratdeck");
    NimBLEDevice::setMTU(512);

    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(this, false);

    _pService = _pServer->createService(SERVICE_UUID);

    // TX: Ratdeck -> remote (NOTIFY)
    _pTxChar = _pService->createCharacteristic(
        TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // RX: remote -> Ratdeck (WRITE)
    _pRxChar = _pService->createCharacteristic(
        RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _pRxChar->setCallbacks(this);

    _pService->start();
    _pServer->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setName("Ratdeck");
    pAdv->start();

    _active = true;
    _online = true;
    Serial.println("[BLE] Transport started, advertising");
    return true;
}

void BLEInterface::stop() {
    if (_active) {
        NimBLEDevice::deinit(true);
        _active = false;
        _online = false;
        _connected = false;
        Serial.println("[BLE] Transport stopped");
    }
}

void BLEInterface::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    _connected = true;
    _lastActivityMs = millis();
    Serial.printf("[BLE] Client connected: %s\n", connInfo.getAddress().toString().c_str());
    if (_sideband) _sideband->notifyConnect();
}

void BLEInterface::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    _connected = false;
    Serial.printf("[BLE] Client disconnected (reason=%d)\n", reason);
    if (_sideband) _sideband->notifyDisconnect();
    // Restart advertising
    NimBLEDevice::getAdvertising()->start();
}

void BLEInterface::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    NimBLEAttValue val = pCharacteristic->getValue();
    const uint8_t* data = val.data();
    size_t len = val.size();

    _lastActivityMs = millis();
    for (size_t i = 0; i < len; i++) {
        processRxByte(data[i]);
    }
}

void BLEInterface::processRxByte(uint8_t b) {
    if (b == FRAME_START) {
        if (_rxActive && !_rxFrame.empty()) {
            // Complete frame received — queue it (mutex: called from NimBLE task)
            if (_framesMutex && xSemaphoreTake(_framesMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                _incomingFrames.push_back(std::move(_rxFrame));
                xSemaphoreGive(_framesMutex);
            }
            _rxFrame.clear();
        }
        _rxActive = true;
        _rxEscape = false;
        _rxFrame.clear();
        return;
    }
    if (!_rxActive) return;

    if (b == FRAME_ESC) {
        _rxEscape = true;
        return;
    }
    if (_rxEscape) {
        b ^= FRAME_XOR;
        _rxEscape = false;
    }
    if (_rxFrame.size() < 600) {
        _rxFrame.push_back(b);
    }
}

void BLEInterface::loop() {
    if (!_active) return;

    // Disconnect idle BLE clients to free the single connection slot
    if (_connected && _lastActivityMs > 0 && millis() - _lastActivityMs > BLE_IDLE_TIMEOUT_MS) {
        Serial.println("[BLE] Idle timeout, disconnecting client");
        if (_pServer) _pServer->disconnect(0);
        _lastActivityMs = 0;
    }

    // Process queued incoming frames (mutex: _incomingFrames shared with NimBLE task)
    if (_framesMutex && xSemaphoreTake(_framesMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        // Swap to local to minimize lock hold time
        std::vector<std::vector<uint8_t>> localFrames;
        localFrames.swap(_incomingFrames);
        xSemaphoreGive(_framesMutex);

        for (auto& frame : localFrames) {
            if (!frame.empty()) {
                RNS::Bytes data(frame.data(), frame.size());
                handle_incoming(data);
            }
        }
    }
}

void BLEInterface::injectIncoming(const RNS::Bytes& data) {
    handle_incoming(data);
}

void BLEInterface::send_outgoing(const RNS::Bytes& data) {
    if (!_connected || !_pTxChar) return;
    _lastActivityMs = millis();
    sendFrame(data.data(), data.size());
}

void BLEInterface::sendFrame(const uint8_t* data, size_t len) {
    // HDLC-frame the data and send via BLE notify
    // BLE MTU limits each notify, so we may need to chunk
    std::vector<uint8_t> frame;
    frame.reserve(len * 2 + 2);
    frame.push_back(FRAME_START);

    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == FRAME_START || b == FRAME_ESC) {
            frame.push_back(FRAME_ESC);
            frame.push_back(b ^ FRAME_XOR);
        } else {
            frame.push_back(b);
        }
    }
    frame.push_back(FRAME_START);

    // Send in MTU-sized chunks
    uint16_t mtu = NimBLEDevice::getMTU() - 3;  // ATT overhead
    if (mtu < 20) mtu = 20;

    for (size_t offset = 0; offset < frame.size(); offset += mtu) {
        size_t chunk = std::min((size_t)mtu, frame.size() - offset);
        _pTxChar->notify(frame.data() + offset, chunk);
    }
}
