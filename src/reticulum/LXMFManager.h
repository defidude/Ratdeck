#pragma once

#include "LXMFMessage.h"
#include "ReticulumManager.h"
#include "storage/MessageStore.h"
#include <Destination.h>
#include <Packet.h>
#include <Link.h>
#include <Identity.h>
#include <functional>
#include <deque>
#include <set>
#include <map>

class LXMFManager {
public:
    using MessageCallback = std::function<void(const LXMFMessage&)>;
    using StatusCallback = std::function<void(const std::string& peerHex, double timestamp, LXMFStatus status)>;
    void setStatusCallback(StatusCallback cb) { _statusCb = cb; }

    bool begin(ReticulumManager* rns, MessageStore* store);
    void loop();

    bool sendMessage(const RNS::Bytes& destHash, const std::string& content, const std::string& title = "");
    void setMessageCallback(MessageCallback cb) { _onMessage = cb; }
    int queuedCount() const { return _outQueue.size(); }
    const std::vector<std::string>& conversations() const;
    std::vector<LXMFMessage> getMessages(const std::string& peerHex) const;
    int unreadCount(const std::string& peerHex = "") const;
    void markRead(const std::string& peerHex);
    const ConversationSummary* getConversationSummary(const std::string& peerHex) const;

private:
    bool sendDirect(LXMFMessage& msg);
    void processIncoming(const uint8_t* data, size_t len, const RNS::Bytes& destHash);
    static void onPacketReceived(const RNS::Bytes& data, const RNS::Packet& packet);
    static void onLinkEstablished(RNS::Link& link);
    static void onOutLinkEstablished(RNS::Link& link);
    static void onOutLinkClosed(RNS::Link& link);

    ReticulumManager* _rns = nullptr;
    MessageStore* _store = nullptr;
    MessageCallback _onMessage;
    StatusCallback _statusCb;
    std::deque<LXMFMessage> _outQueue;

    // Outbound link state (opportunistic-first, link upgrades in background)
    RNS::Link _outLink{RNS::Type::NONE};
    RNS::Bytes _outLinkDestHash;       // Destination the ACTIVE _outLink is for
    RNS::Bytes _outLinkPendingHash;    // Destination being connected to (not yet established)
    bool _outLinkPending = false;

    // Deduplication: recently seen message IDs
    std::set<std::string> _seenMessageIds;
    static constexpr int MAX_SEEN_IDS = 100;

    // Outstanding delivery proofs — keyed by receipt hash hex. The recipient
    // returns a PROOF packet over RNS for opportunistic and link single
    // packets; when it arrives, we flip the message to DELIVERED.
    struct PendingProof {
        std::string peerHex;
        double timestamp;
    };
    static std::map<std::string, PendingProof> _pendingProofs;
    static void onProofDelivered(const RNS::PacketReceipt& r);
    static void onProofTimeout(const RNS::PacketReceipt& r);
    void registerProofTracking(RNS::PacketReceipt& receipt, const LXMFMessage& msg);

    static LXMFManager* _instance;
};
