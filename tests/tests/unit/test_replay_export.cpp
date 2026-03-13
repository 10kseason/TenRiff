#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "config/SimpleJson.h"
#include "gameplay/Replay.h"

using tenriff::config::parse_json;
using tenriff::gameplay::ReplayEvent;
using tenriff::gameplay::ReplayFile;
using tenriff::gameplay::ResultFile;
using tenriff::gameplay::save_replay_json;
using tenriff::gameplay::save_result_json;
using tenriff::input::InputState;

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace

TEST_CASE("replay export writes JSON with trace events") {
    ReplayFile replay;
    replay.chart_path = "Songs/test.bms";
    replay.chart_format = "bms";
    replay.created_utc = "20250101_000000Z";
    replay.sample_rate = 48000;
    replay.rate = 1.0;
    replay.input_offset_ms = -2.5;

    replay.trace.sample_rate = 48000;
    replay.trace.rate = 1.0;
    replay.trace.lane_count = 10;
    replay.trace.duration_samples = 123456;
    replay.trace.events.push_back(ReplayEvent{1, InputState::Pressed, 100});
    replay.trace.events.push_back(ReplayEvent{1, InputState::Released, 200});

    replay.stats.counts.pg = 1;
    replay.stats.max_combo = 1;
    replay.stats.total_notes = 1;
    replay.stats.mean_delta_ms = 1.25;

    const std::filesystem::path path = "replay_export_test.json";
    auto exported = save_replay_json(path.string(), replay);
    CHECK(exported.success());

    auto parsed = parse_json(read_file(path));
    CHECK(parsed.success());
    REQUIRE(parsed.root.has_value());
    const auto* root = parsed.root->as_object();
    REQUIRE(root != nullptr);

    auto trace_it = root->find("trace");
    REQUIRE(trace_it != root->end());
    const auto* trace = trace_it->second.as_object();
    REQUIRE(trace != nullptr);

    auto events_it = trace->find("events");
    REQUIRE(events_it != trace->end());
    const auto* events = events_it->second.as_array();
    REQUIRE(events != nullptr);
    CHECK(events->size() == 2);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("result export writes JSON with replay reference") {
    ResultFile result;
    result.chart_path = "Songs/test.bms";
    result.chart_format = "bms";
    result.created_utc = "20250101_000000Z";
    result.replay_path = "profiles/default/replays/replay_20250101_000000Z.json";
    result.clear_status = "CLEAR";
    result.final_gauge = "normal";
    result.sample_rate = 48000;
    result.rate = 1.0;
    result.game_over = false;
    result.stats.counts.pg = 5;
    result.stats.max_combo = 5;
    result.stats.total_notes = 5;

    const std::filesystem::path path = "result_export_test.json";
    auto exported = save_result_json(path.string(), result);
    CHECK(exported.success());

    auto parsed = parse_json(read_file(path));
    CHECK(parsed.success());
    REQUIRE(parsed.root.has_value());
    const auto* root = parsed.root->as_object();
    REQUIRE(root != nullptr);

    auto replay_path_it = root->find("replay_path");
    REQUIRE(replay_path_it != root->end());
    CHECK(replay_path_it->second.as_string() == result.replay_path);

    auto clear_status_it = root->find("clear_status");
    REQUIRE(clear_status_it != root->end());
    CHECK(clear_status_it->second.as_string() == result.clear_status);

    auto final_gauge_it = root->find("final_gauge");
    REQUIRE(final_gauge_it != root->end());
    CHECK(final_gauge_it->second.as_string() == result.final_gauge);

    auto game_over_it = root->find("game_over");
    REQUIRE(game_over_it != root->end());
    CHECK(game_over_it->second.as_bool(true) == result.game_over);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
