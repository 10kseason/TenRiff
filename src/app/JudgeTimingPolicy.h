#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

#include "config/Config.h"

namespace tenriff::app {

inline constexpr double kCurrentEasyJudgeScale = 1.35;
inline constexpr double kCurrentEasyBadWindowMs = 210.0;
inline constexpr double kCurrentHardBadWindowMs = 225.0;
inline constexpr double kExBadWindowMs = 250.0;
inline constexpr double kCurrentAutomaticMissWindowMs = 340.0;

enum class JudgeTimingVersion { Ruleset1, Ruleset2And3, Current };

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

// The hit window and automatic miss deadline serve different purposes. Preserve
// historical policies so new mod windows never reinterpret recorded inputs.
[[nodiscard]] inline config::JudgeConfig judge_timing_for_policy(
    config::JudgeConfig judge, bool easy, bool hard,
    JudgeTimingVersion version = JudgeTimingVersion::Current) {
    const bool legacy = version == JudgeTimingVersion::Ruleset1;
    const bool current = version == JudgeTimingVersion::Current;
    const double scale = easy ? (legacy ? 1.25 : kCurrentEasyJudgeScale) : 1.0;
    judge.pg_ms *= scale;
    judge.gr_ms *= scale;
    judge.gd_ms *= scale;
    judge.bd_ms *= scale;
    judge.hold_grace_ms *= scale;
    judge.hold_break_ms = std::max(judge.hold_break_ms * scale, judge.hold_grace_ms);
    // RANK has already scaled PG/GR/GD. BAD is an absolute mod window in R4,
    // including for stricter RANKs; do not scale or cap it with the chart's BAD.
    if (current && easy) judge.bd_ms = kCurrentEasyBadWindowMs;
    if (hard) {
        if (current) {
            judge.pg_ms *= 17.5 / 21.0;
            judge.gr_ms *= 18.0 / 21.0;
            judge.gd_ms *= 18.0 / 21.0;
            judge.bd_ms = kCurrentHardBadWindowMs;
        } else {
            judge.bd_ms = legacy ? 340.0 : std::min(judge.bd_ms, 180.0);
        }
    }
    judge.indirect_miss_ms = legacy ? judge.bd_ms : kCurrentAutomaticMissWindowMs;
    judge.indirect_miss_enabled = hard;
    return judge;
}

// EX keeps chart RANK scaling and Hard's hold/miss policy. Only the three
// graded hit windows shrink; BAD is an absolute input window, not the later
// automatic miss deadline for an untouched note.
[[nodiscard]] inline config::JudgeConfig judge_timing_for_ex(config::JudgeConfig judge) {
    judge = judge_timing_for_policy(judge, false, true);
    judge.pg_ms *= 0.8;
    judge.gr_ms *= 0.8;
    judge.gd_ms *= 0.8;
    judge.bd_ms = kExBadWindowMs;
    return judge;
}

// User-pinned 2026-10-18. Change only on an explicit user request. R1-R8 keep
// the functions above, including their fractional boundaries. Stored profiles
// keep the 21ms reference calibration so historical/custom values are not migrated.
// Normal changes PG only. Hard reuses the complete Normal NM row and EX the
// complete Normal VHD row, independent of authored RANK, without rounding.
[[nodiscard]] inline config::JudgeConfig judge_timing_for_fixed_normal_rank(
    config::JudgeConfig base, int rank) {
    rank = rank >= 0 && rank <= 3 ? rank : 3;
    auto judge = judge_timing_for_policy(judge_timing_for_bms_rank(base, rank), false, false);
    constexpr double normal_pg[]{10.0, 15.0, 19.0, 22.0};
    judge.pg_ms = (base.pg_ms / 21.0) * normal_pg[rank];
    return judge;
}

[[nodiscard]] inline config::JudgeConfig judge_timing_for_fixed_baseline(
    config::JudgeConfig base, std::optional<int> bms_rank, bool easy, bool hard, bool ex) {
    const int rank = bms_rank && *bms_rank >= 0 && *bms_rank <= 3 ? *bms_rank : 3;
    // Easy is no longer selectable; preserve its historical token behavior.
    if (easy && !ex) {
        auto judge = bms_rank ? judge_timing_for_bms_rank(base, rank) : base;
        return judge_timing_for_policy(judge, true, hard);
    }
    auto judge = judge_timing_for_fixed_normal_rank(base, ex ? 0 : hard ? 2 : rank);
    // Only hit-window selection is shared. Hard/EX retain their separate
    // untouched-note miss classification; hold/mask/deadline policies stay put.
    judge.indirect_miss_enabled = hard || ex;
    return judge;
}

}  // namespace tenriff::app
