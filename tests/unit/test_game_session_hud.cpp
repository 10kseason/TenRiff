#include "doctest/doctest.h"

#include <optional>

#include "app/JudgementLoopTiming.h"
#include "app/GameplayHudRevisions.h"
#include "app/GameplayHudWindow.h"
#include "gameplay/GameplayChart.h"

namespace {

int64_t note_visible_end_sample(const tenriff::gameplay::NoteEvent& note) {
    return note.end_sample.value_or(note.start_sample);
}

bool note_is_expired_for_hud(const tenriff::gameplay::NoteEvent& note, int64_t current_sample, int64_t past_samples) {
    return note_visible_end_sample(note) < current_sample - past_samples;
}

}  // namespace

TEST_CASE("short note expires once its head leaves the HUD past window") {
    tenriff::gameplay::NoteEvent note{1, 1000, std::nullopt};

    CHECK_FALSE(note_is_expired_for_hud(note, 1170, 180));
    CHECK(note_is_expired_for_hud(note, 1181, 180));
}

TEST_CASE("hold note stays visible until its tail leaves the HUD past window") {
    tenriff::gameplay::NoteEvent note{1, 1000, 2000};

    CHECK_FALSE(note_is_expired_for_hud(note, 2100, 180));
    CHECK_FALSE(note_is_expired_for_hud(note, 2180, 180));
    CHECK(note_is_expired_for_hud(note, 2181, 180));
}

TEST_CASE("display offset expands HUD lookahead so notes do not spawn late") {
    const auto window = tenriff::app::expand_gameplay_hud_window(1000, 180, 2200, 120.0, 24.0);

    CHECK(window.past_samples == 204);
    CHECK(window.lookahead_samples == 2344);
}

TEST_CASE("negative display offset keeps past notes alive longer in the HUD") {
    const auto window = tenriff::app::expand_gameplay_hud_window(1000, 180, 2200, -150.0, 24.0);

    CHECK(window.past_samples == 354);
    CHECK(window.lookahead_samples == 2224);
}

TEST_CASE("gameplay chart sample offsets move notes audio and duration together") {
    tenriff::gameplay::GameplayChart chart;
    chart.duration_samples = 4000;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(tenriff::gameplay::NoteEvent{2, 2000, 2800});
    chart.audio_cues.push_back(tenriff::gameplay::AudioCueEvent{500, 0});

    tenriff::gameplay::offset_gameplay_chart_samples(chart, 3000);

    REQUIRE(chart.notes.size() == 2);
    REQUIRE(chart.audio_cues.size() == 1);
    CHECK(chart.duration_samples == 7000);
    CHECK(chart.notes[0].start_sample == 4000);
    CHECK(chart.notes[1].start_sample == 5000);
    REQUIRE(chart.notes[1].end_sample.has_value());
    CHECK(chart.notes[1].end_sample.value() == 5800);
    CHECK(chart.audio_cues[0].start_sample == 3500);
}

TEST_CASE("gameplay hud revisions ignore sample-only updates for text caches") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.current_sample = 1200;
    previous.audio_sample_time_ns = 10'000'000LL;
    previous.audio_buffer_frames = 256;
    previous.note_count = 1;
    previous.notes[0] = {1, 1300, 1300, false, true};

    auto next = previous;
    next.current_sample = 1216;
    next.audio_sample_time_ns = 12'000'000LL;

    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.motion_changed);
    CHECK_FALSE(diff.text_changed);
}

TEST_CASE("gameplay hud revisions bump text caches for score and feedback changes") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.combo = 7;
    previous.max_combo = 19;
    previous.counts.pg = 5;
    previous.gauge = 48.0;
    previous.gauge_type = tenriff::game::GaugeType::Normal;
    previous.rate = 1.0;
    previous.hispeed = 3.5;
    previous.has_feedback = true;
    previous.feedback = tenriff::game::Judgement::GR;

    auto next = previous;
    next.combo = 8;
    next.max_combo = 20;
    next.counts.pg = 6;
    next.gauge = 49.5;
    next.feedback = tenriff::game::Judgement::PG;

    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.text_changed);
}

TEST_CASE("gameplay hud revisions treat timing history updates as motion changes") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.timing_history_count = 2;
    previous.timing_history_delta_ms[0] = -6.0;
    previous.timing_history_delta_ms[1] = 3.0;

    auto next = previous;
    next.timing_history_delta_ms[1] = 4.0;

    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.motion_changed);
}

TEST_CASE("judgement loop timing plan clamps to supported polling range") {
    const auto plan = tenriff::app::build_judgement_loop_timing_plan(48000, 12000);

    CHECK(plan.target_hz == 8000);
    CHECK(plan.base_step_samples == 6);
    CHECK(plan.remainder_samples == 0);
}

TEST_CASE("judgement loop timing preserves the requested average hz at 44.1k") {
    const auto plan = tenriff::app::build_judgement_loop_timing_plan(44100, 8000);

    REQUIRE(plan.target_hz == 8000);
    CHECK(plan.base_step_samples == 5);
    CHECK(plan.remainder_samples == 4100);

    int64_t carry = 0;
    int64_t accumulated_samples = 0;
    int five_sample_steps = 0;
    int six_sample_steps = 0;
    for (int i = 0; i < plan.target_hz; ++i) {
        const int64_t step = tenriff::app::next_judgement_loop_step_samples(plan, carry);
        accumulated_samples += step;
        if (step == 5) {
            ++five_sample_steps;
        } else if (step == 6) {
            ++six_sample_steps;
        }
    }

    CHECK(accumulated_samples == 44100);
    CHECK(five_sample_steps == 3900);
    CHECK(six_sample_steps == 4100);
    CHECK(carry == 0);
}
