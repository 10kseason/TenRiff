#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "gameplay/Replay.h"
#include "app/SitesLeaderboardConnection.h"

namespace tenriff::app {
[[nodiscard]] bool build_sites_score_json(const gameplay::ReplayFile& replay,
    std::string_view replay_sha256, std::string_view chart_title,
    std::string_view clear_status, std::string& payload, std::string& error);

// Opt-in, bounded background uploads. No account tokens or filesystem paths enter the payload.
class SitesLeaderboardService {
public:
    SitesLeaderboardService() = default;
    ~SitesLeaderboardService();
    SitesLeaderboardService(const SitesLeaderboardService&) = delete;
    SitesLeaderboardService& operator=(const SitesLeaderboardService&) = delete;
    void enqueue(std::filesystem::path replay_path, std::string replay_sha256,
                 std::string chart_title, std::string clear_status);
    [[nodiscard]] std::vector<std::string> take_messages();
    void cancel_pending();
    void shutdown();
private:
    struct PendingScore {
        std::filesystem::path replay_path;
        std::string replay_sha256;
        std::string chart_title;
        std::string clear_status;
    };
    void worker_main();
    std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stopping_ = false;
    std::uint64_t generation_ = 0;
    std::deque<PendingScore> pending_;
    std::vector<std::string> messages_;
};
}  // namespace tenriff::app
