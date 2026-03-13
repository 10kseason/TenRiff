#include "gameplay/Replay.h"

#include <filesystem>
#include <fstream>
#include <utility>

#include "config/SimpleJson.h"

namespace tenriff::gameplay {

namespace {

std::string input_state_label(input::InputState state) {
    return (state == input::InputState::Pressed) ? "down" : "up";
}

std::string gauge_label(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::Hard: return "hard";
        case game::GaugeType::Easy: return "easy";
        case game::GaugeType::Normal:
        default: return "normal";
    }
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
    obj.emplace("mean_delta_ms", config::JsonValue{stats.mean_delta_ms});
    obj.emplace("stddev_delta_ms", config::JsonValue{stats.stddev_delta_ms()});
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
    obj.emplace("stats", build_stats_json(result_file.stats));
    return save_json_file(path, config::JsonValue{std::move(obj)}, indent);
}

}  // namespace tenriff::gameplay
