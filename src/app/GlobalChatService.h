#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/RankedRecordsClient.h"

namespace tenriff::app {

struct GlobalChatSnapshot {
    bool configured = false;
    bool online = false;
    bool sending = false;
    std::string error;
    std::vector<RankedGlobalChatMessage> messages;
    std::string room_error;
    std::vector<RankedMultiplayerRoom> rooms;
    std::uint64_t revision = 0;
};

class GlobalChatService {
public:
    GlobalChatService() = default;
    ~GlobalChatService();
    GlobalChatService(const GlobalChatService&) = delete;
    GlobalChatService& operator=(const GlobalChatService&) = delete;

    void configure(std::string base_url, std::string bearer_token);
    void clear();
    [[nodiscard]] bool send(std::string text);
    [[nodiscard]] GlobalChatSnapshot snapshot() const;
    void shutdown();

private:
    void ensure_worker();
    void worker_main();

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stopping_ = false;
    bool configured_ = false;
    bool online_ = false;
    bool sending_ = false;
    std::string base_url_;
    std::string bearer_token_;
    std::string error_;
    std::deque<std::string> pending_messages_;
    std::vector<RankedGlobalChatMessage> messages_;
    std::string room_error_;
    std::vector<RankedMultiplayerRoom> rooms_;
    std::int64_t last_message_id_ = 0;
    std::uint64_t revision_ = 0;
};

}  // namespace tenriff::app
