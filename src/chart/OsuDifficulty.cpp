#include "chart/OsuDifficulty.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace tenriff::chart {

namespace {

constexpr double kTimeWindowMs = 1000.0;
constexpr double kHalfWindowMs = 500.0;
constexpr double kFdWeight = 0.5;
constexpr double kRdWeight = 0.5;
constexpr double kLfdWeight = 1.0;
constexpr double kLrdWeight = 1.0;
constexpr double kDistanceWeight = 1.0;
constexpr double kVibroNpsCondition = 10.0;
constexpr double kVibroNpsNerfWeight = 0.9;
constexpr double kVibroRelationNerfWeight = 0.0;
constexpr double kLdbWeight = 1.0;

constexpr double kScoreWeight = 0.1;
constexpr double kAccWeight = 0.9;
constexpr double kRatingWeight = 0.05014784238342812454604006185774;
constexpr double kRatingPower = 0.35163702826872596;
constexpr double kAverageRatingPower = 1.0;
constexpr double kAverageLevelPower = 1.0;
constexpr double kReviveScoreWeight = 0.1;
constexpr double kReviveAccWeight = 0.9;
constexpr double kReviveLevelScale = 0.05237980772028835522591806121352;
constexpr double kReviveLevelPower = 0.6151479234411744;
constexpr double kReviveMaxLevel = 24.0;

constexpr std::array<double, 4> kJackBaseIntervalsMs = {{75.0, 100.0, 125.0, 150.0}};
constexpr std::array<double, 4> kJackLnIntervalsMs = {{37.5, 50.0, 62.5, 75.0}};
constexpr std::array<double, 4> kJackCoefficients = {{1.0, 0.5, 0.4, 0.1}};
constexpr std::array<double, 4> kVibroJackNerfWeights = {{0.5, 1.0, 0.0, 0.0}};
constexpr double kLongNoteMissMsScale = 0.5;

enum class DifficultyEventType {
    Rice,
    HoldStart,
    HoldEnd,
};

enum class JudgmentNoteType {
    Rice,
    Head,
    Tail,
};

double scale_difficulty_timing_offset(double ms_offset, JudgmentNoteType note_type) {
    // This eases LN difficulty only; gameplay judgement windows remain unchanged.
    return note_type == JudgmentNoteType::Rice ? ms_offset : ms_offset * kLongNoteMissMsScale;
}

enum class MatrixKey {
    K4,
    K5,
    K5P1,
    K6,
    K7,
    K7P1,
    K8,
    K9,
    K10,
    K10P2,
    K14P2,
    K16,
};

using Matrix = std::vector<std::vector<double>>;

struct DifficultyEvent {
    double time_seconds = 0.0;
    int column = 1;
    DifficultyEventType type = DifficultyEventType::Rice;
};

struct JudgmentDefinition {
    std::optional<double> plus_rice;
    std::optional<double> minus_rice;
    std::optional<double> plus_head;
    std::optional<double> minus_head;
    std::optional<double> plus_tail;
    std::optional<double> minus_tail;
    std::optional<double> score_pct;
    std::optional<double> acc_pct;
};

using JudgmentSet = std::vector<JudgmentDefinition>;

struct TimeDeltas {
    double score_plus_rice = 0.0;
    double score_minus_rice = 0.0;
    double score_plus_head = 0.0;
    double score_minus_head = 0.0;
    double score_plus_tail = 0.0;
    double score_minus_tail = 0.0;
    double acc_plus_rice = 0.0;
    double acc_minus_rice = 0.0;
    double acc_plus_head = 0.0;
    double acc_minus_head = 0.0;
    double acc_plus_tail = 0.0;
    double acc_minus_tail = 0.0;
};

struct LineNoteInfo {
    std::size_t idx = 0;
    double time_seconds = 0.0;
    DifficultyEventType type = DifficultyEventType::Rice;
    std::size_t timing_order = 0;
};

struct NpsDifficultyData {
    std::vector<int> nps;
    std::vector<double> nps_v2;
    std::vector<double> distance_difficulty;
    std::vector<double> jack_nps_v2;
    std::vector<double> same_line_nps_v2;
    std::vector<double> minimum_distance_sum;
    std::vector<double> same_line_minimum_distance_sum;
    std::vector<double> jack_interval;
    std::vector<double> jack_score_uniformity;
    std::vector<double> jack_acc_uniformity;
};

struct NoteDifficultyData {
    std::vector<int> nps;
    std::vector<double> nps_v2;
    std::vector<double> same_line_nps_v2;
    std::vector<double> minimum_distance_sum;
    std::vector<double> same_line_minimum_distance_sum;
    std::vector<double> note_jack_diff_score;
    std::vector<double> note_jack_diff_acc;
    std::vector<double> fds;
    std::vector<double> fda;
    std::vector<double> rds;
    std::vector<double> rda;
    std::vector<double> lfds;
    std::vector<double> lfda;
    std::vector<double> lrds;
    std::vector<double> lrda;
    std::vector<double> ldb;
    std::vector<double> ldbd;
    std::vector<double> vrs;
    std::vector<double> vra;
};

double round_nine(double value) {
    if (!std::isfinite(value)) {
        return value;
    }
    return std::round(value * 1'000'000'000.0) / 1'000'000'000.0;
}

double clamp_min(double value, double minimum) {
    if (!std::isfinite(value)) {
        return minimum;
    }
    return std::max(value, minimum);
}

std::optional<double> parse_optional_token(const std::string& token) {
    if (token == "x") {
        return std::nullopt;
    }
    return std::stod(token);
}

Matrix parse_matrix(int rows, int columns, const char* data) {
    std::istringstream stream(data);
    Matrix matrix(static_cast<std::size_t>(rows), std::vector<double>(static_cast<std::size_t>(columns), 0.0));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            stream >> matrix[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
        }
    }
    return matrix;
}

JudgmentSet parse_judgments(const char* data) {
    std::istringstream stream(data);
    JudgmentSet judgments;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string token;
        std::array<std::optional<double>, 8> values;
        for (auto& value : values) {
            row >> token;
            value = parse_optional_token(token);
        }
        judgments.push_back(JudgmentDefinition{values[0], values[1], values[2], values[3], values[4], values[5],
                                               values[6], values[7]});
    }
    return judgments;
}

int note_type_order(DifficultyEventType type) {
    switch (type) {
    case DifficultyEventType::HoldEnd:
        return 0;
    case DifficultyEventType::Rice:
        return 1;
    case DifficultyEventType::HoldStart:
        return 2;
    }
    return 1;
}

int note_type_index(DifficultyEventType type) {
    switch (type) {
    case DifficultyEventType::Rice:
        return 0;
    case DifficultyEventType::HoldStart:
        return 1;
    case DifficultyEventType::HoldEnd:
        return 2;
    }
    return 0;
}

JudgmentNoteType note_type_for_event(DifficultyEventType type) {
    switch (type) {
    case DifficultyEventType::HoldStart:
        return JudgmentNoteType::Head;
    case DifficultyEventType::HoldEnd:
        return JudgmentNoteType::Tail;
    case DifficultyEventType::Rice:
        return JudgmentNoteType::Rice;
    }
    return JudgmentNoteType::Rice;
}

bool is_hold_boundary(DifficultyEventType type) {
    return type == DifficultyEventType::HoldStart || type == DifficultyEventType::HoldEnd;
}

std::string normalize_mode_name(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

std::optional<MatrixKey> resolve_matrix_key(int key_count, std::string_view mode_name) {
    const std::string normalized = normalize_mode_name(mode_name);
    if (normalized == "5+1" || normalized == "5+1k" || normalized == "5p1" || normalized == "5p1k" ||
        normalized == "5+1sp") {
        return MatrixKey::K5P1;
    }
    if (normalized == "7+1" || normalized == "7+1k" || normalized == "7p1" || normalized == "7p1k" ||
        normalized == "7+1sp") {
        return MatrixKey::K7P1;
    }
    if (normalized == "10+2" || normalized == "10+2k" || normalized == "10p2" || normalized == "10p2k" ||
        normalized == "dp12" || normalized == "10+2dp") {
        return MatrixKey::K10P2;
    }
    if (normalized == "14+2" || normalized == "14+2k" || normalized == "14p2" || normalized == "14p2k" ||
        normalized == "dp16" || normalized == "14+2dp") {
        return MatrixKey::K14P2;
    }

    switch (key_count) {
    case 4:
        return MatrixKey::K4;
    case 5:
        return MatrixKey::K5;
    case 6:
        return MatrixKey::K6;
    case 7:
        return MatrixKey::K7;
    case 8:
        return MatrixKey::K8;
    case 9:
        return MatrixKey::K9;
    case 10:
        return MatrixKey::K10;
    case 16:
        return MatrixKey::K16;
    default:
        return std::nullopt;
    }
}

std::vector<int> scratch_indices(MatrixKey key) {
    switch (key) {
    case MatrixKey::K5P1:
    case MatrixKey::K7P1:
        return {0};
    case MatrixKey::K10P2:
        return {0, 11};
    case MatrixKey::K14P2:
        return {0, 15};
    default:
        return {};
    }
}

const Matrix& weighted_nps_matrix(MatrixKey key) {
    static const Matrix k4 = parse_matrix(4, 4, R"(1 1 0.1 0.1 0.75 1 0.35 0.1 0.1 0.35 1 0.75 0.1 0.1 1 1)");
    static const Matrix k5 = parse_matrix(5, 5, R"(1 1 0.3 0.1 0.1 0.75 1 0.55 0.1 0.1 0.33 0.495 1.1 0.495 0.33 0.1 0.1 0.55 1 0.75 0.1 0.1 0.3 1 1)");
    static const Matrix k5p1 = parse_matrix(6, 6, R"(1.2 0.432 0.432 0.432 0.432 0.432 0.3 1 1 0.3 0.1 0.1 0.3 0.75 1 0.55 0.1 0.1 0.33 0.33 0.495 1.1 0.495 0.33 0.3 0.1 0.1 0.55 1 0.75 0.3 0.1 0.1 0.3 1 1)");
    static const Matrix k6 = parse_matrix(6, 6, R"(1.1 1.1 0.55 0.11 0.11 0.11 0.75 1 0.75 0.1 0.1 0.1 0.5 0.75 1 0.35 0.1 0.1 0.1 0.1 0.35 1 0.75 0.5 0.1 0.1 0.1 0.75 1 0.75 0.11 0.11 0.11 0.55 1.1 1.1)");
    static const Matrix k7 = parse_matrix(7, 7, R"(1.1 1.1 0.55 0.33 0.11 0.11 0.11 0.75 1 0.75 0.3 0.1 0.1 0.1 0.5 0.75 1 0.55 0.1 0.1 0.1 0.33 0.33 0.495 1.1 0.495 0.33 0.33 0.1 0.1 0.1 0.55 1 0.75 0.5 0.1 0.1 0.1 0.3 0.75 1 0.75 0.11 0.11 0.11 0.33 0.55 1.1 1.1)");
    static const Matrix k7p1 = parse_matrix(8, 8, R"(1.2 0.41143 0.41143 0.41143 0.41143 0.41143 0.41143 0.41143 0.33 1.1 1.1 0.55 0.33 0.11 0.11 0.11 0.3 0.75 1 0.75 0.3 0.1 0.1 0.1 0.3 0.5 0.75 1 0.55 0.1 0.1 0.1 0.33 0.33 0.33 0.495 1.1 0.495 0.33 0.33 0.3 0.1 0.1 0.1 0.55 1 0.75 0.5 0.3 0.1 0.1 0.1 0.3 0.75 1 0.75 0.33 0.11 0.11 0.11 0.33 0.55 1.1 1.1)");
    static const Matrix k8 = parse_matrix(8, 8, R"(1.1 1.1 0.55 0.55 0.11 0.11 0.11 0.11 0.75 1 0.75 0.5 0.1 0.1 0.1 0.1 0.5 0.75 1 0.75 0.1 0.1 0.1 0.1 0.55 0.55 0.825 1.1 0.385 0.11 0.11 0.11 0.11 0.11 0.11 0.385 1.1 0.825 0.55 0.55 0.1 0.1 0.1 0.1 0.75 1 0.75 0.5 0.1 0.1 0.1 0.1 0.5 0.75 1 0.75 0.11 0.11 0.11 0.11 0.55 0.55 1.1 1.1)");
    static const Matrix k9 = parse_matrix(9, 9, R"(1.2 1.2 0.6 0.6 0.36 0.12 0.12 0.12 0.12 0.825 1.1 0.825 0.55 0.33 0.11 0.11 0.11 0.11 0.5 0.75 1 0.75 0.3 0.1 0.1 0.1 0.1 0.5 0.5 0.75 1 0.55 0.1 0.1 0.1 0.1 0.33 0.33 0.33 0.495 1.1 0.495 0.33 0.33 0.33 0.1 0.1 0.1 0.1 0.55 1 0.75 0.5 0.5 0.1 0.1 0.1 0.1 0.3 0.75 1 0.75 0.5 0.11 0.11 0.11 0.11 0.33 0.55 0.825 1.1 0.825 0.12 0.12 0.12 0.12 0.36 0.6 0.6 1.2 1.2)");
    static const Matrix k10 = parse_matrix(10, 10, R"(1.2 1.2 0.6 0.6 0.6 0.12 0.12 0.12 0.12 0.12 0.825 1.1 0.825 0.55 0.55 0.11 0.11 0.11 0.11 0.11 0.5 0.75 1 0.75 0.5 0.1 0.1 0.1 0.1 0.1 0.5 0.5 0.75 1 0.75 0.1 0.1 0.1 0.1 0.1 0.55 0.55 0.55 0.825 1.1 0.385 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.385 1.1 0.825 0.55 0.55 0.55 0.1 0.1 0.1 0.1 0.1 0.75 1 0.75 0.5 0.5 0.1 0.1 0.1 0.1 0.1 0.5 0.75 1 0.75 0.5 0.11 0.11 0.11 0.11 0.11 0.55 0.55 0.825 1.1 0.825 0.12 0.12 0.12 0.12 0.12 0.6 0.6 0.6 1.2 1.2)");
    static const Matrix k10p2 = parse_matrix(12, 12, R"(1.2 0.72 0.72 0.72 0.72 0.72 0.12 0.12 0.12 0.12 0.12 0.12 0.6 1.2 1.2 0.6 0.6 0.6 0.12 0.12 0.12 0.12 0.12 0.12 0.55 0.825 1.1 0.825 0.55 0.55 0.11 0.11 0.11 0.11 0.11 0.11 0.5 0.5 0.75 1 0.75 0.5 0.1 0.1 0.1 0.1 0.1 0.1 0.5 0.5 0.5 0.75 1 0.75 0.1 0.1 0.1 0.1 0.1 0.1 0.55 0.55 0.55 0.55 0.825 1.1 0.385 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.385 1.1 0.825 0.55 0.55 0.55 0.55 0.1 0.1 0.1 0.1 0.1 0.1 0.75 1 0.75 0.5 0.5 0.5 0.1 0.1 0.1 0.1 0.1 0.1 0.5 0.75 1 0.75 0.5 0.5 0.11 0.11 0.11 0.11 0.11 0.11 0.55 0.55 0.825 1.1 0.825 0.55 0.12 0.12 0.12 0.12 0.12 0.12 0.6 0.6 0.6 1.2 1.2 0.6 0.12 0.12 0.12 0.12 0.12 0.12 0.72 0.72 0.72 0.72 0.72 1.2)");
    static const Matrix k14p2 = parse_matrix(16, 16, R"(1.2 0.68571 0.68571 0.68571 0.68571 0.68571 0.68571 0.68571 0.12 0.12 0.12 0.12 0.12 0.12 0.12 0.12 0.54 1.08 1.08 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 0.54 0.81 1.08 0.378 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.378 1.08 0.81 0.54 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 1.08 1.08 0.54 0.12 0.12 0.12 0.12 0.12 0.12 0.12 0.12 0.68571 0.68571 0.68571 0.68571 0.68571 0.68571 0.68571 1.2)");
    static const Matrix k16 = parse_matrix(16, 16, R"(1.08 1.08 0.54 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 0.54 0.81 1.08 0.378 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.378 1.08 0.81 0.54 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.54 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 0.81 1.08 0.81 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.108 0.54 0.54 0.54 0.54 0.54 0.54 1.08 1.08)");

    switch (key) {
    case MatrixKey::K4:
        return k4;
    case MatrixKey::K5:
        return k5;
    case MatrixKey::K5P1:
        return k5p1;
    case MatrixKey::K6:
        return k6;
    case MatrixKey::K7:
        return k7;
    case MatrixKey::K7P1:
        return k7p1;
    case MatrixKey::K8:
        return k8;
    case MatrixKey::K9:
        return k9;
    case MatrixKey::K10:
        return k10;
    case MatrixKey::K10P2:
        return k10p2;
    case MatrixKey::K14P2:
        return k14p2;
    case MatrixKey::K16:
        return k16;
    }
    return k10;
}

const Matrix& visual_distance_matrix(MatrixKey key) {
    static const Matrix k4 = parse_matrix(4, 4, R"(1 1 1.5 1.25 1 1 1 1.5 1.5 1 1 1 1.25 1.5 1 1)");
    static const Matrix k5 = parse_matrix(5, 5, R"(1 1 2 1.5 1.25 1 1 1 1.25 1.5 2 1 1 1 2 1.5 1.25 1 1 1 1.25 1.5 2 1 1)");
    static const Matrix k5p1 = parse_matrix(6, 6, R"(1 3 3 3 3 3 3 1 1 2 1.5 1.25 3 1 1 1 1.25 1.5 3 2 1 1 1 2 3 1.5 1.25 1 1 1 3 1.25 1.5 2 1 1)");
    static const Matrix k6 = parse_matrix(6, 6, R"(1 1 2 1.5 2.25 1.25 1 1 1 2 1.25 2.25 2 1 1 1 2 1.5 1.5 2 1 1 1 2 2.25 1.25 2 1 1 1 1.25 2.25 1.5 2 1 1)");
    static const Matrix k7 = parse_matrix(7, 7, R"(1 1 2 2.5 1.5 2.25 1.25 1 1 1 2 2.25 1.25 2.25 2 1 1 1 1.25 2.25 1.5 2.5 2 1 1 1 2 2.5 1.5 2.25 1.25 1 1 1 2 2.25 1.25 2.25 2 1 1 1 1.25 2.25 1.5 2.5 2 1 1)");
    static const Matrix k7p1 = parse_matrix(8, 8, R"(1 3 3 3 3 3 3 3 3 1 1 2 2.5 1.5 2.25 1.25 3 1 1 1 2 2.25 1.25 2.25 3 2 1 1 1 1.25 2.25 1.5 3 2.5 2 1 1 1 2 2.5 3 1.5 2.25 1.25 1 1 1 2 3 2.25 1.25 2.25 2 1 1 1 3 1.25 2.25 1.5 2.5 2 1 1)");
    static const Matrix k8 = parse_matrix(8, 8, R"(1 1 2 2.5 1.5 2.5 2.25 1.25 1 1 1 2 2.5 1.5 1.25 2.25 2 1 1 1 2 1.25 1.5 2.5 2.5 2 1 1 1 2 2.5 1.5 1.5 2.5 2 1 1 1 2 2.5 2.5 1.5 1.25 2 1 1 1 2 2.25 1.25 1.5 2.5 2 1 1 1 1.25 2.25 2.5 1.5 2.5 2 1 1)");
    static const Matrix k9 = parse_matrix(9, 9, R"(1 1 2 3 2.5 1.5 2.5 2.25 1.25 1 1 1 2 3 2.5 1.5 1.25 2.25 2 1 1 1 2 2.25 1.25 1.5 2.5 3 2 1 1 1 1.25 2.25 2.5 1.5 2.5 3 2 1 1 1 2 3 2.5 1.5 2.5 2.25 1.25 1 1 1 2 3 2.5 1.5 1.25 2.25 2 1 1 1 2 2.25 1.25 1.5 2.5 3 2 1 1 1 1.25 2.25 2.5 1.5 2.5 3 2 1 1)");
    static const Matrix k10 = parse_matrix(10, 10, R"(1 1 2 3 2.5 1.5 2.5 3 2.25 1.25 1 1 1 2 3 2.5 1.5 2.25 1.25 2.25 2 1 1 1 2 3 2.25 1.25 2.25 3 3 2 1 1 1 2 1.25 2.25 1.5 2.5 2.5 3 2 1 1 1 2 3 2.5 1.5 1.5 2.5 3 2 1 1 1 2 3 2.5 2.5 1.5 2.25 1.25 2 1 1 1 2 3 3 2.25 1.25 2.25 3 2 1 1 1 2 2.25 1.25 2.25 1.5 2.5 3 2 1 1 1 1.25 2.25 3 2.5 1.5 2.5 3 2 1 1)");
    static const Matrix k10p2 = parse_matrix(12, 12, R"(1 3 3 3 3 3 3 3 3 3 3 3 3 1 1 2 3 2.5 1.5 2.5 3 2.25 1.25 3 3 1 1 1 2 3 2.5 1.5 2.25 1.25 2.25 3 3 2 1 1 1 2 3 2.25 1.25 2.25 3 3 3 2 1 1 1 2 1.25 2.25 1.5 2.5 3 3 2.5 3 2 1 1 1 2 3 2.5 1.5 3 3 1.5 2.5 3 2 1 1 1 2 3 2.5 3 3 2.5 1.5 2.25 1.25 2 1 1 1 2 3 3 3 2.25 1.25 2.25 3 2 1 1 1 2 3 3 2.25 1.25 2.25 1.5 2.5 3 2 1 1 1 3 3 1.25 2.25 3 2.5 1.5 2.5 3 2 1 1 3 3 3 3 3 3 3 3 3 3 3 1)");
    static const Matrix k14p2 = parse_matrix(16, 16, R"(1.0 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 1.0 1 2 3 3 3 2.5 1.5 2.5 3 3 3 2.25 1.25 3 3 1 1.0 1 2 3 3 3 2.5 1.5 2.5 3 2.25 1.25 2.25 3 3 2 1 1.0 1 2 3 3 3 2.5 1.5 2.25 1.25 2.25 3 3 3 3 2 1 1.0 1 2 3 3 3 2.25 1.25 2.25 3 3 3 3 3 3 2 1 1.0 1 2 3 2.25 1.25 2.25 1.5 2.5 3 3 3 3 3 3 2 1 1.0 1 2 1.25 2.25 3 2.5 1.5 2.5 3 3 2.5 3 3 3 2 1 1.0 1 2 3 3 3 2.5 1.5 3 3 1.5 2.5 3 3 3 2 1 1.0 1 2 3 3 3 2.5 3 3 2.5 1.5 2.5 3 2.25 1.25 2 1 1.0 1 2 3 3 3 3 3 3 2.5 1.5 2.25 1.25 2.25 3 2 1 1.0 1 2 3 3 3 3 3 3 2.25 1.25 2.25 3 3 3 2 1 1.0 1 2 3 3 3 3 2.25 1.25 2.25 1.5 2.5 3 3 3 2 1 1.0 1 2 3 3 2.25 1.25 2.25 3 2.5 1.5 2.5 3 3 3 2 1 1.0 1 3 3 1.25 2.25 3 3 3 2.5 1.5 2.5 3 3 3 2 1 1.0 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 1.0)");
    static const Matrix k16 = parse_matrix(16, 16, R"(1.0 1 2 3 3 3 3 2.5 1.5 2.5 3 3 3 3 2.25 1.25 1 1.0 1 2 3 3 3 3 2.5 1.5 2.5 3 3 2.25 1.25 2.25 2 1 1.0 1 2 3 3 3 3 2.5 1.5 2.5 2.25 1.25 2.25 3 3 2 1 1.0 1 2 3 3 3 3 2.5 1.5 1.25 2.25 3 3 3 3 2 1 1.0 1 2 3 3 3 2.25 1.25 1.5 2.5 3 3 3 3 3 2 1 1.0 1 2 3 2.25 1.25 2.25 2.5 1.5 2.5 3 3 3 3 3 2 1 1.0 1 2 1.25 2.25 3 3 2.5 1.5 2.5 2.5 3 3 3 3 2 1 1.0 1 2 3 3 3 3 2.5 1.5 1.5 2.5 3 3 3 3 2 1 1.0 1 2 3 3 3 3 2.5 2.5 1.5 2.5 3 3 2.25 1.25 2 1 1.0 1 2 3 3 3 3 3 2.5 1.5 2.5 2.25 1.25 2.25 3 2 1 1.0 1 2 3 3 3 3 3 2.5 1.5 1.25 2.25 3 3 3 2 1 1.0 1 2 3 3 3 3 2.25 1.25 1.5 2.5 3 3 3 3 2 1 1.0 1 2 3 3 2.25 1.25 2.25 2.5 1.5 2.5 3 3 3 3 2 1 1.0 1 2 2.25 1.25 2.25 3 3 2.5 1.5 2.5 3 3 3 3 2 1 1.0 1 1.25 2.25 3 3 3 3 2.5 1.5 2.5 3 3 3 3 2 1 1.0)");

    switch (key) {
    case MatrixKey::K4:
        return k4;
    case MatrixKey::K5:
        return k5;
    case MatrixKey::K5P1:
        return k5p1;
    case MatrixKey::K6:
        return k6;
    case MatrixKey::K7:
        return k7;
    case MatrixKey::K7P1:
        return k7p1;
    case MatrixKey::K8:
        return k8;
    case MatrixKey::K9:
        return k9;
    case MatrixKey::K10:
        return k10;
    case MatrixKey::K10P2:
        return k10p2;
    case MatrixKey::K14P2:
        return k14p2;
    case MatrixKey::K16:
        return k16;
    }
    return k10;
}

const Matrix& type_distance_matrix() {
    static const Matrix matrix = parse_matrix(3, 3, R"(0 1 0 1 0 0 0 0 0)");
    return matrix;
}

const JudgmentSet& qwilight_bms_ez_judgments() {
    static const JudgmentSet judgments = parse_judgments(R"(19.231 -19.231 19.231 -19.231 28.8465 -28.8465 100 100
46.154 -46.154 46.154 -46.154 69.231 -69.231 90 100
73.077 -73.077 73.077 -73.077 109.6155 -109.6155 10 70
100 -100 100 -100 150 -150 1 50
126.923 -126.923 126.923 -126.923 x -190.3845 0 30
x -153.846 x -153.846 x x 0 0
x x x x x x x x)");
    return judgments;
}

const std::array<JudgmentSet, 4>& osu_stable_presets() {
    static const std::array<JudgmentSet, 4> presets = {{
        parse_judgments(R"(16.5 -16.5 14.85 -14.85 29.7 -29.7 100 100
64.5 -64.5 58.05 58.05 116.1 -116.1 96.875 100
97.5 -97.5 87.75 -87.75 175.5 -151.5 31.25 66.666666667
127.5 -127.5 114.75 -114.75 229.5 -151.5 15.625 33.333333334
151.5 -151.5 151.5 -151.5 x -151.5 7.8125 16.666666667
x -188.5 x -188.5 x x 0 0
x x x x x x x x)"),
        parse_judgments(R"(16.5 -16.5 14.85 -14.85 29.7 -29.7 100 100
49.5 -49.5 44.55 -44.55 89.1 -89.1 96.875 100
82.5 -82.5 74.25 -74.25 148.5 -136.5 31.25 66.666666667
112.5 -112.5 101.25 -101.25 202.5 -136.5 15.625 33.333333334
136.5 -136.5 136.5 -136.5 x -136.5 7.8125 16.666666667
x -173.5 x -173.5 x x 0 0
x x x x x x x x)"),
        parse_judgments(R"(16.5 -16.5 14.85 -14.85 29.7 -29.7 100 100
40.5 -40.5 36.45 -36.45 72.9 -72.9 96.875 100
73.5 -73.5 66.15 -66.15 132.3 -127.5 31.25 66.666666667
103.5 -103.5 93.15 -93.15 186.3 -127.5 15.625 33.333333334
127.5 -127.5 127.5 -127.5 x -127.5 7.8125 16.666666667
x -164.5 x -164.5 x x 0 0
x x x x x x x x)"),
        parse_judgments(R"(16.5 -16.5 14.85 -14.85 29.7 -29.7 100 100
34.5 -34.5 31.05 -31.05 62.1 -62.1 96.875 100
67.5 -67.5 60.75 -60.75 121.5 -121.5 31.25 66.666666667
97.5 -97.5 87.75 -87.75 175.5 -121.5 15.625 33.333333334
121.5 -121.5 121.5 -121.5 x -121.5 7.8125 16.666666667
x -158.5 x -158.5 x x 0 0
x x x x x x x x)"),
    }};
    return presets;
}

JudgmentSet interpolate_osu_stable_judgments(double od) {
    const auto& presets = osu_stable_presets();
    const std::array<double, 4> od_points = {{0.0, 5.0, 8.0, 10.0}};
    od = std::clamp(od, 0.0, 10.0);
    for (std::size_t i = 0; i < od_points.size(); ++i) {
        if (std::abs(od - od_points[i]) < 0.001) {
            return presets[i];
        }
    }

    std::size_t lower_index = 0;
    std::size_t upper_index = od_points.size() - 1;
    for (std::size_t i = 0; i < od_points.size(); ++i) {
        if (od_points[i] < od) {
            lower_index = i;
        }
        if (od_points[i] > od) {
            upper_index = i;
            break;
        }
    }

    const auto& lower = presets[lower_index];
    const auto& upper = presets[upper_index];
    const double lower_od = od_points[lower_index];
    const double upper_od = od_points[upper_index];
    const double ratio = (upper_od != lower_od) ? ((od - lower_od) / (upper_od - lower_od)) : 0.0;
    JudgmentSet interpolated;
    interpolated.reserve(lower.size());

    auto interp = [ratio](std::optional<double> a, std::optional<double> b) -> std::optional<double> {
        if (!a.has_value() || !b.has_value()) {
            return std::nullopt;
        }
        return *a + ratio * (*b - *a);
    };

    for (std::size_t i = 0; i < lower.size(); ++i) {
        interpolated.push_back(JudgmentDefinition{interp(lower[i].plus_rice, upper[i].plus_rice),
                                                  interp(lower[i].minus_rice, upper[i].minus_rice),
                                                  interp(lower[i].plus_head, upper[i].plus_head),
                                                  interp(lower[i].minus_head, upper[i].minus_head),
                                                  interp(lower[i].plus_tail, upper[i].plus_tail),
                                                  interp(lower[i].minus_tail, upper[i].minus_tail),
                                                  lower[i].score_pct, lower[i].acc_pct});
    }
    return interpolated;
}

JudgmentSet select_judgments(const OsuManiaChart& chart, const ManiaDifficultyOptions& options) {
    if (options.preset == DifficultyPreset::QwilightBmsEz) {
        return qwilight_bms_ez_judgments();
    }
    return interpolate_osu_stable_judgments(chart.overall_difficulty);
}

std::optional<double> judgment_range(const JudgmentDefinition& judgment, JudgmentNoteType type, bool plus) {
    switch (type) {
    case JudgmentNoteType::Rice:
        return plus ? judgment.plus_rice : judgment.minus_rice;
    case JudgmentNoteType::Head:
        return plus ? judgment.plus_head : judgment.minus_head;
    case JudgmentNoteType::Tail:
        return plus ? judgment.plus_tail : judgment.minus_tail;
    }
    return std::nullopt;
}

std::pair<std::optional<double>, std::optional<double>> judgment_result_value(const JudgmentDefinition& judgment) {
    return {judgment.score_pct.has_value() ? judgment.score_pct : std::optional<double>(0.0),
            judgment.acc_pct.has_value() ? judgment.acc_pct : std::optional<double>(0.0)};
}

double matrix_value(const Matrix& matrix, int row, int column, double fallback) {
    if (row < 0 || column < 0) {
        return fallback;
    }
    const auto row_index = static_cast<std::size_t>(row);
    const auto column_index = static_cast<std::size_t>(column);
    if (row_index >= matrix.size() || column_index >= matrix[row_index].size()) {
        return fallback;
    }
    return matrix[row_index][column_index];
}

std::pair<std::optional<double>, std::optional<double>> get_judgment_by_timing(double ms_offset,
                                                                               const JudgmentSet& judgments,
                                                                               JudgmentNoteType note_type,
                                                                               bool use_full_range);
std::pair<std::optional<double>, std::optional<double>> get_judgment_result_typed(double ms_offset,
                                                                                   const JudgmentSet& judgments,
                                                                                   JudgmentNoteType note_type);
std::pair<std::optional<double>, std::optional<double>> get_judgment_for_fds_rds(double timing_offset_ms,
                                                                                  const JudgmentSet& judgments,
                                                                                  JudgmentNoteType note_type);
TimeDeltas calculate_time_deltas(const JudgmentSet& judgments);
void build_line_note_index(const std::vector<DifficultyEvent>& notes,
                           int key_count,
                           std::vector<std::vector<LineNoteInfo>>& line_notes,
                           std::vector<LineNoteInfo>& idx_info,
                           std::vector<std::vector<double>>& line_times);
std::vector<double> compute_ln_tail_weights(const std::vector<DifficultyEvent>& notes,
                                            const std::vector<std::vector<LineNoteInfo>>& line_notes,
                                            const std::vector<LineNoteInfo>& idx_info);
double apply_vibro_nerf(double base_coef, double uniformity_ratio, double nerf_weight);
std::optional<double> ratio_at(double press_time,
                               const LineNoteInfo* note_info,
                               JudgmentNoteType note_type,
                               double delta_seconds,
                               bool use_score,
                               const JudgmentSet& judgments);
std::optional<double> select_ratio(std::optional<double> selected_ratio, std::optional<double> prev_ratio);
void accumulate_pair(double& numer,
                     double& denom,
                     double press_time,
                     const LineNoteInfo* selected_note_info,
                     JudgmentNoteType selected_type,
                     double selected_delta,
                     const LineNoteInfo* prev_note_info,
                     JudgmentNoteType prev_type,
                     double prev_delta,
                     bool use_score,
                     double weight,
                     const JudgmentSet& judgments);
void accumulate_ratio(double& numer,
                      double& denom,
                      double timing_offset_seconds,
                      const JudgmentSet& judgments,
                      JudgmentNoteType note_type,
                      double weight,
                      bool use_score);
NpsDifficultyData calculate_nps_v2_and_distance(const std::vector<DifficultyEvent>& notes,
                                                const Matrix& column_weights,
                                                const Matrix& visual_distances,
                                                const std::vector<double>& ln_tail_weights,
                                                const JudgmentSet& judgments,
                                                const TimeDeltas& deltas);
NoteDifficultyData calculate_note_difficulty(const std::vector<DifficultyEvent>& notes,
                                             int key_count,
                                             const Matrix& column_weights,
                                             const Matrix& visual_distances,
                                             const std::vector<int>& scratch_index_list,
                                             const JudgmentSet& judgments,
                                             const TimeDeltas& deltas);
std::pair<double, double> judge_average_ratio(const JudgmentSet& judgments, JudgmentNoteType note_type);
OsuDifficultyMetrics calculate_from_events(const std::vector<DifficultyEvent>& notes,
                                          double duration_seconds,
                                          int key_count,
                                          const JudgmentSet& judgments,
                                          const Matrix& column_weights,
                                          const Matrix& visual_distances,
                                          const std::vector<int>& scratch_index_list);
std::optional<std::string> canonical_mode_name(std::string_view raw_mode_name);

std::pair<std::optional<double>, std::optional<double>> get_judgment_by_timing(double ms_offset,
                                                                               const JudgmentSet& judgments,
                                                                               JudgmentNoteType note_type,
                                                                               bool use_full_range) {
    if (judgments.empty()) {
        return {100.0, 100.0};
    }

    const double effective_ms_offset = scale_difficulty_timing_offset(ms_offset, note_type);
    for (const auto& judgment : judgments) {
        const auto plus_range = judgment_range(judgment, note_type, true);
        const auto minus_range = judgment_range(judgment, note_type, false);
        if (!plus_range.has_value()) {
            return judgment_result_value(judgment);
        }

        double total_range = std::abs(*plus_range);
        if (use_full_range) {
            if (!minus_range.has_value()) {
                return judgment_result_value(judgment);
            }
            total_range += std::abs(*minus_range);
        }

        if (effective_ms_offset <= total_range) {
            return judgment_result_value(judgment);
        }
    }

    return {std::nullopt, std::nullopt};
}

std::pair<std::optional<double>, std::optional<double>> get_judgment_result_typed(double ms_offset,
                                                                                   const JudgmentSet& judgments,
                                                                                   JudgmentNoteType note_type) {
    const auto positive = get_judgment_by_timing(ms_offset, judgments, note_type, false);
    const auto full = get_judgment_by_timing(ms_offset, judgments, note_type, true);
    if (!positive.first.has_value() && !full.first.has_value()) {
        return {std::nullopt, std::nullopt};
    }
    if (!positive.first.has_value()) {
        return full;
    }
    if (!full.first.has_value()) {
        return positive;
    }

    const double average_score = (*positive.first + *full.first) / 2.0;
    const double average_acc =
        ((positive.second.has_value() ? *positive.second : 0.0) + (full.second.has_value() ? *full.second : 0.0)) /
        2.0;
    return {average_score, average_acc};
}

std::pair<std::optional<double>, std::optional<double>> get_judgment_for_fds_rds(double timing_offset_ms,
                                                                                  const JudgmentSet& judgments,
                                                                                  JudgmentNoteType note_type) {
    if (judgments.empty()) {
        return {100.0, 100.0};
    }

    const double effective_timing_offset_ms = scale_difficulty_timing_offset(timing_offset_ms, note_type);
    const double abs_offset = std::abs(effective_timing_offset_ms);
    const bool is_late = effective_timing_offset_ms >= 0.0;
    for (const auto& judgment : judgments) {
        const auto range = judgment_range(judgment, note_type, is_late);
        if (!range.has_value()) {
            return {judgment.score_pct, judgment.acc_pct};
        }
        if (abs_offset <= std::abs(*range)) {
            return {judgment.score_pct, judgment.acc_pct};
        }
    }

    const auto& last = judgments.back();
    return {last.score_pct, last.acc_pct};
}

TimeDeltas calculate_time_deltas(const JudgmentSet& judgments) {
    TimeDeltas deltas;
    for (const auto& judgment : judgments) {
        if (!judgment.score_pct.has_value() || std::abs(*judgment.score_pct - 100.0) > 0.000001) {
            continue;
        }

        auto update = [](double& target, std::optional<double> source) {
            if (source.has_value()) {
                target = std::max(target, static_cast<double>(static_cast<int>(std::abs(*source))) / 1000.0);
            }
        };

        update(deltas.score_plus_rice, judgment.plus_rice);
        update(deltas.score_minus_rice, judgment.minus_rice);
        update(deltas.score_plus_head, judgment.plus_head);
        update(deltas.score_minus_head, judgment.minus_head);
        update(deltas.score_plus_tail, judgment.plus_tail);
        update(deltas.score_minus_tail, judgment.minus_tail);
    }

    deltas.acc_plus_rice = deltas.score_plus_rice;
    deltas.acc_minus_rice = deltas.score_minus_rice;
    deltas.acc_plus_head = deltas.score_plus_head;
    deltas.acc_minus_head = deltas.score_minus_head;
    deltas.acc_plus_tail = deltas.score_plus_tail;
    deltas.acc_minus_tail = deltas.score_minus_tail;
    return deltas;
}

void build_line_note_index(const std::vector<DifficultyEvent>& notes,
                           int key_count,
                           std::vector<std::vector<LineNoteInfo>>& line_notes,
                           std::vector<LineNoteInfo>& idx_info,
                           std::vector<std::vector<double>>& line_times) {
    line_notes.assign(static_cast<std::size_t>(std::max(key_count, 0) + 1), {});
    idx_info.assign(notes.size(), {});
    line_times.assign(static_cast<std::size_t>(std::max(key_count, 0) + 1), {});

    for (std::size_t idx = 0; idx < notes.size(); ++idx) {
        const auto& note = notes[idx];
        if (note.column < 0 || note.column > key_count) {
            continue;
        }
        line_notes[static_cast<std::size_t>(note.column)].push_back(LineNoteInfo{idx, note.time_seconds, note.type, 0});
    }

    for (int column = 1; column <= key_count; ++column) {
        auto& infos = line_notes[static_cast<std::size_t>(column)];
        auto& times = line_times[static_cast<std::size_t>(column)];
        times.reserve(infos.size());
        for (std::size_t order = 0; order < infos.size(); ++order) {
            infos[order].timing_order = order;
            idx_info[infos[order].idx] = infos[order];
            times.push_back(infos[order].time_seconds);
        }
    }
}

std::vector<double> compute_ln_tail_weights(const std::vector<DifficultyEvent>& notes,
                                            const std::vector<std::vector<LineNoteInfo>>& line_notes,
                                            const std::vector<LineNoteInfo>& idx_info) {
    std::vector<double> weights(notes.size(), 1.0);
    for (std::size_t idx = 0; idx < notes.size(); ++idx) {
        if (notes[idx].type != DifficultyEventType::HoldEnd) {
            continue;
        }

        const auto& info = idx_info[idx];
        if (info.timing_order == 0 || notes[idx].column < 0 ||
            static_cast<std::size_t>(notes[idx].column) >= line_notes.size()) {
            continue;
        }

        const auto& column_notes = line_notes[static_cast<std::size_t>(notes[idx].column)];
        const double previous_time = column_notes[info.timing_order - 1].time_seconds;
        const double ln_duration_ms = (notes[idx].time_seconds - previous_time) * 1000.0;
        const double raw_weight = ln_duration_ms / kTimeWindowMs;
        weights[idx] = std::clamp(raw_weight, 0.05, 1.0);
    }
    return weights;
}

double apply_vibro_nerf(double base_coef, double uniformity_ratio, double nerf_weight) {
    return std::max(base_coef - (base_coef * uniformity_ratio * nerf_weight), 0.0);
}

std::optional<double> ratio_at(double press_time,
                               const LineNoteInfo* note_info,
                               JudgmentNoteType note_type,
                               double delta_seconds,
                               bool use_score,
                               const JudgmentSet& judgments) {
    if (note_info == nullptr) {
        return std::nullopt;
    }

    const double offset_seconds = (press_time + delta_seconds) - note_info->time_seconds;
    const auto result = get_judgment_for_fds_rds(offset_seconds * 1000.0, judgments, note_type);
    return use_score ? result.first : result.second;
}

std::optional<double> select_ratio(std::optional<double> selected_ratio, std::optional<double> prev_ratio) {
    if (selected_ratio.has_value() && prev_ratio.has_value() && *prev_ratio > 0.0) {
        return std::max(*selected_ratio, *prev_ratio);
    }
    if (selected_ratio.has_value()) {
        return selected_ratio;
    }
    if (prev_ratio.has_value() && *prev_ratio > 0.0) {
        return prev_ratio;
    }
    return std::nullopt;
}

void accumulate_pair(double& numer,
                     double& denom,
                     double press_time,
                     const LineNoteInfo* selected_note_info,
                     JudgmentNoteType selected_type,
                     double selected_delta,
                     const LineNoteInfo* prev_note_info,
                     JudgmentNoteType prev_type,
                     double prev_delta,
                     bool use_score,
                     double weight,
                     const JudgmentSet& judgments) {
    const auto selected_ratio =
        ratio_at(press_time, selected_note_info, selected_type, selected_delta, use_score, judgments);
    const auto prev_ratio = ratio_at(press_time, prev_note_info, prev_type, prev_delta, use_score, judgments);
    const auto ratio = select_ratio(selected_ratio, prev_ratio);
    if (!ratio.has_value()) {
        return;
    }

    numer += *ratio * weight;
    denom += weight;
}

void accumulate_ratio(double& numer,
                      double& denom,
                      double timing_offset_seconds,
                      const JudgmentSet& judgments,
                      JudgmentNoteType note_type,
                      double weight,
                      bool use_score) {
    const auto result = get_judgment_for_fds_rds(timing_offset_seconds * 1000.0, judgments, note_type);
    const auto ratio = use_score ? result.first : result.second;
    if (!ratio.has_value()) {
        return;
    }

    numer += *ratio * weight;
    denom += weight;
}

NpsDifficultyData calculate_nps_v2_and_distance(const std::vector<DifficultyEvent>& notes,
                                                const Matrix& column_weights,
                                                const Matrix& visual_distances,
                                                const std::vector<double>& ln_tail_weights,
                                                const JudgmentSet& judgments,
                                                const TimeDeltas& deltas) {
    NpsDifficultyData data;
    const std::size_t note_count = notes.size();
    data.nps.assign(note_count, 0);
    data.nps_v2.assign(note_count, 0.0);
    data.distance_difficulty.assign(note_count, 0.0);
    data.jack_nps_v2.assign(note_count, 0.0);
    data.same_line_nps_v2.assign(note_count, 0.0);
    data.minimum_distance_sum.assign(note_count, 0.0);
    data.same_line_minimum_distance_sum.assign(note_count, 0.0);
    data.jack_interval.assign(note_count, kTimeWindowMs);
    data.jack_score_uniformity.assign(note_count, 100.0);
    data.jack_acc_uniformity.assign(note_count, 100.0);
    if (note_count == 0) {
        return data;
    }

    std::vector<double> nps_v2_nerf_values(note_count, 0.0);
    std::vector<std::size_t> window_start_indices(note_count, 0);
    std::vector<std::size_t> window_finish_indices(note_count, 0);
    std::optional<double> previous_note_time;

    std::size_t window_start_index = 0;
    std::size_t window_finish_index = 0;
    const bool has_uniformity = !judgments.empty();

    for (std::size_t i = 0; i < note_count; ++i) {
        const auto& note = notes[i];
        const double time_seconds = note.time_seconds;
        const int col_i = std::max(note.column - 1, 0);

        std::vector<std::size_t> same_col_note_list;
        std::vector<std::size_t> half_same_col_note_list;
        int nps = 0;
        double weighted_count = 0.0;
        double jack_nps_v2 = 0.0;
        double same_line_nps_v2 = 0.0;
        double jack_interval_seconds = kTimeWindowMs / 1000.0;
        double jack_score_uniformity = 100.0;
        double jack_acc_uniformity = 100.0;
        double half_jack_score_uniformity = 100.0;
        double half_jack_acc_uniformity = 100.0;
        double minimum_plus_delta_ms = kTimeWindowMs;
        double minimum_minus_delta_ms = kTimeWindowMs;

        if (!previous_note_time.has_value() || *previous_note_time != time_seconds) {
            while (window_start_index < window_finish_index &&
                   notes[window_start_index].time_seconds <= (time_seconds - (kTimeWindowMs / 1000.0))) {
                ++window_start_index;
            }
            while (window_finish_index < note_count &&
                   notes[window_finish_index].time_seconds < (time_seconds + (kTimeWindowMs / 1000.0))) {
                ++window_finish_index;
            }
            previous_note_time = time_seconds;
        }

        for (std::size_t j = window_start_index; j < window_finish_index; ++j) {
            const auto& window_note = notes[j];
            const double dt_ms = (time_seconds - window_note.time_seconds) * 1000.0;
            const double abs_dt_ms = std::abs(dt_ms);
            if (-kHalfWindowMs < dt_ms && dt_ms <= kHalfWindowMs) {
                ++nps;
            }
            if (abs_dt_ms >= kTimeWindowMs) {
                continue;
            }

            const double time_weight = 1.0 - (abs_dt_ms / kTimeWindowMs);
            const int col_j = std::max(window_note.column - 1, 0);
            const double col_weight = matrix_value(column_weights, col_i, col_j, 1.0);
            double tail_weight = 1.0;
            if (j != i && window_note.type == DifficultyEventType::HoldEnd && j < ln_tail_weights.size()) {
                tail_weight = ln_tail_weights[j];
            }

            weighted_count += col_weight * tail_weight * time_weight;
            if (note.column == window_note.column) {
                same_line_nps_v2 += col_weight * tail_weight * time_weight;
            }

            if (note.column == window_note.column && window_note.type != DifficultyEventType::HoldEnd) {
                same_col_note_list.push_back(j);
                if (abs_dt_ms < (kTimeWindowMs * 0.5)) {
                    half_same_col_note_list.push_back(j);
                }
                jack_nps_v2 += time_weight;
            }

            if (dt_ms > 0.0) {
                minimum_plus_delta_ms = std::min(minimum_plus_delta_ms, dt_ms);
            } else if (dt_ms < 0.0) {
                minimum_minus_delta_ms = std::min(minimum_minus_delta_ms, abs_dt_ms);
            }
        }

        if (has_uniformity && same_col_note_list.size() >= 2) {
            const double first_time = notes[same_col_note_list.front()].time_seconds;
            const double last_time = notes[same_col_note_list.back()].time_seconds;
            jack_interval_seconds = (same_col_note_list.size() > 1)
                                        ? ((last_time - first_time) / static_cast<double>(same_col_note_list.size() - 1))
                                        : 0.0;
            jack_interval_seconds = std::max(jack_interval_seconds, 0.0);

            double jack_score_sum = 0.0;
            double jack_acc_sum = 0.0;
            for (std::size_t jack_step = 0; jack_step < same_col_note_list.size(); ++jack_step) {
                const auto target_index = same_col_note_list[jack_step];
                const auto& target_note = notes[target_index];
                const bool is_head = target_note.type == DifficultyEventType::HoldStart;
                const auto note_kind = is_head ? JudgmentNoteType::Head : JudgmentNoteType::Rice;
                const double score_plus = is_head ? deltas.score_plus_head : deltas.score_plus_rice;
                const double score_minus = is_head ? deltas.score_minus_head : deltas.score_minus_rice;
                const double acc_plus = is_head ? deltas.acc_plus_head : deltas.acc_plus_rice;
                const double acc_minus = is_head ? deltas.acc_minus_head : deltas.acc_minus_rice;
                const double target_timing = first_time + (static_cast<double>(jack_step) * jack_interval_seconds);

                for (double offset : {0.0, score_plus, -score_minus}) {
                    const double offset_ms = std::abs((target_note.time_seconds - (target_timing + offset)) * 1000.0);
                    const auto result = get_judgment_result_typed(offset_ms, judgments, note_kind);
                    if (result.first.has_value()) {
                        jack_score_sum += *result.first;
                    }
                }
                for (double offset : {0.0, acc_plus, -acc_minus}) {
                    const double offset_ms = std::abs((target_note.time_seconds - (target_timing + offset)) * 1000.0);
                    const auto result = get_judgment_result_typed(offset_ms, judgments, note_kind);
                    if (result.second.has_value()) {
                        jack_acc_sum += *result.second;
                    }
                }
            }

            jack_score_uniformity =
                std::min(std::max((jack_score_sum / 6.0) - (static_cast<double>(same_col_note_list.size()) * 50.0) + 100.0, 0.0), 100.0);
            jack_acc_uniformity =
                std::min(std::max((jack_acc_sum / 6.0) - (static_cast<double>(same_col_note_list.size()) * 50.0) + 100.0, 0.0), 100.0);
        }

        if (has_uniformity && half_same_col_note_list.size() >= 3) {
            const double first_time = notes[half_same_col_note_list.front()].time_seconds;
            const double last_time = notes[half_same_col_note_list.back()].time_seconds;
            double half_jack_interval_seconds = (half_same_col_note_list.size() > 1)
                                                    ? ((last_time - first_time) /
                                                       static_cast<double>(half_same_col_note_list.size() - 1))
                                                    : 0.0;
            half_jack_interval_seconds = std::max(half_jack_interval_seconds, 0.0);

            double half_jack_score_sum = 0.0;
            double half_jack_acc_sum = 0.0;
            for (std::size_t jack_step = 0; jack_step < half_same_col_note_list.size(); ++jack_step) {
                const auto target_index = half_same_col_note_list[jack_step];
                const auto& target_note = notes[target_index];
                const bool is_head = target_note.type == DifficultyEventType::HoldStart;
                const auto note_kind = is_head ? JudgmentNoteType::Head : JudgmentNoteType::Rice;
                const double score_plus = is_head ? deltas.score_plus_head : deltas.score_plus_rice;
                const double score_minus = is_head ? deltas.score_minus_head : deltas.score_minus_rice;
                const double acc_plus = is_head ? deltas.acc_plus_head : deltas.acc_plus_rice;
                const double acc_minus = is_head ? deltas.acc_minus_head : deltas.acc_minus_rice;
                const double target_timing = first_time + (static_cast<double>(jack_step) * half_jack_interval_seconds);

                for (double offset : {0.0, score_plus, -score_minus}) {
                    const double offset_ms = std::abs((target_note.time_seconds - (target_timing + offset)) * 1000.0);
                    const auto result = get_judgment_result_typed(offset_ms, judgments, note_kind);
                    if (result.first.has_value()) {
                        half_jack_score_sum += *result.first;
                    }
                }
                for (double offset : {0.0, acc_plus, -acc_minus}) {
                    const double offset_ms = std::abs((target_note.time_seconds - (target_timing + offset)) * 1000.0);
                    const auto result = get_judgment_result_typed(offset_ms, judgments, note_kind);
                    if (result.second.has_value()) {
                        half_jack_acc_sum += *result.second;
                    }
                }
            }

            half_jack_score_uniformity = std::min(
                std::max((half_jack_score_sum / 3.0) - (static_cast<double>(half_same_col_note_list.size()) * 100.0) + 100.0, 0.0),
                100.0);
            half_jack_acc_uniformity = std::min(
                std::max((half_jack_acc_sum / 3.0) - (static_cast<double>(half_same_col_note_list.size()) * 100.0) + 100.0, 0.0),
                100.0);

            if (jack_score_uniformity < half_jack_score_uniformity) {
                jack_interval_seconds = half_jack_interval_seconds;
            }
        }

        jack_score_uniformity = std::max(jack_score_uniformity, half_jack_score_uniformity);
        jack_acc_uniformity = std::max(jack_acc_uniformity, half_jack_acc_uniformity);

        data.nps[i] = nps;
        data.jack_nps_v2[i] = jack_nps_v2;
        data.jack_interval[i] = jack_interval_seconds * 1000.0;
        data.jack_score_uniformity[i] = jack_score_uniformity;
        data.jack_acc_uniformity[i] = jack_acc_uniformity;

        double nps_v2_nerf = 0.0;
        if (has_uniformity && jack_nps_v2 > kVibroNpsCondition && jack_nps_v2 > 0.0) {
            const double uniformity_ratio = std::clamp(jack_score_uniformity, 0.0, 100.0) / 100.0;
            nps_v2_nerf = ((jack_nps_v2 - kVibroNpsCondition) / jack_nps_v2) * uniformity_ratio;
            nps_v2_nerf = std::clamp(nps_v2_nerf, 0.0, 1.0) * kVibroNpsNerfWeight;
            data.same_line_nps_v2[i] = same_line_nps_v2 * (1.0 - nps_v2_nerf);
            data.nps_v2[i] = weighted_count * (1.0 - nps_v2_nerf);
        } else {
            data.same_line_nps_v2[i] = same_line_nps_v2;
            data.nps_v2[i] = weighted_count;
        }
        nps_v2_nerf_values[i] = nps_v2_nerf;
        window_start_indices[i] = window_start_index;
        window_finish_indices[i] = window_finish_index;

        const double minimum_delta_ms = std::max(std::min(minimum_plus_delta_ms, minimum_minus_delta_ms), 10.0);
        double minimum_distance = 0.0;
        for (std::size_t j = window_start_index; j < window_finish_index; ++j) {
            if (j == i) {
                continue;
            }

            const auto& other_note = notes[j];
            const double dt_abs_ms = std::abs((time_seconds - other_note.time_seconds) * 1000.0);
            const int col_j = std::max(other_note.column - 1, 0);
            const double visual_distance = matrix_value(visual_distances, col_i, col_j, 0.0);
            const double type_distance =
                matrix_value(type_distance_matrix(),
                             note_type_index(note.type),
                             note_type_index(other_note.type),
                             note_type_index(note.type) == note_type_index(other_note.type) ? 0.0 : 1.0);
            const double denom = minimum_delta_ms > 0.0 ? minimum_delta_ms : dt_abs_ms;
            const double timing_distance = std::min(dt_abs_ms / denom, 3.0) + (dt_abs_ms / kTimeWindowMs);
            const double total_distance = visual_distance + type_distance + timing_distance;
            minimum_distance = (minimum_distance == 0.0) ? total_distance : std::min(minimum_distance, total_distance);
        }
        data.distance_difficulty[i] = std::max(minimum_distance, 1.0);
    }

    for (std::size_t i = 0; i < note_count; ++i) {
        const auto& note = notes[i];
        double minimum_distance_sum = 0.0;
        double same_line_minimum_distance_sum = 0.0;
        const std::size_t start = window_start_indices[i];
        const std::size_t finish = window_finish_indices[i];
        const double nps_v2_nerf = nps_v2_nerf_values[i];
        for (std::size_t j = start; j < finish; ++j) {
            const auto& other_note = notes[j];
            const double abs_dt_ms = std::abs((note.time_seconds - other_note.time_seconds) * 1000.0);
            const double time_weight = 1.0 - (abs_dt_ms / kTimeWindowMs);
            double tail_weight = 1.0;
            if (j != i && other_note.type == DifficultyEventType::HoldEnd && j < ln_tail_weights.size()) {
                tail_weight = ln_tail_weights[j];
            }

            const double weighted_distance = data.distance_difficulty[j] * tail_weight * time_weight;
            minimum_distance_sum += weighted_distance;
            if (note.column == other_note.column) {
                same_line_minimum_distance_sum += weighted_distance;
            }
        }

        data.minimum_distance_sum[i] = minimum_distance_sum * (1.0 - nps_v2_nerf);
        data.same_line_minimum_distance_sum[i] = same_line_minimum_distance_sum * (1.0 - nps_v2_nerf);
    }

    return data;
}

NoteDifficultyData calculate_note_difficulty(const std::vector<DifficultyEvent>& notes,
                                             int key_count,
                                             const Matrix& column_weights,
                                             const Matrix& visual_distances,
                                             const std::vector<int>& scratch_index_list,
                                             const JudgmentSet& judgments,
                                             const TimeDeltas& deltas) {
    NoteDifficultyData data;
    const std::size_t note_count = notes.size();
    data.nps.assign(note_count, 0);
    data.nps_v2.assign(note_count, 0.0);
    data.same_line_nps_v2.assign(note_count, 0.0);
    data.minimum_distance_sum.assign(note_count, 0.0);
    data.same_line_minimum_distance_sum.assign(note_count, 0.0);
    data.note_jack_diff_score.assign(note_count, 0.0);
    data.note_jack_diff_acc.assign(note_count, 0.0);
    data.fds.assign(note_count, 100.0);
    data.fda.assign(note_count, 100.0);
    data.rds.assign(note_count, 100.0);
    data.rda.assign(note_count, 100.0);
    data.lfds.assign(note_count, 100.0);
    data.lfda.assign(note_count, 100.0);
    data.lrds.assign(note_count, 100.0);
    data.lrda.assign(note_count, 100.0);
    data.ldb.assign(note_count, 0.0);
    data.ldbd.assign(note_count, 0.0);
    data.vrs.assign(note_count, 1.0);
    data.vra.assign(note_count, 1.0);
    if (note_count == 0) {
        return data;
    }

    std::vector<std::vector<LineNoteInfo>> line_notes;
    std::vector<LineNoteInfo> idx_info;
    std::vector<std::vector<double>> line_times;
    build_line_note_index(notes, key_count, line_notes, idx_info, line_times);
    const auto ln_tail_weights = compute_ln_tail_weights(notes, line_notes, idx_info);
    const auto nps_data =
        calculate_nps_v2_and_distance(notes, column_weights, visual_distances, ln_tail_weights, judgments, deltas);

    data.nps = nps_data.nps;
    data.nps_v2 = nps_data.nps_v2;
    data.same_line_nps_v2 = nps_data.same_line_nps_v2;
    data.minimum_distance_sum = nps_data.minimum_distance_sum;
    data.same_line_minimum_distance_sum = nps_data.same_line_minimum_distance_sum;

    struct ColumnState {
        std::optional<double> last_time_ms;
        DifficultyEventType last_type = DifficultyEventType::Rice;
        std::array<double, 4> accumulated = {{0.0, 0.0, 0.0, 0.0}};
    };

    std::vector<ColumnState> column_states(static_cast<std::size_t>(key_count + 1));
    std::vector<bool> scratch_lines(static_cast<std::size_t>(key_count + 1), false);
    for (int index : scratch_index_list) {
        if (index + 1 >= 1 && index + 1 <= key_count) {
            scratch_lines[static_cast<std::size_t>(index + 1)] = true;
        }
    }

    std::vector<std::vector<std::optional<double>>> prev_offsets_by_idx(note_count);
    std::vector<double> prev_ln_length_by_idx(note_count, 0.0);
    std::vector<double> prev_ln_space_by_idx(note_count, 0.0);
    std::vector<std::optional<double>> fds_n_by_idx(note_count);
    std::vector<std::optional<double>> fds_d_by_idx(note_count);
    std::vector<std::optional<double>> fda_n_by_idx(note_count);
    std::vector<std::optional<double>> fda_d_by_idx(note_count);
    std::vector<std::optional<double>> rds_n_by_idx(note_count);
    std::vector<std::optional<double>> rds_d_by_idx(note_count);
    std::vector<std::optional<double>> rda_n_by_idx(note_count);
    std::vector<std::optional<double>> rda_d_by_idx(note_count);

    for (std::size_t i = 0; i < note_count; ++i) {
        const auto& current_note = notes[i];
        const double time_seconds = current_note.time_seconds;
        const double time_ms = time_seconds * 1000.0;
        const int current_line = current_note.column;
        const int col_i = std::max(current_line - 1, 0);

        auto& state = column_states[static_cast<std::size_t>(current_line)];
        std::array<double, 4> jack_values = {{0.0, 0.0, 0.0, 0.0}};
        if (state.last_time_ms.has_value()) {
            const double actual_interval = time_ms - *state.last_time_ms;
            const bool use_ln_half_interval = is_hold_boundary(state.last_type);
            const double interval_multiplier =
                scratch_lines[static_cast<std::size_t>(current_line)] ? 0.5 : 1.0;
            for (std::size_t jack_index = 0; jack_index < kJackBaseIntervalsMs.size(); ++jack_index) {
                const double base_interval =
                    (use_ln_half_interval ? kJackLnIntervalsMs[jack_index] : kJackBaseIntervalsMs[jack_index]) *
                    interval_multiplier;
                if (actual_interval < base_interval) {
                    state.accumulated[jack_index] += (base_interval - actual_interval);
                } else {
                    state.accumulated[jack_index] =
                        std::max(0.0, state.accumulated[jack_index] - (actual_interval - base_interval));
                }
                jack_values[jack_index] = state.accumulated[jack_index];
            }
        }
        state.last_time_ms = time_ms;
        state.last_type = current_note.type;

        const auto jack_note_type = note_type_for_event(current_note.type);
        const double jack_score_uniformity =
            i < nps_data.jack_score_uniformity.size() ? nps_data.jack_score_uniformity[i] : 100.0;
        const double jack_acc_uniformity =
            i < nps_data.jack_acc_uniformity.size() ? nps_data.jack_acc_uniformity[i] : 100.0;
        const double score_uniformity_ratio = std::clamp(jack_score_uniformity, 0.0, 100.0) / 100.0;
        const double acc_uniformity_ratio = std::clamp(jack_acc_uniformity, 0.0, 100.0) / 100.0;
        double note_score_loss = 0.0;
        double note_acc_loss = 0.0;
        for (std::size_t jack_index = 0; jack_index < jack_values.size(); ++jack_index) {
            if (jack_values[jack_index] <= 0.0) {
                continue;
            }

            const double base_coef = kJackCoefficients[jack_index];
            const double score_coef =
                apply_vibro_nerf(base_coef, score_uniformity_ratio, kVibroJackNerfWeights[jack_index]);
            const double acc_coef =
                apply_vibro_nerf(base_coef, acc_uniformity_ratio, kVibroJackNerfWeights[jack_index]);
            const auto result = get_judgment_result_typed(jack_values[jack_index], judgments, jack_note_type);
            if (!result.first.has_value() || !result.second.has_value()) {
                continue;
            }

            note_score_loss += score_coef * (100.0 - *result.first);
            note_acc_loss += acc_coef * (100.0 - *result.second);
        }
        data.note_jack_diff_score[i] = round_nine(note_score_loss);
        data.note_jack_diff_acc[i] = round_nine(note_acc_loss);

        const LineNoteInfo* prev_note_info = nullptr;
        const LineNoteInfo* next_note_info = nullptr;
        const auto& current_info = idx_info[i];
        const auto& current_line_notes = line_notes[static_cast<std::size_t>(current_line)];
        if (current_info.timing_order > 0) {
            prev_note_info = &current_line_notes[current_info.timing_order - 1];
        }
        if (current_info.timing_order + 1 < current_line_notes.size()) {
            next_note_info = &current_line_notes[current_info.timing_order + 1];
        }

        std::vector<std::optional<double>> prev_offsets(static_cast<std::size_t>(key_count + 1));
        double prev_ln_length = 0.0;
        double prev_ln_space = 0.0;
        if (prev_note_info != nullptr) {
            const std::size_t prev_idx = prev_note_info->idx;
            if (!prev_offsets_by_idx[prev_idx].empty()) {
                prev_offsets = prev_offsets_by_idx[prev_idx];
            }
            prev_ln_length = prev_ln_length_by_idx[prev_idx];
            prev_ln_space = std::min(prev_ln_space_by_idx[prev_idx], 75.0 / 1000.0);
            if (current_note.type == DifficultyEventType::HoldEnd) {
                prev_ln_length = time_seconds - prev_note_info->time_seconds;
                if (next_note_info != nullptr) {
                    prev_ln_space =
                        std::min(std::max(0.0, next_note_info->time_seconds - time_seconds), 75.0 / 1000.0);
                }
            }
        }

        double min_ln_length = 0.0;
        double min_ln_space = 0.0;
        double ldb = 0.0;
        double ldbd = 0.0;
        const double current_weight = matrix_value(column_weights, col_i, col_i, 1.0);
        double fds_n = 3.0 * 100.0 * current_weight;
        double fds_d = 3.0 * current_weight;
        double fda_n = 3.0 * 100.0 * current_weight;
        double fda_d = 3.0 * current_weight;
        double rds_n = 3.0 * 100.0 * current_weight;
        double rds_d = 3.0 * current_weight;
        double rda_n = 3.0 * 100.0 * current_weight;
        double rda_d = 3.0 * current_weight;
        double lfds_n = 3.0 * 100.0 * current_weight;
        double lfds_d = 3.0 * current_weight;
        double lfda_n = 3.0 * 100.0 * current_weight;
        double lfda_d = 3.0 * current_weight;
        double lrds_n = 3.0 * 100.0 * current_weight;
        double lrds_d = 3.0 * current_weight;
        double lrda_n = 3.0 * 100.0 * current_weight;
        double lrda_d = 3.0 * current_weight;
        double vrs_n = 0.0;
        double vrs_d = 0.0;
        double vra_n = 0.0;
        double vra_d = 0.0;

        const bool skip_fds_rds = current_note.type == DifficultyEventType::HoldEnd;
        if (skip_fds_rds && prev_note_info != nullptr) {
            const std::size_t prev_idx = prev_note_info->idx;
            if (fds_n_by_idx[prev_idx].has_value()) {
                fds_n = *fds_n_by_idx[prev_idx];
                fds_d = *fds_d_by_idx[prev_idx];
                fda_n = *fda_n_by_idx[prev_idx];
                fda_d = *fda_d_by_idx[prev_idx];
                rds_n = *rds_n_by_idx[prev_idx];
                rds_d = *rds_d_by_idx[prev_idx];
                rda_n = *rda_n_by_idx[prev_idx];
                rda_d = *rda_d_by_idx[prev_idx];
            }
        }

        const double current_jack_interval =
            (i < nps_data.jack_interval.size() && nps_data.jack_interval[i] > 0.0) ? nps_data.jack_interval[i] : kTimeWindowMs;
        for (int direction = 0; direction < 2; ++direction) {
            double selected_press_time = time_seconds;
            double selected_release_time = time_seconds;
            const int line_start = (direction == 0) ? (current_line - 1) : (current_line + 1);
            const int line_end = (direction == 0) ? 0 : (key_count + 1);
            const int line_step = (direction == 0) ? -1 : 1;
            for (int selected_line = line_start; selected_line != line_end; selected_line += line_step) {
                if (selected_line < 1 || selected_line > key_count) {
                    continue;
                }

                const auto& selected_line_notes = line_notes[static_cast<std::size_t>(selected_line)];
                if (selected_line_notes.empty()) {
                    continue;
                }
                const auto& selected_line_times = line_times[static_cast<std::size_t>(selected_line)];
                const auto pos_it = std::lower_bound(selected_line_times.begin(), selected_line_times.end(), time_seconds);
                const std::size_t pos = static_cast<std::size_t>(std::distance(selected_line_times.begin(), pos_it));

                const LineNoteInfo* selected_note_info = nullptr;
                const LineNoteInfo* selected_tail_info = nullptr;
                const LineNoteInfo* selected_head_info = nullptr;
                const LineNoteInfo* prev_selected_note_info = nullptr;

                if (pos < selected_line_notes.size()) {
                    const auto* candidate = &selected_line_notes[pos];
                    if (candidate->type == DifficultyEventType::HoldEnd) {
                        selected_tail_info = candidate;
                        if (pos > 0) {
                            selected_head_info = &selected_line_notes[pos - 1];
                        }
                        if (pos + 1 < selected_line_notes.size()) {
                            const auto* next_candidate = &selected_line_notes[pos + 1];
                            if (next_candidate->type != DifficultyEventType::HoldEnd) {
                                selected_note_info = next_candidate;
                            }
                        }
                    } else {
                        selected_note_info = candidate;
                    }
                }

                if (pos > 0) {
                    const auto* prev_candidate = &selected_line_notes[pos - 1];
                    if (prev_candidate->type == DifficultyEventType::HoldEnd) {
                        if (pos >= 2) {
                            prev_selected_note_info = &selected_line_notes[pos - 2];
                        }
                    } else {
                        prev_selected_note_info = prev_candidate;
                    }
                }

                if (selected_note_info == nullptr && selected_tail_info == nullptr && prev_selected_note_info == nullptr) {
                    continue;
                }

                const double selected_weight = matrix_value(column_weights, col_i, selected_line - 1, 1.0);
                const LineNoteInfo* closest_note_info = nullptr;
                const LineNoteInfo* next_candidate = (pos < selected_line_notes.size()) ? &selected_line_notes[pos] : nullptr;
                const LineNoteInfo* prev_candidate = (pos > 0) ? &selected_line_notes[pos - 1] : nullptr;
                if (next_candidate != nullptr && prev_candidate != nullptr) {
                    closest_note_info =
                        (std::abs(next_candidate->time_seconds - time_seconds) < std::abs(time_seconds - prev_candidate->time_seconds))
                            ? next_candidate
                            : prev_candidate;
                } else {
                    closest_note_info = (next_candidate != nullptr) ? next_candidate : prev_candidate;
                }

                if (closest_note_info != nullptr) {
                    const std::size_t closest_idx = closest_note_info->idx;
                    const double closest_jack_interval =
                        (closest_idx < nps_data.jack_interval.size() && nps_data.jack_interval[closest_idx] > 0.0)
                            ? nps_data.jack_interval[closest_idx]
                            : kTimeWindowMs;
                    const double closest_score_uniformity =
                        (closest_idx < nps_data.jack_score_uniformity.size()) ? nps_data.jack_score_uniformity[closest_idx] : 100.0;
                    const double closest_acc_uniformity =
                        (closest_idx < nps_data.jack_acc_uniformity.size()) ? nps_data.jack_acc_uniformity[closest_idx] : 100.0;
                    const double jack_interval_ratio_diff =
                        (kTimeWindowMs / current_jack_interval) - (kTimeWindowMs / closest_jack_interval);
                    const double jack_interval_relation = std::max(1.0 - std::abs(jack_interval_ratio_diff), 0.0);
                    const double relation_score_rate =
                        (jack_score_uniformity / 100.0) * (closest_score_uniformity / 100.0) * jack_interval_relation;
                    const double relation_acc_rate =
                        (jack_acc_uniformity / 100.0) * (closest_acc_uniformity / 100.0) * jack_interval_relation;
                    const double relation_time_delta = std::abs(time_seconds - closest_note_info->time_seconds) * 1000.0;
                    const double relation_time_weight = std::max((kTimeWindowMs - relation_time_delta) / kTimeWindowMs, 0.0);
                    vrs_n += selected_weight * relation_score_rate * relation_time_weight;
                    vrs_d += selected_weight * relation_time_weight;
                    vra_n += selected_weight * relation_acc_rate * relation_time_weight;
                    vra_d += selected_weight * relation_time_weight;
                }

                auto note_type_and_deltas =
                    [&deltas](const LineNoteInfo* info)
                    -> std::tuple<JudgmentNoteType, double, double, double, double> {
                    if (info != nullptr && info->type == DifficultyEventType::HoldStart) {
                        return {JudgmentNoteType::Head,
                                deltas.score_plus_head,
                                deltas.score_minus_head,
                                deltas.acc_plus_head,
                                deltas.acc_minus_head};
                    }
                    return {JudgmentNoteType::Rice,
                            deltas.score_plus_rice,
                            deltas.score_minus_rice,
                            deltas.acc_plus_rice,
                            deltas.acc_minus_rice};
                };

                const auto [selected_type, selected_sp, selected_sm, selected_ap, selected_am] =
                    note_type_and_deltas(selected_note_info);
                const auto [prev_type, prev_sp, prev_sm, prev_ap, prev_am] = note_type_and_deltas(prev_selected_note_info);

                if (!skip_fds_rds && (selected_note_info != nullptr || prev_selected_note_info != nullptr)) {
                    accumulate_pair(fds_n, fds_d, selected_press_time, selected_note_info, selected_type, 0.0,
                                    prev_selected_note_info, prev_type, 0.0, true, selected_weight, judgments);
                    accumulate_pair(fds_n, fds_d, selected_press_time, selected_note_info, selected_type, selected_sp,
                                    prev_selected_note_info, prev_type, prev_sp, true, selected_weight, judgments);
                    accumulate_pair(fds_n, fds_d, selected_press_time, selected_note_info, selected_type, -selected_sm,
                                    prev_selected_note_info, prev_type, -prev_sm, true, selected_weight, judgments);
                    accumulate_pair(fda_n, fda_d, selected_press_time, selected_note_info, selected_type, 0.0,
                                    prev_selected_note_info, prev_type, 0.0, false, selected_weight, judgments);
                    accumulate_pair(fda_n, fda_d, selected_press_time, selected_note_info, selected_type, selected_ap,
                                    prev_selected_note_info, prev_type, prev_ap, false, selected_weight, judgments);
                    accumulate_pair(fda_n, fda_d, selected_press_time, selected_note_info, selected_type, -selected_am,
                                    prev_selected_note_info, prev_type, -prev_am, false, selected_weight, judgments);

                    if (prev_offsets[static_cast<std::size_t>(selected_line)].has_value()) {
                        const double ref_time = time_seconds + *prev_offsets[static_cast<std::size_t>(selected_line)];
                        accumulate_pair(rds_n, rds_d, ref_time, selected_note_info, selected_type, 0.0,
                                        prev_selected_note_info, prev_type, 0.0, true, selected_weight, judgments);
                        accumulate_pair(rds_n, rds_d, ref_time, selected_note_info, selected_type, selected_sp,
                                        prev_selected_note_info, prev_type, prev_sp, true, selected_weight, judgments);
                        accumulate_pair(rds_n, rds_d, ref_time, selected_note_info, selected_type, -selected_sm,
                                        prev_selected_note_info, prev_type, -prev_sm, true, selected_weight, judgments);
                        accumulate_pair(rda_n, rda_d, ref_time, selected_note_info, selected_type, 0.0,
                                        prev_selected_note_info, prev_type, 0.0, false, selected_weight, judgments);
                        accumulate_pair(rda_n, rda_d, ref_time, selected_note_info, selected_type, selected_ap,
                                        prev_selected_note_info, prev_type, prev_ap, false, selected_weight, judgments);
                        accumulate_pair(rda_n, rda_d, ref_time, selected_note_info, selected_type, -selected_am,
                                        prev_selected_note_info, prev_type, -prev_am, false, selected_weight, judgments);
                    }
                }

                if (current_note.type != DifficultyEventType::HoldEnd &&
                    (selected_note_info != nullptr || prev_selected_note_info != nullptr)) {
                    const auto selected_press_result =
                        ratio_at(time_seconds, selected_note_info, selected_type, selected_sp, true, judgments);
                    const auto prev_press_result =
                        ratio_at(time_seconds, prev_selected_note_info, prev_type, -prev_sm, true, judgments);
                    const std::optional<double> selected_time =
                        (selected_note_info != nullptr) ? std::optional<double>(selected_note_info->time_seconds) : std::nullopt;
                    const std::optional<double> prev_time =
                        (prev_selected_note_info != nullptr) ? std::optional<double>(prev_selected_note_info->time_seconds) : std::nullopt;

                    std::optional<double> chosen_time;
                    if (selected_press_result.has_value() && prev_press_result.has_value() && *prev_press_result > 0.0) {
                        if (selected_time.has_value() && prev_time.has_value()) {
                            chosen_time =
                                ((*selected_time - time_seconds) < (time_seconds - *prev_time)) ? selected_time : prev_time;
                        } else {
                            chosen_time = selected_time.has_value() ? selected_time : prev_time;
                        }
                    } else if (selected_press_result.has_value()) {
                        chosen_time = selected_time;
                    } else if (prev_press_result.has_value() && *prev_press_result > 0.0) {
                        chosen_time = prev_time;
                    }

                    if (chosen_time.has_value()) {
                        selected_press_time = *chosen_time;
                        prev_offsets[static_cast<std::size_t>(selected_line)] = *chosen_time - time_seconds;
                    } else {
                        prev_offsets[static_cast<std::size_t>(selected_line)] = std::nullopt;
                    }
                }

                if (selected_tail_info != nullptr && selected_head_info != nullptr) {
                    const double tail_time = selected_tail_info->time_seconds;
                    const double head_time = selected_head_info->time_seconds;
                    const std::optional<double> next_selected_time =
                        (selected_note_info != nullptr) ? std::optional<double>(selected_note_info->time_seconds) : std::nullopt;
                    const double ln_distance = std::min(time_seconds - head_time, tail_time - time_seconds);
                    if (ln_distance > 0.0) {
                        ldb += selected_weight * std::min(ln_distance, 1.0);
                        ldbd += std::min(ln_distance, 1.0);
                    }

                    auto ratio_or_zero = [&judgments](double timing_offset_seconds, JudgmentNoteType note_type, bool use_score) {
                        const auto ratio =
                            use_score ? get_judgment_for_fds_rds(timing_offset_seconds * 1000.0, judgments, note_type).first
                                      : get_judgment_for_fds_rds(timing_offset_seconds * 1000.0, judgments, note_type).second;
                        return ratio.has_value() ? *ratio : 0.0;
                    };
                    auto ratio_or_none = [&judgments](double timing_offset_seconds, JudgmentNoteType note_type, bool use_score) {
                        return use_score ? get_judgment_for_fds_rds(timing_offset_seconds * 1000.0, judgments, note_type).first
                                         : get_judgment_for_fds_rds(timing_offset_seconds * 1000.0, judgments, note_type).second;
                    };

                    const double ln_alpha_score = (ratio_or_zero(time_seconds - tail_time, JudgmentNoteType::Tail, true) +
                                                   ratio_or_zero((time_seconds + deltas.score_plus_tail) - tail_time,
                                                                 JudgmentNoteType::Tail,
                                                                 true) +
                                                   ratio_or_zero((time_seconds - deltas.score_minus_tail) - tail_time,
                                                                 JudgmentNoteType::Tail,
                                                                 true)) /
                                                  3.0;
                    const double ln_beta_score = (ratio_or_zero(time_seconds - head_time, JudgmentNoteType::Head, true) +
                                                  ratio_or_zero((time_seconds + deltas.score_plus_head) - head_time,
                                                                JudgmentNoteType::Head,
                                                                true) +
                                                  ratio_or_zero((time_seconds - deltas.score_minus_head) - head_time,
                                                                JudgmentNoteType::Head,
                                                                true)) /
                                                 3.0;
                    const double ln_alpha_acc = (ratio_or_zero(time_seconds - tail_time, JudgmentNoteType::Tail, false) +
                                                 ratio_or_zero((time_seconds + deltas.acc_plus_tail) - tail_time,
                                                               JudgmentNoteType::Tail,
                                                               false) +
                                                 ratio_or_zero((time_seconds - deltas.acc_minus_tail) - tail_time,
                                                               JudgmentNoteType::Tail,
                                                               false)) /
                                                3.0;
                    const double ln_beta_acc = (ratio_or_zero(time_seconds - head_time, JudgmentNoteType::Head, false) +
                                                ratio_or_zero((time_seconds + deltas.acc_plus_head) - head_time,
                                                              JudgmentNoteType::Head,
                                                              false) +
                                                ratio_or_zero((time_seconds - deltas.acc_minus_head) - head_time,
                                                              JudgmentNoteType::Head,
                                                              false)) /
                                               3.0;

                    const double ln_weight_score = (100.0 - std::max(ln_alpha_score, ln_beta_score)) / 100.0;
                    const double ln_weight_acc = (100.0 - std::max(ln_alpha_acc, ln_beta_acc)) / 100.0;
                    const double score_weight = selected_weight * ln_weight_score;
                    const double acc_weight = selected_weight * ln_weight_acc;

                    if (ln_weight_score > 0.0) {
                        accumulate_ratio(lfds_n, lfds_d, selected_release_time - tail_time, judgments, JudgmentNoteType::Tail,
                                         score_weight, true);
                        accumulate_ratio(lfds_n, lfds_d, (selected_release_time + deltas.score_plus_tail) - tail_time, judgments,
                                         JudgmentNoteType::Tail, score_weight, true);
                        accumulate_ratio(lfds_n, lfds_d, (selected_release_time - deltas.score_minus_tail) - tail_time, judgments,
                                         JudgmentNoteType::Tail, score_weight, true);

                        const double ref_time1 = head_time + prev_ln_length;
                        if (!next_selected_time.has_value()) {
                            accumulate_ratio(lrds_n, lrds_d, ref_time1 - tail_time, judgments, JudgmentNoteType::Tail,
                                             score_weight, true);
                            accumulate_ratio(lrds_n, lrds_d, (ref_time1 + deltas.score_plus_tail) - tail_time, judgments,
                                             JudgmentNoteType::Tail, score_weight, true);
                            accumulate_ratio(lrds_n, lrds_d, (ref_time1 - deltas.score_minus_tail) - tail_time, judgments,
                                             JudgmentNoteType::Tail, score_weight, true);
                        } else {
                            const double ref_time2 = *next_selected_time - prev_ln_space;
                            for (double delta_value : {0.0, deltas.score_plus_tail, -deltas.score_minus_tail}) {
                                const auto ratio1 =
                                    ratio_or_none((ref_time1 + delta_value) - tail_time, JudgmentNoteType::Tail, true);
                                const auto ratio2 =
                                    ratio_or_none((ref_time2 + delta_value) - tail_time, JudgmentNoteType::Tail, true);
                                const auto ratio = select_ratio(ratio1, ratio2);
                                if (ratio.has_value()) {
                                    lrds_n += *ratio * score_weight;
                                    lrds_d += score_weight;
                                }
                            }
                        }
                    }

                    if (ln_weight_acc > 0.0) {
                        accumulate_ratio(lfda_n, lfda_d, selected_release_time - tail_time, judgments, JudgmentNoteType::Tail,
                                         acc_weight, false);
                        accumulate_ratio(lfda_n, lfda_d, (selected_release_time + deltas.acc_plus_tail) - tail_time, judgments,
                                         JudgmentNoteType::Tail, acc_weight, false);
                        accumulate_ratio(lfda_n, lfda_d, (selected_release_time - deltas.acc_minus_tail) - tail_time, judgments,
                                         JudgmentNoteType::Tail, acc_weight, false);

                        const double ref_time1 = head_time + prev_ln_length;
                        if (!next_selected_time.has_value()) {
                            accumulate_ratio(lrda_n, lrda_d, ref_time1 - tail_time, judgments, JudgmentNoteType::Tail,
                                             acc_weight, false);
                            accumulate_ratio(lrda_n, lrda_d, (ref_time1 + deltas.acc_plus_tail) - tail_time, judgments,
                                             JudgmentNoteType::Tail, acc_weight, false);
                            accumulate_ratio(lrda_n, lrda_d, (ref_time1 - deltas.acc_minus_tail) - tail_time, judgments,
                                             JudgmentNoteType::Tail, acc_weight, false);
                        } else {
                            const double ref_time2 = *next_selected_time - prev_ln_space;
                            for (double delta_value : {0.0, deltas.acc_plus_tail, -deltas.acc_minus_tail}) {
                                const auto ratio1 =
                                    ratio_or_none((ref_time1 + delta_value) - tail_time, JudgmentNoteType::Tail, false);
                                const auto ratio2 =
                                    ratio_or_none((ref_time2 + delta_value) - tail_time, JudgmentNoteType::Tail, false);
                                const auto ratio = select_ratio(ratio1, ratio2);
                                if (ratio.has_value()) {
                                    lrda_n += *ratio * acc_weight;
                                    lrda_d += acc_weight;
                                }
                            }
                        }
                    }

                    const auto tail_check = get_judgment_for_fds_rds(
                        ((time_seconds + deltas.score_plus_tail) - tail_time) * 1000.0, judgments, JudgmentNoteType::Tail);
                    if (tail_check.first.has_value()) {
                        selected_release_time = tail_time;
                    }

                    const double ln_length = tail_time - head_time;
                    if (ln_length > 0.0) {
                        min_ln_length = (min_ln_length <= 0.0) ? ln_length : std::min(min_ln_length, ln_length);
                    }
                    if (next_selected_time.has_value()) {
                        const double ln_space = *next_selected_time - tail_time;
                        if (ln_space > 0.0) {
                            min_ln_space = (min_ln_space <= 0.0) ? ln_space : std::min(min_ln_space, ln_space);
                        }
                    }
                }
            }
        }

        prev_offsets_by_idx[i] = prev_offsets;
        if (min_ln_length > 0.0) {
            prev_ln_length = min_ln_length;
        }
        if (min_ln_space > 0.0) {
            prev_ln_space = std::min(min_ln_space, 75.0 / 1000.0);
        }
        prev_ln_length_by_idx[i] = prev_ln_length;
        prev_ln_space_by_idx[i] = prev_ln_space;
        fds_n_by_idx[i] = fds_n;
        fds_d_by_idx[i] = fds_d;
        fda_n_by_idx[i] = fda_n;
        fda_d_by_idx[i] = fda_d;
        rds_n_by_idx[i] = rds_n;
        rds_d_by_idx[i] = rds_d;
        rda_n_by_idx[i] = rda_n;
        rda_d_by_idx[i] = rda_d;

        data.fds[i] = round_nine((fds_d > 0.0) ? (fds_n / fds_d) : 100.0);
        data.fda[i] = round_nine((fda_d > 0.0) ? (fda_n / fda_d) : 100.0);
        data.rds[i] = round_nine((rds_d > 0.0) ? (rds_n / rds_d) : 100.0);
        data.rda[i] = round_nine((rda_d > 0.0) ? (rda_n / rda_d) : 100.0);
        data.lfds[i] = round_nine((lfds_d > 0.0) ? (lfds_n / lfds_d) : 100.0);
        data.lfda[i] = round_nine((lfda_d > 0.0) ? (lfda_n / lfda_d) : 100.0);
        data.lrds[i] = round_nine((lrds_d > 0.0) ? (lrds_n / lrds_d) : 100.0);
        data.lrda[i] = round_nine((lrda_d > 0.0) ? (lrda_n / lrda_d) : 100.0);
        data.vrs[i] = round_nine((vrs_d > 0.0) ? (vrs_n / vrs_d) : 1.0);
        data.vra[i] = round_nine((vra_d > 0.0) ? (vra_n / vra_d) : 1.0);
        data.ldb[i] = round_nine(ldb);
        data.ldbd[i] = round_nine(ldbd);
    }

    return data;
}

std::pair<double, double> judge_average_ratio(const JudgmentSet& judgments, JudgmentNoteType note_type) {
    if (judgments.empty()) {
        return {100.0, 100.0};
    }

    double score_numerator = 0.0;
    double score_denominator = 0.0;
    double acc_numerator = 0.0;
    double acc_denominator = 0.0;
    for (int offset = 0; offset < static_cast<int>(kTimeWindowMs); ++offset) {
        const double weight = 1.0 / static_cast<double>(offset + 1);
        for (double signed_offset : {static_cast<double>(offset), -static_cast<double>(offset)}) {
            const auto result = get_judgment_for_fds_rds(signed_offset, judgments, note_type);
            if (result.first.has_value()) {
                score_numerator += *result.first * weight;
                score_denominator += weight;
            }
            if (result.second.has_value()) {
                acc_numerator += *result.second * weight;
                acc_denominator += weight;
            }
        }
    }

    const double score = (score_denominator > 0.0) ? (score_numerator / score_denominator) : 100.0;
    const double acc = (acc_denominator > 0.0) ? (acc_numerator / acc_denominator) : 100.0;
    return {score, acc};
}

std::optional<std::string> canonical_mode_name(std::string_view raw_mode_name) {
    const std::string normalized = normalize_mode_name(raw_mode_name);
    if (normalized.empty()) {
        return std::nullopt;
    }
    if (normalized == "5+1" || normalized == "5+1k" || normalized == "5p1" || normalized == "5p1k" ||
        normalized == "5+1sp") {
        return std::string("5+1");
    }
    if (normalized == "7+1" || normalized == "7+1k" || normalized == "7p1" || normalized == "7p1k" ||
        normalized == "7+1sp") {
        return std::string("7+1");
    }
    if (normalized == "10+2" || normalized == "10+2k" || normalized == "10p2" || normalized == "10p2k" ||
        normalized == "dp12" || normalized == "10+2dp") {
        return std::string("DP12");
    }
    if (normalized == "14+2" || normalized == "14+2k" || normalized == "14p2" || normalized == "14p2k" ||
        normalized == "dp16" || normalized == "14+2dp") {
        return std::string("DP16");
    }
    if (normalized == "pms9k" || normalized == "9kpms") {
        return std::string("9K");
    }
    if (normalized == "4k" || normalized == "5k" || normalized == "6k" || normalized == "7k" || normalized == "8k" ||
        normalized == "9k" || normalized == "10k" || normalized == "16k") {
        std::string value = normalized;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }
    return std::string(raw_mode_name);
}

OsuDifficultyMetrics calculate_from_events(const std::vector<DifficultyEvent>& notes,
                                           double duration_seconds,
                                           int key_count,
                                           const JudgmentSet& judgments,
                                           const Matrix& column_weights,
                                           const Matrix& visual_distances,
                                           const std::vector<int>& scratch_index_list) {
    OsuDifficultyMetrics metrics;
    metrics.note_count = static_cast<int>(notes.size());
    if (notes.empty() || key_count <= 0) {
        return metrics;
    }

    const auto deltas = calculate_time_deltas(judgments);
    const auto note_diff =
        calculate_note_difficulty(notes, key_count, column_weights, visual_distances, scratch_index_list, judgments, deltas);
    const auto [judge_score_rice, judge_acc_rice] = judge_average_ratio(judgments, JudgmentNoteType::Rice);
    const auto [judge_score_head, judge_acc_head] = judge_average_ratio(judgments, JudgmentNoteType::Head);
    const auto [judge_score_tail, judge_acc_tail] = judge_average_ratio(judgments, JudgmentNoteType::Tail);

    double raw_score_l1_sum = 0.0;
    double raw_acc_l1_sum = 0.0;
    double raw_score_l5_sum = 0.0;
    double raw_acc_l5_sum = 0.0;
    int peak_nps = 0;

    for (std::size_t i = 0; i < notes.size(); ++i) {
        peak_nps = std::max(peak_nps, note_diff.nps[i]);

        const double ldb = note_diff.ldb[i];
        const double ldbd = note_diff.ldbd[i];
        const double jack_score = note_diff.note_jack_diff_score[i];
        const double jack_acc = note_diff.note_jack_diff_acc[i];
        const double fds = clamp_min(note_diff.fds[i], 0.01);
        const double fda = clamp_min(note_diff.fda[i], 0.01);
        const double rds = clamp_min(note_diff.rds[i], 0.01);
        const double rda = clamp_min(note_diff.rda[i], 0.01);
        const double lfds = clamp_min(note_diff.lfds[i], 0.01);
        const double lfda = clamp_min(note_diff.lfda[i], 0.01);
        const double lrds = clamp_min(note_diff.lrds[i], 0.01);
        const double lrda = clamp_min(note_diff.lrda[i], 0.01);

        double judge_score = judge_score_rice;
        double judge_acc = judge_acc_rice;
        if (notes[i].type == DifficultyEventType::HoldStart) {
            judge_score = judge_score_head;
            judge_acc = judge_acc_head;
        } else if (notes[i].type == DifficultyEventType::HoldEnd) {
            judge_score = judge_score_tail;
            judge_acc = judge_acc_tail;
        }
        judge_score = clamp_min(judge_score, 0.01);
        judge_acc = clamp_min(judge_acc, 0.01);

        const double flex_read_mult_score =
            (((100.0 / fds) - 1.0) * kFdWeight) + (((100.0 / lfds) - 1.0) * kLfdWeight) +
            (((100.0 / rds) - 1.0) * kRdWeight) + (((100.0 / lrds) - 1.0) * kLrdWeight) + 1.0;
        const double flex_read_mult_acc =
            (((100.0 / fda) - 1.0) * kFdWeight) + (((100.0 / lfda) - 1.0) * kLfdWeight) +
            (((100.0 / rda) - 1.0) * kRdWeight) + (((100.0 / lrda) - 1.0) * kLrdWeight) + 1.0;

        const double same_line_nps = note_diff.same_line_nps_v2[i];
        const double other_line_nps = note_diff.nps_v2[i] - same_line_nps + (kLdbWeight * ldb);
        const double same_line_distance = note_diff.same_line_minimum_distance_sum[i];
        const double other_line_distance =
            note_diff.minimum_distance_sum[i] - same_line_distance + (kLdbWeight * ldbd);
        const double other_line_nerf_score =
            std::clamp(note_diff.vrs[i] * kVibroRelationNerfWeight, 0.0, 1.0);
        const double other_line_nerf_acc =
            std::clamp(note_diff.vra[i] * kVibroRelationNerfWeight, 0.0, 1.0);

        const double base_score =
            std::pow((same_line_nps + (other_line_nps * (1.0 - other_line_nerf_score))) *
                         std::pow((same_line_distance + (other_line_distance * (1.0 - other_line_nerf_score))),
                                  kDistanceWeight),
                     1.0 / (1.0 + kDistanceWeight)) *
            (1.0 + (jack_score / 100.0));
        const double base_acc =
            std::pow((same_line_nps + (other_line_nps * (1.0 - other_line_nerf_acc))) *
                         std::pow((same_line_distance + (other_line_distance * (1.0 - other_line_nerf_acc))),
                                  kDistanceWeight),
                     1.0 / (1.0 + kDistanceWeight)) *
            (1.0 + (jack_acc / 100.0));

        const double score_diff = base_score * flex_read_mult_score * (100.0 / judge_score);
        const double acc_diff = base_acc * flex_read_mult_acc * (100.0 / judge_acc);
        raw_score_l1_sum += score_diff;
        raw_acc_l1_sum += acc_diff;
        raw_score_l5_sum += std::pow(score_diff, 5.0);
        raw_acc_l5_sum += std::pow(acc_diff, 5.0);
    }

    const double score_diff_l5_sum = raw_score_l5_sum > 0.0 ? std::pow(raw_score_l5_sum, 0.2) : 0.0;
    const double acc_diff_l5_sum = raw_acc_l5_sum > 0.0 ? std::pow(raw_acc_l5_sum, 0.2) : 0.0;
    const double global_nps = static_cast<double>(notes.size()) / std::max(duration_seconds, 1.0);
    const double score_diff_l5_avg = !notes.empty() ? (raw_score_l1_sum / static_cast<double>(notes.size())) : 0.0;
    const double acc_diff_l5_avg = !notes.empty() ? (raw_acc_l1_sum / static_cast<double>(notes.size())) : 0.0;

    const double score_avg_term = score_diff_l5_avg > 0.0 ? std::pow(score_diff_l5_avg, kAverageRatingPower) : 0.0;
    const double acc_avg_term = acc_diff_l5_avg > 0.0 ? std::pow(acc_diff_l5_avg, kAverageRatingPower) : 0.0;
    const double score_avg_lv_term = score_diff_l5_avg > 0.0 ? std::pow(score_diff_l5_avg, kAverageLevelPower) : 0.0;
    const double acc_avg_lv_term = acc_diff_l5_avg > 0.0 ? std::pow(acc_diff_l5_avg, kAverageLevelPower) : 0.0;

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
        const double revive_powered = revive_base > 0.0 ? std::pow(revive_base, kReviveLevelPower) : 0.0;
        revive_level = static_cast<int>(std::ceil(
            kReviveMaxLevel - ((kReviveMaxLevel * kReviveMaxLevel) / (revive_powered + kReviveMaxLevel))));
    }

    metrics.circus_rating = round_nine(circus_rating);
    metrics.revive_level = revive_level;
    metrics.peak_nps = static_cast<double>(peak_nps);
    metrics.average_nps = round_nine(global_nps);
    return metrics;
}

}  // namespace

OsuDifficultyMetrics calculate_osu_mania_difficulty(const OsuManiaChart& chart, const ManiaDifficultyOptions& options) {
    OsuDifficultyMetrics metrics;
    if (chart.key_count <= 0 || chart.notes.empty()) {
        return metrics;
    }

    const auto mode_name = canonical_mode_name(options.mode_name);
    const auto matrix_key = resolve_matrix_key(chart.key_count, mode_name.has_value() ? *mode_name : options.mode_name);
    if (!matrix_key.has_value()) {
        return metrics;
    }

    std::vector<DifficultyEvent> events;
    events.reserve(chart.notes.size() * 2);
    for (const auto& note : chart.notes) {
        const int column = std::clamp(note.column, 0, std::max(chart.key_count - 1, 0)) + 1;
        const double start_seconds = static_cast<double>(note.start_time_ms) / 1000.0;
        if (note.end_time_ms.has_value() && *note.end_time_ms > note.start_time_ms) {
            events.push_back(DifficultyEvent{start_seconds, column, DifficultyEventType::HoldStart});
            events.push_back(
                DifficultyEvent{static_cast<double>(*note.end_time_ms) / 1000.0, column, DifficultyEventType::HoldEnd});
        } else {
            events.push_back(DifficultyEvent{start_seconds, column, DifficultyEventType::Rice});
        }
    }

    std::sort(events.begin(), events.end(), [](const DifficultyEvent& lhs, const DifficultyEvent& rhs) {
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

    double duration_seconds = 0.0;
    if (!events.empty()) {
        duration_seconds = events.back().time_seconds - events.front().time_seconds;
        if (duration_seconds < 1.0) {
            duration_seconds = 1.0;
        }
    }

    const auto judgments = select_judgments(chart, options);
    return calculate_from_events(
        events, duration_seconds, chart.key_count, judgments, weighted_nps_matrix(*matrix_key),
        visual_distance_matrix(*matrix_key), scratch_indices(*matrix_key));
}

OsuDifficultyMetrics calculate_osu_mania_difficulty(const OsuManiaChart& chart) {
    ManiaDifficultyOptions options;
    return calculate_osu_mania_difficulty(chart, options);
}

OsuDifficultyMetrics calculate_osu_10k_difficulty(const OsuManiaChart& chart) {
    return calculate_osu_mania_difficulty(chart);
}

}  // namespace tenriff::chart
