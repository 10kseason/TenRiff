#pragma once

#include <algorithm>

#include "config/Config.h"

namespace tenriff::app {

inline constexpr double kCurrentEasyJudgeScale = 1.35;
inline constexpr double kCurrentHardBadWindowMs = 180.0;
inline constexpr double kCurrentAutomaticMissWindowMs = 340.0;

// Rank scales only note judgement windows. Hold-release tolerance, automatic
// miss deadlines, and input masking are separate policies and stay unchanged.
[[nodiscard]] inline config::JudgeConfig judge_timing_for_bms_rank(
    config::JudgeConfig judge, int rank) {
    const double perfect_ms = rank == 0 ? 8.0 : rank == 1 ? 15.0 : rank == 2 ? 18.0 : 21.0;
    const double scale = perfect_ms / 21.0;
    judge.pg_ms *= scale;
    judge.gr_ms *= scale;
    judge.gd_ms *= scale;
    judge.bd_ms *= scale;
    return judge;
}

// The hit window and automatic miss deadline serve different purposes. Keeping
// the old exact policy here lets ruleset-1 replays retain their original timing.
[[nodiscard]] inline config::JudgeConfig judge_timing_for_policy(
    config::JudgeConfig judge, bool easy, bool hard, bool legacy = false) {
    const double scale = easy ? (legacy ? 1.25 : kCurrentEasyJudgeScale) : 1.0;
    judge.pg_ms *= scale;
    judge.gr_ms *= scale;
    judge.gd_ms *= scale;
    judge.bd_ms *= scale;
    judge.hold_grace_ms *= scale;
    judge.hold_break_ms = std::max(judge.hold_break_ms * scale, judge.hold_grace_ms);
    if (hard) {
        judge.bd_ms = legacy ? 340.0 : std::min(judge.bd_ms, kCurrentHardBadWindowMs);
    }
    judge.indirect_miss_ms = legacy ? judge.bd_ms : kCurrentAutomaticMissWindowMs;
    judge.indirect_miss_enabled = hard;
    return judge;
}

}  // namespace tenriff::app
