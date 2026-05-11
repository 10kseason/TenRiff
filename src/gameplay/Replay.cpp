#include "gameplay/Replay.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "config/SimpleJson.h"

namespace tenriff::gameplay {

namespace {

std::string input_state_label(input::InputState state) {
    return (state == input::InputState::Pressed) ? "down" : "up";
}

std::string gauge_label(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::ExHard: return "ex_hard";
        case game::GaugeType::Hard: return "hard";
        case game::GaugeType::Easy: return "easy";
        case game::GaugeType::Normal:
        default: return "normal";
    }
}

game::GaugeType gauge_type_from_string(std::string_view token) {
    if (token == "ex_hard" || token == "ex-hard" || token == "exhard") {
        return game::GaugeType::ExHard;
    }
    if (token == "hard") {
        return game::GaugeType::Hard;
    }
    if (token == "easy") {
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
    return value ? value->as_object() : nullptr;
}

const config::JsonArray* find_json_array(const config::JsonObject& root, std::string_view key) {
    const auto* value = find_json_value(root, key);
    return value ? value->as_array() : nullptr;
}

int read_json_int(const config::JsonObject& root, std::string_view key, int fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return static_cast<int>(std::llround(value->as_number(static_cast<double>(fallback))));
}

int64_t read_json_i64(const config::JsonObject& root, std::string_view key, int64_t fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return static_cast<int64_t>(std::llround(value->as_number(static_cast<double>(fallback))));
}

double read_json_number(const config::JsonObject& root, std::string_view key, double fallback) {
    const auto* value = find_json_value(root, key);
    return value ? value->as_number(fallback) : fallback;
}

bool read_json_bool(const config::JsonObject& root, std::string_view key, bool fallback) {
    const auto* value = find_json_value(root, key);
    return value ? value->as_bool(fallback) : fallback;
}

std::string read_json_string(const config::JsonObject& root, std::string_view key, std::string fallback = {}) {
    const auto* value = find_json_value(root, key);
    return value ? value->as_string(std::move(fallback)) : fallback;
}

std::vector<std::string> read_json_string_array(const config::JsonObject& root, std::string_view key) {
    std::vector<std::string> out;
    const auto* values = find_json_array(root, key);
    if (!values) {
        return out;
    }
    out.reserve(values->size());
    for (const auto& value : *values) {
        if (value.is_string()) {
            out.push_back(value.as_string());
        }
    }
    return out;
}

std::optional<input::InputState> parse_input_state(std::string_view token) {
    if (token == "down" || token == "pressed") {
        return input::InputState::Pressed;
    }
    if (token == "up" || token == "released") {
        return input::InputState::Released;
    }
    return std::nullopt;
}

int64_t derived_raw_score(const ResultStats& stats) {
    if (stats.raw_score > 0) {
        return stats.raw_score;
    }
    int64_t score = static_cast<int64_t>(stats.counts.pg) * 1000 +
                    static_cast<int64_t>(stats.counts.gr) * 700 +
                    static_cast<int64_t>(stats.counts.gd) * 300;
    score -= static_cast<int64_t>(stats.counts.bd) * 200;
    return std::max<int64_t>(0, score);
}

int64_t clamp_final_score(int64_t raw_score, double multiplier) {
    const double safe_multiplier = (std::isfinite(multiplier) && multiplier > 0.0) ? multiplier : 1.0;
    return std::max<int64_t>(0, static_cast<int64_t>(std::llround(static_cast<double>(raw_score) * safe_multiplier)));
}

config::JsonValue build_counts_json(const JudgementCounts& counts) {
    config::JsonObject obj;
    obj.emplace("pg", config::JsonValue{static_cast<double>(counts.pg)});
    obj.emplace("gr", config::JsonValue{static_cast<double>(counts.gr)});
    obj.emplace("gd", config::JsonValue{static_cast<double>(counts.gd)});
    obj.emplace("bd", config::JsonValue{static_cast<double>(counts.bd)});
    obj.emplace("pr", config::JsonValue{static_cast<double>(counts.pr)});
    return config::JsonValue{std::move(obj)};
}

config::JsonValue build_string_array_json(const std::vector<std::string>& values) {
    config::JsonArray out;
    out.reserve(values.size());
    for (const auto& value : values) {
        out.emplace_back(value);
    }
    return config::JsonValue{std::move(out)};
}

config::JsonValue build_gauge_history_json(const std::vector<GaugeSample>& history) {
    config::JsonArray values;
    values.reserve(history.size());
    for (const auto& sample : history) {
        config::JsonObject item;
        item.emplace("sample", config::JsonValue{static_cast<double>(sample.sample)});
        item.emplace("value", config::JsonValue{sample.value});
        values.emplace_back(config::JsonValue{std::move(item)});
    }
    return config::JsonValue{std::move(values)};
}

config::JsonValue build_shift_json(const std::vector<ShiftEvent>& shifts) {
    config::JsonArray values;
    values.reserve(shifts.size());
    for (const auto& shift : shifts) {
        config::JsonObject item;
        item.emplace("sample", config::JsonValue{static_cast<double>(shift.sample)});
        item.emplace("from", config::JsonValue{gauge_label(shift.from)});
        item.emplace("to", config::JsonValue{gauge_label(shift.to)});
        values.emplace_back(config::JsonValue{std::move(item)});
    }
    return config::JsonValue{std::move(values)};
}

config::JsonValue build_stats_json(const ResultStats& stats) {
    config::JsonObject obj;
    obj.emplace("counts", build_counts_json(stats.counts));
    obj.emplace("combo", config::JsonValue{static_cast<double>(stats.combo)});
    obj.emplace("max_combo", config::JsonValue{static_cast<double>(stats.max_combo)});
    obj.emplace("total_notes", config::JsonValue{static_cast<double>(stats.total_notes)});
    obj.emplace("raw_score", config::JsonValue{static_cast<double>(stats.raw_score)});
    obj.emplace("mean_delta_ms", config::JsonValue{stats.mean_delta_ms});
    obj.emplace("stddev_delta_ms", config::JsonValue{stats.stddev_delta_ms()});
    obj.emplace("positive_delta_count", config::JsonValue{static_cast<double>(stats.positive_delta_count)});
    obj.emplace("negative_delta_count", config::JsonValue{static_cast<double>(stats.negative_delta_count)});
    obj.emplace("gauge_history", build_gauge_history_json(stats.gauge_history));
    obj.emplace("shifts", build_shift_json(stats.shifts));
    return config::JsonValue{std::move(obj)};
}

config::JsonValue build_trace_events_json(const std::vector<ReplayEvent>& events) {
    config::JsonArray values;
    values.reserve(events.size());
    for (const auto& event : events) {
        config::JsonObject item;
        item.emplace("sample", config::JsonValue{static_cast<double>(event.sample)});
        item.emplace("lane", config::JsonValue{static_cast<double>(event.lane)});
        item.emplace("state", config::JsonValue{input_state_label(event.state)});
        values.emplace_back(config::JsonValue{std::move(item)});
    }
    return config::JsonValue{std::move(values)};
}

config::JsonValue build_trace_json(const ReplayTrace& trace) {
    config::JsonObject obj;
    obj.emplace("sample_rate", config::JsonValue{static_cast<double>(trace.sample_rate)});
    obj.emplace("rate", config::JsonValue{trace.rate});
    obj.emplace("lane_count", config::JsonValue{static_cast<double>(trace.lane_count)});
    obj.emplace("duration_samples", config::JsonValue{static_cast<double>(trace.duration_samples)});
    obj.emplace("events", build_trace_events_json(trace.events));
    return config::JsonValue{std::move(obj)};
}

config::JsonValue build_mode_json(const ReplayModeSettings& mode) {
    config::JsonObject obj;
    if (!mode.key_mode.empty()) {
        obj.emplace("key_mode", config::JsonValue{mode.key_mode});
    }
    if (!mode.random.empty()) {
        obj.emplace("random", config::JsonValue{mode.random});
    }
    if (mode.random_seed.has_value()) {
        obj.emplace("random_seed", config::JsonValue{static_cast<double>(mode.random_seed.value())});
    }
    if (!mode.gauge.empty()) {
        obj.emplace("gauge", config::JsonValue{mode.gauge});
    }
    obj.emplace("autoplay_enabled", config::JsonValue{mode.autoplay_enabled});
    obj.emplace("practice_no_fail_enabled", config::JsonValue{mode.practice_no_fail_enabled});
    return config::JsonValue{std::move(obj)};
}

ExportResult save_json_file(const std::string& path, const config::JsonValue& root, int indent) {
    ExportResult result;

#ifdef _WIN32
    std::filesystem::path file_path = std::filesystem::u8path(path);
#else
    std::filesystem::path file_path(path);
#endif
    if (file_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
        if (ec) {
            result.warnings.push_back("Failed to create export directory: " + file_path.parent_path().u8string());
        }
    }

    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        result.error = "Failed to write export file: " + path;
        return result;
    }

    file << config::json_stringify(root, indent);
    if (!file) {
        result.error = "Failed to write export file: " + path;
        return result;
    }

    return result;
}

}  // namespace

ExportResult save_replay_json(const std::string& path, const ReplayFile& replay, int indent) {
    config::JsonObject obj;
    obj.emplace("version", config::JsonValue{static_cast<double>(replay.version)});
    obj.emplace("chart_path", config::JsonValue{replay.chart_path});
    obj.emplace("chart_format", config::JsonValue{replay.chart_format});
    obj.emplace("created_utc", config::JsonValue{replay.created_utc});
    obj.emplace("sample_rate", config::JsonValue{static_cast<double>(replay.sample_rate)});
    obj.emplace("rate", config::JsonValue{replay.rate});
    obj.emplace("input_offset_ms", config::JsonValue{replay.input_offset_ms});
    obj.emplace("mods", build_string_array_json(replay.mods));
    obj.emplace("rate_multiplier", config::JsonValue{replay.rate_multiplier});
    obj.emplace("score_multiplier", config::JsonValue{replay.score_multiplier});
    obj.emplace("final_score", config::JsonValue{static_cast<double>(replay.final_score)});
    obj.emplace("mode", build_mode_json(replay.mode));
    obj.emplace("trace", build_trace_json(replay.trace));
    obj.emplace("stats", build_stats_json(replay.stats));
    return save_json_file(path, config::JsonValue{std::move(obj)}, indent);
}

ExportResult save_result_json(const std::string& path, const ResultFile& result_file, int indent) {
    config::JsonObject obj;
    obj.emplace("version", config::JsonValue{static_cast<double>(result_file.version)});
    obj.emplace("chart_path", config::JsonValue{result_file.chart_path});
    obj.emplace("chart_format", config::JsonValue{result_file.chart_format});
    obj.emplace("created_utc", config::JsonValue{result_file.created_utc});
    obj.emplace("replay_path", config::JsonValue{result_file.replay_path});
    obj.emplace("clear_status", config::JsonValue{result_file.clear_status});
    obj.emplace("final_gauge", config::JsonValue{result_file.final_gauge});
    obj.emplace("sample_rate", config::JsonValue{static_cast<double>(result_file.sample_rate)});
    obj.emplace("rate", config::JsonValue{result_file.rate});
    obj.emplace("game_over", config::JsonValue{result_file.game_over});
    obj.emplace("mods", build_string_array_json(result_file.mods));
    obj.emplace("rate_multiplier", config::JsonValue{result_file.rate_multiplier});
    obj.emplace("score_multiplier", config::JsonValue{result_file.score_multiplier});
    obj.emplace("final_score", config::JsonValue{static_cast<double>(result_file.final_score)});
    obj.emplace("autoplay_enabled", config::JsonValue{result_file.autoplay_enabled});
    obj.emplace("practice_no_fail_enabled", config::JsonValue{result_file.practice_no_fail_enabled});
    obj.emplace("stats", build_stats_json(result_file.stats));
    return save_json_file(path, config::JsonValue{std::move(obj)}, indent);
}

ReplayLoadResult load_replay_json(const std::string& path) {
    ReplayLoadResult result;

#ifdef _WIN32
    const std::filesystem::path file_path = std::filesystem::u8path(path);
#else
    const std::filesystem::path file_path(path);
#endif
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open replay JSON.";
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto parsed = config::parse_json(buffer.str());
    if (!parsed.success() || !parsed.root.has_value()) {
        result.error = parsed.error.empty() ? "Failed to parse replay JSON." : parsed.error;
        return result;
    }

    const auto* root = parsed.root->as_object();
    if (!root) {
        result.error = "Replay JSON root must be an object.";
        return result;
    }

    const auto* trace = find_json_object(*root, "trace");
    if (!trace) {
        result.error = "Replay JSON missing trace object.";
        return result;
    }

    ReplayFile replay;
    replay.version = read_json_int(*root, "version", 1);
    replay.chart_path = read_json_string(*root, "chart_path");
    replay.chart_format = read_json_string(*root, "chart_format");
    replay.created_utc = read_json_string(*root, "created_utc");
    replay.sample_rate = read_json_int(*root, "sample_rate", read_json_int(*trace, "sample_rate", 0));
    replay.rate = read_json_number(*root, "rate", read_json_number(*trace, "rate", 1.0));
    replay.input_offset_ms = read_json_number(*root, "input_offset_ms", 0.0);
    replay.mods = read_json_string_array(*root, "mods");
    replay.rate_multiplier = read_json_number(*root, "rate_multiplier", 1.0);
    replay.score_multiplier = read_json_number(*root, "score_multiplier", 1.0);

    if (const auto* mode = find_json_object(*root, "mode")) {
        replay.mode.key_mode = read_json_string(*mode, "key_mode");
        replay.mode.random = read_json_string(*mode, "random");
        if (find_json_value(*mode, "random_seed")) {
            replay.mode.random_seed = read_json_int(*mode, "random_seed", 0);
        }
        replay.mode.gauge = read_json_string(*mode, "gauge");
        replay.mode.autoplay_enabled = read_json_bool(*mode, "autoplay_enabled", false);
        replay.mode.practice_no_fail_enabled =
            read_json_bool(*mode, "practice_no_fail_enabled", false);
    }

    replay.trace.sample_rate = read_json_int(*trace, "sample_rate", replay.sample_rate);
    replay.trace.rate = read_json_number(*trace, "rate", replay.rate);
    replay.trace.lane_count = read_json_int(*trace, "lane_count", 0);
    replay.trace.duration_samples = read_json_i64(*trace, "duration_samples", 0);

    const auto* events = find_json_array(*trace, "events");
    if (!events) {
        result.error = "Replay JSON missing trace.events array.";
        return result;
    }
    replay.trace.events.reserve(events->size());
    for (std::size_t i = 0; i < events->size(); ++i) {
        const auto* item = (*events)[i].as_object();
        if (!item) {
            result.warnings.push_back("Replay event #" + std::to_string(i) + " is not an object.");
            continue;
        }
        const int lane = read_json_int(*item, "lane", 0);
        const int64_t sample = read_json_i64(*item, "sample", 0);
        const auto state = parse_input_state(read_json_string(*item, "state"));
        if (lane <= 0 || !state.has_value()) {
            result.warnings.push_back("Replay event #" + std::to_string(i) + " is malformed.");
            continue;
        }
        replay.trace.events.push_back(ReplayEvent{lane, state.value(), sample});
    }

    if (const auto* stats = find_json_object(*root, "stats")) {
        if (const auto* counts = find_json_object(*stats, "counts")) {
            replay.stats.counts.pg = read_json_int(*counts, "pg", 0);
            replay.stats.counts.gr = read_json_int(*counts, "gr", 0);
            replay.stats.counts.gd = read_json_int(*counts, "gd", 0);
            replay.stats.counts.bd = read_json_int(*counts, "bd", 0);
            replay.stats.counts.pr = read_json_int(*counts, "pr", 0);
        }
        replay.stats.combo = read_json_int(*stats, "combo", 0);
        replay.stats.max_combo = read_json_int(*stats, "max_combo", replay.stats.combo);
        replay.stats.total_notes = read_json_int(*stats, "total_notes", 0);
        replay.stats.raw_score = read_json_i64(*stats, "raw_score", derived_raw_score(replay.stats));
        replay.stats.mean_delta_ms = read_json_number(*stats, "mean_delta_ms", 0.0);
        replay.stats.positive_delta_count = read_json_int(*stats, "positive_delta_count", 0);
        replay.stats.negative_delta_count = read_json_int(*stats, "negative_delta_count", 0);
        const double stddev_delta_ms = read_json_number(*stats, "stddev_delta_ms", 0.0);
        const int judged = replay.stats.counts.pg + replay.stats.counts.gr +
                           replay.stats.counts.gd + replay.stats.counts.bd;
        replay.stats.delta_samples = judged;
        if (replay.stats.delta_samples > 1) {
            replay.stats.m2_delta_ms =
                stddev_delta_ms * stddev_delta_ms * static_cast<double>(replay.stats.delta_samples - 1);
        }

        if (const auto* gauge_history = find_json_array(*stats, "gauge_history")) {
            replay.stats.gauge_history.reserve(gauge_history->size());
            for (const auto& value : *gauge_history) {
                const auto* item = value.as_object();
                if (!item) {
                    continue;
                }
                replay.stats.gauge_history.push_back(GaugeSample{
                    read_json_i64(*item, "sample", 0),
                    read_json_number(*item, "value", 0.0),
                });
            }
        }

        if (const auto* shifts = find_json_array(*stats, "shifts")) {
            replay.stats.shifts.reserve(shifts->size());
            for (const auto& value : *shifts) {
                const auto* item = value.as_object();
                if (!item) {
                    continue;
                }
                replay.stats.shifts.push_back(ShiftEvent{
                    read_json_i64(*item, "sample", 0),
                    gauge_type_from_string(read_json_string(*item, "from", "normal")),
                    gauge_type_from_string(read_json_string(*item, "to", "normal")),
                });
            }
        }
    }

    replay.final_score = read_json_i64(*root, "final_score",
                                       clamp_final_score(replay.stats.raw_score, replay.score_multiplier));
    result.replay = std::move(replay);
    return result;
}

}  // namespace tenriff::gameplay
