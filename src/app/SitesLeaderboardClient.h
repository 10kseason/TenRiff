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
#include "app/RankedRecordsClient.h"
#include "app/SitesUploadStatus.h"

namespace tenriff::app {
[[nodiscard]] bool build_sites_score_json(const gameplay::ReplayFile& replay,
    std::string_view replay_sha256, std::string_view chart_title,
    std::string_view clear_status, std::string& payload, std::string& error,
    std::string_view chart_author = {}, std::string_view bms_branch_sha256 = {});

// Worker-only chart I/O: validates replay policy and exact original chart bytes,
// then hashes the selected active content for control v2. Legacy/nonbranch
// records return an empty identity without opening a chart file.
[[nodiscard]] bool prepare_sites_bms_branch_identity(const gameplay::ReplayFile& replay,
    std::string& branch_sha256, std::string& error);

// Branch identity v1 hashes this domain followed by BmsParseResult::active_text.
inline constexpr std::string_view kSitesBmsBranchIdentityDomain = "TenRiff:BMS-active-branch:v1\n";

// Opt-in, bounded background uploads. No account tokens or filesystem paths enter the payload.
class SitesLeaderboardService {
public:
    enum class EnqueueResult { Queued, NotConnected, Unavailable, Disabled };
    using SubmitScore = bool (*)(const std::string&, const std::string&, const std::string&, bool&, std::string&);
    explicit SitesLeaderboardService(SubmitScore submit_score = submit_sites_leaderboard_score)
        : submit_score_(submit_score) {}
    ~SitesLeaderboardService();
    SitesLeaderboardService(const SitesLeaderboardService&) = delete;
    SitesLeaderboardService& operator=(const SitesLeaderboardService&) = delete;
    [[nodiscard]] EnqueueResult enqueue(std::filesystem::path replay_path, std::string replay_sha256,
                 std::string chart_title, std::string clear_status, std::string chart_author = {});
    [[nodiscard]] std::vector<SitesUploadMessage> take_messages();
    // Disabling waits for a started submission, then clears all queued work.
    // After return no submission may start until explicitly enabled again.
    void set_online_enabled(bool enabled);
    void cancel_pending();
    void shutdown();
private:
    friend struct SitesLeaderboardServiceTestAccess;
    struct PendingScore {
        std::filesystem::path replay_path;
        std::string replay_sha256;
        std::string chart_title;
        std::string clear_status;
        std::string chart_author;
    };
    void worker_main();
    bool submit_if_current(std::uint64_t generation, const SitesLeaderboardConnection& connection,
                           const std::string& payload, bool& retryable, std::string& error);
    // Lock order: submission_mutex_ before mutex_. Do not hold mutex_ across HTTP.
    std::mutex submission_mutex_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stopping_ = false;
    bool online_enabled_ = false;
    SubmitScore submit_score_;
    std::uint64_t generation_ = 0;
    std::deque<PendingScore> pending_;
    std::vector<SitesUploadMessage> messages_;
};
}  // namespace tenriff::app
