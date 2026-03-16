#include "doctest/doctest.h"

#include <optional>

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
