#include "doctest/doctest.h"

#include <cmath>
#include <limits>
#include <numeric>
#include <type_traits>

#include "gameplay/GameplayEngine.h"
#include "gameplay/TimingAnalysis.h"
#include "gameplay/TimingAnalysisJson.h"

namespace {

using namespace tenriff::gameplay;
using tenriff::game::Judgement;
using tenriff::input::InputState;

GameplayConfig analysis_config() {
    GameplayConfig config;
    config.sample_rate = 1000;
    config.practice_no_fail_enabled = true;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.indirect_miss_ms = 340.0;
    config.judge.hold_break_ms = 100.0;
    config.judge.mask_ms = 0.0;
    return config;
}

const TimingDistribution& kind(const TimingAnalysis& analysis, TimingEventKind value) {
    return analysis.by_kind[static_cast<std::size_t>(value)];
}

void check_accuracy_matches(const ResultStats& stats) {
    double points = 0.0;
    double weight = 0.0;
    uint64_t events = 0;
    for (const auto& segment : stats.analysis.segments) {
        points += segment.accuracy_points;
        weight += segment.accuracy_weight;
        events += segment.event_count;
    }
    CHECK(points == doctest::Approx(stats.accuracy_points));
    CHECK(weight == doctest::Approx(stats.accuracy_weight));
    CHECK(events == stats.analysis.total.event_count);
    CHECK(events == static_cast<uint64_t>(stats.counts.pg + stats.counts.gr + stats.counts.gd +
                                         stats.counts.bd + stats.counts.pr));
}

}  // namespace

TEST_CASE("timing analysis uses fixed storage and rejects unavailable dimensions") {
    static_assert(std::is_trivially_copyable_v<TimingAnalysis>);
    static_assert(std::is_nothrow_default_constructible_v<TimingAnalysis>);
    static_assert(noexcept(TimingAnalysis{}.observe(TimingEventKind::Tap, 1, 0, Judgement::PG, 0.0, 1.0)));
    TimingAnalysis analysis;
    CHECK_FALSE(analysis.available);
    analysis.observe(TimingEventKind::Tap, 1, 0, Judgement::PG, 0.0, 1.0);
    CHECK(analysis.total.event_count == 0);
    for (const int lanes : {0, 33}) {
        analysis.initialize(lanes, 1000, 12000);
        CHECK_FALSE(analysis.available);
    }
    analysis.initialize(32, 1000, 12000);
    analysis.observe(TimingEventKind::Tap, 32, 11999, Judgement::PG, 0.0, 1.0);
    analysis.observe(TimingEventKind::Tap, 33, 11999, Judgement::PG, 0.0, 1.0);
    CHECK(analysis.total.measured_count == 1);
    CHECK(analysis.by_lane[31].measured_count == 1);
    analysis.initialize(1, 0, 12000);
    CHECK_FALSE(analysis.available);
    CHECK(analysis.total.event_count == 0);
}

TEST_CASE("timing distribution separates all timing signs from non PG FAST SLOW") {
    TimingAnalysis analysis;
    analysis.initialize(2, 1000, 12000);
    analysis.observe(TimingEventKind::Tap, 1, 1000, Judgement::PG, -10.0, 1.0);
    analysis.observe(TimingEventKind::LnHead, 2, 2000, Judgement::GR, 20.0, 0.5);
    analysis.observe(TimingEventKind::LnRelease, 2, 3000, Judgement::GD, -30.0, 0.5);
    const auto& total = analysis.total;
    CHECK(total.event_count == 3);
    CHECK(total.measured_count == 3);
    CHECK(total.mean_delta_ms == doctest::Approx(-20.0 / 3.0));
    CHECK(total.mean_abs_delta_ms == doctest::Approx(20.0));
    CHECK(total.stddev_delta_ms() == doctest::Approx(std::sqrt(1900.0 / 3.0)));
    CHECK(total.early_count == 2);
    CHECK(total.late_count == 1);
    CHECK(total.fast_count == 1);
    CHECK(total.slow_count == 1);
    CHECK(analysis.by_lane[0].measured_count == 1);
    CHECK(analysis.by_lane[1].measured_count == 2);
    CHECK(kind(analysis, TimingEventKind::LnRelease).mean_delta_ms == -30.0);
    analysis.observe(TimingEventKind::Tap, 1, 4000, Judgement::BD, -0.05, 1.0);
    analysis.observe(TimingEventKind::Tap, 1, 4000, Judgement::BD, 0.05, 1.0);
    analysis.observe(TimingEventKind::Tap, 1, 4000, Judgement::PG, 0.0, 1.0);
    CHECK(total.center_count == 3);
    CHECK(total.fast_count == 1);
    CHECK(total.slow_count == 1);
}

TEST_CASE("timing histogram boundaries include tails without clamping measured values") {
    TimingAnalysis analysis;
    analysis.initialize(1, 1000, 12000);
    for (const double delta : {-101.0, -100.0, -95.0, 0.0, 99.99, 100.0, 500.0}) {
        analysis.observe(TimingEventKind::Tap, 1, 0, Judgement::BD, delta, 1.0);
    }
    const auto& bins = analysis.total.histogram;
    CHECK(bins[0] == 1);
    CHECK(bins[1] == 1);
    CHECK(bins[2] == 1);
    CHECK(bins[21] == 1);
    CHECK(bins[40] == 1);
    CHECK(bins[41] == 2);
    CHECK(std::accumulate(bins.begin(), bins.end(), uint64_t{0}) == analysis.total.measured_count);
    CHECK(analysis.total.mean_delta_ms > 50.0);
}

TEST_CASE("unmeasured events cannot become synthetic zero timing samples") {
    TimingAnalysis analysis;
    analysis.initialize(1, 1000, 12000);
    analysis.observe(TimingEventKind::Tap, 1, 1000, Judgement::BD, std::nullopt, 1.0);
    analysis.observe(TimingEventKind::LnHead, 1, 2000, Judgement::PR,
                     std::numeric_limits<double>::quiet_NaN(), 1.0);
    analysis.observe(TimingEventKind::LnRelease, 1, 3000, Judgement::BD, std::nullopt, 0.5);
    // Even an erroneous caller-supplied zero is discarded for non-measured kinds.
    analysis.observe(TimingEventKind::AutoCompletion, 1, 4000, Judgement::PG, 0.0, 0.5);
    analysis.observe(TimingEventKind::EmptyPoor, 1, 5000, Judgement::PR, 0.0, 1.0);
    CHECK(analysis.total.event_count == 5);
    CHECK(analysis.total.measured_count == 0);
    CHECK(analysis.total.unmeasured_miss_count == 3);
    CHECK(analysis.total.stddev_delta_ms() == 0.0);
    CHECK(std::accumulate(analysis.total.histogram.begin(), analysis.total.histogram.end(), uint64_t{0}) == 0);
}

TEST_CASE("analysis accuracy segments use chart time native weights and retain empty segments") {
    TimingAnalysis analysis;
    analysis.initialize(1, 1000, 12000);
    analysis.observe(TimingEventKind::LnHead, 1, 999, Judgement::PG, 0.0, 0.5);
    analysis.observe(TimingEventKind::LnRelease, 1, 1000, Judgement::GR, 20.0, 0.5);
    analysis.observe(TimingEventKind::EmptyPoor, 1, 1000, Judgement::PR, std::nullopt, 1.0);
    analysis.observe(TimingEventKind::Tap, 1, -500, Judgement::GD, -20.0, 1.0);
    analysis.observe(TimingEventKind::Tap, 1, 14000, Judgement::BD, std::nullopt, 1.0);
    CHECK(analysis.segments[0].accuracy_percent() == doctest::Approx(100.0 / 1.5));
    CHECK(analysis.segments[1].accuracy_percent() == doctest::Approx(40.0 / 1.5));
    CHECK(analysis.segments[11].accuracy_percent() == doctest::Approx(20.0));
    CHECK(analysis.segments[2].event_count == 0);
    CHECK(analysis.segments[2].accuracy_weight == 0.0);
    CHECK(analysis.segments[2].accuracy_percent() == 0.0);
}

TEST_CASE("engine standard LN automatic tail preserves score but is not a measured zero") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 12000;
    chart.notes = {NoteEvent{2, 1000, 2000, false}};
    GameplayEngine engine(chart, analysis_config());
    (void)engine.handle_input(2, InputState::Pressed, 1010);
    engine.advance(2000);
    const auto& stats = engine.stats();
    REQUIRE(stats.analysis.available);
    CHECK(stats.counts.pg == 2);
    CHECK(stats.raw_score == 10000);
    CHECK(stats.max_combo == 2);
    CHECK(stats.delta_samples == 2); // Historical ResultStats still includes automatic tails.
    CHECK(stats.mean_delta_ms == doctest::Approx(5.0));
    CHECK(stats.analysis.total.event_count == 2);
    CHECK(stats.analysis.total.measured_count == 1);
    CHECK(stats.analysis.total.mean_delta_ms == doctest::Approx(10.0));
    CHECK(kind(stats.analysis, TimingEventKind::AutoCompletion).event_count == 1);
    CHECK(stats.analysis.total.unmeasured_miss_count == 0);
    CHECK(stats.analysis.by_lane[1].event_count == 2);
    CHECK(stats.analysis.segments[1].event_count == 1);
    CHECK(stats.analysis.segments[2].event_count == 1);
    CHECK(engine.replay().events.size() == 1);
    check_accuracy_matches(stats);
}

TEST_CASE("engine measured LN releases distinguish graded tails from dropped holds") {
    for (const bool charge : {false, true}) {
        for (const int release : {1975, 1500}) {
            GameplayChart chart;
            chart.lane_count = 1;
            chart.duration_samples = 12000;
            chart.notes = {NoteEvent{1, 1000, 2000, charge}};
            GameplayEngine engine(chart, analysis_config());
            (void)engine.handle_input(1, InputState::Pressed, 1000);
            engine.sync_input_state(1, InputState::Released, release);
            (void)engine.handle_input(1, InputState::Pressed, release + 1);
            engine.advance(2500);
            const auto& analysis = engine.stats().analysis;
            const bool dropped = release == 1500 || !charge;
            const auto expected_kind = dropped ? TimingEventKind::HoldFailure : TimingEventKind::LnRelease;
            CHECK(analysis.total.event_count == 2);
            CHECK(analysis.total.measured_count == 2);
            CHECK(analysis.total.unmeasured_miss_count == 0);
            CHECK(kind(analysis, expected_kind).event_count == 1);
            CHECK(kind(analysis, expected_kind).mean_delta_ms == release - 2000);
            CHECK(kind(analysis, TimingEventKind::AutoCompletion).event_count == 0);
            CHECK(engine.stats().counts.pg == 1);
            CHECK(engine.stats().counts.gd == (!dropped ? 1 : 0));
            CHECK(engine.stats().counts.bd == (dropped ? 1 : 0));
            check_accuracy_matches(engine.stats());
        }
    }
}

TEST_CASE("engine required-release timeout is an unmeasured release miss at its target segment") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 12000;
    chart.notes = {NoteEvent{1, 1000, 2000, true}};
    GameplayEngine engine(chart, analysis_config());
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(2100);
    CHECK(engine.stats().analysis.total.event_count == 1);
    engine.advance(2101);
    const auto& analysis = engine.stats().analysis;
    CHECK(analysis.total.event_count == 2);
    CHECK(analysis.total.measured_count == 1);
    CHECK(kind(analysis, TimingEventKind::LnRelease).unmeasured_miss_count == 1);
    CHECK(kind(analysis, TimingEventKind::LnRelease).measured_count == 0);
    CHECK(analysis.segments[2].event_count == 1);
    CHECK(engine.stats().counts.bd == 1);
    check_accuracy_matches(engine.stats());
}

TEST_CASE("engine missed heads keep tap and LN identities without inventing tail samples") {
    for (const bool hard : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 2;
        chart.duration_samples = 12000;
        chart.notes = {NoteEvent{1, 1000, std::nullopt}, NoteEvent{2, 1000, 2000, false}};
        auto config = analysis_config();
        config.judge.indirect_miss_enabled = hard;
        GameplayEngine engine(chart, config);
        engine.advance(1340);
        CHECK(engine.stats().analysis.total.event_count == 0);
        engine.advance(1341);
        engine.advance(3000);
        const auto& analysis = engine.stats().analysis;
        CHECK(analysis.total.event_count == 2);
        CHECK(analysis.total.unmeasured_miss_count == 2);
        CHECK(analysis.total.measured_count == 0);
        CHECK(kind(analysis, TimingEventKind::Tap).unmeasured_miss_count == 1);
        CHECK(kind(analysis, TimingEventKind::LnHead).unmeasured_miss_count == 1);
        CHECK(kind(analysis, TimingEventKind::AutoCompletion).event_count == 0);
        CHECK(engine.stats().counts.bd == (hard ? 0 : 2));
        CHECK(engine.stats().counts.pr == (hard ? 2 : 0));
        check_accuracy_matches(engine.stats());
    }
}

TEST_CASE("engine empty poor and dense recovery retain native accuracy and trace") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 12000;
    chart.notes = {NoteEvent{1, 1000, std::nullopt}, NoteEvent{1, 1100, std::nullopt}};
    GameplayEngine engine(chart, analysis_config());
    (void)engine.handle_input(1, InputState::Pressed, 100);
    (void)engine.handle_input(1, InputState::Released, 101);
    (void)engine.handle_input(1, InputState::Pressed, 1100);
    const auto& stats = engine.stats();
    CHECK(stats.counts.pr == 1);
    CHECK(stats.counts.bd == 1);
    CHECK(stats.counts.pg == 1);
    CHECK(stats.analysis.total.event_count == 3);
    CHECK(stats.analysis.total.measured_count == 1);
    CHECK(stats.analysis.total.unmeasured_miss_count == 1);
    CHECK(kind(stats.analysis, TimingEventKind::EmptyPoor).event_count == 1);
    CHECK(kind(stats.analysis, TimingEventKind::Tap).event_count == 2);
    CHECK(stats.raw_score == 5000);
    CHECK(stats.accuracy_percent() == doctest::Approx(60.0));
    REQUIRE(engine.replay().events.size() == 3);
    CHECK(engine.replay().events[0].sample == 100);
    CHECK(engine.replay().events[1].sample == 101);
    CHECK(engine.replay().events[2].sample == 1100);
    check_accuracy_matches(stats);
}

TEST_CASE("empty POOR analysis keeps events and roundtrips zero accuracy segments") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 12000;
    chart.notes = {NoteEvent{1, 1000, std::nullopt}};
    for (const bool legacy : {false, true}) {
        auto config = analysis_config();
        config.empty_poor_affects_accuracy = legacy;
        GameplayEngine engine(chart, config);
        (void)engine.handle_input(1, InputState::Pressed, 100);
        CHECK(engine.stats().analysis.total.event_count == 1);
        check_accuracy_matches(engine.stats());
        const auto json = build_timing_analysis_json(engine.stats().analysis);
        const auto restored = parse_timing_analysis_json(&json);
        CHECK(restored.available);
        CHECK(restored.total.event_count == 1);
        CHECK(restored.segments[0].event_count == 1);
        CHECK(restored.segments[0].accuracy_weight == (legacy ? 1.0 : 0.0));
    }
}

TEST_CASE("analysis observes historical deferred LN policy without modernizing R1 R2") {
    for (const bool charge : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 1;
        chart.duration_samples = 12000;
        chart.notes = {NoteEvent{1, 1000, 2000, charge}};
        auto config = analysis_config();
        config.legacy_hold_release = true;
        config.chronological_judgement = false;
        GameplayEngine engine(chart, config);
        engine.sync_input_state(1, InputState::Released, 0);
        CHECK(engine.replay().events.empty());
        (void)engine.handle_input(1, InputState::Pressed, 1000);
        (void)engine.handle_input(1, InputState::Released, 1500);
        (void)engine.handle_input(1, InputState::Pressed, 1600);
        CHECK(engine.stats().analysis.total.event_count == 1);
        if (charge) {
            (void)engine.handle_input(1, InputState::Released, 2000);
        }
        engine.advance(2101);
        CHECK(engine.stats().counts.pg == 2);
        CHECK(kind(engine.stats().analysis, TimingEventKind::HoldFailure).event_count == 0);
        CHECK(kind(engine.stats().analysis, charge ? TimingEventKind::LnRelease : TimingEventKind::AutoCompletion).event_count == 1);
        CHECK(engine.stats().analysis.total.measured_count == (charge ? 2 : 1));
        check_accuracy_matches(engine.stats());
    }
}

TEST_CASE("analysis does not fill future song segments after early game over") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 12000;
    chart.notes = {NoteEvent{1, 1000, std::nullopt}, NoteEvent{1, 11000, std::nullopt}};
    auto config = analysis_config();
    config.one_miss_fail_enabled = true;
    GameplayEngine engine(chart, config);
    engine.advance(1341);
    REQUIRE(engine.is_game_over());
    engine.advance(12000);
    CHECK(engine.stats().analysis.total.event_count == 1);
    CHECK(engine.stats().analysis.segments[11].event_count == 0);
    CHECK(engine.stats().analysis.segments[11].accuracy_weight == 0.0);
    check_accuracy_matches(engine.stats());
}
