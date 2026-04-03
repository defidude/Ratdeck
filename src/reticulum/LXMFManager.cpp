// Direct port from Ratputer — LXMF messaging protocol
#include "LXMFManager.h"
#include "config/Config.h"
#include <Transport.h>
#include <time.h>

LXMFManager* LXMFManager::_instance = nullptr;

bool LXMFManager::begin(ReticulumManager* rns, MessageStore* store) {
    _rns = rns; _store = store; _instance = this;
    RNS::Destination& dest = _rns->destination();
    dest.set_packet_callback(onPacketReceived);
    dest.set_link_established_callback(onLinkEstablished);
    Serial.println("[LXMF] Manager started");
    return true;
}

void LXMFManager::loop() {
    if (_outQueue.empty()) return;
    unsigned long now = millis();
    int processed = 0;

    for (auto it = _outQueue.begin(); it != _outQueue.end(); ) {
        // Time-budgeted: process up to 3 messages within 10ms
        if (processed >= 3 || (processed > 0 && millis() - now >= 10)) break;

        LXMFMessage& msg = *it;

        // Per-message retry cooldown: 2 seconds between attempts
        if (msg.retries > 0 && (millis() - msg.lastRetryMs) < 2000) { ++it; continue; }

        msg.lastRetryMs = millis();

        if (sendDirect(msg)) {
            processed++;
            Serial.printf("[LXMF] Queue drain: status=%s dest=%s\n",
                          msg.statusStr(), msg.destHash.toHex().substr(0, 8).c_str());

            // Persist updated status to disk so reloads don't revert to QUEUED
            std::string peerHex = msg.destHash.toHex();
            if (_store && msg.savedCounter > 0) {
                _store->updateMessageStatusByCounter(peerHex, msg.savedCounter, false, msg.status);
            } else if (_store) {
                _store->updateMessageStatus(peerHex, msg.timestamp, false, msg.status);
            }

            if (_statusCb) {
                _statusCb(peerHex, msg.timestamp, msg.status);
            }
            it = _outQueue.erase(it);
        } else {
            // sendDirect returned false — message stays in queue, try next
            ++it;
        }
    }
}

bool LXMFManager::sendMessage(const RNS::Bytes& destHash, const std::string& content, const std::string& title) {
    LXMFMessage msg;
    msg.sourceHash = _rns->destination().hash();
    msg.destHash = destHash;
    // Use real epoch time when NTP is synced, uptime fallback otherwise
    time_t now = time(nullptr);
    if (now > 1700000000) {
        msg.timestamp = (double)now;
    } else {
        msg.timestamp = millis() / 1000.0;
    }
    msg.content = content;
    msg.title = title;
    msg.incoming = false;
    msg.status = LXMFStatus::QUEUED;
    if ((int)_outQueue.size() >= RATDECK_MAX_OUTQUEUE) { _outQueue.pop_front(); }
    _outQueue.push_back(msg);
    // Immediately save with QUEUED status so it appears in getMessages() right away
    // Save the queue copy so savedCounter propagates back to the queued message
    if (_store) { _store->saveMessage(_outQueue.back()); }

    // Proactively request path so it's ready when sendDirect runs
    if (!RNS::Transport::has_path(destHash)) {
        RNS::Transport::request_path(destHash);
        Serial.printf("[LXMF] Message queued for %s — requesting path\n",
                      destHash.toHex().substr(0, 8).c_str());
    }
    return true;
}

bool LXMFManager::sendDirect(LXMFMessage& msg) {
    Serial.printf("[LXMF] sendDirect: dest=%s link=%s pending=%s\n",
        msg.destHash.toHex().substr(0, 12).c_str(),
        _outLink ? (_outLink.status() == RNS::Type::Link::ACTIVE ? "ACTIVE" : "INACTIVE") : "NONE",
        _outLinkPending ? "yes" : "no");

    // Reset stale link-pending state: if pending but link never became ACTIVE,
    // allow a new link attempt. Covers: link object destroyed (NONE), timed out, or failed.
    if (_outLinkPending) {
        bool linkReady = _outLink && _outLink.status() == RNS::Type::Link::ACTIVE;
        if (!linkReady) {
            _outLinkPending = false;
            _outLink = {RNS::Type::NONE};
            Serial.println("[LXMF] Clearing stale link-pending (link not active)");
        }
    }

    RNS::Identity recipientId = RNS::Identity::recall(msg.destHash);
    if (!recipientId) {
        msg.retries++;
        // Request path on first retry and every 10 retries
        if (msg.retries == 1 || msg.retries % 10 == 0) {
            Serial.printf("[LXMF] Requesting path for %s\n",
                          msg.destHash.toHex().substr(0, 8).c_str());
            RNS::Transport::request_path(msg.destHash);
        }
        if (msg.retries >= 30) {
            Serial.printf("[LXMF] recall FAILED for %s after %d retries — marking FAILED\n",
                          msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
            msg.status = LXMFStatus::FAILED;
            return true;
        }
        Serial.printf("[LXMF] recall pending for %s (retry %d/30) — identity not known yet\n",
                      msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
        return false;  // keep in queue, retry next loop
    }
    Serial.printf("[LXMF] recall OK: identity=%s\n", recipientId.hexhash().c_str());

    // Ensure path exists — without a path, Transport::outbound() broadcasts as
    // Header1 which the Python hub silently drops
    if (!RNS::Transport::has_path(msg.destHash)) {
        msg.retries++;
        if (msg.retries == 1 || msg.retries % 5 == 0) {
            Serial.printf("[LXMF] No path for %s, requesting (retry %d)\n",
                          msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
            RNS::Transport::request_path(msg.destHash);
        }
        if (msg.retries >= 30) {
            Serial.printf("[LXMF] No path for %s after %d retries — FAILED\n",
                          msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
            msg.status = LXMFStatus::FAILED;
            return true;
        }
        return false;  // keep in queue, retry later
    }

    Serial.printf("[LXMF] path OK: %s hops=%d\n",
                  msg.destHash.toHex().substr(0, 8).c_str(),
                  RNS::Transport::hops_to(msg.destHash));

    RNS::Destination outDest(recipientId, RNS::Type::Destination::OUT,
        RNS::Type::Destination::SINGLE, "lxmf", "delivery");
    Serial.printf("[DIAG] LXMF: outDest=%s msgDest=%s match=%s\n",
        outDest.hash().toHex().c_str(), msg.destHash.toHex().c_str(),
        (outDest.hash() == msg.destHash) ? "YES" : "NO");

    // packFull returns opportunistic format: [src:16][sig:64][msgpack]
    std::vector<uint8_t> payload = msg.packFull(_rns->identity());
    if (payload.empty()) { Serial.println("[LXMF] packFull returned empty!"); msg.status = LXMFStatus::FAILED; return true; }

    msg.status = LXMFStatus::SENDING;
    bool sent = false;

    // Try link-based delivery if we have an active link to this peer
    if (_outLink && _outLinkDestHash == msg.destHash
        && _outLink.status() == RNS::Type::Link::ACTIVE) {
        // Link delivery: prepend dest_hash (Python DIRECT format)
        std::vector<uint8_t> linkPayload;
        linkPayload.reserve(16 + payload.size());
        linkPayload.insert(linkPayload.end(), msg.destHash.data(), msg.destHash.data() + 16);
        linkPayload.insert(linkPayload.end(), payload.begin(), payload.end());
        RNS::Bytes linkBytes(linkPayload.data(), linkPayload.size());
        if (linkBytes.size() <= RNS::Type::Reticulum::MDU) {
            // Small enough for single link packet
            Serial.printf("[LXMF] sending via link packet: %d bytes to %s\n",
                          (int)linkBytes.size(), msg.destHash.toHex().substr(0, 8).c_str());
            RNS::Packet packet(_outLink, linkBytes);
            RNS::PacketReceipt receipt = packet.send();
            if (receipt) { sent = true; }
        } else {
            // Too large for single packet — use Resource transfer (chunked)
            Serial.printf("[LXMF] sending via link resource: %d bytes to %s\n",
                          (int)linkBytes.size(), msg.destHash.toHex().substr(0, 8).c_str());
            if (_outLink.start_resource_transfer(linkBytes)) {
                sent = true;
            } else {
                Serial.println("[LXMF] resource transfer failed to start");
            }
        }
    }

    // Fallback: opportunistic or queue for link-based resource transfer
    if (!sent) {
        RNS::Bytes payloadBytes(payload.data(), payload.size());
        // Use opportunistic only for packets that fit in a single LoRa frame (254 bytes).
        // Larger packets require split-frame over LoRa, which is unreliable — any single
        // frame loss (CRC error, collision, half-duplex timing) kills the entire transfer
        // with no recovery. Link-based delivery handles retransmission at the protocol level.
        static constexpr size_t SINGLE_FRAME_MAX = 254;
        if (payloadBytes.size() <= SINGLE_FRAME_MAX) {
            // Fits in single LoRa frame — send opportunistic
            Serial.printf("[LXMF] sending opportunistic: %d bytes to %s\n",
                          (int)payloadBytes.size(), outDest.hash().toHex().substr(0, 12).c_str());
            RNS::Packet packet(outDest, payloadBytes);
            RNS::PacketReceipt receipt = packet.send();
            if (receipt) { sent = true; }
        } else {
            // Too large for single frame — need link + resource transfer
            Serial.printf("[LXMF] Message needs link delivery (%d bytes > %d single-frame), retry %d\n",
                          (int)payloadBytes.size(), (int)SINGLE_FRAME_MAX, msg.retries);
            if (msg.retries % 3 == 0 && (!_outLink || _outLinkDestHash != msg.destHash
                || _outLink.status() != RNS::Type::Link::ACTIVE)) {
                _outLinkPendingHash = msg.destHash;
                _outLinkPending = true;
                Serial.printf("[LXMF] Establishing link to %s for resource transfer\n",
                              msg.destHash.toHex().substr(0, 8).c_str());
                RNS::Link newLink(outDest, onOutLinkEstablished, onOutLinkClosed);
            }
            msg.retries++;
            if (msg.retries >= 30) {
                Serial.printf("[LXMF] Link for %s not established after %d retries — FAILED\n",
                              msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
                msg.status = LXMFStatus::FAILED;
                return true;
            }
            return false;  // Keep in queue, retry when link is established
        }
    }

    if (sent) {
        msg.status = LXMFStatus::SENT;
        Serial.printf("[LXMF] SENT OK: msgId=%s\n", msg.messageId.toHex().substr(0, 8).c_str());
    } else {
        Serial.println("[LXMF] send FAILED: no receipt");
        msg.status = LXMFStatus::FAILED;
    }

    // Link establishment is now only triggered on-demand when a message
    // is too large for opportunistic delivery (see large-message path above).
    // Speculative background links cause collisions on LoRa when both
    // devices try to establish simultaneously after exchanging short messages.

    return true;
}

void LXMFManager::onPacketReceived(const RNS::Bytes& data, const RNS::Packet& packet) {
    if (!_instance) return;
    // Non-link delivery: dest_hash is NOT in LXMF payload (it's in the RNS packet header).
    // Reconstruct full format by prepending it, matching Python LXMRouter.delivery_packet().
    const RNS::Bytes& destHash = packet.destination_hash();
    std::vector<uint8_t> fullData;
    fullData.reserve(destHash.size() + data.size());
    fullData.insert(fullData.end(), destHash.data(), destHash.data() + destHash.size());
    fullData.insert(fullData.end(), data.data(), data.data() + data.size());
    _instance->processIncoming(fullData.data(), fullData.size(), destHash);
}

void LXMFManager::onOutLinkEstablished(RNS::Link& link) {
    if (!_instance) return;
    _instance->_outLink = link;
    _instance->_outLinkDestHash = _instance->_outLinkPendingHash;
    _instance->_outLinkPending = false;
    Serial.printf("[LXMF] Outbound link established to %s\n",
                  _instance->_outLinkDestHash.toHex().substr(0, 8).c_str());
}

void LXMFManager::onOutLinkClosed(RNS::Link& link) {
    if (!_instance) return;
    _instance->_outLink = {RNS::Type::NONE};
    _instance->_outLinkPending = false;
    Serial.println("[LXMF] Outbound link closed");
}

void LXMFManager::onLinkEstablished(RNS::Link& link) {
    if (!_instance) return;
    Serial.printf("[LXMF-DIAG] onLinkEstablished fired! link_id=%s status=%d\n",
        link.link_id().toHex().substr(0, 16).c_str(), (int)link.status());
    link.set_packet_callback([](const RNS::Bytes& data, const RNS::Packet& packet) {
        if (!_instance) return;
        Serial.printf("[LXMF-DIAG] Link packet received! %d bytes pkt_dest=%s\n",
            (int)data.size(), packet.destination_hash().toHex().substr(0, 16).c_str());
        // Link delivery: data already contains [dest:16][src:16][sig:64][msgpack]
        // Do NOT use packet.destination_hash() — that's the link_id, not the LXMF dest.
        // Pass empty Bytes so processIncoming uses the destHash from unpackFull().
        _instance->processIncoming(data.data(), data.size(), RNS::Bytes());
    });
}

void LXMFManager::processIncoming(const uint8_t* data, size_t len, const RNS::Bytes& destHash) {
    Serial.printf("[LXMF-DIAG] processIncoming: %d bytes callerDestHash=%s\n",
        (int)len, destHash.size() > 0 ? destHash.toHex().substr(0, 16).c_str() : "EMPTY(link)");
    LXMFMessage msg;
    if (!LXMFMessage::unpackFull(data, len, msg)) {
        Serial.printf("[LXMF] Failed to unpack incoming message (%d bytes)\n", (int)len);
        return;
    }
    if (_rns && msg.sourceHash == _rns->destination().hash()) return;

    Serial.printf("[LXMF-DIAG] unpackFull OK: src=%s dest=%s content_len=%d\n",
        msg.sourceHash.toHex().substr(0, 8).c_str(),
        msg.destHash.toHex().substr(0, 8).c_str(), (int)msg.content.size());

    // Deduplication: skip messages we've already processed
    std::string msgIdHex = msg.messageId.toHex();
    if (_seenMessageIds.count(msgIdHex)) {
        Serial.printf("[LXMF] Duplicate message from %s (already seen)\n",
                      msg.sourceHash.toHex().substr(0, 8).c_str());
        return;
    }
    _seenMessageIds.insert(msgIdHex);
    if ((int)_seenMessageIds.size() > MAX_SEEN_IDS) {
        _seenMessageIds.erase(_seenMessageIds.begin());
    }

    // Only overwrite destHash if caller provided a real one (non-link delivery).
    // For link delivery, unpackFull already parsed the correct destHash from the payload.
    if (destHash.size() > 0) {
        msg.destHash = destHash;
    }
    // Stamp with receiver's local time if sender's timestamp is missing/invalid
    if (msg.timestamp < 1700000000) {
        time_t now = time(nullptr);
        if (now > 1700000000) msg.timestamp = (double)now;
    }
    Serial.printf("[LXMF] Message from %s (%d bytes) content_len=%d\n",
                  msg.sourceHash.toHex().substr(0, 8).c_str(), (int)len, (int)msg.content.size());
    if (_store) { _store->saveMessage(msg); }
    if (_onMessage) { _onMessage(msg); }
}

const std::vector<std::string>& LXMFManager::conversations() const {
    if (_store) return _store->conversations();
    static std::vector<std::string> empty;
    return empty;
}

std::vector<LXMFMessage> LXMFManager::getMessages(const std::string& peerHex) const {
    if (_store) return _store->loadConversation(peerHex);
    return {};
}

int LXMFManager::unreadCount(const std::string& peerHex) const {
    if (!_store) return 0;
    if (peerHex.empty()) return _store->totalUnreadCount();
    const ConversationSummary* s = _store->getSummary(peerHex);
    return s ? s->unreadCount : 0;
}

const ConversationSummary* LXMFManager::getConversationSummary(const std::string& peerHex) const {
    if (!_store) return nullptr;
    return _store->getSummary(peerHex);
}

void LXMFManager::markRead(const std::string& peerHex) {
    if (_store) { _store->markConversationRead(peerHex); }
}
