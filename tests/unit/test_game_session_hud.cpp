#include "doctest/doctest.h"

#include <optional>

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
