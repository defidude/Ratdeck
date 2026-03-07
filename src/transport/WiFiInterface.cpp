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
}

WiFiInterface::~WiFiInterface() {
    stop();
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
        _clients.push_back(newClient);
        Serial.printf("[WIFI] Client connected (%d total)\n", (int)_clients.size());
    }
}

void WiFiInterface::readClients() {
    for (int i = _clients.size() - 1; i >= 0; i--) {
        if (!_clients[i].connected()) {
            _clients[i].stop();
            _clients.erase(_clients.begin() + i);
            Serial.printf("[WIFI] Client disconnected (%d total)\n", (int)_clients.size());
            continue;
        }

        int len = readFrame(_clients[i], _rxBuffer, sizeof(_rxBuffer));
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

// HDLC-like framing: [0x7E] [escaped data] [0x7E]
void WiFiInterface::sendFrame(WiFiClient& client, const uint8_t* data, size_t len) {
    client.write(FRAME_START);
    for (size_t i = 0; i < len; i++) {
        if (data[i] == FRAME_START || data[i] == FRAME_ESC) {
            client.write(FRAME_ESC);
            client.write(data[i] ^ FRAME_XOR);
        } else {
            client.write(data[i]);
        }
    }
    client.write(FRAME_START);
    client.flush();
}

int WiFiInterface::readFrame(WiFiClient& client, uint8_t* buffer, size_t maxLen) {
    if (!client.available()) return 0;

    // Look for frame start
    bool inFrame = false;
    bool escaped = false;
    size_t pos = 0;

    while (client.available() && pos < maxLen) {
        uint8_t b = client.read();

        if (b == FRAME_START) {
            if (inFrame && pos > 0) {
                return pos;  // End of frame
            }
            inFrame = true;
            pos = 0;
            continue;
        }

        if (!inFrame) continue;

        if (b == FRAME_ESC) {
            escaped = true;
            continue;
        }

        if (escaped) {
            buffer[pos++] = b ^ FRAME_XOR;
            escaped = false;
        } else {
            buffer[pos++] = b;
        }
    }

    return 0;  // Incomplete frame
}
