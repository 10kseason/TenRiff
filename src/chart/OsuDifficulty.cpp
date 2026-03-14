#include "chart/OsuDifficulty.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace tenriff::chart {

namespace {

constexpr double kTimeWindowMs = 1000.0;
constexpr double kTimeWindowSeconds = kTimeWindowMs / 1000.0;
constexpr double kLdbWeight = 1.0;
constexpr double kDistanceWeight = 1.0;
constexpr double kScoreWeight = 0.1;
constexpr double kAccWeight = 0.9;
constexpr double kRatingWeight = 0.050147842383428124;
constexpr double kRatingPower = 0.35163702826872596;
constexpr double kAverageRatingPower = 1.0;
constexpr double kAverageLevelPower = 1.0;
constexpr double kReviveScoreWeight = 0.1;
constexpr double kReviveAccWeight = 0.9;
constexpr double kReviveLevelScale = 0.052379807720288355;
constexpr double kReviveLevelPower = 0.6151479234411744;
constexpr double kReviveMaxLevel = 24.0;

constexpr std::array<std::array<double, 10>, 10> kWeightedNpsMatrix = {{
    {{1.2, 1.2, 0.6, 0.6, 0.6, 0.12, 0.12, 0.12, 0.12, 0.12}},
    {{0.825, 1.1, 0.825, 0.55, 0.55, 0.11, 0.11, 0.11, 0.11, 0.11}},
    {{0.5, 0.75, 1.0, 0.75, 0.5, 0.1, 0.1, 0.1, 0.1, 0.1}},
    {{0.5, 0.5, 0.75, 1.0, 0.75, 0.1, 0.1, 0.1, 0.1, 0.1}},
    {{0.55, 0.55, 0.55, 0.825, 1.1, 0.385, 0.11, 0.11, 0.11, 0.11}},
    {{0.11, 0.11, 0.11, 0.11, 0.385, 1.1, 0.825, 0.55, 0.55, 0.55}},
    {{0.1, 0.1, 0.1, 0.1, 0.1, 0.75, 1.0, 0.75, 0.5, 0.5}},
    {{0.1, 0.1, 0.1, 0.1, 0.1, 0.5, 0.75, 1.0, 0.75, 0.5}},
    {{0.11, 0.11, 0.11, 0.11, 0.11, 0.55, 0.55, 0.825, 1.1, 0.825}},
    {{0.12, 0.12, 0.12, 0.12, 0.12, 0.6, 0.6, 0.6, 1.2, 1.2}},
}};

constexpr std::array<std::array<double, 10>, 10> kVisualDistanceMatrix = {{
    {{1.0, 1.0, 2.0, 3.0, 2.5, 1.5, 2.5, 3.0, 2.25, 1.25}},
    {{1.0, 1.0, 1.0, 2.0, 3.0, 2.5, 1.5, 2.25, 1.25, 2.25}},
    {{2.0, 1.0, 1.0, 1.0, 2.0, 3.0, 2.25, 1.25, 2.25, 3.0}},
    {{3.0, 2.0, 1.0, 1.0, 1.0, 2.0, 1.25, 2.25, 1.5, 2.5}},
    {{2.5, 3.0, 2.0, 1.0, 1.0, 1.0, 2.0, 3.0, 2.5, 1.5}},
    {{1.5, 2.5, 3.0, 2.0, 1.0, 1.0, 1.0, 2.0, 3.0, 2.5}},
    {{2.5, 1.5, 2.25, 1.25, 2.0, 1.0, 1.0, 1.0, 2.0, 3.0}},
    {{3.0, 2.25, 1.25, 2.25, 3.0, 2.0, 1.0, 1.0, 1.0, 2.0}},
    {{2.25, 1.25, 2.25, 1.5, 2.5, 3.0, 2.0, 1.0, 1.0, 1.0}},
    {{1.25, 2.25, 3.0, 2.5, 1.5, 2.5, 3.0, 2.0, 1.0, 1.0}},
}};

constexpr std::array<std::array<double, 3>, 3> kTypeDistanceMatrix = {{
    {{0.0, 1.0, 0.0}},
    {{1.0, 0.0, 0.0}},
    {{0.0, 0.0, 0.0}},
}};

constexpr std::array<double, 4> kJackBaseIntervalsMs = {{75.0, 100.0, 125.0, 150.0}};
constexpr std::array<double, 4> kJackLnIntervalsMs = {{37.5, 50.0, 62.5, 75.0}};
constexpr std::array<double, 4> kJackCoefficients = {{1.0, 0.5, 0.4, 0.1}};

enum class DifficultyNoteType {
    Rice,
    HoldStart,
    HoldEnd,
};

struct DifficultyNote {
    double time_seconds = 0.0;
    int column = 1;
    DifficultyNoteType type = DifficultyNoteType::Rice;
};

struct HoldInterval {
    double start_seconds = 0.0;
    double end_seconds = 0.0;
};

struct JudgmentWindow {
    double plus_ms = 0.0;
    double minus_ms = 0.0;
    double score_pct = 0.0;
    double acc_pct = 0.0;
};

int note_type_order(DifficultyNoteType type) {
    switch (type) {
    case DifficultyNoteType::HoldEnd:
        return 0;
    case DifficultyNoteType::Rice:
        return 1;
    case DifficultyNoteType::HoldStart:
        return 2;
    }
    return 1;
}

int note_type_index(DifficultyNoteType type) {
    switch (type) {
    case DifficultyNoteType::Rice:
        return 0;
    case DifficultyNoteType::HoldStart:
        return 1;
    case DifficultyNoteType::HoldEnd:
        return 2;
    }
    return 0;
}

bool is_hold_boundary(DifficultyNoteType type) {
    return type == DifficultyNoteType::HoldStart || type == DifficultyNoteType::HoldEnd;
}

std::array<JudgmentWindow, 6> build_osu_stable_windows(double od, DifficultyNoteType type) {
    od = std::clamp(od, 0.0, 10.0);
    const double rice_300g = 16.5;
    const double rice_300 = 64.5 - (3.0 * od);
    const double rice_200 = 97.5 - (3.0 * od);
    const double rice_100 = 127.5 - (3.0 * od);
    const double rice_50 = 151.5 - (3.0 * od);
    const double rice_miss = 188.5 - (3.0 * od);

    double early_factor = 1.0;
    if (type == DifficultyNoteType::HoldStart) {
        early_factor = 0.9;
    }

    return {{
        JudgmentWindow{rice_300g * early_factor, rice_300g * early_factor, 100.0, 100.0},
        JudgmentWindow{rice_300 * early_factor, rice_300 * early_factor, 96.875, 100.0},
        JudgmentWindow{rice_200 * early_factor, rice_200 * early_factor, 31.25, 66.666666667},
        JudgmentWindow{rice_100 * early_factor, rice_100 * early_factor, 15.625, 33.333333334},
        JudgmentWindow{rice_50, rice_50, 7.8125, 16.666666667},
        JudgmentWindow{rice_miss, rice_miss, 0.0, 0.0},
    }};
}

JudgmentWindow judgment_for_offset(double offset_ms, DifficultyNoteType type, double od, bool full_range) {
    const auto windows = build_osu_stable_windows(od, type);
    for (const auto& window : windows) {
        const double range = full_range ? (std::abs(window.plus_ms) + std::abs(window.minus_ms))
                                        : std::abs(window.plus_ms);
        if (offset_ms <= range) {
            return window;
        }
    }
    return windows.back();
}

void judgment_result_typed(double offset_ms, DifficultyNoteType type, double od, double& score_pct, double& acc_pct) {
    const auto positive = judgment_for_offset(offset_ms, type, od, false);
    const auto full = judgment_for_offset(offset_ms, type, od, true);
    score_pct = (positive.score_pct + full.score_pct) * 0.5;
    acc_pct = (positive.acc_pct + full.acc_pct) * 0.5;
}

void judgment_result_direct(double offset_ms, DifficultyNoteType type, double od, double& score_pct, double& acc_pct) {
    const auto result = judgment_for_offset(std::abs(offset_ms), type, od, false);
    score_pct = result.score_pct;
    acc_pct = result.acc_pct;
}

double judge_average_ratio(DifficultyNoteType type, double od, bool use_score) {
    double numer = 0.0;
    double denom = 0.0;
    for (int i = 0; i < static_cast<int>(kTimeWindowMs); ++i) {
        const double weight = 1.0 / static_cast<double>(i + 1);
        for (const double offset : {static_cast<double>(i), -static_cast<double>(i)}) {
            double score = 100.0;
            double acc = 100.0;
            judgment_result_direct(offset, type, od, score, acc);
            numer += (use_score ? score : acc) * weight;
            denom += weight;
        }
    }
    if (denom <= 0.0) {
        return 100.0;
    }
    return numer / denom;
}

double clamp_positive(double value, double minimum) {
    if (!std::isfinite(value)) {
        return minimum;
    }
    return std::max(value, minimum);
}

double normalized_lane_distance(int key_count, std::size_t column_i, std::size_t column_j) {
    const int safe_key_count = std::max(1, key_count);
    if (safe_key_count <= 1) {
        return 0.0;
    }
    const double distance = static_cast<double>(
        std::abs(static_cast<int>(column_i) - static_cast<int>(column_j)));
    return distance / static_cast<double>(safe_key_count - 1);
}

double weighted_nps_weight(int key_count, std::size_t column_i, std::size_t column_j) {
    if (key_count == 10 && column_i < 10 && column_j < 10) {
        return kWeightedNpsMatrix[column_i][column_j];
    }

    const double normalized = normalized_lane_distance(key_count, column_i, column_j);
    const double weight =
        1.20 - (1.02 * normalized) + ((column_i == column_j) ? 0.0 : (0.08 * (1.0 - normalized)));
    return std::clamp(weight, 0.12, 1.20);
}

double visual_distance_weight(int key_count, std::size_t column_i, std::size_t column_j) {
    if (key_count == 10 && column_i < 10 && column_j < 10) {
        return kVisualDistanceMatrix[column_i][column_j];
    }

    const double normalized = normalized_lane_distance(key_count, column_i, column_j);
    const double center = static_cast<double>(std::max(0, key_count - 1)) * 0.5;
    const double side_i = static_cast<double>(column_i) - center;
    const double side_j = static_cast<double>(column_j) - center;
    const bool crosses_center = (side_i < 0.0 && side_j > 0.0) || (side_i > 0.0 && side_j < 0.0);
    const double center_penalty = crosses_center ? 0.35 : 0.0;
    return std::clamp(1.0 + (2.0 * normalized) + center_penalty, 1.0, 3.0);
}

}  // namespace

OsuDifficultyMetrics calculate_osu_mania_difficulty(const OsuManiaChart& chart) {
    OsuDifficultyMetrics metrics;
    const int key_count = std::clamp(chart.key_count, 1, 10);
    if (key_count < 4 || key_count > 10) {
        return metrics;
    }

    std::vector<DifficultyNote> notes;
    notes.reserve(chart.notes.size() * 2);
    std::vector<std::vector<HoldInterval>> holds_by_column(static_cast<std::size_t>(key_count));
    for (const auto& note : chart.notes) {
        const int column = std::clamp(note.column, 0, key_count - 1) + 1;
        if (note.end_time_ms.has_value() && note.end_time_ms.value() > note.start_time_ms) {
            const double start_seconds = static_cast<double>(note.start_time_ms) / 1000.0;
            const double end_seconds = static_cast<double>(note.end_time_ms.value()) / 1000.0;
            notes.push_back(DifficultyNote{start_seconds, column, DifficultyNoteType::HoldStart});
            notes.push_back(DifficultyNote{end_seconds, column, DifficultyNoteType::HoldEnd});
            holds_by_column[static_cast<std::size_t>(column - 1)].push_back(HoldInterval{start_seconds, end_seconds});
        } else {
            notes.push_back(
                DifficultyNote{static_cast<double>(note.start_time_ms) / 1000.0, column, DifficultyNoteType::Rice});
        }
    }

    if (notes.empty()) {
        return metrics;
    }

    std::sort(notes.begin(), notes.end(), [](const DifficultyNote& lhs, const DifficultyNote& rhs) {
        if (lhs.time_seconds != rhs.time_seconds) {
            return lhs.time_seconds < rhs.time_seconds;
        }
        const int lhs_order = note_type_order(lhs.type);
        const int rhs_order = note_type_order(rhs.type);
        if (lhs_order != rhs_order) {
            return lhs_order < rhs_order;
        }
        return lhs.column < rhs.column;
    });

    for (auto& holds : holds_by_column) {
        std::sort(holds.begin(), holds.end(), [](const HoldInterval& lhs, const HoldInterval& rhs) {
            return lhs.start_seconds < rhs.start_seconds;
        });
    }

    const double duration_seconds =
        std::max(notes.back().time_seconds - notes.front().time_seconds, 1.0);
    const std::size_t note_count = notes.size();

    std::vector<std::vector<std::size_t>> line_note_indices(static_cast<std::size_t>(key_count));
    std::vector<std::size_t> timing_order(note_count, 0);
    for (std::size_t i = 0; i < note_count; ++i) {
        const std::size_t column_index = static_cast<std::size_t>(notes[i].column - 1);
        timing_order[i] = line_note_indices[column_index].size();
        line_note_indices[column_index].push_back(i);
    }

    std::vector<double> hold_end_weights(note_count, 1.0);
    for (std::size_t i = 0; i < note_count; ++i) {
        if (notes[i].type != DifficultyNoteType::HoldEnd) {
            continue;
        }
        const std::size_t column_index = static_cast<std::size_t>(notes[i].column - 1);
        const std::size_t order = timing_order[i];
        if (order == 0) {
            continue;
        }
        const std::size_t prev_index = line_note_indices[column_index][order - 1];
        const double hold_length_seconds = notes[i].time_seconds - notes[prev_index].time_seconds;
        const double raw_weight = hold_length_seconds / kTimeWindowSeconds;
        hold_end_weights[i] = std::clamp(raw_weight, 0.05, 1.0);
    }

    std::vector<double> nps_v2(note_count, 0.0);
    std::vector<double> same_line_nps(note_count, 0.0);
    std::vector<double> distance_difficulty(note_count, 1.0);
    std::vector<double> minimum_distance_sum(note_count, 0.0);
    std::vector<double> same_line_distance_sum(note_count, 0.0);
    std::vector<int> local_nps(note_count, 0);
    std::vector<std::size_t> window_start(note_count, 0);
    std::vector<std::size_t> window_finish(note_count, 0);

    std::size_t start_index = 0;
    std::size_t finish_index = 0;
    double previous_time = std::numeric_limits<double>::quiet_NaN();

    for (std::size_t i = 0; i < note_count; ++i) {
        const double time_seconds = notes[i].time_seconds;
        const std::size_t column_i = static_cast<std::size_t>(notes[i].column - 1);
        if (!std::isfinite(previous_time) || time_seconds != previous_time) {
            while (start_index < finish_index &&
                   notes[start_index].time_seconds <= (time_seconds - kTimeWindowSeconds)) {
                ++start_index;
            }
            while (finish_index < note_count &&
                   notes[finish_index].time_seconds < (time_seconds + kTimeWindowSeconds)) {
                ++finish_index;
            }
            previous_time = time_seconds;
        }

        double weighted_count = 0.0;
        double same_column_weighted_count = 0.0;
        double minimum_plus_delta_ms = kTimeWindowMs;
        double minimum_minus_delta_ms = kTimeWindowMs;
        int nps_count = 0;

        for (std::size_t j = start_index; j < finish_index; ++j) {
            const double dt_ms = (time_seconds - notes[j].time_seconds) * 1000.0;
            const double abs_dt_ms = std::abs(dt_ms);
            if (dt_ms > -500.0 && dt_ms <= 500.0) {
                ++nps_count;
            }
            if (abs_dt_ms >= kTimeWindowMs) {
                continue;
            }

            const double time_weight = 1.0 - (abs_dt_ms / kTimeWindowMs);
            const std::size_t column_j = static_cast<std::size_t>(notes[j].column - 1);
            const double column_weight = weighted_nps_weight(key_count, column_i, column_j);
            const double tail_weight =
                (j != i && notes[j].type == DifficultyNoteType::HoldEnd) ? hold_end_weights[j] : 1.0;
            const double weighted = column_weight * tail_weight * time_weight;
            weighted_count += weighted;
            if (column_i == column_j) {
                same_column_weighted_count += weighted;
            }

            if (dt_ms > 0.0) {
                minimum_plus_delta_ms = std::min(minimum_plus_delta_ms, dt_ms);
            } else if (dt_ms < 0.0) {
                minimum_minus_delta_ms = std::min(minimum_minus_delta_ms, abs_dt_ms);
            }
        }

        const double minimum_delta_ms = std::max(std::min(minimum_plus_delta_ms, minimum_minus_delta_ms), 10.0);
        double minimum_distance = 0.0;
        for (std::size_t j = start_index; j < finish_index; ++j) {
            if (j == i) {
                continue;
            }
            const double dt_abs_ms = std::abs((time_seconds - notes[j].time_seconds) * 1000.0);
            const std::size_t column_j = static_cast<std::size_t>(notes[j].column - 1);
            const double visual_distance = visual_distance_weight(key_count, column_i, column_j);
            const double type_distance =
                kTypeDistanceMatrix[static_cast<std::size_t>(note_type_index(notes[i].type))]
                                   [static_cast<std::size_t>(note_type_index(notes[j].type))];
            const double timing_distance =
                std::min(dt_abs_ms / minimum_delta_ms, 3.0) + (dt_abs_ms / kTimeWindowMs);
            const double total_distance = visual_distance + type_distance + timing_distance;
            if (minimum_distance == 0.0 || total_distance < minimum_distance) {
                minimum_distance = total_distance;
            }
        }

        nps_v2[i] = weighted_count;
        same_line_nps[i] = same_column_weighted_count;
        distance_difficulty[i] = std::max(minimum_distance, 1.0);
        local_nps[i] = nps_count;
        window_start[i] = start_index;
        window_finish[i] = finish_index;
    }

    for (std::size_t i = 0; i < note_count; ++i) {
        const double time_seconds = notes[i].time_seconds;
        const int column = notes[i].column;
        double weighted_distance_sum = 0.0;
        double same_column_distance_sum = 0.0;

        for (std::size_t j = window_start[i]; j < window_finish[i]; ++j) {
            const double abs_dt_ms = std::abs((time_seconds - notes[j].time_seconds) * 1000.0);
            const double time_weight = 1.0 - (abs_dt_ms / kTimeWindowMs);
            const double tail_weight =
                (j != i && notes[j].type == DifficultyNoteType::HoldEnd) ? hold_end_weights[j] : 1.0;
            const double weighted = distance_difficulty[j] * tail_weight * time_weight;
            weighted_distance_sum += weighted;
            if (column == notes[j].column) {
                same_column_distance_sum += weighted;
            }
        }

        minimum_distance_sum[i] = weighted_distance_sum;
        same_line_distance_sum[i] = same_column_distance_sum;
    }

    std::vector<double> ldb_values(note_count, 0.0);
    std::vector<double> ldbd_values(note_count, 0.0);
    for (std::size_t i = 0; i < note_count; ++i) {
        const double time_seconds = notes[i].time_seconds;
        const std::size_t column_i = static_cast<std::size_t>(notes[i].column - 1);
        double ldb = 0.0;
        double ldbd = 0.0;
        for (std::size_t column_j = 0; column_j < holds_by_column.size(); ++column_j) {
            if (column_i == column_j) {
                continue;
            }
            const auto& holds = holds_by_column[column_j];
            if (holds.empty()) {
                continue;
            }

            const auto it = std::upper_bound(
                holds.begin(), holds.end(), time_seconds,
                [](double value, const HoldInterval& interval) { return value < interval.start_seconds; });
            if (it == holds.begin()) {
                continue;
            }
            const auto& candidate = *std::prev(it);
            if (candidate.start_seconds >= time_seconds || candidate.end_seconds <= time_seconds) {
                continue;
            }

            const double overlap_distance =
                std::min({time_seconds - candidate.start_seconds, candidate.end_seconds - time_seconds, 1.0});
            if (overlap_distance <= 0.0) {
                continue;
            }
            ldb += weighted_nps_weight(key_count, column_i, column_j) * overlap_distance;
            ldbd += overlap_distance;
        }
        ldb_values[i] = ldb;
        ldbd_values[i] = ldbd;
    }

    struct ColumnJackState {
        double last_time_ms = -1.0;
        DifficultyNoteType last_type = DifficultyNoteType::Rice;
        std::array<double, 4> accumulated{{0.0, 0.0, 0.0, 0.0}};
    };

    std::vector<ColumnJackState> jack_state(static_cast<std::size_t>(key_count));
    std::vector<double> note_jack_score(note_count, 0.0);
    std::vector<double> note_jack_acc(note_count, 0.0);
    const double overall_difficulty = chart.overall_difficulty > 0.0 ? chart.overall_difficulty : 8.0;

    for (std::size_t i = 0; i < note_count; ++i) {
        const std::size_t column_index = static_cast<std::size_t>(notes[i].column - 1);
        auto& state = jack_state[column_index];
        const double current_time_ms = notes[i].time_seconds * 1000.0;
        if (state.last_time_ms >= 0.0) {
            const double actual_interval_ms = current_time_ms - state.last_time_ms;
            const auto& intervals = is_hold_boundary(state.last_type) ? kJackLnIntervalsMs : kJackBaseIntervalsMs;
            for (std::size_t j = 0; j < state.accumulated.size(); ++j) {
                if (actual_interval_ms < intervals[j]) {
                    state.accumulated[j] += intervals[j] - actual_interval_ms;
                } else {
                    state.accumulated[j] = std::max(0.0, state.accumulated[j] - (actual_interval_ms - intervals[j]));
                }
            }
        }

        double jack_score_loss = 0.0;
        double jack_acc_loss = 0.0;
        for (std::size_t j = 0; j < state.accumulated.size(); ++j) {
            if (state.accumulated[j] <= 0.0) {
                continue;
            }
            double score = 100.0;
            double acc = 100.0;
            judgment_result_typed(state.accumulated[j], notes[i].type, overall_difficulty, score, acc);
            jack_score_loss += kJackCoefficients[j] * (100.0 - score);
            jack_acc_loss += kJackCoefficients[j] * (100.0 - acc);
        }
        note_jack_score[i] = jack_score_loss;
        note_jack_acc[i] = jack_acc_loss;
        state.last_time_ms = current_time_ms;
        state.last_type = notes[i].type;
    }

    const double judge_score_rice = judge_average_ratio(DifficultyNoteType::Rice, overall_difficulty, true);
    const double judge_acc_rice = judge_average_ratio(DifficultyNoteType::Rice, overall_difficulty, false);
    const double judge_score_head = judge_average_ratio(DifficultyNoteType::HoldStart, overall_difficulty, true);
    const double judge_acc_head = judge_average_ratio(DifficultyNoteType::HoldStart, overall_difficulty, false);
    const double judge_score_tail = judge_average_ratio(DifficultyNoteType::HoldEnd, overall_difficulty, true);
    const double judge_acc_tail = judge_average_ratio(DifficultyNoteType::HoldEnd, overall_difficulty, false);

    auto judge_values_for_note = [&](DifficultyNoteType type, bool use_score) {
        switch (type) {
        case DifficultyNoteType::HoldStart:
            return use_score ? judge_score_head : judge_acc_head;
        case DifficultyNoteType::HoldEnd:
            return use_score ? judge_score_tail : judge_acc_tail;
        case DifficultyNoteType::Rice:
            return use_score ? judge_score_rice : judge_acc_rice;
        }
        return 100.0;
    };

    double raw_score_l1_sum = 0.0;
    double raw_acc_l1_sum = 0.0;
    double raw_score_l5_sum = 0.0;
    double raw_acc_l5_sum = 0.0;
    int peak_nps = 0;

    for (std::size_t i = 0; i < note_count; ++i) {
        peak_nps = std::max(peak_nps, local_nps[i]);
        const double same_line_distance = same_line_distance_sum[i];
        const double other_line_distance = std::max(0.0, minimum_distance_sum[i] - same_line_distance + (kLdbWeight * ldbd_values[i]));
        const double same_line_nps_value = same_line_nps[i];
        const double other_line_nps_value = std::max(0.0, nps_v2[i] - same_line_nps[i] + (kLdbWeight * ldb_values[i]));

        const double combined_distance = std::max(1.0, same_line_distance + other_line_distance);
        const double base_score =
            std::pow((same_line_nps_value + other_line_nps_value) * std::pow(combined_distance, kDistanceWeight),
                     1.0 / (1.0 + kDistanceWeight)) *
            (1.0 + (note_jack_score[i] / 100.0));
        const double base_acc =
            std::pow((same_line_nps_value + other_line_nps_value) * std::pow(combined_distance, kDistanceWeight),
                     1.0 / (1.0 + kDistanceWeight)) *
            (1.0 + (note_jack_acc[i] / 100.0));

        const double judge_score_mult = 100.0 / clamp_positive(judge_values_for_note(notes[i].type, true), 0.01);
        const double judge_acc_mult = 100.0 / clamp_positive(judge_values_for_note(notes[i].type, false), 0.01);

        const double score_diff = base_score * judge_score_mult;
        const double acc_diff = base_acc * judge_acc_mult;

        raw_score_l1_sum += score_diff;
        raw_acc_l1_sum += acc_diff;
        raw_score_l5_sum += std::pow(score_diff, 5.0);
        raw_acc_l5_sum += std::pow(acc_diff, 5.0);
    }

    const double score_diff_l5_sum = raw_score_l5_sum > 0.0 ? std::pow(raw_score_l5_sum, 0.2) : 0.0;
    const double acc_diff_l5_sum = raw_acc_l5_sum > 0.0 ? std::pow(raw_acc_l5_sum, 0.2) : 0.0;
    const double score_diff_l5_avg = raw_score_l1_sum / static_cast<double>(note_count);
    const double acc_diff_l5_avg = raw_acc_l1_sum / static_cast<double>(note_count);

    const double score_avg_term =
        score_diff_l5_avg > 0.0 ? std::pow(score_diff_l5_avg, kAverageRatingPower) : 0.0;
    const double acc_avg_term =
        acc_diff_l5_avg > 0.0 ? std::pow(acc_diff_l5_avg, kAverageRatingPower) : 0.0;
    const double score_avg_lv_term =
        score_diff_l5_avg > 0.0 ? std::pow(score_diff_l5_avg, kAverageLevelPower) : 0.0;
    const double acc_avg_lv_term =
        acc_diff_l5_avg > 0.0 ? std::pow(acc_diff_l5_avg, kAverageLevelPower) : 0.0;

    const double rating_base =
        (kScoreWeight * score_diff_l5_sum * score_avg_term) + (kAccWeight * acc_diff_l5_sum * acc_avg_term);
    const double circus_rating =
        (rating_base > 0.0 && kRatingWeight > 0.0) ? std::pow(kRatingWeight * rating_base, kRatingPower) : 0.0;

    const double revive_difficulty =
        (kReviveScoreWeight * score_diff_l5_sum * score_avg_lv_term) +
        (kReviveAccWeight * acc_diff_l5_sum * acc_avg_lv_term);

    int revive_level = 0;
    if (revive_difficulty > 0.0) {
        const double revive_base = kReviveLevelScale * revive_difficulty;
        const double revive_powered =
            revive_base > 0.0 ? std::pow(revive_base, kReviveLevelPower) : 0.0;
        revive_level = static_cast<int>(std::ceil(
            kReviveMaxLevel - ((kReviveMaxLevel * kReviveMaxLevel) / (revive_powered + kReviveMaxLevel))));
    }

    metrics.circus_rating = circus_rating;
    metrics.revive_level = std::max(0, revive_level);
    metrics.peak_nps = static_cast<double>(peak_nps);
    metrics.average_nps = static_cast<double>(note_count) / duration_seconds;
    metrics.note_count = static_cast<int>(note_count);
    return metrics;
}

OsuDifficultyMetrics calculate_osu_10k_difficulty(const OsuManiaChart& chart) {
    return calculate_osu_mania_difficulty(chart);
}

}  // namespace tenriff::chart
