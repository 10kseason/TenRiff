#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

struct OnlineRecordEntry {
    int rank = 0;
    std::string player_name;
    std::int64_t score = 0;
    double accuracy = 0.0;
    int max_combo = 0;
    std::string clear_status;
    std::string ruleset_id;
    std::string verification_status;
    std::string verified_at_utc;
    std::int64_t detail_score = 0;
    double detailed_accuracy = 0.0;
    bool detail_score_available = false;
    bool detailed_accuracy_available = false;
};

struct SitesRecordBoard {
    std::string id;
    std::string title;
    std::string chart_sha256;
    std::string key_mode;
    int rate_milli = 1000;
    std::string conditions_label;
    std::string ruleset_id;
};

struct OnlineRecordsResponse {
    std::string chart_sha256;
    std::vector<OnlineRecordEntry> records;
    std::vector<SitesRecordBoard> boards;
    int board_index = 0;
};

[[nodiscard]] bool parse_sites_record_boards(std::string_view json, std::string_view title,
    std::vector<SitesRecordBoard>& output, std::string& error);
[[nodiscard]] bool parse_sites_record_rankings(std::string_view json, const SitesRecordBoard& board,
    std::vector<OnlineRecordEntry>& output, std::string& error);
[[nodiscard]] bool fetch_sites_records_once(const std::string& chart_sha256,
    const std::string& title, int board_index, OnlineRecordsResponse& output, std::string& error);

[[nodiscard]] bool parse_online_records_response(
    std::string_view json,
    std::string_view expected_chart_sha256,
    OnlineRecordsResponse& output,
    std::string& error);

// Synchronous smoke-test boundary. MenuApp uses OnlineRecordsService instead.
[[nodiscard]] bool fetch_online_records_once(
    const std::string& base_url,
    const std::string& chart_sha256,
    OnlineRecordsResponse& output,
    std::string& error);

enum class OnlineRecordsState {
    Idle,
    Loading,
    Ready,
    Error,
};

struct OnlineRecordsSnapshot {
    OnlineRecordsState state = OnlineRecordsState::Idle;
    std::string chart_sha256;
    std::vector<OnlineRecordEntry> records;
    std::string error;
    std::uint64_t revision = 0;
    bool sites = false;
    std::vector<SitesRecordBoard> boards;
    int board_index = 0;
};

// One background worker owns all HTTP activity so Song Select never blocks on
// DNS, connection, or server response latency.
class OnlineRecordsService {
public:
    OnlineRecordsService();
    ~OnlineRecordsService();

    OnlineRecordsService(const OnlineRecordsService&) = delete;
    OnlineRecordsService& operator=(const OnlineRecordsService&) = delete;

    void request(std::string base_url,
                 std::string chart_sha256,
                 bool force_refresh = false);
    void request_sites(std::string chart_sha256, std::string title, int board_index,
                       bool force_refresh = false);
    [[nodiscard]] OnlineRecordsSnapshot snapshot() const;
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::app
