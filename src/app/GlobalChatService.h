#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/RankedRecordsClient.h"
#include "network/HostedPeerSession.h"

namespace tenriff::app {

struct GlobalChatSnapshot {
    bool configured = false;
    bool online = false;
    bool sending = false;
    std::string error;
    std::vector<RankedGlobalChatMessage> messages;
    std::uint64_t revision = 0;
};

class GlobalChatService {
public:
    explicit GlobalChatService(network::HostedHttpTransport transport = {});
    ~GlobalChatService();
    GlobalChatService(const GlobalChatService&) = delete;
    GlobalChatService& operator=(const GlobalChatService&) = delete;

    // Only the official fixed-origin service receives the account credential.
    // Repeating an active configuration does not clear history or reconnect.
    void configure(std::string upload_key);
    void clear();
    [[nodiscard]] bool send(std::string text);
    [[nodiscard]] GlobalChatSnapshot snapshot() const;
    void shutdown();

private:
    void worker_main();
    network::HostedHttpTransport transport_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stopping_ = false;
    bool configured_ = false;
    bool blocked_ = false;
    bool online_ = false;
    bool sending_ = false;
    std::string upload_key_;
    std::string error_;
    std::deque<std::string> pending_messages_;
    std::vector<RankedGlobalChatMessage> messages_;
    std::int64_t last_message_id_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t revision_ = 0;
};

}  // namespace tenriff::app
