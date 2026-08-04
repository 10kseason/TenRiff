#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "gameplay/ResultStats.h"

namespace tenriff::app::menu_records {

struct ParsedResultRecord {
    std::string chart_path;
    std::string chart_format;
    std::string created_utc;
    std::string player_name;
    std::string replay_path;
    std::string key_conversion_note_add_mode;
    std::string clear_status;
    std::string final_gauge;
    std::vector<std::string> mods;
    double rate_multiplier = 1.0;
    double score_multiplier = 1.0;
    int64_t final_score = 0;
    bool pause_used = false;
    gameplay::ResultStats stats;
    bool game_over = false;
    bool autoplay_enabled = false;
    bool practice_no_fail_enabled = false;
    bool one_miss_fail_enabled = false;
};

struct ParsedReplayRecord {
    int sample_rate = 0;
    int lane_count = 0;
    int event_count = 0;
    int64_t duration_samples = 0;
    int64_t raw_score = 0;
    double rate = 1.0;
    double input_offset_ms = 0.0;
    std::vector<std::string> mods;
    double rate_multiplier = 1.0;
    double score_multiplier = 1.0;
    int64_t final_score = 0;
    bool pause_used = false;
    bool autoplay_enabled = false;
    bool practice_no_fail_enabled = false;
    bool one_miss_fail_enabled = false;
};

int judged_total(const gameplay::JudgementCounts& counts);
bool clear_status_is_assist(std::string_view clear_status);
bool clear_status_is_autoplay(std::string_view clear_status);
bool clear_status_is_practice(std::string_view clear_status);
bool assist_flags_active(bool autoplay_enabled, bool practice_no_fail_enabled);
bool default_ghost_replay_allowed(bool autoplay_enabled,
                                  bool practice_no_fail_enabled,
                                  std::string_view clear_status);
int clear_status_priority(std::string_view clear_status, bool game_over, std::string_view final_gauge);
std::string normalized_clear_status(std::string_view clear_status, bool game_over, std::string_view final_gauge);
std::string compact_timestamp_label(std::string_view created_utc);
double calculate_accuracy(const gameplay::ResultStats& stats);
int64_t calculate_score(const gameplay::ResultStats& stats);
int64_t calculate_final_score(const gameplay::ResultStats& stats, double multiplier);
bool infer_game_over(const gameplay::ResultStats& stats);
std::string calculate_rank(const gameplay::ResultStats& stats, bool game_over);
bool is_better_record(int64_t candidate_score,
                      int candidate_clear_priority,
                      int candidate_combo,
                      int candidate_judged,
                      std::string_view candidate_created,
                      int64_t current_score,
                      int current_clear_priority,
                      int current_combo,
                      int current_judged,
                      std::string_view current_created);
std::optional<ParsedResultRecord> parse_result_file(const std::filesystem::path& path, std::string* error);
std::optional<ParsedReplayRecord> parse_replay_file(const std::filesystem::path& path, std::string* error);

}  // namespace tenriff::app::menu_records
