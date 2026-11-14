#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "gameplay/ResultStats.h"

namespace tenriff::app {

inline constexpr std::array<std::string_view, 4> kLiveRankNames{"S", "S+", "S++", "S+++"};
inline constexpr std::array<double, 4> kLiveRankAccuracyTargets{95.0, 97.0, 98.0, 99.0};

struct LiveScoreComparison {
    bool live = false;
    bool best_available = false;
    bool rank_available = false;
    int64_t best_score = 0;
    int64_t score_difference = 0;
    std::array<double, 4> rank_difference{};
};

// Grades retain categorical accuracy. EX BEST compares 2*PG+GR at the same
// replay sample, independent of historical score multipliers or normalized scores.
// BEST is the replay engine's score at this same chart sample, never its final
// score or an estimate proportional to wall-clock time.
[[nodiscard]] inline LiveScoreComparison live_score_comparison(
    const gameplay::ResultStats& stats, double multiplier, bool eligible,
    const gameplay::ResultStats* best_stats = nullptr, double best_multiplier = 1.0) {
    LiveScoreComparison out;
    out.live = eligible;
    if (!eligible) return out;
    out.best_available = best_stats != nullptr;
    if (best_stats) {
        out.best_score = gameplay::ex_score_from_counts(best_stats->counts);
        out.score_difference = gameplay::ex_score_from_counts(stats.counts) - out.best_score;
    }
    out.rank_available = stats.accuracy_weight > 0.0 && std::isfinite(stats.accuracy_points);
    if (out.rank_available) {
        const double accuracy = stats.accuracy_percent();
        for (std::size_t i = 0; i < out.rank_difference.size(); ++i)
            out.rank_difference[i] = accuracy - kLiveRankAccuracyTargets[i];
    }
    return out;
}

// Current-run score comparisons cannot silently adopt a historical replay's
// rules or conversion. Older ghosts can still be played through Ghost Battle.
[[nodiscard]] inline bool live_score_replay_policy_matches(
    std::string_view ruleset, std::string_view current_ruleset,
    int conversion, int current_conversion, int timing, int current_timing,
    int gauge, int current_gauge, int control_flow, int current_control_flow,
    bool assisted, bool aborted) {
    return !assisted && !aborted && !ruleset.empty() && ruleset == current_ruleset &&
        ruleset.substr(0, 6) != "custom" && conversion == current_conversion &&
        timing == current_timing && gauge == current_gauge && control_flow == current_control_flow;
}

} // namespace tenriff::app
