#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tenriff::app {

enum class SessionMixPhase {
    Warmup,
    Challenge,
    Cooldown,
};

struct SessionMixPhaseCounts {
    int warmup = 0;
    int challenge = 0;
    int cooldown = 0;

    [[nodiscard]] int total() const { return warmup + challenge + cooldown; }
};

struct SessionMixCandidate {
    std::string chart_id;
    int difficulty = 0;
    bool has_record = false;
    bool cleared = false;
    double accuracy = 0.0;
    // Same music with alternate difficulties shares this identity; empty means chart_id.
    std::string music_id;
};

struct SessionMixEntry {
    std::string chart_id;
    SessionMixPhase phase = SessionMixPhase::Challenge;
    int difficulty = 0;
};

struct SessionMixDraftEntry {
    std::string chart_id;
    std::string chart_md5;
    std::string title;
    int key_count = 0;
    int difficulty = 0;
};

struct SessionMixPlan {
    int target_minutes = 30;
    std::vector<SessionMixEntry> entries;
};

[[nodiscard]] inline int cycle_session_mix_minutes(int minutes, int direction) {
    constexpr int values[] = {15, 30, 60};
    int index = 1;
    for (int i = 0; i < 3; ++i) {
        if (values[i] == minutes) {
            index = i;
            break;
        }
    }
    index = (index + (direction < 0 ? -1 : 1) + 3) % 3;
    return values[index];
}

[[nodiscard]] inline SessionMixPhaseCounts session_mix_phase_counts(int minutes,
                                                                     std::size_t available) {
    SessionMixPhaseCounts desired;
    if (minutes <= 15) {
        desired = {1, 3, 1};
    } else if (minutes >= 60) {
        desired = {5, 12, 3};
    } else {
        desired = {3, 5, 2};
    }

    const int target = static_cast<int>(std::min<std::size_t>(
        available, static_cast<std::size_t>(desired.total())));
    if (target <= 0) {
        return {};
    }
    if (target == 1) {
        return {0, 1, 0};
    }
    if (target == 2) {
        return {1, 1, 0};
    }

    SessionMixPhaseCounts counts;
    counts.warmup = std::max(1, target * desired.warmup / desired.total());
    counts.cooldown = std::max(1, target * desired.cooldown / desired.total());
    counts.challenge = target - counts.warmup - counts.cooldown;
    while (counts.challenge < 1 && counts.warmup > 1) {
        --counts.warmup;
        ++counts.challenge;
    }
    while (counts.challenge < 1 && counts.cooldown > 1) {
        --counts.cooldown;
        ++counts.challenge;
    }
    return counts;
}

namespace session_mix_detail {

[[nodiscard]] inline std::uint64_t stable_jitter(std::string_view chart_id,
                                                 std::uint32_t seed,
                                                 SessionMixPhase phase) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char ch : chart_id) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    hash ^= static_cast<std::uint64_t>(seed) +
            static_cast<std::uint64_t>(phase) * 0x9E3779B97F4A7C15ULL;
    // Avalanche the seed so rerolling changes ties independently per song.
    hash ^= hash >> 30; hash *= 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 27; hash *= 0x94d049bb133111ebULL;
    hash ^= hash >> 31;
    return hash % 300ULL;
}

[[nodiscard]] inline int player_difficulty_anchor(const std::vector<SessionMixCandidate>& candidates) {
    std::vector<int> cleared_levels;
    std::vector<int> known_levels;
    for (const auto& candidate : candidates) {
        if (candidate.difficulty <= 0) {
            continue;
        }
        known_levels.push_back(candidate.difficulty);
        if (candidate.has_record && candidate.cleared && std::isfinite(candidate.accuracy) &&
            candidate.accuracy >= 80.0 && candidate.accuracy <= 100.0) {
            cleared_levels.push_back(candidate.difficulty);
        }
    }

    auto& levels = cleared_levels.empty() ? known_levels : cleared_levels;
    if (levels.empty()) {
        return 1;
    }
    std::sort(levels.begin(), levels.end());
    const std::size_t index = cleared_levels.empty()
                                  ? (levels.size() - 1) / 4
                                  : ((levels.size() - 1) * 3) / 4;
    return levels[index];
}

[[nodiscard]] inline std::int64_t candidate_score(const SessionMixCandidate& candidate,
                                                  SessionMixPhase phase,
                                                  int anchor,
                                                  bool any_known_difficulty,
                                                  std::uint32_t seed,
                                                  int slot, int count, int previous) {
    int64_t target = anchor;
    if (phase == SessionMixPhase::Warmup) {
        target = std::max<int64_t>(1, static_cast<int64_t>(anchor) - 2 + (slot * 2 / std::max(1, count)));
    } else if (phase == SessionMixPhase::Challenge) {
        target = static_cast<int64_t>(anchor) + (slot * 2 / std::max(1, count));
    } else {
        target = std::max<int64_t>(1, static_cast<int64_t>(anchor) - 1 - (slot * 2 / std::max(1, count)));
    }

    std::int64_t score = 0;
    if (candidate.difficulty > 0) {
        const int64_t distance = candidate.difficulty > target
                                 ? candidate.difficulty - target
                                 : target - candidate.difficulty;
        score += distance * 1000;
        // Prefer a gradual route and avoid overshooting the local-record anchor.
        if (previous > 0) score += std::abs(static_cast<int64_t>(candidate.difficulty) - previous) * 200;
        const int64_t ceiling = static_cast<int64_t>(anchor) + (phase == SessionMixPhase::Challenge ? 2 : 0);
        if (candidate.difficulty > ceiling) score += (candidate.difficulty - ceiling) * 4000;
    } else if (any_known_difficulty) {
        score += 4000;
    }

    if (phase == SessionMixPhase::Challenge) {
        if (!candidate.has_record) {
            score -= 300;
        } else if (!candidate.cleared) {
            score -= 500;
        } else if (candidate.accuracy < 95.0) {
            score -= 200;
        } else {
            score += 400;
        }
    } else {
        if (!candidate.has_record) {
            score += 800;
        } else if (!candidate.cleared) {
            score += 1200;
        } else if (candidate.accuracy < 90.0) {
            score += 500;
        }
    }

    score += static_cast<std::int64_t>(stable_jitter(candidate.chart_id, seed, phase));
    return score;
}

}  // namespace session_mix_detail

[[nodiscard]] inline SessionMixPlan build_session_mix_plan(std::vector<SessionMixCandidate> candidates,
                                                            int minutes,
                                                            std::uint32_t seed) {
    SessionMixPlan plan;
    plan.target_minutes = minutes <= 15 ? 15 : (minutes >= 60 ? 60 : 30);

    std::unordered_set<std::string> seen;
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(), [&](const SessionMixCandidate& candidate) {
            return candidate.chart_id.empty() || !seen.insert(candidate.chart_id).second;
        }),
        candidates.end());
    if (candidates.empty()) {
        return plan;
    }

    std::unordered_set<std::string> music;
    for (const auto& candidate : candidates)
        music.insert(candidate.music_id.empty() ? candidate.chart_id : candidate.music_id);
    const SessionMixPhaseCounts counts =
        session_mix_phase_counts(plan.target_minutes, music.size());
    const int anchor = session_mix_detail::player_difficulty_anchor(candidates);
    const bool any_known_difficulty = std::any_of(
        candidates.begin(), candidates.end(),
        [](const SessionMixCandidate& candidate) { return candidate.difficulty > 0; });
    std::unordered_set<std::string> used_music;
    int previous_difficulty = 0;

    const auto append_phase = [&](SessionMixPhase phase, int count) {
        for (int slot = 0; slot < count; ++slot) {
            std::size_t best_index = candidates.size();
            std::int64_t best_score = (std::numeric_limits<std::int64_t>::max)();
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                const auto& identity = candidates[i].music_id.empty() ? candidates[i].chart_id : candidates[i].music_id;
                if (used_music.count(identity)) {
                    continue;
                }
                const std::int64_t score = session_mix_detail::candidate_score(
                    candidates[i], phase, anchor, any_known_difficulty, seed, slot, count, previous_difficulty);
                if (score < best_score || (score == best_score && best_index < candidates.size() &&
                                          candidates[i].chart_id < candidates[best_index].chart_id)) {
                    best_score = score;
                    best_index = i;
                }
            }
            if (best_index == candidates.size()) {
                return;
            }
            const auto& chosen = candidates[best_index];
            used_music.insert(chosen.music_id.empty() ? chosen.chart_id : chosen.music_id);
            previous_difficulty = chosen.difficulty;
            plan.entries.push_back(SessionMixEntry{
                candidates[best_index].chart_id,
                phase,
                candidates[best_index].difficulty,
            });
        }
    };

    append_phase(SessionMixPhase::Warmup, counts.warmup);
    append_phase(SessionMixPhase::Challenge, counts.challenge);
    append_phase(SessionMixPhase::Cooldown, counts.cooldown);
    return plan;
}

[[nodiscard]] inline SessionMixPlan build_session_mix_draft_plan(
    const std::vector<SessionMixDraftEntry>& draft) {
    SessionMixPlan plan;
    plan.target_minutes = static_cast<int>(draft.size()) * 3;
    plan.entries.reserve(draft.size());
    for (const auto& entry : draft) {
        if (!entry.chart_id.empty()) {
            plan.entries.push_back(
                SessionMixEntry{entry.chart_id, SessionMixPhase::Challenge, entry.difficulty});
        }
    }
    return plan;
}

}  // namespace tenriff::app
