// Adapted C++ port of the N2NC keymode-conversion logic from
// krrcream's Toolkit: https://github.com/krrcream/krrcream-Toolkit
// Original reference path: Tools/N2NC/N2NC.cs
// See THIRD_PARTY_NOTICES.md for attribution and license details.
#include "gameplay/KeyModeConverter.h"
#include "gameplay/Nk2KeyModeAdapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tenriff::gameplay {

namespace {

constexpr int kMatrixEmpty = -1;
constexpr int kMatrixHoldBody = -7;
constexpr double kTimingFudgeMs = 10.0;
constexpr std::array<double, 9> kTransformSpeedValues = {0.125, 0.25, 0.5, 0.75, 1.0, 2.0, 3.0, 4.0, 999.0};

class IntMatrix {
public:
    IntMatrix() = default;

    IntMatrix(int rows, int cols)
        : rows_(std::max(0, rows)),
          cols_(std::max(0, cols)),
          data_(static_cast<std::size_t>(std::max(0, rows) * std::max(0, cols)), kMatrixEmpty) {}

    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] int cols() const { return cols_; }

    [[nodiscard]] int& at(int row, int col) {
        return data_[static_cast<std::size_t>(row * cols_ + col)];
    }

    [[nodiscard]] int at(int row, int col) const {
        return data_[static_cast<std::size_t>(row * cols_ + col)];
    }

private:
    int rows_ = 0;
    int cols_ = 0;
    std::vector<int> data_;
};

class BoolMatrix {
public:
    BoolMatrix() = default;

    BoolMatrix(int rows, int cols)
        : rows_(std::max(0, rows)),
          cols_(std::max(0, cols)),
          data_(static_cast<std::size_t>(std::max(0, rows) * std::max(0, cols)), 0) {}

    [[nodiscard]] bool at(int row, int col) const {
        return data_[static_cast<std::size_t>(row * cols_ + col)] != 0;
    }

    void set(int row, int col, bool value) {
        data_[static_cast<std::size_t>(row * cols_ + col)] = value ? 1 : 0;
    }

private:
    int rows_ = 0;
    int cols_ = 0;
    std::vector<uint8_t> data_;
};

class RandomSource {
public:
    explicit RandomSource(uint32_t seed)
        : engine_(seed) {}

    [[nodiscard]] int next_int(int min_inclusive, int max_exclusive) {
        if (max_exclusive <= min_inclusive) {
            return min_inclusive;
        }
        std::uniform_int_distribution<int> dist(min_inclusive, max_exclusive - 1);
        return dist(engine_);
    }

    [[nodiscard]] std::size_t next_index(std::size_t max_exclusive) {
        if (max_exclusive == 0) {
            return 0;
        }
        std::uniform_int_distribution<std::size_t> dist(0, max_exclusive - 1);
        return dist(engine_);
    }

    [[nodiscard]] double next_double() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(engine_);
    }

private:
    std::mt19937 engine_;
};

class OscillatorGenerator {
public:
    OscillatorGenerator(int max_value, RandomSource& random)
        : max_value_(max_value) {
        if (max_value_ < 0) {
            throw std::invalid_argument("max_value must be non-negative");
        }

        if (max_value_ == 0) {
            current_value_ = 0;
            special_case_ = true;
            return;
        }

        if (max_value_ == 1) {
            current_value_ = random.next_int(0, 2);
            direction_ = (random.next_int(0, 2) == 0) ? -1 : 1;
            special_case_ = true;
            return;
        }

        current_value_ = random.next_int(1, max_value_);
        direction_ = (random.next_int(0, 2) == 0) ? -1 : 1;
    }

    [[nodiscard]] int current() const { return current_value_; }

    void next() {
        if (special_case_) {
            if (max_value_ == 1) {
                current_value_ = 1 - current_value_;
            }
            return;
        }

        current_value_ += direction_;
        if (current_value_ > max_value_) {
            current_value_ = max_value_ - 1;
            direction_ = -1;
        } else if (current_value_ < 0) {
            current_value_ = 1;
            direction_ = 1;
        }
    }

private:
    int max_value_ = 0;
    int current_value_ = 0;
    int direction_ = 1;
    bool special_case_ = false;
};

struct ConvertMatrices {
    IntMatrix old_matrix;
    IntMatrix insert_matrix;
};

[[nodiscard]] int resolve_lane_count(const GameplayChart& chart) {
    int lane_count = chart.lane_count;
    for (const auto& note : chart.notes) {
        lane_count = std::max(lane_count, note.lane);
    }
    return std::max(0, lane_count);
}

[[nodiscard]] int resolve_sample_rate(int sample_rate) {
    return sample_rate > 0 ? sample_rate : 44100;
}

[[nodiscard]] double resolve_base_bpm(double bpm) {
    return std::isfinite(bpm) && bpm > 0.0 ? bpm : 180.0;
}

[[nodiscard]] int64_t samples_from_ms(double value_ms, int sample_rate) {
    return std::max<int64_t>(0, static_cast<int64_t>(std::llround(value_ms * static_cast<double>(sample_rate) / 1000.0)));
}

[[nodiscard]] std::vector<int64_t> build_time_axis(const GameplayChart& chart) {
    std::vector<int64_t> axis;
    axis.reserve(chart.notes.size() * 2);
    for (const auto& note : chart.notes) {
        axis.push_back(note.start_sample);
        if (note.end_sample.has_value() && *note.end_sample > note.start_sample) {
            axis.push_back(*note.end_sample);
        }
    }
    std::sort(axis.begin(), axis.end());
    axis.erase(std::unique(axis.begin(), axis.end()), axis.end());
    return axis;
}

[[nodiscard]] std::vector<double> build_beat_length_axis(std::size_t rows, double beat_length_samples) {
    return std::vector<double>(rows, std::max(1.0, beat_length_samples));
}

[[nodiscard]] std::vector<int64_t> build_end_time_axis(const GameplayChart& chart) {
    std::vector<int64_t> axis(chart.notes.size(), 0);
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        axis[i] = chart.notes[i].end_sample.value_or(chart.notes[i].start_sample);
    }
    return axis;
}

[[nodiscard]] std::vector<uint8_t> build_hold_head_axis(const GameplayChart& chart) {
    std::vector<uint8_t> axis(chart.notes.size(), 0);
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const auto& note = chart.notes[i];
        axis[i] = (note.end_sample.has_value() && *note.end_sample > note.start_sample) ? 1u : 0u;
    }
    return axis;
}

[[nodiscard]] IntMatrix build_note_matrix(const GameplayChart& chart,
                                          int lane_count,
                                          const std::unordered_map<int64_t, int>& row_by_time) {
    IntMatrix matrix(static_cast<int>(row_by_time.size()), lane_count);
    for (std::size_t note_index = 0; note_index < chart.notes.size(); ++note_index) {
        const auto& note = chart.notes[note_index];
        if (note.lane <= 0 || note.lane > lane_count) {
            continue;
        }

        const auto start_it = row_by_time.find(note.start_sample);
        if (start_it == row_by_time.end()) {
            continue;
        }

        const int row = start_it->second;
        const int col = note.lane - 1;
        matrix.at(row, col) = static_cast<int>(note_index);

        if (!note.end_sample.has_value() || *note.end_sample <= note.start_sample) {
            continue;
        }

        const auto end_it = row_by_time.find(*note.end_sample);
        if (end_it == row_by_time.end()) {
            continue;
        }

        for (int body_row = row + 1; body_row <= end_it->second; ++body_row) {
            matrix.at(body_row, col) = kMatrixHoldBody;
        }
    }
    return matrix;
}

[[nodiscard]] std::vector<int> build_original_column_axis(const IntMatrix& matrix, std::size_t note_count) {
    std::vector<int> axis(note_count, -1);
    for (int row = 0; row < matrix.rows(); ++row) {
        for (int col = 0; col < matrix.cols(); ++col) {
            const int note_index = matrix.at(row, col);
            if (note_index >= 0 && note_index < static_cast<int>(axis.size()) && axis[static_cast<std::size_t>(note_index)] < 0) {
                axis[static_cast<std::size_t>(note_index)] = col;
            }
        }
    }

    for (auto& value : axis) {
        if (value < 0) {
            value = 0;
        }
    }
    return axis;
}

void shift_insert(std::vector<int>& row, int insert_index) {
    for (int i = static_cast<int>(row.size()) - 1; i > insert_index; --i) {
        row[static_cast<std::size_t>(i)] = row[static_cast<std::size_t>(i - 1)];
    }
    row[static_cast<std::size_t>(insert_index)] = kMatrixEmpty;
}

[[nodiscard]] ConvertMatrices build_convert_matrices(int turn,
                                                     const std::vector<int64_t>& time_axis,
                                                     int64_t convert_time,
                                                     int source_cols,
                                                     bool max_keys_equal_source,
                                                     RandomSource& random) {
    ConvertMatrices result;
    result.old_matrix = IntMatrix(static_cast<int>(time_axis.size()), turn);
    result.insert_matrix = IntMatrix(static_cast<int>(time_axis.size()), turn);

    if (time_axis.empty() || turn <= 0) {
        return result;
    }

    if (!max_keys_equal_source) {
        for (int col = 0; col < turn; ++col) {
            OscillatorGenerator old_index(source_cols - 1, random);
            int64_t time_counter = 0;
            int64_t last_time = time_axis.front();

            for (int row = 0; row < static_cast<int>(time_axis.size()); ++row) {
                result.old_matrix.at(row, col) = old_index.current();
                time_counter += time_axis[static_cast<std::size_t>(row)] - last_time;
                last_time = time_axis[static_cast<std::size_t>(row)];

                if (time_counter >= convert_time) {
                    old_index.next();
                    time_counter = 0;
                }
            }

            const int random_moves = random.next_int(0, source_cols - 1);
            for (int i = 0; i < random_moves; ++i) {
                old_index.next();
            }
        }
    }

    for (int col = 0; col < turn; ++col) {
        OscillatorGenerator insert_index(source_cols + col, random);
        int64_t time_counter = 0;
        int64_t last_time = time_axis.front();

        for (int row = 0; row < static_cast<int>(time_axis.size()); ++row) {
            result.insert_matrix.at(row, col) = insert_index.current();
            time_counter += time_axis[static_cast<std::size_t>(row)] - last_time;
            last_time = time_axis[static_cast<std::size_t>(row)];

            if (time_counter >= convert_time) {
                insert_index.next();
                time_counter = 0;
            }
        }

        const int random_moves = random.next_int(0, source_cols - 1 + col);
        for (int i = 0; i < random_moves; ++i) {
            insert_index.next();
        }
    }

    return result;
}

[[nodiscard]] IntMatrix perform_initial_convert(const IntMatrix& source,
                                                const IntMatrix& old_matrix,
                                                const IntMatrix& insert_matrix,
                                                int target_lane_count,
                                                bool max_keys_equal_source) {
    const int rows = source.rows();
    const int original_cols = source.cols();
    const int turn = insert_matrix.cols();

    IntMatrix converted(rows, target_lane_count);
    for (int row = 0; row < rows; ++row) {
        std::vector<int> temp(static_cast<std::size_t>(target_lane_count), kMatrixEmpty);
        for (int col = 0; col < original_cols && col < target_lane_count; ++col) {
            temp[static_cast<std::size_t>(col)] = source.at(row, col);
        }

        if (!max_keys_equal_source) {
            for (int col = 0; col < turn; ++col) {
                const int insert_at = std::clamp(insert_matrix.at(row, col), 0, target_lane_count - 1);
                shift_insert(temp, insert_at);

                const int copy_col = std::clamp(old_matrix.at(row, col), 0, original_cols - 1);
                if (source.at(row, copy_col) >= 0) {
                    temp[static_cast<std::size_t>(insert_at)] = source.at(row, copy_col);
                }
            }
        } else {
            for (int col = 0; col < turn; ++col) {
                const int insert_at = std::clamp(insert_matrix.at(row, col), 0, target_lane_count - 1);
                shift_insert(temp, insert_at);
            }
        }

        for (int col = 0; col < target_lane_count; ++col) {
            converted.at(row, col) = temp[static_cast<std::size_t>(col)];
        }
    }

    return converted;
}

[[nodiscard]] BoolMatrix generate_delete_mark(const IntMatrix& new_matrix,
                                              const std::vector<int64_t>& time_axis,
                                              const std::vector<int64_t>& end_time_axis,
                                              const std::vector<double>& beat_length_axis,
                                              const std::vector<int>& org_col_index_axis,
                                              int64_t timing_fudge) {
    BoolMatrix mark(new_matrix.rows(), new_matrix.cols());
    if (new_matrix.rows() <= 1 || new_matrix.cols() <= 0 || time_axis.empty()) {
        return mark;
    }

    std::vector<int64_t> end_time_temp_row(static_cast<std::size_t>(new_matrix.cols()), 0);
    std::vector<int64_t> convert_time_point_row(static_cast<std::size_t>(new_matrix.cols()), time_axis.front());
    std::vector<int> org_col_index_row(static_cast<std::size_t>(new_matrix.cols()), -1);

    for (int row = 1; row < new_matrix.rows(); ++row) {
        for (int col = 0; col < new_matrix.cols(); ++col) {
            const int old_index = new_matrix.at(row, col);
            const int previous_old_index = new_matrix.at(row - 1, col);
            const double space = beat_length_axis[static_cast<std::size_t>(row - 1)] / 4.0;

            if (previous_old_index >= 0 && previous_old_index < static_cast<int>(end_time_axis.size())) {
                end_time_temp_row[static_cast<std::size_t>(col)] =
                    std::max(end_time_axis[static_cast<std::size_t>(previous_old_index)],
                             end_time_temp_row[static_cast<std::size_t>(col)]);
            }

            if (static_cast<double>(time_axis[static_cast<std::size_t>(row)]) <
                static_cast<double>(end_time_temp_row[static_cast<std::size_t>(col)]) + space -
                    static_cast<double>(timing_fudge)) {
                mark.set(row, col, true);
            }

            if (old_index >= 0 && old_index < static_cast<int>(org_col_index_axis.size()) &&
                org_col_index_axis[static_cast<std::size_t>(old_index)] != org_col_index_row[static_cast<std::size_t>(col)]) {
                org_col_index_row[static_cast<std::size_t>(col)] = org_col_index_axis[static_cast<std::size_t>(old_index)];
                convert_time_point_row[static_cast<std::size_t>(col)] = time_axis[static_cast<std::size_t>(row - 1)];
            }

            if (static_cast<double>(time_axis[static_cast<std::size_t>(row)]) <
                static_cast<double>(convert_time_point_row[static_cast<std::size_t>(col)]) + space +
                    static_cast<double>(timing_fudge)) {
                mark.set(row, col, true);
            }
        }
    }

    return mark;
}

void apply_position_based_deletion(IntMatrix& matrix,
                                   const BoolMatrix& mark,
                                   const std::vector<uint8_t>& hold_head_axis) {
    for (int row = 0; row < matrix.rows(); ++row) {
        for (int col = 0; col < matrix.cols(); ++col) {
            const int note_index = matrix.at(row, col);
            if (mark.at(row, col) &&
                !(note_index >= 0 &&
                  note_index < static_cast<int>(hold_head_axis.size()) &&
                  hold_head_axis[static_cast<std::size_t>(note_index)] != 0)) {
                matrix.at(row, col) = kMatrixEmpty;
            }
        }
    }
}

[[nodiscard]] IntMatrix convert_up(const IntMatrix& source,
                                   const std::vector<int64_t>& time_axis,
                                   const std::vector<double>& beat_length_axis,
                                   const std::vector<int64_t>& end_time_axis,
                                   const std::vector<int>& org_col_index_axis,
                                   const std::vector<uint8_t>& hold_head_axis,
                                   int target_lane_count,
                                   int64_t convert_time,
                                   int64_t timing_fudge,
                                   bool max_keys_equal_source,
                                   RandomSource& random) {
    const int turn = target_lane_count - source.cols();
    const auto matrices = build_convert_matrices(turn,
                                                 time_axis,
                                                 convert_time,
                                                 source.cols(),
                                                 max_keys_equal_source,
                                                 random);
    IntMatrix converted = perform_initial_convert(source,
                                                  matrices.old_matrix,
                                                  matrices.insert_matrix,
                                                  target_lane_count,
                                                  max_keys_equal_source);
    const BoolMatrix mark = generate_delete_mark(converted,
                                                 time_axis,
                                                 end_time_axis,
                                                 beat_length_axis,
                                                 org_col_index_axis,
                                                 timing_fudge);
    apply_position_based_deletion(converted, mark, hold_head_axis);
    return converted;
}

[[nodiscard]] double calculate_column_risk(const IntMatrix& matrix,
                                           int column,
                                           int region_start,
                                           int region_end) {
    int empty_rows = 0;
    int total_rows = 0;

    for (int row = region_start; row <= region_end; ++row) {
        bool has_other_note = false;
        for (int col = 0; col < matrix.cols(); ++col) {
            if (col != column && matrix.at(row, col) >= 0) {
                has_other_note = true;
                break;
            }
        }

        if (!has_other_note && matrix.at(row, column) >= 0) {
            ++empty_rows;
        }
        ++total_rows;
    }

    return total_rows > 0 ? static_cast<double>(empty_rows) / static_cast<double>(total_rows) : 0.0;
}

[[nodiscard]] std::vector<int> get_columns_to_remove(const IntMatrix& matrix,
                                                     const std::vector<int>& column_weights,
                                                     int target_cols,
                                                     int region_start,
                                                     int region_end) {
    struct Candidate {
        int index = 0;
        int weight = 0;
        double risk = 0.0;
    };

    const int remove_count = matrix.cols() - target_cols;
    if (remove_count <= 0) {
        return {};
    }

    std::vector<Candidate> candidates;
    candidates.reserve(static_cast<std::size_t>(matrix.cols()));
    for (int col = 0; col < matrix.cols(); ++col) {
        candidates.push_back({col,
                              column_weights[static_cast<std::size_t>(col)],
                              calculate_column_risk(matrix, col, region_start, region_end)});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.weight != rhs.weight) {
            return lhs.weight < rhs.weight;
        }
        return lhs.risk < rhs.risk;
    });

    std::vector<int> removed;
    removed.reserve(static_cast<std::size_t>(remove_count));
    for (int i = 0; i < remove_count && i < static_cast<int>(candidates.size()); ++i) {
        removed.push_back(candidates[static_cast<std::size_t>(i)].index);
    }
    return removed;
}

void apply_random_adjustment_to_column_selection(std::vector<int>& columns_to_remove,
                                                 const std::vector<int>& column_weights,
                                                 int original_cols,
                                                 RandomSource& random) {
    for (std::size_t i = 0; i < columns_to_remove.size(); ++i) {
        if (random.next_double() >= 0.25) {
            continue;
        }

        const int current_col = columns_to_remove[i];
        const int current_weight = column_weights[static_cast<std::size_t>(current_col)];
        std::vector<int> candidates;

        for (int col = 0; col < original_cols; ++col) {
            if (std::find(columns_to_remove.begin(), columns_to_remove.end(), col) != columns_to_remove.end()) {
                continue;
            }
            if (std::abs(column_weights[static_cast<std::size_t>(col)] - current_weight) <= 1) {
                candidates.push_back(col);
            }
        }

        if (!candidates.empty()) {
            columns_to_remove[i] = candidates[random.next_index(candidates.size())];
        }
    }
}

[[nodiscard]] std::vector<int> create_column_mapping(int original_cols, const std::vector<int>& columns_to_remove) {
    std::vector<bool> removed(static_cast<std::size_t>(original_cols), false);
    for (const int col : columns_to_remove) {
        if (col >= 0 && col < original_cols) {
            removed[static_cast<std::size_t>(col)] = true;
        }
    }

    std::vector<int> mapping(static_cast<std::size_t>(original_cols), -1);
    int new_col = 0;
    for (int old_col = 0; old_col < original_cols; ++old_col) {
        if (!removed[static_cast<std::size_t>(old_col)]) {
            mapping[static_cast<std::size_t>(old_col)] = new_col++;
        }
    }
    return mapping;
}

[[nodiscard]] bool is_position_available(const IntMatrix& matrix,
                                         const IntMatrix& origin_column_map,
                                         int old_col,
                                         int row,
                                         int col,
                                         const std::vector<int64_t>& time_axis,
                                         double beat_length,
                                         int64_t timing_fudge) {
    if (matrix.at(row, col) != kMatrixEmpty) {
        return false;
    }

    for (int scan = std::max(0, row - 3); scan < row; ++scan) {
        if (time_axis[static_cast<std::size_t>(row)] - time_axis[static_cast<std::size_t>(scan)] <=
                static_cast<int64_t>(std::llround(beat_length / 2.5)) + timing_fudge &&
            origin_column_map.at(scan, col) != old_col &&
            (matrix.at(scan, col) >= 0 || matrix.at(scan, col) == kMatrixHoldBody)) {
            return false;
        }
    }

    for (int scan = row + 1; scan <= std::min(matrix.rows() - 1, row + 3); ++scan) {
        if (time_axis[static_cast<std::size_t>(scan)] - time_axis[static_cast<std::size_t>(row)] <=
                static_cast<int64_t>(std::llround(beat_length / 2.5)) + timing_fudge &&
            origin_column_map.at(scan, col) != old_col &&
            (matrix.at(scan, col) >= 0 || matrix.at(scan, col) == kMatrixHoldBody)) {
            return false;
        }
    }

    return true;
}

void handle_long_note_extensions(IntMatrix& new_matrix, int row) {
    for (int col = 0; col < new_matrix.cols(); ++col) {
        if (new_matrix.at(row, col) == kMatrixEmpty &&
            row > 0 &&
            new_matrix.at(row - 1, col) == kMatrixHoldBody) {
            new_matrix.at(row, col) = kMatrixHoldBody;
        }
    }
}

void copy_long_note_body(const IntMatrix& source_matrix,
                         IntMatrix& target_matrix,
                         int start_row,
                         int old_col,
                         int new_col) {
    for (int row = start_row + 1; row < source_matrix.rows() && source_matrix.at(row, old_col) == kMatrixHoldBody; ++row) {
        if (new_col >= 0 && new_col < target_matrix.cols()) {
            target_matrix.at(row, new_col) = kMatrixHoldBody;
        }
    }
}

[[nodiscard]] bool is_position_available_for_empty_row(const IntMatrix& matrix,
                                                       const std::vector<int64_t>& time_axis,
                                                       int row,
                                                       int col,
                                                       double beat_length,
                                                       int64_t timing_fudge) {
    if (matrix.at(row, col) != kMatrixEmpty) {
        return false;
    }

    for (int scan = std::max(0, row - 3); scan < row; ++scan) {
        if (time_axis[static_cast<std::size_t>(row)] - time_axis[static_cast<std::size_t>(scan)] <=
                static_cast<int64_t>(std::llround(beat_length / 2.5)) + timing_fudge &&
            (matrix.at(scan, col) >= 0 || matrix.at(scan, col) == kMatrixHoldBody)) {
            return false;
        }
    }

    for (int scan = row + 1; scan <= std::min(matrix.rows() - 1, row + 3); ++scan) {
        if (time_axis[static_cast<std::size_t>(scan)] - time_axis[static_cast<std::size_t>(row)] <=
                static_cast<int64_t>(std::llround(beat_length / 2.5)) + timing_fudge &&
            (matrix.at(scan, col) >= 0 || matrix.at(scan, col) == kMatrixHoldBody)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool is_hold_note_tail_too_close(const IntMatrix& source_matrix,
                                               const IntMatrix& target_matrix,
                                               const std::vector<int64_t>& time_axis,
                                               int row,
                                               int original_col,
                                               int target_col,
                                               double beat_length,
                                               int64_t timing_fudge) {
    int hold_length = 0;
    for (int scan = row + 1; scan < source_matrix.rows(); ++scan) {
        if (source_matrix.at(scan, original_col) == kMatrixHoldBody) {
            ++hold_length;
        } else {
            break;
        }
    }

    if (hold_length == 0) {
        return false;
    }

    const int tail_row = row + hold_length;
    if (tail_row >= static_cast<int>(time_axis.size()) || tail_row >= target_matrix.rows()) {
        return false;
    }

    const int64_t min_time_distance =
        static_cast<int64_t>(std::llround(beat_length / 2.5)) - timing_fudge;
    for (int scan = row + 1; scan <= tail_row; ++scan) {
        if (target_matrix.at(scan, target_col) >= 0) {
            const int64_t time_distance =
                time_axis[static_cast<std::size_t>(scan)] - time_axis[static_cast<std::size_t>(tail_row)];
            return time_distance < min_time_distance;
        }
    }

    return false;
}

[[nodiscard]] bool try_insert_note_directly(IntMatrix& target_matrix,
                                            const IntMatrix& source_matrix,
                                            const std::vector<int64_t>& time_axis,
                                            int row,
                                            double beat_length,
                                            int64_t timing_fudge,
                                            RandomSource& random) {
    std::vector<int> available_columns;
    for (int col = 0; col < target_matrix.cols(); ++col) {
        if (is_position_available_for_empty_row(target_matrix, time_axis, row, col, beat_length, timing_fudge)) {
            available_columns.push_back(col);
        }
    }
    if (available_columns.empty()) {
        return false;
    }

    std::vector<std::pair<int, int>> candidates;
    for (int col = 0; col < source_matrix.cols(); ++col) {
        if (source_matrix.at(row, col) >= 0) {
            candidates.push_back({col, source_matrix.at(row, col)});
        }
    }
    if (candidates.empty()) {
        return false;
    }

    const int target_col = available_columns[random.next_index(available_columns.size())];
    const auto selected = candidates[random.next_index(candidates.size())];
    if (is_hold_note_tail_too_close(source_matrix,
                                    target_matrix,
                                    time_axis,
                                    row,
                                    selected.first,
                                    target_col,
                                    beat_length,
                                    timing_fudge)) {
        return false;
    }

    target_matrix.at(row, target_col) = selected.second;
    return true;
}

void try_clear_space_and_insert(const IntMatrix& source_matrix,
                                IntMatrix& target_matrix,
                                const std::vector<int64_t>& time_axis,
                                int empty_row,
                                double beat_length,
                                int64_t timing_fudge,
                                RandomSource& random) {
    const int64_t time_threshold =
        static_cast<int64_t>(std::llround(beat_length / 14.0)) + timing_fudge;
    std::vector<bool> processed(static_cast<std::size_t>(target_matrix.cols()), false);
    std::vector<int> time_range_rows;
    time_range_rows.reserve(static_cast<std::size_t>(target_matrix.rows()));

    for (int row = 0; row < target_matrix.rows(); ++row) {
        if (std::llabs(time_axis[static_cast<std::size_t>(row)] - time_axis[static_cast<std::size_t>(empty_row)]) <= time_threshold) {
            time_range_rows.push_back(row);
        }
    }
    if (time_range_rows.empty()) {
        return;
    }

    std::vector<int> columns_to_try(static_cast<std::size_t>(target_matrix.cols()));
    std::iota(columns_to_try.begin(), columns_to_try.end(), 0);
    for (int i = static_cast<int>(columns_to_try.size()) - 1; i > 0; --i) {
        const int swap_index = random.next_int(0, i + 1);
        std::swap(columns_to_try[static_cast<std::size_t>(i)], columns_to_try[static_cast<std::size_t>(swap_index)]);
    }

    for (const int col : columns_to_try) {
        if (processed[static_cast<std::size_t>(col)]) {
            continue;
        }

        bool has_notes_to_remove = false;
        for (const int row : time_range_rows) {
            if (target_matrix.at(row, col) >= 0) {
                has_notes_to_remove = true;
                break;
            }
        }
        if (!has_notes_to_remove) {
            continue;
        }

        std::vector<std::pair<int, int>> original_values;
        original_values.reserve(time_range_rows.size());
        for (const int row : time_range_rows) {
            original_values.push_back({row, target_matrix.at(row, col)});
            if (target_matrix.at(row, col) >= 0) {
                target_matrix.at(row, col) = kMatrixEmpty;
            }
        }

        bool creates_empty_rows = false;
        for (const int row : time_range_rows) {
            bool is_empty_row = true;
            for (int scan_col = 0; scan_col < target_matrix.cols(); ++scan_col) {
                if (target_matrix.at(row, scan_col) != kMatrixEmpty) {
                    is_empty_row = false;
                    break;
                }
            }

            if (is_empty_row) {
                creates_empty_rows = true;
                break;
            }
        }

        if (creates_empty_rows) {
            for (const auto& [row, value] : original_values) {
                target_matrix.at(row, col) = value;
            }
            processed[static_cast<std::size_t>(col)] = true;
            continue;
        }

        if (try_insert_note_directly(target_matrix,
                                     source_matrix,
                                     time_axis,
                                     empty_row,
                                     beat_length,
                                     timing_fudge,
                                     random)) {
            return;
        }

        for (const auto& [row, value] : original_values) {
            target_matrix.at(row, col) = value;
        }
        processed[static_cast<std::size_t>(col)] = true;
    }
}

void apply_minimum_notes_constraint(IntMatrix& target_matrix,
                                    const IntMatrix& source_matrix,
                                    int start_row,
                                    int end_row,
                                    const std::vector<int64_t>& time_axis,
                                    const std::vector<double>& beat_length_axis,
                                    int64_t timing_fudge,
                                    RandomSource& random) {
    for (int row = start_row; row <= end_row; ++row) {
        bool has_note = false;
        for (int col = 0; col < target_matrix.cols(); ++col) {
            if (target_matrix.at(row, col) >= 0) {
                has_note = true;
                break;
            }
        }
        if (has_note) {
            continue;
        }

        std::vector<int> candidate_notes;
        for (int col = 0; col < source_matrix.cols(); ++col) {
            if (source_matrix.at(row, col) >= 0) {
                candidate_notes.push_back(col);
            }
        }
        if (candidate_notes.empty()) {
            continue;
        }

        std::vector<int> available_positions;
        for (int col = 0; col < target_matrix.cols(); ++col) {
            if (!is_position_available_for_empty_row(target_matrix,
                                                    time_axis,
                                                    row,
                                                    col,
                                                    beat_length_axis[static_cast<std::size_t>(row)],
                                                    timing_fudge)) {
                continue;
            }
            if (!is_hold_note_tail_too_close(source_matrix,
                                            target_matrix,
                                            time_axis,
                                            row,
                                            candidate_notes.front(),
                                            col,
                                            beat_length_axis[static_cast<std::size_t>(row)],
                                            timing_fudge)) {
                available_positions.push_back(col);
            }
        }
        if (available_positions.empty()) {
            continue;
        }

        const int selected_original_col = candidate_notes[random.next_index(candidate_notes.size())];
        const int target_col = available_positions[random.next_index(available_positions.size())];
        target_matrix.at(row, target_col) = source_matrix.at(row, selected_original_col);
    }
}

void process_empty_rows(const IntMatrix& source_matrix,
                        IntMatrix& target_matrix,
                        const std::vector<int64_t>& time_axis,
                        const std::vector<double>& beat_length_axis,
                        int64_t timing_fudge,
                        RandomSource& random) {
    for (int row = 0; row < target_matrix.rows(); ++row) {
        bool is_empty_row = true;
        for (int col = 0; col < target_matrix.cols(); ++col) {
            if (target_matrix.at(row, col) >= 0) {
                is_empty_row = false;
                break;
            }
        }
        if (!is_empty_row) {
            continue;
        }

        if (try_insert_note_directly(target_matrix,
                                     source_matrix,
                                     time_axis,
                                     row,
                                     beat_length_axis[static_cast<std::size_t>(row)],
                                     timing_fudge,
                                     random)) {
            continue;
        }

        try_clear_space_and_insert(source_matrix,
                                   target_matrix,
                                   time_axis,
                                   row,
                                   beat_length_axis[static_cast<std::size_t>(row)],
                                   timing_fudge,
                                   random);
    }
}

void process_region(const IntMatrix& source_matrix,
                    IntMatrix& target_matrix,
                    IntMatrix& origin_column_map,
                    const std::vector<int64_t>& time_axis,
                    int region_start,
                    int region_end,
                    const std::vector<double>& beat_length_axis,
                    int64_t timing_fudge,
                    RandomSource& random) {
    std::vector<int> column_weights(static_cast<std::size_t>(source_matrix.cols()), 0);
    for (int row = region_start; row <= region_end; ++row) {
        for (int col = 0; col < source_matrix.cols(); ++col) {
            if (source_matrix.at(row, col) >= 0) {
                ++column_weights[static_cast<std::size_t>(col)];
            }
        }
    }

    std::vector<int> columns_to_remove = get_columns_to_remove(source_matrix,
                                                               column_weights,
                                                               target_matrix.cols(),
                                                               region_start,
                                                               region_end);
    apply_random_adjustment_to_column_selection(columns_to_remove, column_weights, source_matrix.cols(), random);
    const auto column_mapping = create_column_mapping(source_matrix.cols(), columns_to_remove);

    for (int row = region_start; row <= region_end; ++row) {
        for (int col = 0; col < source_matrix.cols(); ++col) {
            const int value = source_matrix.at(row, col);
            if (value < 0) {
                continue;
            }

            const int new_col = column_mapping[static_cast<std::size_t>(col)];
            if (new_col < 0) {
                continue;
            }

            if (!is_position_available(target_matrix,
                                       origin_column_map,
                                       col,
                                       row,
                                       new_col,
                                       time_axis,
                                       beat_length_axis[static_cast<std::size_t>(row)],
                                       timing_fudge)) {
                continue;
            }

            target_matrix.at(row, new_col) = value;
            origin_column_map.at(row, new_col) = col;
            copy_long_note_body(source_matrix, target_matrix, row, col, new_col);
        }
    }

    for (int row = region_start; row <= region_end; ++row) {
        handle_long_note_extensions(target_matrix, row);
    }

    apply_minimum_notes_constraint(target_matrix,
                                   source_matrix,
                                   region_start,
                                   region_end,
                                   time_axis,
                                   beat_length_axis,
                                   timing_fudge,
                                   random);
}

[[nodiscard]] IntMatrix smart_reduce_columns(const IntMatrix& source_matrix,
                                             const std::vector<int64_t>& time_axis,
                                             int turn,
                                             int64_t convert_time,
                                             const std::vector<double>& beat_length_axis,
                                             int64_t timing_fudge,
                                             RandomSource& random) {
    const int rows = source_matrix.rows();
    const int target_cols = source_matrix.cols() - turn;
    IntMatrix target_matrix(rows, target_cols);
    IntMatrix origin_column_map(rows, target_cols);

    if (rows <= 0) {
        return target_matrix;
    }

    if (rows == 1) {
        process_region(source_matrix,
                       target_matrix,
                       origin_column_map,
                       time_axis,
                       0,
                       0,
                       beat_length_axis,
                       timing_fudge,
                       random);
        process_empty_rows(source_matrix, target_matrix, time_axis, beat_length_axis, timing_fudge, random);
        return target_matrix;
    }

    int region_start = 0;
    for (int region_end = 1; region_end < rows; ++region_end) {
        const bool is_region_end =
            time_axis[static_cast<std::size_t>(region_end)] - time_axis[static_cast<std::size_t>(region_start)] >= convert_time;
        const bool is_last_row = region_end == rows - 1;

        if (!is_region_end && !is_last_row) {
            continue;
        }

        process_region(source_matrix,
                       target_matrix,
                       origin_column_map,
                       time_axis,
                       region_start,
                       region_end,
                       beat_length_axis,
                       timing_fudge,
                       random);
        region_start = region_end;
    }

    if (region_start < rows - 1) {
        process_region(source_matrix,
                       target_matrix,
                       origin_column_map,
                       time_axis,
                       region_start,
                       rows - 1,
                       beat_length_axis,
                       timing_fudge,
                       random);
    }

    process_empty_rows(source_matrix, target_matrix, time_axis, beat_length_axis, timing_fudge, random);
    return target_matrix;
}

[[nodiscard]] IntMatrix convert_down(const IntMatrix& source_matrix,
                                     const std::vector<int64_t>& time_axis,
                                     const std::vector<double>& beat_length_axis,
                                     int target_lane_count,
                                     int64_t convert_time,
                                     int64_t timing_fudge,
                                     RandomSource& random) {
    return smart_reduce_columns(source_matrix,
                                time_axis,
                                source_matrix.cols() - target_lane_count,
                                convert_time,
                                beat_length_axis,
                                timing_fudge,
                                random);
}

void density_reducer(IntMatrix& matrix,
                     int max_keys,
                     int min_keys,
                     int target_lane_count,
                     RandomSource& random) {
    const int max_to_remove_per_row = target_lane_count - max_keys;
    if (max_to_remove_per_row <= 0) {
        return;
    }

    std::vector<int> column_deletions(static_cast<std::size_t>(matrix.cols()), 0);
    for (int row = 0; row < matrix.rows(); ++row) {
        std::vector<int> active_columns;
        for (int col = 0; col < matrix.cols(); ++col) {
            if (matrix.at(row, col) >= 0) {
                active_columns.push_back(col);
            }
        }

        if (static_cast<int>(active_columns.size()) <= min_keys) {
            continue;
        }

        const int target_notes = std::max(
            min_keys,
            std::min(static_cast<int>(active_columns.size()),
                     static_cast<int>(static_cast<double>(active_columns.size()) *
                                      static_cast<double>(target_lane_count - max_to_remove_per_row) /
                                      static_cast<double>(target_lane_count))));

        int remove_count = std::max(0, static_cast<int>(active_columns.size()) - target_notes);
        while (remove_count > 0 && !active_columns.empty()) {
            std::vector<double> weights(active_columns.size(), 0.0);
            double total_weight = 0.0;
            for (std::size_t i = 0; i < active_columns.size(); ++i) {
                weights[i] = 1.0 / (1.0 + static_cast<double>(column_deletions[static_cast<std::size_t>(active_columns[i])]));
                total_weight += weights[i];
            }

            const double pick = random.next_double() * total_weight;
            double running = 0.0;
            std::size_t selected = 0;
            for (std::size_t i = 0; i < weights.size(); ++i) {
                running += weights[i];
                if (pick <= running) {
                    selected = i;
                    break;
                }
            }

            matrix.at(row, active_columns[selected]) = kMatrixEmpty;
            ++column_deletions[static_cast<std::size_t>(active_columns[selected])];
            active_columns.erase(active_columns.begin() + static_cast<std::ptrdiff_t>(selected));
            --remove_count;
        }
    }
}

[[nodiscard]] GameplayChart rebuild_chart_from_matrix(const GameplayChart& source,
                                                      const IntMatrix& matrix,
                                                      int target_lane_count) {
    GameplayChart converted = source;
    converted.lane_count = target_lane_count;
    converted.notes.clear();
    converted.notes.reserve(source.notes.size() + source.notes.size() / 2);

    for (int row = 0; row < matrix.rows(); ++row) {
        for (int col = 0; col < matrix.cols(); ++col) {
            const int note_index = matrix.at(row, col);
            if (note_index < 0 || note_index >= static_cast<int>(source.notes.size())) {
                continue;
            }

            NoteEvent note = source.notes[static_cast<std::size_t>(note_index)];
            note.lane = col + 1;
            converted.notes.push_back(std::move(note));
        }
    }

    std::stable_sort(converted.notes.begin(), converted.notes.end(), [](const NoteEvent& lhs, const NoteEvent& rhs) {
        if (lhs.start_sample != rhs.start_sample) {
            return lhs.start_sample < rhs.start_sample;
        }
        return lhs.lane < rhs.lane;
    });
    return converted;
}

struct OverlapResolutionStats {
    std::size_t clipped = 0;
    std::size_t removed = 0;
};

[[nodiscard]] OverlapResolutionStats resolve_lane_overlaps(GameplayChart& chart) {
    OverlapResolutionStats stats;
    if (chart.lane_count <= 0 || chart.notes.empty()) {
        return stats;
    }

    std::vector<NoteEvent> normalized;
    normalized.reserve(chart.notes.size());
    std::vector<std::size_t> last_note_by_lane(static_cast<std::size_t>(chart.lane_count), std::numeric_limits<std::size_t>::max());

    for (const auto& note : chart.notes) {
        if (note.lane <= 0 || note.lane > chart.lane_count) {
            normalized.push_back(note);
            continue;
        }

        const auto lane_index = static_cast<std::size_t>(note.lane - 1);
        const std::size_t previous_index = last_note_by_lane[lane_index];
        if (previous_index != std::numeric_limits<std::size_t>::max()) {
            auto& previous = normalized[previous_index];
            int64_t previous_end = previous.end_sample.value_or(previous.start_sample);
            if (note.start_sample <= previous_end) {
                if (previous.end_sample.has_value()) {
                    const int64_t clipped_end = note.start_sample - 1;
                    if (clipped_end > previous.start_sample) {
                        previous.end_sample = clipped_end;
                    } else {
                        previous.end_sample.reset();
                        previous.release_required = false;
                    }
                    ++stats.clipped;
                    previous_end = previous.end_sample.value_or(previous.start_sample);
                }

                if (note.start_sample <= previous_end) {
                    ++stats.removed;
                    continue;
                }
            }
        }

        normalized.push_back(note);
        last_note_by_lane[lane_index] = normalized.size() - 1;
    }

    chart.notes = std::move(normalized);
    return stats;
}

}  // namespace

KeyModeConverterResult convert_key_mode_chart(const GameplayChart& chart, const KeyModeConverterOptions& options) {
    if (options.algorithm == KeyModeConversionAlgorithm::NK2) {
        KeyModeConverterResult nk2_result = convert_key_mode_chart_nk2(chart, options);
        if (!nk2_result.converted) {
            return nk2_result;
        }

        const OverlapResolutionStats overlap_stats = resolve_lane_overlaps(nk2_result.chart);
        if (nk2_result.chart.notes.empty()) {
            nk2_result.chart = chart;
            nk2_result.converted = false;
            nk2_result.warnings.push_back("nK2 produced no playable notes after overlap repair.");
            return nk2_result;
        }
        if (overlap_stats.clipped > 0 || overlap_stats.removed > 0) {
            nk2_result.warnings.push_back("nK2 resolved lane overlaps (clipped=" +
                                          std::to_string(overlap_stats.clipped) + ", removed=" +
                                          std::to_string(overlap_stats.removed) + ").");
        }
        return nk2_result;
    }
    KeyModeConverterResult result;
    result.chart = chart;

    const int source_lane_count = resolve_lane_count(chart);
    if (source_lane_count <= 0 || chart.notes.empty() || options.target_lane_count <= 0 ||
        options.target_lane_count == source_lane_count) {
        return result;
    }

    const int sample_rate = resolve_sample_rate(options.sample_rate);
    const double beat_length_samples = static_cast<double>(sample_rate) * 60.0 / resolve_base_bpm(options.base_bpm);
    const int64_t timing_fudge = samples_from_ms(kTimingFudgeMs, sample_rate);
    const int speed_slot = std::clamp(options.transform_speed_slot, 0, static_cast<int>(kTransformSpeedValues.size()) - 1);
    const int64_t convert_time = std::max<int64_t>(
        1,
        static_cast<int64_t>(std::llround(kTransformSpeedValues[static_cast<std::size_t>(speed_slot)] *
                                          beat_length_samples * 4.0)) -
            timing_fudge);

    const auto time_axis = build_time_axis(chart);
    if (time_axis.empty()) {
        return result;
    }

    std::unordered_map<int64_t, int> row_by_time;
    row_by_time.reserve(time_axis.size());
    for (int row = 0; row < static_cast<int>(time_axis.size()); ++row) {
        row_by_time.emplace(time_axis[static_cast<std::size_t>(row)], row);
    }

    const IntMatrix source_matrix = build_note_matrix(chart, source_lane_count, row_by_time);
    const auto beat_length_axis = build_beat_length_axis(time_axis.size(), beat_length_samples);
    const auto end_time_axis = build_end_time_axis(chart);
    const auto hold_head_axis = build_hold_head_axis(chart);
    RandomSource random(options.seed);

    const int resolved_max_keys =
        std::clamp(options.max_keys > 0 ? options.max_keys : std::min(options.target_lane_count, 8), 1, options.target_lane_count);
    const int resolved_min_keys =
        std::clamp(options.min_keys > 0 ? options.min_keys : std::min(options.target_lane_count, 2), 1, options.target_lane_count);
    const auto org_col_index_axis = build_original_column_axis(source_matrix, chart.notes.size());
    IntMatrix converted_matrix;
    if (options.target_lane_count > source_lane_count) {
        const bool max_keys_equal_source = options.max_keys > 0 && options.max_keys == source_lane_count;
        converted_matrix = convert_up(source_matrix,
                                      time_axis,
                                      beat_length_axis,
                                      end_time_axis,
                                      org_col_index_axis,
                                      hold_head_axis,
                                      options.target_lane_count,
                                      convert_time,
                                      timing_fudge,
                                      max_keys_equal_source,
                                      random);
    } else {
        converted_matrix = convert_down(source_matrix,
                                        time_axis,
                                        beat_length_axis,
                                        options.target_lane_count,
                                        convert_time,
                                        timing_fudge,
                                        random);
    }

    density_reducer(converted_matrix,
                    resolved_max_keys,
                    resolved_min_keys,
                    options.target_lane_count,
                    random);

    GameplayChart rebuilt = rebuild_chart_from_matrix(chart, converted_matrix, options.target_lane_count);
    const OverlapResolutionStats overlap_stats = resolve_lane_overlaps(rebuilt);
    if (rebuilt.notes.empty()) {
        result.warnings.push_back("Key mode converter produced no playable notes.");
        return result;
    }

    result.chart = std::move(rebuilt);
    result.converted = true;
    result.warnings.push_back("Key mode converter remapped " +
                              std::to_string(source_lane_count) + "K to " +
                              std::to_string(options.target_lane_count) + "K.");
    if (overlap_stats.clipped > 0 || overlap_stats.removed > 0) {
        result.warnings.push_back("Key mode converter resolved lane overlaps (clipped=" +
                                  std::to_string(overlap_stats.clipped) +
                                  ", removed=" +
                                  std::to_string(overlap_stats.removed) +
                                  ").");
    }
    return result;
}

}  // namespace tenriff::gameplay
