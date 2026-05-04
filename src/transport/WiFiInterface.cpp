#include "WiFiInterface.h"
#include "config/Config.h"

WiFiInterface::WiFiInterface(const char* name)
    : RNS::InterfaceImpl(name), _server(WIFI_AP_PORT)
{
    _IN = true;
    _OUT = true;
    _bitrate = 1000000;  // WiFi is fast
    _HW_MTU = 500;
    _apPassword = WIFI_AP_PASSWORD;
    _txBuffer = (uint8_t*)ps_malloc(TX_BUFFER_SIZE);
    if (!_txBuffer) _txBuffer = (uint8_t*)malloc(TX_BUFFER_SIZE);
}

WiFiInterface::~WiFiInterface() {
    stop();
    if (_txBuffer) { free(_txBuffer); _txBuffer = nullptr; }
}

void WiFiInterface::setAPCredentials(const char* ssid, const char* password) {
    _apSSID = ssid;
    _apPassword = password;
}

void WiFiInterface::setSTACredentials(const char* ssid, const char* password) {
    _staSSID = ssid;
    _staPassword = password;
}

bool WiFiInterface::isSTAConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiInterface::startAP() {
    // Generate SSID from chip ID if not set
    if (_apSSID.isEmpty()) {
        uint32_t chip = ESP.getEfuseMac() & 0xFFFF;
        char ssid[32];
        snprintf(ssid, sizeof(ssid), "ratdeck-%04x", chip);
        _apSSID = ssid;
    }

    // AP-only mode — saves ~20KB vs WIFI_AP_STA
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apSSID.c_str(), _apPassword.c_str());

    Serial.printf("[WIFI] AP started: %s @ %s\n",
                  _apSSID.c_str(),
                  WiFi.softAPIP().toString().c_str());

    _server.begin();
    _apActive = true;
}

bool WiFiInterface::start() {
    startAP();
    _online = true;
    return true;
}

void WiFiInterface::stop() {
    _online = false;
    _apActive = false;
    for (auto& client : _clients) {
        client.stop();
    }
    _clients.clear();
    _clientFrames.clear();
    _server.stop();
    WiFi.softAPdisconnect(true);
}

void WiFiInterface::stopFull() {
    stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WIFI] Full shutdown");
}

void WiFiInterface::acceptClients() {
    WiFiClient newClient = _server.available();
    if (newClient) {
        if ((int)_clients.size() >= MAX_AP_CLIENTS) {
            newClient.stop();
            Serial.printf("[WIFI] Client rejected (max %d reached)\n", MAX_AP_CLIENTS);
            return;
        }
        newClient.setNoDelay(true);
        newClient.setTimeout(5);
        _clients.push_back(newClient);
        _clientFrames.push_back(FrameState{});
        Serial.printf("[WIFI] Client connected (%d total)\n", (int)_clients.size());
    }
}

void WiFiInterface::readClients() {
    for (int i = _clients.size() - 1; i >= 0; i--) {
        if (!_clients[i].connected()) {
            _clients[i].stop();
            _clients.erase(_clients.begin() + i);
            if (i < (int)_clientFrames.size()) _clientFrames.erase(_clientFrames.begin() + i);
            Serial.printf("[WIFI] Client disconnected (%d total)\n", (int)_clients.size());
            continue;
        }

        if (i >= (int)_clientFrames.size()) _clientFrames.resize(_clients.size());
        int len = readFrame(_clients[i], _clientFrames[i], _rxBuffer, sizeof(_rxBuffer));
        if (len > 0) {
            RNS::Bytes data(_rxBuffer, len);
            Serial.printf("[WIFI] RX %d bytes from client\n", len);
            InterfaceImpl::handle_incoming(data);
        }
    }
}

void WiFiInterface::sendToClients(const uint8_t* data, size_t len) {
    for (auto& client : _clients) {
        if (client.connected()) {
            sendFrame(client, data, len);
        }
    }
}

void WiFiInterface::send_outgoing(const RNS::Bytes& data) {
    if (!_online) return;

    sendToClients(data.data(), data.size());
    Serial.printf("[WIFI] TX %d bytes to %d clients\n",
                  (int)data.size(), (int)_clients.size());
    InterfaceImpl::handle_outgoing(data);
}

void WiFiInterface::loop() {
    if (!_online) return;
    acceptClients();
    readClients();
}

std::vector<WiFiInterface::ScanResult> WiFiInterface::scanNetworks(int maxResults) {
    std::vector<ScanResult> results;
    wifi_mode_t prevMode = WiFi.getMode();
    bool wasConnected = (WiFi.status() == WL_CONNECTED);
    String prevSSID, prevPass;

    Serial.printf("[WIFI] Scan: prevMode=%d connected=%d\n", (int)prevMode, wasConnected);

    // Ensure we're in a mode that supports scanning
    if (prevMode == WIFI_OFF || prevMode == WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
    }
    // Disconnect from any active STA connection to free the radio for scanning
    WiFi.disconnect(false);
    delay(300);

    // Delete any previous scan results
    WiFi.scanDelete();

    Serial.println("[WIFI] Starting network scan...");

    // Synchronous scan — blocks until complete (typically 2-5 seconds)
    int n = WiFi.scanNetworks(false, false, false, 300, 0);

    Serial.printf("[WIFI] Scan result: %d\n", n);

    // If synchronous returned -2 (still running), poll for it
    if (n == WIFI_SCAN_RUNNING) {
        unsigned long t0 = millis();
        while (n == WIFI_SCAN_RUNNING && millis() - t0 < 15000) {
            delay(200);
            n = WiFi.scanComplete();
        }
        Serial.printf("[WIFI] Scan poll result: %d\n", n);
    }

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.isEmpty()) continue;
            int rssi = WiFi.RSSI(i);
            bool enc = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            bool found = false;
            for (auto& r : results) {
                if (r.ssid == ssid) {
                    if (rssi > r.rssi) r.rssi = rssi;
                    found = true;
                    break;
                }
            }
            if (!found) results.push_back({ssid, rssi, enc});
        }
        WiFi.scanDelete();
        std::sort(results.begin(), results.end(),
                  [](const ScanResult& a, const ScanResult& b) { return a.rssi > b.rssi; });
        if ((int)results.size() > maxResults) results.resize(maxResults);
    }

    // Restore previous WiFi mode
    if (prevMode == WIFI_OFF) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    } else if (prevMode == WIFI_AP) {
        WiFi.mode(WIFI_AP);
    }
    // Note: if was STA/AP_STA, the reconnect will happen via main loop's STA handler
    return results;
}

void WiFiInterface::startAsyncScan() {
    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    } else if (mode == WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 300, 0);  // async=true
    Serial.println("[WIFI] Async scan started");
}

bool WiFiInterface::isScanComplete() {
    int result = WiFi.scanComplete();
    return result != WIFI_SCAN_RUNNING;
}

std::vector<WiFiInterface::ScanResult> WiFiInterface::getScanResults(int maxResults) {
    std::vector<ScanResult> results;
    int n = WiFi.scanComplete();
    if (n <= 0) return results;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        int rssi = WiFi.RSSI(i);
        bool enc = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        bool found = false;
        for (auto& r : results) {
            if (r.ssid == ssid) {
                if (rssi > r.rssi) r.rssi = rssi;
                found = true;
                break;
            }
        }
        if (!found) results.push_back({ssid, rssi, enc});
    }
    WiFi.scanDelete();
    std::sort(results.begin(), results.end(),
              [](const ScanResult& a, const ScanResult& b) { return a.rssi > b.rssi; });
    if ((int)results.size() > maxResults) results.resize(maxResults);
    Serial.printf("[WIFI] Async scan: %d networks found\n", (int)results.size());
    return results;
}

// HDLC-like framing: [0x7E] [escaped data] [0x7E]
// Buffered: builds full frame then sends in a single write() call
void WiFiInterface::sendFrame(WiFiClient& client, const uint8_t* data, size_t len) {
    if (!_txBuffer) return;
    size_t pos = 0;
    _txBuffer[pos++] = FRAME_START;
    for (size_t i = 0; i < len && pos < TX_BUFFER_SIZE - 2; i++) {
        if (data[i] == FRAME_START || data[i] == FRAME_ESC) {
            _txBuffer[pos++] = FRAME_ESC;
            if (pos < TX_BUFFER_SIZE - 1) _txBuffer[pos++] = data[i] ^ FRAME_XOR;
        } else {
            _txBuffer[pos++] = data[i];
        }
    }
    _txBuffer[pos++] = FRAME_START;
    client.write(_txBuffer, pos);
}

int WiFiInterface::readFrame(WiFiClient& client, FrameState& state, uint8_t* buffer, size_t maxLen) {
    if (!client.available()) return 0;

    while (client.available() && state.pos < maxLen) {
        uint8_t b = client.read();

        if (b == FRAME_START) {
            if (state.inFrame && state.pos > 0) {
                int len = state.pos;
                state.pos = 0;
                state.escaped = false;
                return len;  // End of frame
            }
            state.inFrame = true;
            state.escaped = false;
            state.pos = 0;
            continue;
        }

        if (!state.inFrame) continue;

        if (b == FRAME_ESC) {
            state.escaped = true;
            continue;
        }

        if (state.escaped) {
            buffer[state.pos++] = b ^ FRAME_XOR;
            state.escaped = false;
        } else {
            buffer[state.pos++] = b;
        }
    }

    if (state.pos >= maxLen) {
        state.inFrame = false;
        state.escaped = false;
        state.pos = 0;
    }

    return 0;  // Incomplete frame
}
