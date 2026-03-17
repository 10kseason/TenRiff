#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/MenuRecordUtils.h"
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

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    REQUIRE(file);
    file << content;
    REQUIRE(file.good());
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
    replay.mods = {"judge_easy", "no_ln_release"};
    replay.rate_multiplier = 0.75;
    replay.score_multiplier = 0.75;
    replay.final_score = 1234;

    replay.trace.sample_rate = 48000;
    replay.trace.rate = 1.0;
    replay.trace.lane_count = 10;
    replay.trace.duration_samples = 123456;
    replay.trace.events.push_back(ReplayEvent{1, InputState::Pressed, 100});
    replay.trace.events.push_back(ReplayEvent{1, InputState::Released, 200});

    replay.stats.counts.pg = 1;
    replay.stats.max_combo = 1;
    replay.stats.total_notes = 1;
    replay.stats.raw_score = 1645;
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

    auto mods_it = root->find("mods");
    REQUIRE(mods_it != root->end());
    const auto* mods = mods_it->second.as_array();
    REQUIRE(mods != nullptr);
    REQUIRE(mods->size() == 2u);
    CHECK((*mods)[0].as_string() == "judge_easy");
    CHECK((*mods)[1].as_string() == "no_ln_release");

    CHECK(root->find("rate_multiplier")->second.as_number() == doctest::Approx(0.75));
    CHECK(root->find("score_multiplier")->second.as_number() == doctest::Approx(0.75));
    CHECK(root->find("final_score")->second.as_number() == doctest::Approx(1234.0));

    auto parsed_replay = tenriff::app::menu_records::parse_replay_file(path, nullptr);
    REQUIRE(parsed_replay.has_value());
    CHECK(parsed_replay->mods.size() == 2u);
    CHECK(parsed_replay->rate_multiplier == doctest::Approx(0.75));
    CHECK(parsed_replay->score_multiplier == doctest::Approx(0.75));
    CHECK(parsed_replay->raw_score == 1645);
    CHECK(parsed_replay->final_score == 1234);

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
    result.mods = {"full_short_notes"};
    result.rate_multiplier = 1.05;
    result.score_multiplier = 0.50;
    result.final_score = 900;
    result.stats.counts.pg = 5;
    result.stats.max_combo = 5;
    result.stats.total_notes = 5;
    result.stats.raw_score = 1800;

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

    auto parsed_result = tenriff::app::menu_records::parse_result_file(path, nullptr);
    REQUIRE(parsed_result.has_value());
    CHECK(parsed_result->mods.size() == 1u);
    CHECK(parsed_result->mods[0] == "full_short_notes");
    CHECK(parsed_result->stats.raw_score == 1800);
    CHECK(parsed_result->rate_multiplier == doctest::Approx(1.05));
    CHECK(parsed_result->score_multiplier == doctest::Approx(0.50));
    CHECK(parsed_result->final_score == 900);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("legacy result parsing falls back to derived score metadata") {
    const std::filesystem::path path = "legacy_result_test.json";
    write_file(path,
               "{\n"
               "  \"chart_path\": \"Songs/test.bms\",\n"
               "  \"chart_format\": \"bms\",\n"
               "  \"created_utc\": \"20250101_000000Z\",\n"
               "  \"clear_status\": \"CLEAR\",\n"
               "  \"final_gauge\": \"normal\",\n"
               "  \"game_over\": false,\n"
               "  \"stats\": {\n"
               "    \"counts\": {\"pg\": 1, \"gr\": 1, \"gd\": 1, \"bd\": 1},\n"
               "    \"max_combo\": 3,\n"
               "    \"total_notes\": 4\n"
               "  }\n"
               "}\n");

    auto parsed = tenriff::app::menu_records::parse_result_file(path, nullptr);
    REQUIRE(parsed.has_value());
    CHECK(parsed->mods.empty());
    CHECK(parsed->rate_multiplier == doctest::Approx(1.0));
    CHECK(parsed->score_multiplier == doctest::Approx(1.0));
    CHECK(parsed->stats.raw_score == 1800);
    CHECK(parsed->final_score == 1800);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("legacy replay parsing falls back to derived final score") {
    const std::filesystem::path path = "legacy_replay_test.json";
    write_file(path,
               "{\n"
               "  \"chart_path\": \"Songs/test.bms\",\n"
               "  \"chart_format\": \"bms\",\n"
               "  \"created_utc\": \"20250101_000000Z\",\n"
               "  \"sample_rate\": 48000,\n"
               "  \"rate\": 1.0,\n"
               "  \"trace\": {\n"
               "    \"sample_rate\": 48000,\n"
               "    \"rate\": 1.0,\n"
               "    \"lane_count\": 10,\n"
               "    \"duration_samples\": 123456,\n"
               "    \"events\": [{\"sample\": 100, \"lane\": 1, \"state\": \"down\"}]\n"
               "  },\n"
               "  \"stats\": {\n"
               "    \"counts\": {\"pg\": 1, \"gr\": 1, \"bd\": 1}\n"
               "  }\n"
               "}\n");

    auto parsed = tenriff::app::menu_records::parse_replay_file(path, nullptr);
    REQUIRE(parsed.has_value());
    CHECK(parsed->mods.empty());
    CHECK(parsed->rate_multiplier == doctest::Approx(1.0));
    CHECK(parsed->score_multiplier == doctest::Approx(1.0));
    CHECK(parsed->raw_score == 1500);
    CHECK(parsed->final_score == 1500);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
