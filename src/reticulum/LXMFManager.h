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
    enum class DeliveryPreference : uint8_t {
        Opportunistic = 0,
        Link = 1,
    };

    using MessageCallback = std::function<void(const LXMFMessage&)>;
    using StatusCallback = std::function<void(const std::string& peerHex, double timestamp, uint32_t savedCounter, LXMFStatus status)>;
    void setStatusCallback(StatusCallback cb) { _statusCb = cb; }

    bool begin(ReticulumManager* rns, MessageStore* store);
    void loop();

    bool sendMessage(const RNS::Bytes& destHash, const std::string& content, const std::string& title = "",
                     DeliveryPreference preference = DeliveryPreference::Opportunistic);
    bool sendMessageViaLink(const RNS::Bytes& destHash, const std::string& content,
                            const std::string& title = "");
    void setMessageCallback(MessageCallback cb) { _onMessage = cb; }
    int queuedCount() const { return _outQueue.size(); }
    uint32_t storeRevision() const { return _store ? _store->revision() : 0; }
    const std::vector<std::string>& conversations() const;
    std::vector<LXMFMessage> getMessages(const std::string& peerHex) const;
    int unreadCount(const std::string& peerHex = "") const;
    void markRead(const std::string& peerHex);
    bool deleteConversation(const std::string& peerHex);
    const ConversationSummary* getConversationSummary(const std::string& peerHex) const;

private:
    bool sendDirect(LXMFMessage& msg);
    bool ensureOutboundLink(const RNS::Destination& dest, const RNS::Bytes& destHash, const char* reason);
    void clearDeliveryPreference(const LXMFMessage& msg);
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
    unsigned long _outLinkPendingSinceMs = 0;
    std::set<std::string> _linkRequiredIds;
    std::map<std::string, uint8_t> _linkWaitAttempts;

    // Deduplication: recently seen message IDs
    std::set<std::string> _seenMessageIds;
    std::deque<std::string> _seenMessageOrder;
    static constexpr int MAX_SEEN_IDS = 100;
    void rememberMessageId(const std::string& msgIdHex);

    // Outstanding delivery proofs — keyed by receipt hash hex. The recipient
    // returns a PROOF packet over RNS for opportunistic and link single
    // packets; when it arrives, we flip the message to DELIVERED.
    struct PendingProof {
        std::string peerHex;
        double timestamp;
        uint32_t savedCounter = 0;
        unsigned long createdMs = 0;
        uint8_t proofAttempts = 0;
        LXMFMessage msg;
    };
    static std::map<std::string, PendingProof> _pendingProofs;
    static void onProofDelivered(const RNS::PacketReceipt& r);
    static void onProofTimeout(const RNS::PacketReceipt& r);
    static void handleProofTimeoutHash(const std::string& receiptHash);
    void registerProofTracking(RNS::PacketReceipt& receipt, const LXMFMessage& msg);

    static LXMFManager* _instance;
};
