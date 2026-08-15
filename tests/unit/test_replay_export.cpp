#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#include "app/MenuRecordUtils.h"
#include "app/ChartFileHash.h"
#include "app/ChartLoader.h"
#include "app/ModeManager.h"
#include "app/ReplayVerifier.h"
#include "config/SimpleJson.h"
#include "gameplay/GameplayChart.h"
#include "gameplay/GameplayEngine.h"
#include "gameplay/Replay.h"

using tenriff::config::parse_json;
using tenriff::gameplay::ReplayEvent;
using tenriff::gameplay::ReplayFile;
using tenriff::gameplay::load_replay_json;
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

TEST_CASE("native rank thresholds match the TenRiff accuracy ladder") {
    const auto rank_for = [](double accuracy) {
        tenriff::gameplay::ResultStats stats;
        stats.counts.pg = 1;
        stats.accuracy_points = accuracy / 100.0;
        stats.accuracy_weight = 1.0;
        return tenriff::app::menu_records::calculate_rank(stats, false);
    };

    CHECK(rank_for(74.999) == "F");
    CHECK(rank_for(75.0) == "B");
    CHECK(rank_for(80.5) == "A");
    CHECK(rank_for(86.5) == "A+");
    CHECK(rank_for(90.0) == "S");
    CHECK(rank_for(95.5) == "S+");
    CHECK(rank_for(98.0) == "AA");
    CHECK(rank_for(99.0) == "SS");
    CHECK(rank_for(99.75) == "SSS");
}
TEST_CASE("replay export writes JSON with trace events") {
    ReplayFile replay;
    replay.chart_path = "Songs/test.bms";
    replay.chart_format = "bms";
    replay.chart_sha256 = std::string(64, 'a');
    replay.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    replay.created_utc = "20250101_000000Z";
    replay.sample_rate = 48000;
    replay.rate = 1.0;
    replay.input_offset_ms = -2.5;
    replay.mods = {"judge_easy", "ln_mix_30", "no_ln_release"};
    replay.rate_multiplier = 0.75;
    replay.score_multiplier = 0.75;
    replay.final_score = 1234;
    replay.pause_used = true;
    replay.mode.key_mode = "6k";
    replay.mode.key_conversion_algorithm = "nk2";
    replay.mode.key_conversion_nk2_preset = "transform";
    replay.mode.random = "mirror";
    replay.mode.random_seed = 4123;
    replay.mode.gauge = "shift";
    replay.mode.autoplay_enabled = true;
    replay.mode.practice_no_fail_enabled = true;
    replay.mode.one_miss_fail_enabled = true;

    replay.trace.sample_rate = 48000;
    replay.trace.rate = 1.0;
    replay.trace.lane_count = 10;
    replay.trace.duration_samples = 123456;
    replay.trace.events.push_back(ReplayEvent{1, InputState::Pressed, 100});
    replay.trace.events.push_back(ReplayEvent{1, InputState::Released, 200});

    replay.stats.counts.pg = 1;
    replay.stats.counts.pr = 2;
    replay.stats.max_combo = 1;
    replay.stats.total_notes = 1;
    replay.stats.raw_score = 1645;
    replay.stats.detail_score = 5;
    replay.stats.accuracy_points = 0.8;
    replay.stats.accuracy_weight = 1.0;
    replay.stats.detailed_accuracy_points = 0.9375;
    replay.stats.detailed_accuracy_weight = 1.0;
    replay.stats.mean_delta_ms = 1.25;
    replay.stats.osu_od8.available = true;
    replay.stats.osu_od8.total_objects = 1;
    replay.stats.osu_od8.judged_objects = 1;
    replay.stats.osu_od8.counts.perfect = 1;
    replay.stats.osu_od8.score = 1'000'000;

    const std::filesystem::path path = "replay_export_test.json";
    auto exported = save_replay_json(path.string(), replay);
    CHECK(exported.success());

    auto parsed = parse_json(read_file(path));
    CHECK(parsed.success());
    REQUIRE(parsed.root.has_value());
    const auto* root = parsed.root->as_object();
    REQUIRE(root != nullptr);
    CHECK(root->find("version")->second.as_number() ==
          doctest::Approx(static_cast<double>(tenriff::gameplay::kNativeScoreVersion)));
    CHECK(root->find("replay_format_version")->second.as_number() ==
          doctest::Approx(static_cast<double>(tenriff::gameplay::kReplayFormatVersion)));
    CHECK(root->find("chart_sha256")->second.as_string() == replay.chart_sha256);
    CHECK(root->find("ruleset_id")->second.as_string() == replay.ruleset_id);

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
    REQUIRE(mods->size() == 3u);
    CHECK((*mods)[0].as_string() == "judge_easy");
    CHECK((*mods)[1].as_string() == "ln_mix_30");
    CHECK((*mods)[2].as_string() == "no_ln_release");

    CHECK(root->find("rate_multiplier")->second.as_number() == doctest::Approx(0.75));
    CHECK(root->find("score_multiplier")->second.as_number() == doctest::Approx(0.75));
    CHECK(root->find("final_score")->second.as_number() == doctest::Approx(1234.0));
    CHECK(root->find("pause_used")->second.as_bool(false));
    auto mode_it = root->find("mode");
    REQUIRE(mode_it != root->end());
    const auto* mode = mode_it->second.as_object();
    REQUIRE(mode != nullptr);
    CHECK(mode->find("key_mode")->second.as_string() == "6k");
    CHECK(mode->find("key_conversion_algorithm")->second.as_string() == "nk2");
    CHECK(mode->find("key_conversion_nk2_preset")->second.as_string() == "transform");
    CHECK(mode->find("key_conversion_note_add_mode") == mode->end());
    CHECK(mode->find("random")->second.as_string() == "mirror");
    CHECK(mode->find("random_seed")->second.as_number() == doctest::Approx(4123.0));
    CHECK(mode->find("gauge")->second.as_string() == "shift");
    CHECK(mode->find("autoplay_enabled")->second.as_bool(false));
    CHECK(mode->find("practice_no_fail_enabled")->second.as_bool(false));
    CHECK(mode->find("one_miss_fail_enabled")->second.as_bool(false));

    auto parsed_replay = tenriff::app::menu_records::parse_replay_file(path, nullptr);
    REQUIRE(parsed_replay.has_value());
    CHECK(parsed_replay->mods.size() == 3u);
    CHECK(parsed_replay->mods[1] == "ln_mix_30");
    CHECK(parsed_replay->rate_multiplier == doctest::Approx(0.75));
    CHECK(parsed_replay->score_multiplier == doctest::Approx(0.75));
    CHECK(parsed_replay->raw_score == 1645);
    CHECK(parsed_replay->final_score == 1234);
    CHECK(parsed_replay->pause_used);
    CHECK(parsed_replay->autoplay_enabled);
    CHECK(parsed_replay->practice_no_fail_enabled);
    CHECK(parsed_replay->one_miss_fail_enabled);

    auto loaded_replay = load_replay_json(path.string());
    REQUIRE(loaded_replay.success());
    REQUIRE(loaded_replay.replay.has_value());
    CHECK(loaded_replay.replay->stats.counts.pr == 2);
    REQUIRE(loaded_replay.replay->mods.size() == 3u);
    CHECK(loaded_replay.replay->mods[1] == "ln_mix_30");
    CHECK(loaded_replay.replay->mode.key_mode == "6k");
    CHECK(loaded_replay.replay->mode.key_conversion_algorithm == "nk2");
    CHECK(loaded_replay.replay->mode.key_conversion_nk2_preset == "transform");
    CHECK(loaded_replay.replay->mode.key_conversion_note_add_mode.empty());
    CHECK(loaded_replay.replay->mode.random == "mirror");
    REQUIRE(loaded_replay.replay->mode.random_seed.has_value());
    CHECK(loaded_replay.replay->mode.random_seed.value() == 4123);
    CHECK(loaded_replay.replay->mode.gauge == "shift");
    CHECK(loaded_replay.replay->mode.autoplay_enabled);
    CHECK(loaded_replay.replay->mode.practice_no_fail_enabled);
    CHECK(loaded_replay.replay->mode.one_miss_fail_enabled);
    CHECK(loaded_replay.replay->stats.osu_od8.available);
    CHECK(loaded_replay.replay->stats.osu_od8.score == 1'000'000);
    CHECK(loaded_replay.replay->stats.osu_od8.counts.perfect == 1);
    CHECK(loaded_replay.replay->stats.detail_score == 5);
    CHECK(loaded_replay.replay->stats.accuracy_percent() == doctest::Approx(80.0));
    CHECK(loaded_replay.replay->stats.detailed_accuracy_percent() == doctest::Approx(93.75));
    CHECK(loaded_replay.replay->pause_used);
    CHECK(loaded_replay.replay->trace.events.size() == 2u);
    CHECK(loaded_replay.replay->trace.events[0].lane == 1);
    CHECK(loaded_replay.replay->trace.events[0].state == InputState::Pressed);
    CHECK(loaded_replay.replay->trace.events[1].state == InputState::Released);
    CHECK(loaded_replay.replay->chart_sha256 == replay.chart_sha256);
    CHECK(loaded_replay.replay->ruleset_id == replay.ruleset_id);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("replay v3 strict validation rejects ambiguous or unbounded traces") {
    ReplayFile replay;
    replay.chart_sha256 = std::string(64, 'b');
    replay.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    replay.sample_rate = 48000;
    replay.rate = 1.0;
    replay.trace.sample_rate = 48000;
    replay.trace.rate = 1.0;
    replay.trace.lane_count = 4;
    replay.trace.duration_samples = 480000;
    replay.trace.events = {
        ReplayEvent{1, InputState::Pressed, 100},
        ReplayEvent{1, InputState::Released, 200},
    };
    CHECK(tenriff::gameplay::validate_replay_evidence(replay).success());

    ReplayFile invalid_lane = replay;
    invalid_lane.trace.events[1].lane = 5;
    CHECK_FALSE(tenriff::gameplay::validate_replay_evidence(invalid_lane).success());

    ReplayFile decreasing_sample = replay;
    decreasing_sample.trace.events[1].sample = 99;
    CHECK_FALSE(tenriff::gameplay::validate_replay_evidence(decreasing_sample).success());

    ReplayFile repeated_state = replay;
    repeated_state.trace.events[1].state = InputState::Pressed;
    CHECK_FALSE(tenriff::gameplay::validate_replay_evidence(repeated_state).success());

    ReplayFile non_finite = replay;
    non_finite.score_multiplier = std::numeric_limits<double>::infinity();
    CHECK_FALSE(tenriff::gameplay::validate_replay_evidence(non_finite).success());
}

TEST_CASE("deterministic replay verification ignores edited score claims") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 1000});

    ReplayFile replay;
    replay.chart_sha256 = std::string(64, 'c');
    replay.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    replay.sample_rate = 48000;
    replay.rate = 1.0;
    replay.mode.key_mode = "auto";
    replay.mode.key_conversion_algorithm = "krrcream";
    replay.mode.random = "off";
    replay.mode.random_seed = 0;
    replay.mode.gauge = "normal";
    replay.trace.sample_rate = 48000;
    replay.trace.rate = 1.0;
    replay.trace.lane_count = 1;
    replay.trace.duration_samples = 146000;
    replay.trace.events = {
        ReplayEvent{1, InputState::Pressed, 145000},
        ReplayEvent{1, InputState::Released, 145010},
    };

    // These are deliberately forged. Eligibility uses the recomputed engine
    // result and never promotes the stored score/stat claims.
    replay.final_score = 1;
    replay.stats.raw_score = 1;
    replay.stats.total_notes = 999;

    const auto verified = tenriff::app::verify_replay_against_chart(
        replay, chart, tenriff::app::ChartFormat::Bms, 120.0);
    CHECK(verified.verified());
    CHECK(verified.official_eligible);
    CHECK_FALSE(verified.claims_match);
    CHECK(verified.stats.counts.pg == 1);
    CHECK(verified.stats.total_notes == 1);
    CHECK(verified.final_score == tenriff::gameplay::kNativeScoreMaximum);

    ReplayFile aborted = replay;
    aborted.aborted = true;
    const auto aborted_result = tenriff::app::verify_replay_against_chart(
        aborted, chart, tenriff::app::ChartFormat::Bms, 120.0);
    CHECK(aborted_result.verified());
    CHECK_FALSE(aborted_result.official_eligible);
    CHECK(aborted_result.clear_status == "ABORTED");

    ReplayFile custom_ruleset = replay;
    custom_ruleset.ruleset_id = "custom";
    const auto custom_result = tenriff::app::verify_replay_against_chart(
        custom_ruleset, chart, tenriff::app::ChartFormat::Bms, 120.0);
    CHECK(custom_result.status == tenriff::app::ReplayVerificationStatus::CustomRuleset);
    CHECK_FALSE(custom_result.official_eligible);
}

TEST_CASE("file replay verification binds the replay and exact chart SHA-256") {
    const std::filesystem::path chart_path = "replay_verifier_chart.bms";
    const std::filesystem::path replay_path = "replay_verifier_trace.json";
    write_file(chart_path,
               "#PLAYER 1\n"
               "#TITLE Replay Verifier\n"
               "#BPM 120\n"
               "#PLAYLEVEL 1\n"
               "#00111:01\n");

    tenriff::app::ChartLoader loader;
    const auto loaded = loader.load(chart_path.string(), 48000, 1.0, "ignore", 0);
    REQUIRE(loaded.success());
    tenriff::config::ModeConfig mode;
    mode.key_mode = "auto";
    mode.random = "off";
    mode.random_seed = 0;
    const auto managed = tenriff::app::manage_modes(
        loaded.chart, loaded.format, mode, tenriff::config::JudgeConfig{}, 1.0,
        loaded.base_bpm, 48000);

    auto played_chart = managed.chart;
    tenriff::gameplay::offset_gameplay_chart_samples(played_chart, 144000);
    tenriff::gameplay::GameplayConfig gameplay_config;
    gameplay_config.sample_rate = 48000;
    gameplay_config.rate = 1.0;
    gameplay_config.judge = managed.judge;
    gameplay_config.gauge_shift_enabled = true;
    gameplay_config.initial_gauge = tenriff::game::GaugeType::Normal;
    tenriff::gameplay::GameplayEngine engine(played_chart, gameplay_config);
    for (const auto& note : played_chart.notes) {
        if (note.start_sample > 0) {
            engine.advance(note.start_sample - 1);
        }
        (void)engine.handle_input(note.lane, InputState::Pressed, note.start_sample);
        (void)engine.handle_input(note.lane, InputState::Released, note.start_sample + 1);
    }
    engine.advance(played_chart.duration_samples + 240000);
    REQUIRE(engine.is_finished());

    std::string hash_error;
    const auto chart_hash = tenriff::app::hash_chart_file(chart_path, &hash_error);
    REQUIRE(chart_hash.valid());
    ReplayFile replay;
    replay.chart_path = chart_path.string();
    replay.chart_format = "bms";
    replay.chart_sha256 = chart_hash.sha256;
    replay.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    replay.sample_rate = 48000;
    replay.rate = 1.0;
    replay.mode.key_mode = "auto";
    replay.mode.key_conversion_algorithm = "krrcream";
    replay.mode.key_conversion_nk2_preset = "native";
    replay.mode.random = "off";
    replay.mode.random_seed = 0;
    replay.mode.gauge = "normal";
    replay.trace = engine.replay();
    replay.stats = engine.stats();
    replay.rate_multiplier = managed.rate_multiplier;
    replay.score_multiplier = managed.final_multiplier;
    replay.final_score = tenriff::gameplay::scale_native_score(
        replay.stats.raw_score, replay.score_multiplier);
    REQUIRE(save_replay_json(replay_path.string(), replay).success());

    const auto original_replay_hash = tenriff::app::hash_chart_file(replay_path, &hash_error);
    REQUIRE(original_replay_hash.valid());
    const auto verified = tenriff::app::verify_replay_file(
        replay_path, chart_path, original_replay_hash.sha256);
    CHECK(verified.verified());
    CHECK(verified.official_eligible);
    CHECK(verified.claims_match);

    replay.final_score = 1;
    replay.stats.raw_score = 1;
    REQUIRE(save_replay_json(replay_path.string(), replay).success());
    const auto edited_replay_hash = tenriff::app::hash_chart_file(replay_path, &hash_error);
    REQUIRE(edited_replay_hash.valid());
    const auto stale_binding = tenriff::app::verify_replay_file(
        replay_path, chart_path, original_replay_hash.sha256);
    CHECK_FALSE(stale_binding.verified());
    const auto recomputed = tenriff::app::verify_replay_file(
        replay_path, chart_path, edited_replay_hash.sha256);
    CHECK(recomputed.verified());
    CHECK_FALSE(recomputed.claims_match);
    CHECK(recomputed.final_score == verified.final_score);

    std::error_code ec;
    std::filesystem::remove(chart_path, ec);
    std::filesystem::remove(replay_path, ec);
}

TEST_CASE("result export writes JSON with replay reference") {
    ResultFile result;
    result.player_name = "Luna Pilot";
    result.chart_path = "Songs/test.bms";
    result.chart_format = "bms";
    result.chart_sha256 = std::string(64, 'd');
    result.ruleset_id = std::string(tenriff::app::kCanonicalReplayRulesetId);
    result.created_utc = "20250101_000000Z";
    result.replay_path = "profiles/default/replays/replay_20250101_000000Z.json";
    result.replay_sha256 = std::string(64, 'e');
    result.clear_status = "CLEAR";
    result.final_gauge = "normal";
    result.sample_rate = 48000;
    result.rate = 1.0;
    result.game_over = false;
    result.mods = {"ln_mix_30"};
    result.rate_multiplier = 1.05;
    result.score_multiplier = 0.50;
    result.final_score = 900;
    result.pause_used = true;
    result.autoplay_enabled = false;
    result.practice_no_fail_enabled = true;
    result.one_miss_fail_enabled = true;
    result.stats.counts.pg = 5;
    result.stats.counts.pr = 3;
    result.stats.max_combo = 5;
    result.stats.total_notes = 5;
    result.stats.raw_score = 1800;
    result.stats.detail_score = 25;
    result.stats.accuracy_points = 4.0;
    result.stats.accuracy_weight = 5.0;
    result.stats.detailed_accuracy_points = 4.75;
    result.stats.detailed_accuracy_weight = 5.0;
    result.stats.osu_od8.available = true;
    result.stats.osu_od8.total_objects = 5;
    result.stats.osu_od8.judged_objects = 5;
    result.stats.osu_od8.score = 876'543;

    const std::filesystem::path path = "result_export_test.json";
    auto exported = save_result_json(path.string(), result);
    CHECK(exported.success());

    auto parsed = parse_json(read_file(path));
    CHECK(parsed.success());
    REQUIRE(parsed.root.has_value());
    const auto* root = parsed.root->as_object();
    REQUIRE(root != nullptr);
    CHECK(root->find("version")->second.as_number() ==
          doctest::Approx(static_cast<double>(tenriff::gameplay::kNativeScoreVersion)));

    auto player_name_it = root->find("player_name");
    REQUIRE(player_name_it != root->end());
    CHECK(player_name_it->second.as_string() == result.player_name);

    auto replay_path_it = root->find("replay_path");
    REQUIRE(replay_path_it != root->end());
    CHECK(replay_path_it->second.as_string() == result.replay_path);
    CHECK(root->find("chart_sha256")->second.as_string() == result.chart_sha256);
    CHECK(root->find("ruleset_id")->second.as_string() == result.ruleset_id);
    CHECK(root->find("replay_sha256")->second.as_string() == result.replay_sha256);
    CHECK(root->find("key_conversion_note_add_mode") == root->end());

    auto clear_status_it = root->find("clear_status");
    REQUIRE(clear_status_it != root->end());
    CHECK(clear_status_it->second.as_string() == result.clear_status);

    auto final_gauge_it = root->find("final_gauge");
    REQUIRE(final_gauge_it != root->end());
    CHECK(final_gauge_it->second.as_string() == result.final_gauge);

    auto game_over_it = root->find("game_over");
    REQUIRE(game_over_it != root->end());
    CHECK(game_over_it->second.as_bool(true) == result.game_over);
    CHECK(root->find("autoplay_enabled")->second.as_bool(true) == result.autoplay_enabled);
    CHECK(root->find("practice_no_fail_enabled")->second.as_bool(false) == result.practice_no_fail_enabled);
    CHECK(root->find("one_miss_fail_enabled")->second.as_bool(false) == result.one_miss_fail_enabled);
    CHECK(root->find("pause_used")->second.as_bool(false));

    auto parsed_result = tenriff::app::menu_records::parse_result_file(path, nullptr);
    REQUIRE(parsed_result.has_value());
    CHECK(parsed_result->player_name == "Luna Pilot");
    CHECK(parsed_result->replay_format_version == tenriff::gameplay::kReplayFormatVersion);
    CHECK(parsed_result->chart_sha256 == result.chart_sha256);
    CHECK(parsed_result->ruleset_id == result.ruleset_id);
    CHECK(parsed_result->replay_sha256 == result.replay_sha256);
    CHECK(parsed_result->mods.size() == 1u);
    CHECK(parsed_result->mods[0] == "ln_mix_30");
    CHECK(parsed_result->key_conversion_note_add_mode.empty());
    CHECK(parsed_result->stats.raw_score == 1800);
    CHECK(parsed_result->stats.counts.pr == 3);
    CHECK(parsed_result->stats.detail_score == 25);
    CHECK(parsed_result->stats.accuracy_percent() == doctest::Approx(80.0));
    CHECK(parsed_result->stats.detailed_accuracy_percent() == doctest::Approx(95.0));
    CHECK(parsed_result->rate_multiplier == doctest::Approx(1.05));
    CHECK(parsed_result->score_multiplier == doctest::Approx(0.50));
    CHECK(parsed_result->final_score == 900);
    CHECK(parsed_result->pause_used);
    CHECK_FALSE(parsed_result->autoplay_enabled);
    CHECK(parsed_result->practice_no_fail_enabled);
    CHECK(parsed_result->one_miss_fail_enabled);
    CHECK(parsed_result->stats.osu_od8.available);
    CHECK(parsed_result->stats.osu_od8.score == 876'543);

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
               "  \"key_conversion_note_add_mode\": \"add_25_plus\",\n"
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
    CHECK(parsed->player_name.empty());
    CHECK(parsed->mods.empty());
    CHECK(parsed->key_conversion_note_add_mode == "add_25_plus");
    CHECK(parsed->rate_multiplier == doctest::Approx(1.0));
    CHECK(parsed->score_multiplier == doctest::Approx(1.0));
    CHECK(parsed->stats.raw_score == 4167);
    CHECK(parsed->final_score == 4167);
    CHECK_FALSE(parsed->pause_used);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("legacy autoplay clear is normalized to an unranked result") {
    const std::filesystem::path path = "legacy_autoplay_result_test.json";
    write_file(path,
               R"JSON({
  "chart_path": "Songs/test.bms",
  "chart_format": "bms",
  "created_utc": "20250101_000000Z",
  "clear_status": "ASSIST AUTOPLAY EX-HARD CLEAR",
  "final_gauge": "ex_hard",
  "game_over": false,
  "stats": {
    "counts": {"pg": 4},
    "max_combo": 4,
    "total_notes": 4
  }
})JSON");

    auto parsed = tenriff::app::menu_records::parse_result_file(path, nullptr);
    REQUIRE(parsed.has_value());
    CHECK(parsed->autoplay_enabled);
    CHECK_FALSE(parsed->practice_no_fail_enabled);
    CHECK(parsed->game_over);
    CHECK(parsed->clear_status == "AUTOPLAY");

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
    CHECK(parsed->raw_score == 5000);
    CHECK(parsed->final_score == 5000);
    CHECK_FALSE(parsed->pause_used);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("legacy replay loading preserves removed conversion Note Add metadata") {
    const std::filesystem::path path = "legacy_replay_loader_test.json";
    write_file(path,
               "{\n"
               "  \"chart_path\": \"Songs/test.bms\",\n"
               "  \"chart_format\": \"bms\",\n"
               "  \"created_utc\": \"20250101_000000Z\",\n"
               "  \"sample_rate\": 48000,\n"
               "  \"rate\": 1.0,\n"
               "  \"mode\": {\"key_conversion_note_add_mode\": \"add_25_plus\"},\n"
               "  \"trace\": {\n"
               "    \"sample_rate\": 48000,\n"
               "    \"rate\": 1.0,\n"
               "    \"lane_count\": 4,\n"
               "    \"duration_samples\": 123456,\n"
               "    \"events\": [\n"
               "      {\"sample\": 100, \"lane\": 1, \"state\": \"down\"},\n"
               "      {\"sample\": 220, \"lane\": 1, \"state\": \"up\"}\n"
               "    ]\n"
               "  },\n"
               "  \"stats\": {\n"
               "    \"counts\": {\"pg\": 2, \"gr\": 1, \"bd\": 0},\n"
               "    \"raw_score\": 2700\n"
               "  }\n"
               "}\n");

    auto loaded = load_replay_json(path.string());
    REQUIRE(loaded.success());
    REQUIRE(loaded.replay.has_value());
    CHECK(loaded.replay->mode.key_mode.empty());
    CHECK(loaded.replay->mode.key_conversion_algorithm.empty());
    CHECK(loaded.replay->mode.key_conversion_nk2_preset.empty());
    CHECK(loaded.replay->mode.key_conversion_note_add_mode == "add_25_plus");
    CHECK(loaded.replay->mode.random.empty());
    CHECK_FALSE(loaded.replay->mode.random_seed.has_value());
    CHECK(loaded.replay->trace.lane_count == 4);
    CHECK(loaded.replay->trace.events.size() == 2u);
    CHECK(loaded.replay->stats.raw_score == 270);
    CHECK(loaded.replay->final_score == 270);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("gauge shift record priority follows the final surviving tier") {
    using tenriff::app::menu_records::clear_status_priority;
    CHECK(clear_status_priority("GAUGE SHIFT EASY CLEAR", false, "easy") == 2);
    CHECK(clear_status_priority("GAUGE SHIFT NORMAL CLEAR", false, "normal") == 3);
    CHECK(clear_status_priority("GAUGE SHIFT HARD CLEAR", false, "hard") == 4);
    CHECK(clear_status_priority("GAUGE SHIFT EX-HARD CLEAR", false, "ex_hard") == 5);
}
TEST_CASE("autoplay result status never receives clear priority") {
    using tenriff::app::menu_records::clear_status_priority;
    CHECK(clear_status_priority("AUTOPLAY", false, "ex_hard") == 0);
    CHECK(clear_status_priority("ASSIST AUTOPLAY EX-HARD CLEAR", false, "ex_hard") == 0);
}

TEST_CASE("assist replays are excluded from default ghost selection") {
    CHECK(tenriff::app::menu_records::default_ghost_replay_allowed(false, false, "HARD CLEAR"));
    CHECK_FALSE(tenriff::app::menu_records::default_ghost_replay_allowed(true, false, "CLEAR"));
    CHECK_FALSE(tenriff::app::menu_records::default_ghost_replay_allowed(false, true, "CLEAR"));
    CHECK_FALSE(tenriff::app::menu_records::default_ghost_replay_allowed(false, false,
                                                                         "ASSIST AUTOPLAY CLEAR"));
    CHECK_FALSE(tenriff::app::menu_records::default_ghost_replay_allowed(false, false, "AUTOPLAY"));
}

TEST_CASE("record ties use detail score before detailed accuracy") {
    using tenriff::app::menu_records::is_better_record;

    CHECK(is_better_record(1000, 3, 11, 90.0, 50, 100, "20250102",
                           1000, 3, 10, 99.0, 50, 100, "20250101"));
    CHECK(is_better_record(1000, 3, 11, 95.0, 50, 100, "20250102",
                           1000, 3, 11, 90.0, 50, 100, "20250101"));
    CHECK_FALSE(is_better_record(1000, 3, 10, 100.0, 50, 100, "20250102",
                                 1000, 3, 11, 90.0, 50, 100, "20250101"));
}
