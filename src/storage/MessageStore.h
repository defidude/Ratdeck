#pragma once

#include <Arduino.h>
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "reticulum/LXMFMessage.h"
#include <vector>
#include <string>
#include <map>
#include <set>

struct ConversationSummary {
    double lastTimestamp = 0;
    std::string lastPreview;    // first ~20 chars of last message
    bool lastIncoming = false;
    int unreadCount = 0;
    int totalCount = 0;
    bool hasOutgoing = false;
    bool hasPending = false;
    bool hasFailed = false;
    LXMFStatus lastOutgoingStatus = LXMFStatus::DRAFT;
    uint32_t lastOutgoingCounter = 0;
    uint16_t pendingCount = 0;
    uint16_t failedCount = 0;
};

class MessageStore {
public:
    bool begin(FlashStore* flash, SDStore* sd = nullptr, bool externalStorageEnabled = false);
    void setExternalStorageEnabled(bool enabled) { _externalStorageEnabled = enabled; }
    bool externalStorageEnabled() const { return _externalStorageEnabled; }

    bool saveMessage(LXMFMessage& msg);
    std::vector<LXMFMessage> loadConversation(const std::string& peerHex) const;
    const std::vector<std::string>& conversations() const { return _conversations; }
    void refreshConversations();
    int messageCount(const std::string& peerHex) const;
    bool deleteConversation(const std::string& peerHex);
    std::vector<LXMFMessage> loadPendingOutgoing() const;
    std::vector<std::string> loadRecentMessageIds(size_t maxIds) const;
    void markConversationRead(const std::string& peerHex);
    bool updateMessageStatus(const std::string& peerHex, double timestamp, bool incoming, LXMFStatus newStatus);
    bool updateMessageStatusByCounter(const std::string& peerHex, uint32_t counter, bool incoming, LXMFStatus newStatus);

    const ConversationSummary* getSummary(const std::string& peerHex) const;
    int totalUnreadCount() const;
    uint32_t revision() const { return _revision; }

private:
    String conversationDir(const std::string& peerHex) const;
    String sdConversationDir(const std::string& peerHex) const;
    void enforceFlashLimit(const std::string& peerHex);
    void enforceSDLimit(const std::string& peerHex);
    void migrateFlashToSD();
    void migrateTruncatedDirs();
    void initReceiveCounter();
    void buildSummaries();
    void rebuildSummary(const std::string& peerHex);
    ConversationSummary buildSummaryForPeer(const std::string& peerHex) const;
    void updateSummaryStatus(const std::string& peerHex, uint32_t counter, LXMFStatus oldStatus, LXMFStatus newStatus);
    void bumpRevision();

    FlashStore* _flash = nullptr;
    SDStore* _sd = nullptr;
    bool _externalStorageEnabled = false;
    std::vector<std::string> _conversations;
    std::map<std::string, ConversationSummary> _summaries;
    uint32_t _nextReceiveCounter = 0;
    uint32_t _revision = 0;
};
