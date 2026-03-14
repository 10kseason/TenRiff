#include "doctest/doctest.h"

#include <optional>

#include "chart/OsuDifficulty.h"

using tenriff::chart::OsuDifficultyMetrics;
using tenriff::chart::OsuManiaChart;
using tenriff::chart::OsuManiaNote;
using tenriff::chart::calculate_osu_mania_difficulty;

namespace {

OsuManiaChart make_chart(int key_count = 10) {
    OsuManiaChart chart;
    chart.key_count = key_count;
    chart.overall_difficulty = 8.0;
    chart.base_bpm = 180.0;
    return chart;
}

void add_tap(OsuManiaChart& chart, int column, int time_ms) {
    chart.notes.push_back(OsuManiaNote{column, time_ms, std::nullopt, 0});
}

}  // namespace

TEST_CASE("10k difficulty rates dense streams above sparse streams") {
    auto sparse = make_chart();
    auto dense = make_chart();

    for (int i = 0; i < 48; ++i) {
        add_tap(sparse, i % 10, i * 240);
        add_tap(dense, i % 10, i * 75);
    }

    const OsuDifficultyMetrics sparse_metrics = calculate_osu_mania_difficulty(sparse);
    const OsuDifficultyMetrics dense_metrics = calculate_osu_mania_difficulty(dense);

    CHECK(dense_metrics.circus_rating > sparse_metrics.circus_rating);
    CHECK(dense_metrics.revive_level > sparse_metrics.revive_level);
    CHECK(dense_metrics.average_nps > sparse_metrics.average_nps);
}

TEST_CASE("10k difficulty keeps jack-heavy charts above split patterns") {
    auto jack = make_chart();
    auto split = make_chart();

    for (int i = 0; i < 64; ++i) {
        add_tap(jack, 4, i * 70);
        add_tap(split, i % 2 == 0 ? 4 : 5, i * 70);
    }

    const OsuDifficultyMetrics jack_metrics = calculate_osu_mania_difficulty(jack);
    const OsuDifficultyMetrics split_metrics = calculate_osu_mania_difficulty(split);

    CHECK(jack_metrics.circus_rating > split_metrics.circus_rating);
    CHECK(jack_metrics.revive_level > split_metrics.revive_level);
}

TEST_CASE("generic mania difficulty rates dense 4K charts above sparse 4K charts") {
    auto sparse = make_chart(4);
    auto dense = make_chart(4);

    for (int i = 0; i < 48; ++i) {
        add_tap(sparse, i % 4, i * 250);
        add_tap(dense, i % 4, i * 90);
    }

    const OsuDifficultyMetrics sparse_metrics = calculate_osu_mania_difficulty(sparse);
    const OsuDifficultyMetrics dense_metrics = calculate_osu_mania_difficulty(dense);

    CHECK(dense_metrics.circus_rating > sparse_metrics.circus_rating);
    CHECK(dense_metrics.revive_level >= sparse_metrics.revive_level);
    CHECK(dense_metrics.note_count > 0);
}
