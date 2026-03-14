#include "gameplay/ModeApplier.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>

namespace tenriff::gameplay {

namespace {

int resolve_lane_count(const GameplayChart& chart) {
    int lane_count = chart.lane_count;
    if (lane_count > 0) {
        return lane_count;
    }
    for (const auto& note : chart.notes) {
        lane_count = std::max(lane_count, note.lane);
    }
    return lane_count > 0 ? lane_count : 10;
}

int target_lane_count(KeyMode mode) {
    switch (mode) {
        case KeyMode::Keys4: return 4;
        case KeyMode::Keys5: return 5;
        case KeyMode::Keys6: return 6;
        case KeyMode::Keys7: return 7;
        case KeyMode::Keys8: return 8;
        case KeyMode::Keys9: return 9;
        case KeyMode::Keys10: return 10;
        case KeyMode::Keys16: return 16;
        case KeyMode::Auto: default: return 0;
    }
}

uint32_t normalize_seed(uint32_t seed) {
    return seed;
}

void apply_full_random(GameplayChart& chart, uint32_t seed) {
    const int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        return;
    }

    std::vector<int> permutation(static_cast<std::size_t>(lane_count));
    std::iota(permutation.begin(), permutation.end(), 1);
    std::mt19937 rng(normalize_seed(seed));
    std::shuffle(permutation.begin(), permutation.end(), rng);

    for (auto& note : chart.notes) {
        if (note.lane <= 0 || note.lane > lane_count) {
            continue;
        }
        note.lane = permutation[static_cast<std::size_t>(note.lane - 1)];
    }
}

void apply_super_random(GameplayChart& chart, uint32_t seed, std::vector<std::string>& warnings) {
    const int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        return;
    }

    struct NoteSpan {
        std::size_t index = 0;
        int lane = 0;
        int64_t start = 0;
        int64_t end = 0;
    };

    std::vector<NoteSpan> spans;
    spans.reserve(chart.notes.size());

    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& note = chart.notes[i];
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({i, note.lane, note.start_sample, end});
    }

    std::sort(spans.begin(), spans.end(), [](const NoteSpan& lhs, const NoteSpan& rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });

    std::vector<int64_t> lane_end(static_cast<std::size_t>(lane_count),
                                  std::numeric_limits<int64_t>::min());
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(lane_count));

    std::mt19937 rng(normalize_seed(seed));
    std::size_t fallback_count = 0;

    for (const auto& span : spans) {
        candidates.clear();
        for (int lane = 1; lane <= lane_count; ++lane) {
            if (span.start > lane_end[static_cast<std::size_t>(lane - 1)]) {
                candidates.push_back(lane);
            }
        }

        int chosen_lane = 0;
        if (candidates.empty()) {
            ++fallback_count;
            chosen_lane = std::clamp(span.lane, 1, lane_count);
        } else {
            std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
            chosen_lane = candidates[dist(rng)];
        }

        chart.notes[span.index].lane = chosen_lane;
        const auto lane_index = static_cast<std::size_t>(chosen_lane - 1);
        lane_end[lane_index] = std::max(lane_end[lane_index], span.end);
    }

    if (fallback_count > 0) {
        warnings.push_back("SR fallback: overlapping notes could not be fully avoided (count=" +
                           std::to_string(fallback_count) + ").");
    }
}

}  // namespace

ModeApplyResult apply_mode_settings(const GameplayChart& chart, const ModeSettings& settings) {
    ModeApplyResult result;
    result.chart = chart;
    result.chart.lane_count = resolve_lane_count(chart);

    const int target_count = target_lane_count(settings.key_mode);
    if (target_count > 0 && target_count != result.chart.lane_count) {
        if (target_count < result.chart.lane_count) {
            auto& notes = result.chart.notes;
            notes.erase(std::remove_if(notes.begin(), notes.end(), [target_count](const NoteEvent& note) {
                return note.lane > target_count;
            }), notes.end());
            result.chart.lane_count = target_count;
            result.warnings.push_back("Key mode reduces lanes; notes outside range were dropped.");
        } else {
            result.warnings.push_back("Key mode exceeds chart lanes; keeping original lanes.");
        }
    }

    if (settings.random == RandomMode::FullRandom) {
        apply_full_random(result.chart, settings.random_seed);
    } else if (settings.random == RandomMode::SuperRandom) {
        apply_super_random(result.chart, settings.random_seed, result.warnings);
    }

    return result;
}

}  // namespace tenriff::gameplay
