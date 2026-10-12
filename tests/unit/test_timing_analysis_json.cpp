#include "doctest/doctest.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#include "app/MenuRecordUtils.h"
#include "config/SimpleJson.h"
#include "gameplay/Replay.h"
#include "gameplay/TimingAnalysisJson.h"

namespace {

using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonValue;
using tenriff::game::Judgement;
using tenriff::gameplay::TimingAnalysis;
using tenriff::gameplay::TimingEventKind;
using tenriff::gameplay::build_timing_analysis_json;
using tenriff::gameplay::parse_timing_analysis_json;

TimingAnalysis example_analysis() {
    TimingAnalysis analysis;
    analysis.initialize(2, 48000, 1'234'567);
    analysis.observe(TimingEventKind::Tap, 1, 100, Judgement::GR, -20.0, 1.0);
    analysis.observe(TimingEventKind::Tap, 2, 200000, Judgement::GR, 20.0, 1.0);
    analysis.observe(TimingEventKind::LnHead, 1, 300000, Judgement::PR, std::nullopt, 0.5);
    analysis.observe(TimingEventKind::LnHead, 2, 400000, Judgement::PG, 0.0, 0.5);
    analysis.observe(TimingEventKind::HoldFailure, 2, 600000, Judgement::BD, -400.0, 0.5);
    analysis.observe(TimingEventKind::LnRelease, 1, 700000, Judgement::PR, std::nullopt, 0.5);
    analysis.observe(TimingEventKind::AutoCompletion, 1, 800000, Judgement::PG, std::nullopt, 0.5);
    analysis.observe(TimingEventKind::EmptyPoor, 2, 900000, Judgement::PR, std::nullopt, 1.0);
    return analysis;
}

tenriff::gameplay::ReplayFile example_replay() {
    tenriff::gameplay::ReplayFile replay;
    replay.chart_path = "Songs/analysis.bms";
    replay.chart_sha256 = std::string(64, 'a');
    replay.ruleset_id = "custom";
    replay.sample_rate = replay.trace.sample_rate = 48000;
    replay.trace.lane_count = 2;
    replay.trace.duration_samples = 1'234'567;
    return replay;
}

JsonObject& object(JsonValue& value) { return std::get<JsonObject>(value.value); }
JsonArray& array(JsonValue& value) { return std::get<JsonArray>(value.value); }

struct TestFile {
    std::filesystem::path path;
    ~TestFile() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    JsonValue read() const {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        auto parsed = tenriff::config::parse_json(buffer.str());
        REQUIRE(parsed.success());
        return std::move(*parsed.root);
    }
    void write(const JsonValue& value) const {
        std::ofstream output(path, std::ios::binary);
        output << tenriff::config::json_stringify(value, 2, std::numeric_limits<double>::max_digits10);
        REQUIRE(output.good());
    }
};

}  // namespace

TEST_CASE("timing analysis JSON retains measured distributions and missing timings separately") {
    const auto source = example_analysis();
    const auto value = build_timing_analysis_json(source);
    const auto text = tenriff::config::json_stringify(value, 0, std::numeric_limits<double>::max_digits10);
    auto parsed = tenriff::config::parse_json(text);
    REQUIRE(parsed.success());
    const auto analysis = parse_timing_analysis_json(&*parsed.root);
    REQUIRE(analysis.available);
    CHECK(analysis.lane_count == 2);
    CHECK(analysis.duration_samples == 1'234'567);
    CHECK(analysis.total.event_count == 8);
    CHECK(analysis.total.measured_count == 4);
    CHECK(analysis.total.unmeasured_miss_count == 2);
    CHECK(analysis.total.mean_delta_ms == source.total.mean_delta_ms);
    CHECK(analysis.total.mean_abs_delta_ms == source.total.mean_abs_delta_ms);
    CHECK(analysis.total.m2_delta_ms == source.total.m2_delta_ms);
    CHECK(analysis.total.histogram == source.total.histogram);
    const auto& taps = analysis.by_kind[static_cast<std::size_t>(TimingEventKind::Tap)];
    CHECK(taps.measured_count == 2);
    CHECK(taps.mean_delta_ms == 0.0);
    CHECK(taps.mean_abs_delta_ms == 20.0);
    CHECK(taps.stddev_delta_ms() == doctest::Approx(std::sqrt(800.0)));
    CHECK(analysis.by_lane[0].unmeasured_miss_count == 2);
    CHECK(analysis.by_kind[static_cast<std::size_t>(TimingEventKind::AutoCompletion)].measured_count == 0);
    for (std::size_t i = 0; i < analysis.segments.size(); ++i) {
        CHECK(analysis.segments[i].event_count == source.segments[i].event_count);
        CHECK(analysis.segments[i].accuracy_points == source.segments[i].accuracy_points);
        CHECK(analysis.segments[i].accuracy_weight == source.segments[i].accuracy_weight);
    }
}

TEST_CASE("timing analysis JSON preserves seven digit counters without decimal rounding") {
    TimingAnalysis source;
    source.initialize(1, 48000, 1'234'567);
    source.observe(TimingEventKind::Tap, 1, 1, Judgement::GR, 20.0, 1.0);
    auto distribution = source.total;
    constexpr uint64_t count = 1'234'567;
    distribution.event_count = distribution.measured_count = distribution.late_count = distribution.slow_count = count;
    for (auto& bin : distribution.histogram) bin *= count;
    source.total = source.by_lane[0] = source.by_kind[0] = distribution;
    source.segments[0].event_count = count;
    source.segments[0].accuracy_points = count * 0.8;
    source.segments[0].accuracy_weight = static_cast<double>(count);
    auto replay = example_replay();
    replay.stats.analysis = source;
    const TestFile file{"timing_analysis_large_replay_test.json"};
    REQUIRE(tenriff::gameplay::save_replay_json(file.path.u8string(), replay).success());
    const auto loaded = tenriff::gameplay::load_replay_json(file.path.u8string());
    REQUIRE(loaded.success());
    REQUIRE(loaded.replay->stats.analysis.available);
    CHECK(loaded.replay->stats.analysis.total.measured_count == count);
    CHECK(loaded.replay->stats.analysis.total.histogram[25] == count);
    CHECK(loaded.replay->stats.analysis.duration_samples == 1'234'567);
}

TEST_CASE("timing analysis missing old or unknown schema remains unavailable") {
    CHECK_FALSE(parse_timing_analysis_json(nullptr).available);
    auto value = build_timing_analysis_json(TimingAnalysis{});
    CHECK(value.is_null());
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    object(value)["version"] = JsonValue{2.0};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    object(value).erase("version");
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
}

TEST_CASE("timing analysis parser rejects malformed or excessive dimensions") {
    for (const auto lanes : {-1.0, 0.0, 2.5, 33.0, 1e300}) {
        auto value = build_timing_analysis_json(example_analysis());
        object(value)["lane_count"] = JsonValue{lanes};
        CHECK_FALSE(parse_timing_analysis_json(&value).available);
    }
    auto value = build_timing_analysis_json(example_analysis());
    array(object(value)["by_lane"]).pop_back();
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    array(object(value)["segments"]).push_back(JsonValue{});
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    array(object(object(value)["total"])["histogram"]).pop_back();
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    object(object(value)["by_kind"]).erase("ln_release");
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    object(value)["duration_samples"] = JsonValue{9007199254740992.0};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
}

TEST_CASE("timing analysis rejects nonfinite negative fractional and inconsistent measurements") {
    for (const auto bad : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), -1.0, 1e300}) {
        auto value = build_timing_analysis_json(example_analysis());
        object(object(value)["total"])["mean_abs_delta_ms"] = JsonValue{bad};
        CHECK_FALSE(parse_timing_analysis_json(&value).available);
    }
    for (const auto bad : {-1.0, 0.5, 9007199254740992.0}) {
        auto value = build_timing_analysis_json(example_analysis());
        object(object(value)["total"])["measured_count"] = JsonValue{bad};
        CHECK_FALSE(parse_timing_analysis_json(&value).available);
    }
    auto value = build_timing_analysis_json(example_analysis());
    object(object(value)["total"])["measured_count"] = JsonValue{"4"};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    object(object(value)["total"])["unmeasured_miss_count"] = JsonValue{5.0};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    array(object(object(value)["total"])["histogram"])[0] = JsonValue{0.0};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    object(array(object(value)["by_lane"])[0])["event_count"] = JsonValue{5.0};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    value = build_timing_analysis_json(example_analysis());
    object(array(object(value)["segments"])[0])["accuracy_points"] = JsonValue{4.0};
    CHECK_FALSE(parse_timing_analysis_json(&value).available);
    auto invalid = example_analysis();
    invalid.total.mean_delta_ms = std::numeric_limits<double>::quiet_NaN();
    CHECK(build_timing_analysis_json(invalid).is_null());
}

TEST_CASE("saved result and replay analytics are optional and never replace official scores") {
    tenriff::gameplay::ResultFile result;
    result.chart_path = "Songs/analysis.bms";
    result.final_score = 8765;
    result.stats.raw_score = 8765;
    result.stats.counts.pg = 3;
    result.stats.analysis = example_analysis();
    const TestFile file{"timing_analysis_saved_result_test.json"};
    REQUIRE(tenriff::gameplay::save_result_json(file.path.u8string(), result).success());
    auto value = file.read();
    CHECK(object(value)["version"].as_number() == tenriff::gameplay::kNativeScoreVersion);
    CHECK(object(value)["replay_format_version"].as_number() == tenriff::gameplay::kReplayFormatVersion);
    std::string error;
    auto loaded = tenriff::app::menu_records::parse_result_file(file.path, &error);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->stats.analysis.available);
    CHECK(loaded->stats.analysis.total.measured_count == 4);
    CHECK(loaded->final_score == 8765);
    CHECK(loaded->stats.counts.pg == 3);
    auto& stats = object(object(value)["stats"]);
    object(stats["analysis_v1"])["version"] = JsonValue{999.0};
    file.write(value);
    loaded = tenriff::app::menu_records::parse_result_file(file.path, &error);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->stats.analysis.available);
    CHECK(loaded->final_score == 8765);
    stats.erase("analysis_v1");
    file.write(value);
    loaded = tenriff::app::menu_records::parse_result_file(file.path, &error);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->stats.analysis.available);
    CHECK(loaded->stats.counts.pg == 3);

    auto replay = example_replay();
    replay.chart_path = result.chart_path;
    replay.final_score = result.final_score;
    replay.stats = result.stats;
    const TestFile replay_file{"timing_analysis_optional_replay_test.json"};
    REQUIRE(tenriff::gameplay::save_replay_json(replay_file.path.u8string(), replay).success());
    auto replay_value = replay_file.read();
    object(object(object(replay_value)["stats"])["analysis_v1"])["version"] = JsonValue{999.0};
    replay_file.write(replay_value);
    auto loaded_replay = tenriff::gameplay::load_replay_json(replay_file.path.u8string());
    REQUIRE(loaded_replay.success());
    CHECK_FALSE(loaded_replay.replay->stats.analysis.available);
    CHECK(loaded_replay.replay->final_score == 8765);
    CHECK(loaded_replay.replay->stats.counts.pg == 3);
}
