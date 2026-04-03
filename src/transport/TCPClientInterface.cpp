#include "TCPClientInterface.h"
#include "config/Config.h"

TCPClientInterface::TCPClientInterface(const char* host, uint16_t port, const char* name)
    : RNS::InterfaceImpl(name), _host(host), _port(port)
{
    _IN = true;
    _OUT = true;
    _bitrate = 1000000;
    _HW_MTU = 500;
    _rxBuffer = (uint8_t*)ps_malloc(RX_BUFFER_SIZE);
    if (!_rxBuffer) _rxBuffer = (uint8_t*)malloc(RX_BUFFER_SIZE);
    _txBuffer = (uint8_t*)ps_malloc(TX_BUFFER_SIZE);
    if (!_txBuffer) _txBuffer = (uint8_t*)malloc(TX_BUFFER_SIZE);
    _wrapBuffer = (uint8_t*)ps_malloc(RX_BUFFER_SIZE);
    if (!_wrapBuffer) _wrapBuffer = (uint8_t*)malloc(RX_BUFFER_SIZE);
}

TCPClientInterface::~TCPClientInterface() {
    stop();
    if (_rxBuffer) { free(_rxBuffer); _rxBuffer = nullptr; }
    if (_txBuffer) { free(_txBuffer); _txBuffer = nullptr; }
    if (_wrapBuffer) { free(_wrapBuffer); _wrapBuffer = nullptr; }
}

bool TCPClientInterface::start() {
    _online = true;
    tryConnect();
    return true;
}

void TCPClientInterface::stop() {
    _online = false;
    if (_client.connected()) {
        _client.stop();
        Serial.printf("[TCP] Disconnected from %s:%d\n", _host.c_str(), _port);
    }
}

void TCPClientInterface::tryConnect() {
    _lastAttempt = millis();
    Serial.printf("[TCP] Connecting to %s:%d...\n", _host.c_str(), _port);

    if (_client.connect(_host.c_str(), _port, TCP_CONNECT_TIMEOUT_MS)) {
        // Reset HDLC frame state and hub discovery for new connection
        _inFrame = false;
        _escaped = false;
        _rxPos = 0;
        _hubTransportIdKnown = false;
        _pendingAnnounces.clear();
        _lastRxTime = millis();

        // Set TCP write timeout to prevent blocking on half-open sockets
        _client.setTimeout(5);  // 5 second write timeout
        _client.setNoDelay(true);  // Disable Nagle — send immediately

        Serial.printf("[TCP] Connected to %s:%d\n", _host.c_str(), _port);
    } else {
        Serial.printf("[TCP] Failed to connect to %s:%d\n", _host.c_str(), _port);
    }
}

void TCPClientInterface::loop() {
    if (!_online) return;

    // Auto-reconnect (only if WiFi is connected)
    if (!_client.connected()) {
        if (WiFi.status() != WL_CONNECTED) return;
        if (millis() - _lastAttempt >= TCP_RECONNECT_INTERVAL_MS) {
            tryConnect();
        }
        return;
    }

    // Keepalive: if no RX for 5 minutes, force reconnect (NAT timeout detection)
    if (_lastRxTime > 0 && millis() - _lastRxTime >= TCP_KEEPALIVE_TIMEOUT_MS) {
        Serial.printf("[TCP] No RX for %lus, forcing reconnect to %s:%d\n",
                      (millis() - _lastRxTime) / 1000, _host.c_str(), _port);
        _client.stop();
        _inFrame = false;
        _escaped = false;
        _rxPos = 0;
        return;  // Will reconnect on next loop iteration
    }

    // Drain incoming frames per loop (up to 15, time-boxed)
    unsigned long tcpStart = millis();
    for (int i = 0; i < 15 && _client.available() && (millis() - tcpStart < TCP_LOOP_BUDGET_MS); i++) {
        unsigned long rxStart = millis();
        int len = readFrame();
        if (len > 0) {
            _lastRxTime = millis();
            _hubRxCount++;

            // Learn hub transport_id from incoming Header2 packets (once per connection)
            if (len >= 35) {
                uint8_t flags = _rxBuffer[0];
                uint8_t header_type = (flags >> 6) & 0x01;
                if (header_type == 1 && !_hubTransportIdKnown) {
                    memcpy(_hubTransportId, _rxBuffer + 2, 16);
                    _hubTransportIdKnown = true;
                    char hex[33];
                    for (int j = 0; j < 16; j++) sprintf(hex + j*2, "%02x", _hubTransportId[j]);
                    Serial.printf("[TCP] Learned hub transport_id: %.8s\n", hex);

                    // Flush any announces that were queued before hub ID was known
                    for (auto& pending : _pendingAnnounces) {
                        sendFrame(pending.data(), pending.size());
                        InterfaceImpl::handle_outgoing(pending);
                        Serial.printf("[TCP] TX %d bytes (flushed pending announce) to %s:%d\n",
                                      (int)pending.size(), _host.c_str(), _port);
                    }
                    _pendingAnnounces.clear();
                }
            }

            RNS::Bytes data(_rxBuffer, len);
            Serial.printf("[TCP] RX %d bytes from %s:%d (%lums)\n",
                          len, _host.c_str(), _port, millis() - rxStart);
            InterfaceImpl::handle_incoming(data);
        } else {
            break;  // Incomplete frame, wait for more data
        }
    }
}

void TCPClientInterface::send_outgoing(const RNS::Bytes& data) {
    if (!_online) {
        Serial.printf("[TCP] TX BLOCKED (offline) %d bytes to %s:%d\n", (int)data.size(), _host.c_str(), _port);
        return;
    }
    if (!_client.connected()) {
        Serial.printf("[TCP] TX BLOCKED (disconnected) %d bytes to %s:%d\n", (int)data.size(), _host.c_str(), _port);
        return;
    }

    // Wrap Header1 non-announce packets as Header2 for TCP transport
    // (mirrors Rust actor.rs:653-678 — hub drops raw Header1 data packets)
    if (_hubTransportIdKnown && data.size() >= 19) {
        uint8_t flags = data.data()[0];
        uint8_t header_type = (flags >> 6) & 0x01;
        uint8_t packet_type = flags & 0x03;

        // Diagnostic: identify packet types going through TCP
        static const char* pt_names[] = {"DATA", "ANNOUNCE", "LINKREQ", "PROOF"};
        Serial.printf("[TCP-DIAG] send: %d bytes ht=%d pt=%s(%d) to %s:%d\n",
            (int)data.size(), header_type,
            (packet_type < 4) ? pt_names[packet_type] : "?", packet_type,
            _host.c_str(), _port);
        if (packet_type == 0x03) {
            Serial.printf("[TCP-DIAG] *** PROOF packet being sent via TCP! ***\n");
        }

        if (packet_type != 0x01 && packet_type != 0x03) {  // Not ANNOUNCE, not PROOF
            if (header_type == 0) {
                // Header1 → wrap as Header2 (handles hops==1, hops==0, unknown path)
                uint8_t new_flags = flags | 0x50;  // Set Header2 (bit 6) + Transport (bit 4)

                // Build Header2 packet: flags(1) + hops(1) + transport_id(16) + original[2:]
                size_t new_len = data.size() + 16;
                if (new_len > RX_BUFFER_SIZE) {
                    Serial.printf("[TCP] H1->H2 wrap too large (%d bytes), dropping\n", (int)new_len);
                    _txDropCount++;
                    return;
                }
                if (!_wrapBuffer) return;
                _wrapBuffer[0] = new_flags;
                _wrapBuffer[1] = data.data()[1];  // hops
                memcpy(_wrapBuffer + 2, _hubTransportId, 16);  // transport_id
                memcpy(_wrapBuffer + 18, data.data() + 2, data.size() - 2);  // dest_hash + context + payload

                Serial.printf("[TCP] TX %d->%d bytes (H1->H2 wrap) to %s:%d\n",
                              (int)data.size(), (int)new_len, _host.c_str(), _port);
                sendFrame(_wrapBuffer, new_len);
                InterfaceImpl::handle_outgoing(data);  // Stats use original size
                return;
            }
            else if (data.size() >= 35 && memcmp(data.data() + 2, _hubTransportId, 16) != 0) {
                // Header2 with wrong transport_id → fix it
                // Transport::outbound() may have used _received_from=destination_hash
                if (!_wrapBuffer) return;
                memcpy(_wrapBuffer, data.data(), data.size());
                memcpy(_wrapBuffer + 2, _hubTransportId, 16);

                Serial.printf("[TCP] TX %d bytes (H2 transport_id fixed) to %s:%d\n",
                              (int)data.size(), _host.c_str(), _port);
                sendFrame(_wrapBuffer, data.size());
                InterfaceImpl::handle_outgoing(data);
                return;
            }
        }
    }

    // Queue announces until hub transport_id is learned
    if (!_hubTransportIdKnown) {
        uint8_t flags = data.size() >= 1 ? data.data()[0] : 0;
        uint8_t packet_type = flags & 0x03;
        if (packet_type == 0x01 && _pendingAnnounces.size() < 3) {
            _pendingAnnounces.push_back(data);
            Serial.printf("[TCP] TX %d bytes (queued announce, hub ID pending) to %s:%d\n",
                          (int)data.size(), _host.c_str(), _port);
            return;
        }
        sendFrame(data.data(), data.size());
        Serial.printf("[TCP] TX %d bytes (no hub ID yet) to %s:%d\n", (int)data.size(), _host.c_str(), _port);
    } else {
        // Passthrough: announces, correct Header2
        sendFrame(data.data(), data.size());
        Serial.printf("[TCP] TX %d bytes (passthrough) to %s:%d\n", (int)data.size(), _host.c_str(), _port);
    }
    InterfaceImpl::handle_outgoing(data);
}

// HDLC-like framing: [0x7E] [escaped data] [0x7E]
// Buffered write — single syscall instead of per-byte writes
void TCPClientInterface::sendFrame(const uint8_t* data, size_t len) {
    if (!_txBuffer) return;
    // Worst case: every byte escapes (2x) + 2 delimiters
    size_t maxFrameLen = len * 2 + 2;
    if (maxFrameLen > TX_BUFFER_SIZE) {
        Serial.printf("[TCP] TX frame too large (%d bytes), dropping\n", (int)len);
        _txDropCount++;
        return;
    }
    size_t pos = 0;
    _txBuffer[pos++] = FRAME_START;
    for (size_t i = 0; i < len && pos < TX_BUFFER_SIZE - 2; i++) {
        if (data[i] == FRAME_START || data[i] == FRAME_ESC) {
            _txBuffer[pos++] = FRAME_ESC;
            _txBuffer[pos++] = data[i] ^ FRAME_XOR;
        } else {
            _txBuffer[pos++] = data[i];
        }
    }
    _txBuffer[pos++] = FRAME_START;
    _client.write(_txBuffer, pos);
    // No flush() — TCP_NODELAY sends immediately without Nagle delay
}

int TCPClientInterface::readFrame() {
    if (!_client.available()) return 0;

    // Uses persistent member state: _inFrame, _escaped, _rxPos
    // This allows frames split across TCP segments to be reassembled correctly
    int bytesRead = 0;
    constexpr int MAX_BYTES_PER_CALL = 1024;

    while (_client.available() && _rxPos < RX_BUFFER_SIZE && bytesRead < MAX_BYTES_PER_CALL) {
        uint8_t b = _client.read();
        bytesRead++;

        if (b == FRAME_START) {
            if (_inFrame && _rxPos > 0) {
                // End of frame — return length, caller reads from _rxBuffer
                size_t frameLen = _rxPos;
                _inFrame = false;
                _escaped = false;
                _rxPos = 0;
                return frameLen;
            }
            _inFrame = true;
            _rxPos = 0;
            _escaped = false;
            continue;
        }

        if (!_inFrame) continue;

        if (b == FRAME_ESC) {
            _escaped = true;
            continue;
        }

        if (_escaped) {
            _rxBuffer[_rxPos++] = b ^ FRAME_XOR;
            _escaped = false;
        } else {
            _rxBuffer[_rxPos++] = b;
        }
    }

    // Buffer overflow protection
    if (_rxPos >= RX_BUFFER_SIZE) {
        Serial.printf("[TCP] Frame too large (%d bytes), dropping\n", (int)_rxPos);
        _inFrame = false;
        _escaped = false;
        _rxPos = 0;
    }

    return 0;  // Incomplete frame — state preserved for next call
}
