#include "doctest/doctest.h"
#include "app/KeyNoteOverlay.h"
#include "config/Config.h"
#include "render/SkinGameplayPreview.h"
#include "timing/HighResClock.h"
#include <chrono>
#include <thread>

using tenriff::app::KeyNoteOverlay;
namespace {
int64_t ns(double ms) { return static_cast<int64_t>(ms * 1e6); }
}

TEST_CASE("key overlay is opt in and only supports the five requested modes") {
    KeyNoteOverlay overlay;
    for (int lanes = 1; lanes <= 18; ++lanes) {
        overlay.configure(false, lanes);
        overlay.record(0, true, ns(10));
        CHECK_FALSE(overlay.update(ns(20), true).visible);
        overlay.configure(true, lanes);
        CHECK(overlay.update(ns(20), true).visible == (lanes == 4 || lanes == 5 || lanes == 7 || lanes == 8 || lanes == 10));
    }
}

TEST_CASE("key overlay preserves taps between frames and aligns only chord visuals") {
    KeyNoteOverlay overlay;
    overlay.configure(true, 4);
    overlay.record(0, true, ns(10));
    overlay.record(0, true, ns(12)); // repeat / secondary takeover
    overlay.record(1, true, ns(20));
    overlay.record(0, false, ns(25));
    overlay.record(1, false, ns(35));
    const auto state = overlay.update(ns(50), true);
    REQUIRE(state.note_count == 2);
    CHECK(state.notes[0].press_ms == 10);
    CHECK(state.notes[1].press_ms == 10);
    CHECK(state.notes[0].release_ms == 25);
    CHECK_FALSE(state.held[0]);
    CHECK(state.accepted_presses == 2);
    CHECK(state.kps == 2);
    CHECK_FALSE(state.difficulty.ready);
    CHECK(overlay.update(ns(900), true).note_count == 0);
}

TEST_CASE("key overlay KPS uses a rolling second without estimating input difficulty") {
    for (int lanes : {4, 5, 7, 8, 10}) {
        KeyNoteOverlay overlay;
        overlay.configure(true, lanes);
        for (int i = 0; i < 120; ++i) {
            const int lane = i % lanes;
            const double time = 100 + i * 50;
            overlay.record(lane, true, ns(time));
            overlay.record(lane, false, ns(time + 20));
        }
        const auto actual = overlay.update(ns(6100), true);
        CHECK(actual.kps == 19);
        CHECK(actual.accepted_presses == 120);
        CHECK_FALSE(actual.difficulty.ready);
        CHECK(actual.difficulty.samples == 0);
        CHECK(overlay.update(ns(7100), true).kps == 0);
    }
}

TEST_CASE("key overlay clears pause focus loss overflow and stale samples without stuck holds") {
    KeyNoteOverlay overlay;
    overlay.configure(true, 7);
    overlay.record(0, true, ns(10));
    CHECK(overlay.update(ns(20), true).held[0]);
    CHECK_FALSE(overlay.update(ns(30), false).held[0]);
    overlay.record(0, true, ns(25)); // late edge from before pause
    CHECK(overlay.update(ns(40), true).note_count == 0);
    overlay.record(0, true, ns(50));
    overlay.invalidate();
    const auto invalid = overlay.update(ns(60), true);
    CHECK(invalid.limited);
    CHECK(invalid.note_count == 0);
    for (int i = 0; i < 2200; ++i) overlay.record(0, i % 2 == 0, ns(100 + i));
    const auto overflow = overlay.update(ns(2500), true);
    CHECK(overflow.limited);
    CHECK_FALSE(overflow.held[0]);
    overlay.record(0, true, ns(2600));
    CHECK(overlay.update(ns(2610), true).held[0]);
    overlay.configure(true, 5);
    CHECK(overlay.update(ns(3000), true).note_count == 0);
}

TEST_CASE("key overlay worker publishes independently and pauses without blocking input") {
    KeyNoteOverlay overlay;
    overlay.configure(true, 4);
    REQUIRE(overlay.start());
    auto clock = [] { return tenriff::timing::HighResClock::now_ns(); };
    overlay.snapshot(clock(), true);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto time = clock();
    overlay.record(0, true, time);
    overlay.record(0, false, time + 1000);
    tenriff::app::KeyOverlaySnapshot observed;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        observed = overlay.snapshot(clock(), true);
    } while (observed.kps != 1 && std::chrono::steady_clock::now() < deadline);
    CHECK(observed.kps == 1);
    CHECK(observed.note_count == 1);
    CHECK_FALSE(observed.difficulty.ready);
    const auto paused = overlay.snapshot(clock(), false);
    CHECK(paused.kps == 0);
    CHECK(paused.note_count == 0);
    CHECK_FALSE(paused.held[0]);
    overlay.stop();
    overlay.configure(false, 4);
    REQUIRE(overlay.start());
    CHECK_FALSE(overlay.snapshot(clock(), true).visible);
}

namespace {
tenriff::gameplay::GameplayChart overlay_chart(int lanes, int count, int gap_ms, int offset_ms = 0) {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = lanes;
    for (int i = 0; i < count; ++i) {
        tenriff::gameplay::NoteEvent note;
        note.lane = i % lanes + 1;
        note.start_sample = offset_ms + i * gap_ms;
        chart.notes.push_back(note);
    }
    return chart;
}
}

TEST_CASE("BMS overlay estimate is fixed from the full chart and independent of live KPS") {
    using tenriff::app::estimate_bms_overlay_difficulty;
    for (int lanes : {4, 5, 7, 8, 10}) {
        const auto chart = overlay_chart(lanes, 240, 50);
        const auto fixed = estimate_bms_overlay_difficulty(chart, 1000);
        REQUIRE(fixed.bms_chart);
        REQUIRE(fixed.difficulty.ready);
        CHECK(fixed.difficulty.profile == pattern::ProfileForKeys(lanes));
        const auto shifted = estimate_bms_overlay_difficulty(overlay_chart(lanes, 240, 50, 10000), 1000);
        CHECK(shifted.difficulty.rating == doctest::Approx(fixed.difficulty.rating));
        CHECK(shifted.peak_end_ms == doctest::Approx(fixed.peak_end_ms + 10000));
        // An easy intro separated by silence must not dilute the hardest section.
        auto combined = overlay_chart(lanes, 10, 500);
        const auto hard = overlay_chart(lanes, 240, 50, 20000);
        combined.notes.insert(combined.notes.end(), hard.notes.begin(), hard.notes.end());
        const auto full = estimate_bms_overlay_difficulty(combined, 1000);
        CHECK(full.difficulty.rating == doctest::Approx(fixed.difficulty.rating));
        CHECK(full.peak_end_ms >= 20000);
        KeyNoteOverlay live;
        live.configure(true, lanes);
        for (int i = 0; i < 300; ++i) {
            live.record(i % lanes, true, ns(i * 10));
            live.record(i % lanes, false, ns(i * 10 + 1));
        }
        CHECK_FALSE(live.update(ns(3000), true).difficulty.ready);
        CHECK(estimate_bms_overlay_difficulty(chart, 1000).difficulty.rating == doctest::Approx(fixed.difficulty.rating));
    }
}

TEST_CASE("BMS overlay analysis preserves holds and handles unsupported sparse cancelled or invalid charts") {
    using tenriff::app::estimate_bms_overlay_difficulty;
    auto chart = overlay_chart(7, 240, 50);
    const auto taps = estimate_bms_overlay_difficulty(chart, 1000);
    for (auto& note : chart.notes) note.end_sample = note.start_sample + 200;
    const auto holds = estimate_bms_overlay_difficulty(chart, 1000);
    REQUIRE(holds.difficulty.ready);
    CHECK(holds.difficulty.holdOverlapPercent > 0);
    CHECK(holds.difficulty.stars != doctest::Approx(taps.difficulty.stars));
    const auto cancelled = estimate_bms_overlay_difficulty(chart, 1000, [] { return true; });
    CHECK(cancelled.cancelled);
    CHECK_FALSE(cancelled.difficulty.ready);
    CHECK_FALSE(estimate_bms_overlay_difficulty(overlay_chart(6, 240, 50), 1000).difficulty.ready);
    CHECK_FALSE(estimate_bms_overlay_difficulty(overlay_chart(4, 2, 50), 1000).difficulty.ready);
    CHECK_FALSE(estimate_bms_overlay_difficulty(chart, 0).difficulty.ready);
    chart.notes[0].lane = 99;
    CHECK(estimate_bms_overlay_difficulty(chart, 1000).difficulty.limited);
    auto collision = overlay_chart(7, 240, 50);
    collision.notes[0].end_sample = 400; // The next same-lane head is at 350 ms.
    const auto rejected = estimate_bms_overlay_difficulty(collision, 1000);
    CHECK(rejected.difficulty.limited);
    CHECK_FALSE(rejected.difficulty.ready);
    collision.notes[0].end_sample = 350; // Touching tails/heads remain valid.
    CHECK(estimate_bms_overlay_difficulty(collision, 1000).difficulty.ready);
}

TEST_CASE("key overlay keeps long holds when bounded visual history fills") {
    KeyNoteOverlay overlay;
    overlay.configure(true, 5);
    overlay.record(0, true, ns(1));
    for (int i = 0; i < 350; ++i) {
        overlay.record(1, true, ns(10 + i * 10));
        overlay.record(1, false, ns(15 + i * 10));
    }
    const auto state = overlay.update(ns(3510), true);
    REQUIRE(state.note_count > 0);
    CHECK(state.note_count <= state.notes.size());
    CHECK(state.notes[0].lane == 0);
    CHECK(state.notes[0].release_ms == -1);
    CHECK(state.held[0]);
    overlay.record(0, false, ns(3520));
    CHECK_FALSE(overlay.update(ns(3530), true).held[0]);
    CHECK(overlay.update(ns(4500), true).note_count == 0);
}

TEST_CASE("key overlay accepts an edge published after the HUD clock sample") {
    KeyNoteOverlay overlay;
    overlay.configure(true, 10);
    overlay.record(0, true, ns(101));
    overlay.record(0, false, ns(102));
    const auto state = overlay.update(ns(100), true);
    REQUIRE(state.note_count == 1);
    CHECK(state.now_ms == 102);
    CHECK(state.notes[0].release_ms == 102);
}

TEST_CASE("key overlay skin settings round trip with safe legacy defaults and finite positions") {
    using namespace tenriff::config;
    SkinConfig skin;
    REQUIRE(deserialize_skin_config("{}", skin));
    CHECK_FALSE(skin.key_overlay_enabled);
    CHECK(skin.key_overlay_x == 0.0);
    CHECK(skin.key_overlay_y == doctest::Approx(0.25));
    skin.key_overlay_enabled = true;
    skin.key_overlay_x = 0.23; skin.key_overlay_y = 0.81;
    SkinConfig restored;
    REQUIRE(deserialize_skin_config(serialize_skin_config(skin), restored));
    CHECK(restored.key_overlay_enabled);
    CHECK(restored.key_overlay_x == doctest::Approx(0.23));
    CHECK(restored.key_overlay_y == doctest::Approx(0.81));
    REQUIRE(deserialize_skin_config(R"({"key_overlay_x":-20,"key_overlay_y":99})", restored));
    CHECK(restored.key_overlay_x == 0.0);
    CHECK(restored.key_overlay_y == 1.0);
}

TEST_CASE("key overlay skin preview is visibly synthetic and never invents an estimate") {
    tenriff::render::SkinPreviewData preview;
    preview.key_overlay_enabled = true;
    for (int lanes : {4, 5, 6, 7, 8, 9, 10, 16}) {
        preview.lane_count = lanes;
        const auto hud = tenriff::render::make_skin_gameplay_preview(preview, ns(1000));
        CHECK(hud.key_overlay.visible == tenriff::app::key_overlay_supports(lanes));
        CHECK(hud.key_overlay.preview);
        CHECK_FALSE(hud.key_overlay.difficulty.ready);
        CHECK(hud.key_overlay.difficulty.samples == 0);
    }
}
