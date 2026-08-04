#include "app/MenuRecordUtils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "config/SimpleJson.h"
#include "gameplay/Replay.h"
#include "util/Utf8Compat.h"

namespace tenriff::app::menu_records {

namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string safe_ui_text(std::string_view value, std::string_view fallback = {}) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return std::string(fallback);
}

std::string safe_ui_text_or_placeholder(std::string_view value, std::string_view placeholder) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return value.empty() ? std::string{} : std::string(placeholder);
}

game::GaugeType gauge_type_from_mode_string(std::string_view value) {
    const std::string gauge = to_lower_ascii(std::string(value));
    if (gauge == "ex_hard" || gauge == "ex-hard" || gauge == "exhard") {
        return game::GaugeType::ExHard;
    }
    if (gauge == "hard") {
        return game::GaugeType::Hard;
    }
    if (gauge == "easy") {
        return game::GaugeType::Easy;
    }
    return game::GaugeType::Normal;
}

const config::JsonValue* find_json_value(const config::JsonObject& root, std::string_view key) {
    auto it = root.find(std::string(key));
    if (it == root.end()) {
        return nullptr;
    }
    return &it->second;
}

const config::JsonObject* find_json_object(const config::JsonObject& root, std::string_view key) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return nullptr;
    }
    return value->as_object();
}

int read_json_int(const config::JsonObject& root, std::string_view key, int fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return static_cast<int>(std::llround(value->as_number(static_cast<double>(fallback))));
}

std::string read_json_string(const config::JsonObject& root, std::string_view key, std::string fallback = {}) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return safe_ui_text(value->as_string(std::move(fallback)));
}

bool read_json_bool(const config::JsonObject& root, std::string_view key, bool fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return value->as_bool(fallback);
}

double read_json_number(const config::JsonObject& root, std::string_view key, double fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return value->as_number(fallback);
}

std::vector<std::string> read_json_string_array(const config::JsonObject& root, std::string_view key) {
    std::vector<std::string> values;
    const auto* value = find_json_value(root, key);
    if (!value) {
        return values;
    }
    const auto* array = value->as_array();
    if (!array) {
        return values;
    }
    values.reserve(array->size());
    for (const auto& item : *array) {
        if (!item.is_string()) {
            continue;
        }
        values.push_back(safe_ui_text(item.as_string()));
    }
    return values;
}

int64_t clamp_final_score(int64_t raw_score, double multiplier) {
    return gameplay::scale_native_score(raw_score, multiplier);
}

}  // namespace

int judged_total(const gameplay::JudgementCounts& counts) {
    return counts.pg + counts.gr + counts.gd + counts.bd;
}

bool clear_status_is_assist(std::string_view clear_status) {
    const std::string status = to_lower_ascii(std::string(clear_status));
    return status.find("assist") != std::string::npos;
}

bool clear_status_is_autoplay(std::string_view clear_status) {
    const std::string status = to_lower_ascii(std::string(clear_status));
    return status.find("autoplay") != std::string::npos;
}

bool clear_status_is_practice(std::string_view clear_status) {
    const std::string status = to_lower_ascii(std::string(clear_status));
    return status.find("practice") != std::string::npos;
}

bool assist_flags_active(bool autoplay_enabled, bool practice_no_fail_enabled) {
    return autoplay_enabled || practice_no_fail_enabled;
}

bool default_ghost_replay_allowed(bool autoplay_enabled,
                                  bool practice_no_fail_enabled,
                                  std::string_view clear_status) {
    return !assist_flags_active(autoplay_enabled, practice_no_fail_enabled) &&
           !clear_status_is_autoplay(clear_status) &&
           !clear_status_is_assist(clear_status);
}

int clear_status_priority(std::string_view clear_status, bool game_over, std::string_view final_gauge) {
    const std::string status = to_lower_ascii(std::string(clear_status));
    if (game_over || clear_status_is_autoplay(clear_status)) {
        return 0;
    }
    if (clear_status_is_assist(clear_status)) {
        return 1;
    }
    if (status.find("gauge shift") != std::string::npos) {
        const std::string gauge = to_lower_ascii(std::string(final_gauge));
        if (gauge == "easy") return 2;
        if (gauge == "normal") return 3;
        if (gauge == "hard") return 4;
        return 5;
    }
    if (status.find("sudden death") != std::string::npos) {
        return 6;
    }
    if (status.find("ex-hard") != std::string::npos || status.find("ex hard") != std::string::npos ||
        status.find("exhard") != std::string::npos) {
        return 5;
    }
    if (status.find("easy") != std::string::npos) {
        return 2;
    }
    if (status.find("hard") != std::string::npos) {
        return 4;
    }
    const std::string gauge = to_lower_ascii(std::string(final_gauge));
    if (gauge == "ex_hard" || gauge == "ex-hard" || gauge == "exhard") {
        return 5;
    }
    if (gauge == "hard") {
        return 4;
    }
    if (gauge == "easy") {
        return 2;
    }
    return 3;
}

std::string normalized_clear_status(std::string_view clear_status, bool game_over, std::string_view final_gauge) {
    const std::string status = safe_ui_text(clear_status);
    if (!status.empty()) {
        return status;
    }
    if (game_over) {
        return "FAILED";
    }
    const std::string gauge = to_lower_ascii(std::string(final_gauge));
    if (gauge == "ex_hard" || gauge == "ex-hard" || gauge == "exhard") {
        return "EX-HARD CLEAR";
    }
    if (gauge == "hard") {
        return "HARD CLEAR";
    }
    if (gauge == "easy") {
        return "EASY CLEAR";
    }
    return "CLEAR";
}

std::string compact_timestamp_label(std::string_view created_utc) {
    std::string value(created_utc);
    if (value.size() >= 16 && value[8] == '_' && value.back() == 'Z') {
        return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2) + " " +
               value.substr(9, 2) + ":" + value.substr(11, 2) + ":" + value.substr(13, 2) + " UTC";
    }
    return safe_ui_text_or_placeholder(created_utc, "-");
}

double calculate_accuracy(const gameplay::ResultStats& stats) {
    return stats.accuracy_percent();
}

int64_t calculate_score(const gameplay::ResultStats& stats) {
    if (stats.raw_score > 0) {
        return stats.raw_score;
    }
    int64_t score = static_cast<int64_t>(stats.counts.pg) * 1000 +
                    static_cast<int64_t>(stats.counts.gr) * 700 +
                    static_cast<int64_t>(stats.counts.gd) * 300;
    score -= static_cast<int64_t>(stats.counts.bd) * 200;
    return std::max<int64_t>(0, score);
}

int64_t calculate_final_score(const gameplay::ResultStats& stats, double multiplier) {
    return clamp_final_score(calculate_score(stats), multiplier);
}

bool infer_game_over(const gameplay::ResultStats& stats) {
    const int judged = judged_total(stats.counts);
    if (judged <= 0) {
        return true;
    }
    const double judged_weight = stats.accuracy_weight > 0.0
                                      ? stats.accuracy_weight
                                      : static_cast<double>(judged);
    if (stats.total_notes > 0 && judged_weight + 1e-9 < static_cast<double>(stats.total_notes)) {
        return true;
    }
    return false;
}

std::string calculate_rank(const gameplay::ResultStats& stats, bool game_over) {
    if (game_over) {
        return "F";
    }
    const int judged = judged_total(stats.counts);
    if (judged <= 0) {
        return "--";
    }
    const double accuracy = calculate_accuracy(stats);
    if (accuracy >= 99.75) {
        return "SSS";
    }
    if (accuracy >= 99.0) {
        return "SS";
    }
    if (accuracy >= 98.0) {
        return "AA";
    }
    if (accuracy >= 95.5) {
        return "S+";
    }
    if (accuracy >= 90.0) {
        return "S";
    }
    if (accuracy >= 86.5) {
        return "A+";
    }
    if (accuracy >= 80.5) {
        return "A";
    }
    if (accuracy >= 75.0) {
        return "B";
    }
    return "F";
}

bool is_better_record(int64_t candidate_score,
                      int candidate_clear_priority,
                      int candidate_combo,
                      int candidate_judged,
                      std::string_view candidate_created,
                      int64_t current_score,
                      int current_clear_priority,
                      int current_combo,
                      int current_judged,
                      std::string_view current_created) {
    if (candidate_clear_priority != current_clear_priority) {
        return candidate_clear_priority > current_clear_priority;
    }
    if (candidate_score != current_score) {
        return candidate_score > current_score;
    }
    if (candidate_combo != current_combo) {
        return candidate_combo > current_combo;
    }
    if (candidate_judged != current_judged) {
        return candidate_judged > current_judged;
    }
    return candidate_created > current_created;
}

std::optional<ParsedResultRecord> parse_result_file(const std::filesystem::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "Failed to open result JSON.";
        }
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    auto parsed = config::parse_json(buffer.str());
    if (!parsed.success() || !parsed.root.has_value()) {
        if (error) {
            *error = parsed.error.empty() ? "Failed to parse result JSON." : parsed.error;
        }
        return std::nullopt;
    }

    const auto* root = parsed.root->as_object();
    if (!root) {
        if (error) {
            *error = "Result JSON root must be an object.";
        }
        return std::nullopt;
    }

    const auto* stats_obj = find_json_object(*root, "stats");
    if (!stats_obj) {
        if (error) {
            *error = "Result JSON missing stats object.";
        }
        return std::nullopt;
    }

    ParsedResultRecord out;
    out.chart_path = read_json_string(*root, "chart_path");
    out.chart_format = read_json_string(*root, "chart_format");
    out.created_utc = read_json_string(*root, "created_utc");
    out.player_name = read_json_string(*root, "player_name");
    out.replay_path = read_json_string(*root, "replay_path");
    out.key_conversion_note_add_mode =
        read_json_string(*root, "key_conversion_note_add_mode");
    out.clear_status = read_json_string(*root, "clear_status");
    out.final_gauge = read_json_string(*root, "final_gauge");
    if (out.chart_path.empty()) {
        if (error) {
            *error = "Result JSON missing chart_path.";
        }
        return std::nullopt;
    }

    const auto* counts_obj = find_json_object(*stats_obj, "counts");
    if (counts_obj) {
        out.stats.counts.pg = read_json_int(*counts_obj, "pg", 0);
        out.stats.counts.gr = read_json_int(*counts_obj, "gr", 0);
        out.stats.counts.gd = read_json_int(*counts_obj, "gd", 0);
        out.stats.counts.bd = read_json_int(*counts_obj, "bd", 0);
        out.stats.counts.pr = read_json_int(*counts_obj, "pr", 0);
    }
    out.stats.max_combo = read_json_int(*stats_obj, "max_combo", 0);
    out.stats.total_notes = read_json_int(*stats_obj, "total_notes", 0);
    out.stats.total_combo_steps = read_json_int(*stats_obj, "total_combo_steps", out.stats.total_notes);
    out.stats.raw_score = static_cast<int64_t>(std::llround(
        read_json_number(*stats_obj, "raw_score", static_cast<double>(calculate_score(out.stats)))));
    out.stats.raw_score_accumulator = out.stats.raw_score;
    out.stats.judgement_score_points = read_json_number(*stats_obj, "judgement_score_points", 0.0);
    out.stats.combo_score_units = static_cast<int64_t>(std::llround(
        read_json_number(*stats_obj, "combo_score_units", 0.0)));
    out.stats.accuracy_points = read_json_number(*stats_obj, "accuracy_points", 0.0);
    out.stats.accuracy_weight = read_json_number(*stats_obj, "accuracy_weight", 0.0);
    out.stats.highest_judgement_timing_weight =
        read_json_number(*stats_obj, "highest_judgement_timing_weight", 0.0);
    out.stats.highest_judgement_min_delta_ms =
        read_json_number(*stats_obj, "highest_judgement_min_delta_ms", 0.0);
    out.stats.highest_judgement_max_delta_ms =
        read_json_number(*stats_obj, "highest_judgement_max_delta_ms", 0.0);
    if (const auto* osu_od8 = find_json_object(*stats_obj, "osu_od8")) {
        out.stats.osu_od8.available = read_json_bool(*osu_od8, "available", true);
        out.stats.osu_od8.total_objects = read_json_int(*osu_od8, "total_objects", out.stats.total_notes);
        out.stats.osu_od8.judged_objects = read_json_int(*osu_od8, "judged_objects", 0);
        out.stats.osu_od8.score = static_cast<int64_t>(std::llround(read_json_number(*osu_od8, "score", 0.0)));
        out.stats.osu_od8.score_accumulator = static_cast<double>(out.stats.osu_od8.score);
        out.stats.osu_od8.bonus = read_json_number(*osu_od8, "bonus", 100.0);
        if (const auto* counts = find_json_object(*osu_od8, "counts")) {
            out.stats.osu_od8.counts.perfect = read_json_int(*counts, "perfect", 0);
            out.stats.osu_od8.counts.great = read_json_int(*counts, "great", 0);
            out.stats.osu_od8.counts.good = read_json_int(*counts, "good", 0);
            out.stats.osu_od8.counts.ok = read_json_int(*counts, "ok", 0);
            out.stats.osu_od8.counts.meh = read_json_int(*counts, "meh", 0);
            out.stats.osu_od8.counts.miss = read_json_int(*counts, "miss", 0);
        }
    }
    out.stats.mean_delta_ms = read_json_number(*stats_obj, "mean_delta_ms", 0.0);
    out.stats.positive_delta_count = read_json_int(*stats_obj, "positive_delta_count", 0);
    out.stats.negative_delta_count = read_json_int(*stats_obj, "negative_delta_count", 0);
    const double stddev_delta_ms = read_json_number(*stats_obj, "stddev_delta_ms", 0.0);
    out.mods = read_json_string_array(*root, "mods");
    out.rate_multiplier = read_json_number(*root, "rate_multiplier", 1.0);
    out.score_multiplier = read_json_number(*root, "score_multiplier", 1.0);
    out.pause_used = read_json_bool(*root, "pause_used", false);
    out.autoplay_enabled = read_json_bool(*root, "autoplay_enabled", clear_status_is_autoplay(out.clear_status));
    out.practice_no_fail_enabled =
        read_json_bool(*root, "practice_no_fail_enabled", clear_status_is_practice(out.clear_status));
    out.one_miss_fail_enabled = read_json_bool(*root, "one_miss_fail_enabled", false);

    if (const auto* gauge_history = find_json_value(*stats_obj, "gauge_history")) {
        if (const auto* values = gauge_history->as_array()) {
            out.stats.gauge_history.reserve(values->size());
            for (const auto& value : *values) {
                const auto* item = value.as_object();
                if (!item) {
                    continue;
                }
                out.stats.gauge_history.push_back(gameplay::GaugeSample{
                    static_cast<int64_t>(std::llround(read_json_number(*item, "sample", 0.0))),
                    read_json_number(*item, "value", 0.0),
                });
            }
        }
    }
    if (const auto* shifts = find_json_value(*stats_obj, "shifts")) {
        if (const auto* values = shifts->as_array()) {
            out.stats.shifts.reserve(values->size());
            for (const auto& value : *values) {
                const auto* item = value.as_object();
                if (!item) {
                    continue;
                }
                out.stats.shifts.push_back(gameplay::ShiftEvent{
                    static_cast<int64_t>(std::llround(read_json_number(*item, "sample", 0.0))),
                    gauge_type_from_mode_string(read_json_string(*item, "from", "normal")),
                    gauge_type_from_mode_string(read_json_string(*item, "to", "normal")),
                });
            }
        }
    }

    const int judged = judged_total(out.stats.counts);
    if (out.stats.total_notes <= 0) {
        out.stats.total_notes = judged;
    }
    out.stats.delta_samples = judged;
    if (out.stats.delta_samples > 1) {
        out.stats.m2_delta_ms = stddev_delta_ms * stddev_delta_ms *
                                static_cast<double>(out.stats.delta_samples - 1);
    }
    out.final_score = static_cast<int64_t>(std::llround(read_json_number(
        *root,
        "final_score",
        static_cast<double>(clamp_final_score(out.stats.raw_score, out.score_multiplier)))));
    out.game_over = read_json_bool(*root, "game_over", infer_game_over(out.stats));
    // Normalize legacy ASSIST AUTOPLAY ... CLEAR exports at load time so they
    // cannot keep an old clear lamp or outrank a failed manual play.
    if (out.autoplay_enabled || clear_status_is_autoplay(out.clear_status)) {
        out.autoplay_enabled = true;
        out.game_over = true;
        out.clear_status = "AUTOPLAY";
    } else {
        out.clear_status = normalized_clear_status(out.clear_status, out.game_over, out.final_gauge);
    }
    return out;
}

std::optional<ParsedReplayRecord> parse_replay_file(const std::filesystem::path& path, std::string* error) {
    auto loaded = gameplay::load_replay_json(path.u8string());
    if (!loaded.success()) {
        if (error) {
            *error = loaded.error.empty() ? "Failed to parse replay JSON." : loaded.error;
        }
        return std::nullopt;
    }

    const auto& replay = loaded.replay.value();
    ParsedReplayRecord out;
    out.sample_rate = replay.sample_rate > 0 ? replay.sample_rate : replay.trace.sample_rate;
    out.rate = replay.rate;
    out.input_offset_ms = replay.input_offset_ms;
    out.mods = replay.mods;
    out.rate_multiplier = replay.rate_multiplier;
    out.score_multiplier = replay.score_multiplier;
    out.pause_used = replay.pause_used;
    out.lane_count = replay.trace.lane_count;
    out.duration_samples = replay.trace.duration_samples;
    out.raw_score = calculate_score(replay.stats);
    out.final_score = replay.final_score > 0 ? replay.final_score
                                             : clamp_final_score(out.raw_score, out.score_multiplier);
    out.event_count = static_cast<int>(replay.trace.events.size());
    out.autoplay_enabled = replay.mode.autoplay_enabled;
    out.practice_no_fail_enabled = replay.mode.practice_no_fail_enabled;
    out.one_miss_fail_enabled = replay.mode.one_miss_fail_enabled;
    if (!loaded.warnings.empty() && error) {
        *error = loaded.warnings.front();
    }
    return out;
}

}  // namespace tenriff::app::menu_records
