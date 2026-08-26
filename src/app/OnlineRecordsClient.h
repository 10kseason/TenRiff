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
};

struct OnlineRecordsResponse {
    std::string chart_sha256;
    std::vector<OnlineRecordEntry> records;
};

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
    [[nodiscard]] OnlineRecordsSnapshot snapshot() const;
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::app
