#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "app/ChartLoader.h"
#include "config/Config.h"
#include "gameplay/Replay.h"

namespace tenriff::app {

inline constexpr std::string_view kCanonicalReplayRulesetId =
    "tenriff-native-score-v2-ruleset-1";
inline constexpr int64_t kReplayVerificationLeadInMs = 3000;

enum class ReplayVerificationStatus {
    Verified,
    LegacyUnverified,
    CustomRuleset,
    MissingEvidence,
    Invalid,
};

struct ReplayVerificationResult {
    ReplayVerificationStatus status = ReplayVerificationStatus::Invalid;
    std::string detail;
    std::string chart_sha256;
    std::string replay_sha256;
    gameplay::ResultStats stats;
    std::string clear_status = "FAILED";
    std::string final_gauge = "normal";
    double final_gauge_value = 0.0;
    double rate_multiplier = 1.0;
    double score_multiplier = 1.0;
    int64_t final_score = 0;
    bool game_over = true;
    bool claims_match = false;
    bool official_eligible = false;

    [[nodiscard]] bool verified() const {
        return status == ReplayVerificationStatus::Verified;
    }
};

[[nodiscard]] std::string_view replay_verification_status_token(ReplayVerificationStatus status);
[[nodiscard]] bool is_canonical_score_ruleset(const config::JudgeConfig& judge,
                                              const game::GaugeConfig& gauge);
[[nodiscard]] std::string replay_ruleset_id_for_runtime(const config::JudgeConfig& judge,
                                                        const game::GaugeConfig& gauge,
                                                        bool standard_gauge_shift,
                                                        bool course_gauge,
                                                        std::string_view pacemaker_mode);

[[nodiscard]] ReplayVerificationResult verify_replay_against_chart(
    const gameplay::ReplayFile& replay,
    const gameplay::GameplayChart& source_chart,
    ChartFormat chart_format,
    double base_bpm = 0.0);

[[nodiscard]] ReplayVerificationResult verify_replay_file(
    const std::filesystem::path& replay_path,
    const std::filesystem::path& expected_chart_path = {},
    std::string_view expected_replay_sha256 = {});

}  // namespace tenriff::app
