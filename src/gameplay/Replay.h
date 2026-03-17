#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gameplay/ResultStats.h"
#include "input/InputEvent.h"

namespace tenriff::gameplay {

struct ReplayEvent {
    int lane = 0;
    input::InputState state = input::InputState::Released;
    int64_t sample = 0;
};

struct ReplayTrace {
    int sample_rate = 0;
    double rate = 1.0;
    int lane_count = 0;
    int64_t duration_samples = 0;
    std::vector<ReplayEvent> events;
};

struct ReplayFile {
    int version = 1;
    std::string chart_path;
    std::string chart_format;
    std::string created_utc;

    int sample_rate = 0;
    double rate = 1.0;
    double input_offset_ms = 0.0;
    std::vector<std::string> mods;
    double rate_multiplier = 1.0;
    double score_multiplier = 1.0;
    int64_t final_score = 0;

    ReplayTrace trace;
    ResultStats stats;
};

struct ResultFile {
    int version = 1;
    std::string chart_path;
    std::string chart_format;
    std::string created_utc;
    std::string replay_path;
    std::string clear_status;
    std::string final_gauge;

    int sample_rate = 0;
    double rate = 1.0;
    bool game_over = false;
    std::vector<std::string> mods;
    double rate_multiplier = 1.0;
    double score_multiplier = 1.0;
    int64_t final_score = 0;

    ResultStats stats;
};

struct ExportResult {
    std::vector<std::string> warnings;
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

[[nodiscard]] ExportResult save_replay_json(const std::string& path, const ReplayFile& replay, int indent = 2);
[[nodiscard]] ExportResult save_result_json(const std::string& path, const ResultFile& result, int indent = 2);

}  // namespace tenriff::gameplay
