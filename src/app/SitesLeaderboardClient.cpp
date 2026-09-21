#include "app/SitesLeaderboardClient.h"
#include "app/ChartFileHash.h"
#include "app/ModeManager.h"
#include "app/RankedRecordsClient.h"
#include "app/ReplayVerifier.h"
#include "config/SimpleJson.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>

namespace tenriff::app {
namespace {
bool is_hash(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
std::string sites_utc_timestamp(std::string_view value) {
    // GameSession exports filename-safe YYYYMMDD_HHMMSSZ timestamps.
    if (value.size() == 16 && value[8] == '_' && value[15] == 'Z') {
        const std::string text(value);
        return text.substr(0, 4) + "-" + text.substr(4, 2) + "-" + text.substr(6, 2) +
               "T" + text.substr(9, 2) + ":" + text.substr(11, 2) + ":" + text.substr(13, 2) + "Z";
    }
    return std::string(value);
}
std::string quoted(std::string_view text) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') output << '\\' << c;
        else if (c < 0x20) output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else output << c;
    }
    output << '"';
    return output.str();
}
}  // namespace

bool build_sites_score_json(const gameplay::ReplayFile& replay,
    std::string_view replay_sha256, std::string_view chart_title,
    std::string_view clear_status, std::string& payload, std::string& error) {
    payload.clear();
    error.clear();
    if (replay.ruleset_id != kCanonicalReplayRulesetId || replay.replay_format_version != gameplay::kReplayFormatVersion ||
        replay.aborted || replay.pause_used || replay.mode.autoplay_enabled ||
        replay.mode.practice_no_fail_enabled || replay.mode.one_miss_fail_enabled ||
        !replay.mode.course_gauge.empty() || mode_mod_adds_notes(replay.mods)) {
        error = "Sites upload skipped: this play is not eligible for the community leaderboard."; return false;
    }
    const auto& stats = replay.stats;
    // Percentage helpers deliberately tolerate legacy stats. Reject damaged raw
    // accumulators here so NaN/Inf cannot silently become a valid upload of 0%.
    const auto& counts = stats.counts;
    const int64_t judged = static_cast<int64_t>(counts.pg) + counts.gr + counts.gd + counts.bd + counts.pr;
    if (stats.detail_score < 0 || stats.detail_score > 1'000'000'000 ||
        counts.pg < 0 || counts.gr < 0 || counts.gd < 0 || counts.bd < 0 || counts.pr < 0 ||
        judged > 10'000'000 ||
        !std::isfinite(stats.accuracy_points) || stats.accuracy_points < 0 ||
        !std::isfinite(stats.accuracy_weight) || stats.accuracy_weight < 0 ||
        !std::isfinite(stats.detailed_accuracy_points) || stats.detailed_accuracy_points < 0 ||
        !std::isfinite(stats.detailed_accuracy_weight) || stats.detailed_accuracy_weight < 0 ||
        stats.accuracy_points > stats.accuracy_weight ||
        stats.detailed_accuracy_points > stats.detailed_accuracy_weight) {
        error = "Sites upload skipped: invalid score statistics."; return false;
    }
    const auto lanes = replay.trace.lane_count;
    const double accuracy = replay.stats.accuracy_percent();
    const double detailed_accuracy = replay.stats.detailed_accuracy_percent();
    if (!is_hash(replay.chart_sha256) || !is_hash(replay_sha256) || lanes < 4 || lanes > 14 ||
        !std::isfinite(replay.rate) || replay.rate < 0.5 || replay.rate > 2.0 ||
        replay.final_score < 0 || replay.final_score > 1'000'000'000 ||
        !std::isfinite(accuracy) || accuracy < 0 || accuracy > 100 ||
        !std::isfinite(detailed_accuracy) || detailed_accuracy < 0 || detailed_accuracy > 100 ||
        replay.stats.max_combo < 0 || replay.stats.max_combo > 10'000'000 ||
        chart_title.empty() || chart_title.size() > 480 || clear_status.empty() || clear_status.size() > 32) {
        error = "Sites upload skipped: invalid result metadata."; return false;
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\"schema_version\":1,\"chart_sha256\":" << quoted(replay.chart_sha256)
           << ",\"replay_sha256\":" << quoted(replay_sha256)
           << ",\"chart_title\":" << quoted(chart_title)
           << ",\"key_mode\":" << quoted(std::to_string(lanes) + "K")
           << ",\"rate_milli\":" << std::llround(replay.rate * 1000.0)
           << ",\"ruleset_id\":" << quoted(replay.ruleset_id)
           << ",\"gauge\":" << quoted(replay.mode.gauge.empty() ? "normal" : replay.mode.gauge)
           << ",\"random\":" << quoted(replay.mode.random.empty() ? "off" : replay.mode.random)
           << ",\"key_conversion\":" << quoted(replay.mode.key_conversion_algorithm.empty() ? "none" :
               replay.mode.key_conversion_algorithm + ":" + replay.mode.key_conversion_nk2_preset)
           << ",\"mods\":[";
    for (std::size_t i = 0; i < replay.mods.size(); ++i) {
        if (i) output << ',';
        output << quoted(replay.mods[i]);
    }
    // Keep the binary64 percentages intact through JSON; display rounding must
    // not turn distinct tied scores into identical comparison values.
    output << "],\"score\":" << replay.final_score
           << ",\"detail_score\":" << stats.detail_score
           << ",\"accuracy\":" << std::setprecision(std::numeric_limits<double>::max_digits10) << accuracy
           << ",\"detailed_accuracy\":" << detailed_accuracy
           << ",\"max_combo\":" << replay.stats.max_combo << ",\"clear_status\":" << quoted(clear_status)
           << ",\"played_at\":" << quoted(sites_utc_timestamp(replay.created_utc))
           << ",\"autoplay\":false,\"practice\":false,\"aborted\":false,\"pause_used\":false}";
    payload = output.str();
    return true;
}

SitesLeaderboardService::~SitesLeaderboardService() { shutdown(); }

void SitesLeaderboardService::enqueue(std::filesystem::path replay_path, std::string replay_sha256,
                                      std::string chart_title, std::string clear_status) {
    std::error_code ec;
    const bool protected_file = std::filesystem::is_regular_file("config/sites-leaderboard.dpapi", ec);
    ec.clear();
    if (!protected_file && !std::filesystem::is_regular_file("config/sites-leaderboard.json", ec)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) return;
    if (pending_.size() >= 16) {
        messages_.push_back("Sites upload queue is full; this result remains saved locally.");
        return;
    }
    pending_.push_back({std::move(replay_path), std::move(replay_sha256), std::move(chart_title), std::move(clear_status)});
    if (!worker_.joinable()) worker_ = std::thread(&SitesLeaderboardService::worker_main, this);
    wake_.notify_one();
}

void SitesLeaderboardService::worker_main() {
    for (;;) {
        PendingScore pending;
        std::uint64_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
            if (stopping_) return;
            generation = generation_;
            pending = std::move(pending_.front());
            pending_.pop_front();
        }
        std::string message;
        try {
            std::string error, payload;
            bool missing = false;
            SitesLeaderboardConnection connection;
            std::error_code ec;
            const auto replay_size = std::filesystem::file_size(pending.replay_path, ec);
            if (ec || replay_size > 32 * 1024 * 1024) {
                message = "Sites upload skipped: replay evidence is missing or too large.";
            } else if (!load_sites_leaderboard_connection("config", connection, missing, error) || missing) {
                message = error;
            } else {
                const auto hashes = hash_chart_file(pending.replay_path, &error);
                const auto loaded = gameplay::load_replay_json(pending.replay_path.u8string());
                if (!hashes.valid() || hashes.sha256 != pending.replay_sha256) {
                    message = "Sites upload skipped: replay evidence no longer matches the result.";
                } else if (!loaded.success()) {
                    message = "Sites upload skipped: " + loaded.error;
                } else if (!build_sites_score_json(*loaded.replay, pending.replay_sha256,
                           pending.chart_title, pending.clear_status, payload, error)) {
                    message = error;
                } else {
                    bool accepted = false;
                    for (int attempt = 0; attempt < 3; ++attempt) {
                        { std::lock_guard<std::mutex> lock(mutex_); if (stopping_ || generation != generation_) break; }
                        bool retryable = false;
                        accepted = submit_sites_leaderboard_score(connection.site_url, connection.upload_token,
                                                                  payload, retryable, error);
                        if (accepted || !retryable) break;
                        std::unique_lock<std::mutex> lock(mutex_);
                        wake_.wait_for(lock, std::chrono::seconds(attempt + 1), [this, generation] { return stopping_ || generation != generation_; });
                        if (stopping_) return;
                        if (generation != generation_) break;
                    }
                    message = accepted ? "Sites community record uploaded." : "Sites upload failed; result saved locally: " + error;
                }
            }
            std::fill(connection.upload_token.begin(), connection.upload_token.end(), '\0');
        } catch (...) {
            message = "Sites upload failed; result remains saved locally.";
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation != generation_) continue;
            if (messages_.size() >= 16) messages_.erase(messages_.begin());
            messages_.push_back(std::move(message));
        }
    }
}
std::vector<std::string> SitesLeaderboardService::take_messages() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> messages;
    messages.swap(messages_);
    return messages;
}
void SitesLeaderboardService::cancel_pending() {
    { std::lock_guard<std::mutex> lock(mutex_); ++generation_; pending_.clear(); messages_.clear(); }
    wake_.notify_all();
}
void SitesLeaderboardService::shutdown() {
    { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; pending_.clear(); }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}
}  // namespace tenriff::app
