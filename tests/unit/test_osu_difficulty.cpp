#include "doctest/doctest.h"

#include <optional>

#include "chart/OsuDifficulty.h"

using tenriff::chart::DifficultyPreset;
using tenriff::chart::ManiaDifficultyOptions;
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

void add_hold(OsuManiaChart& chart, int column, int start_ms, int end_ms) {
    chart.notes.push_back(OsuManiaNote{column, start_ms, end_ms, 0});
}

}  // namespace

TEST_CASE("10k difficulty matches python dense stream reference") {
    auto chart = make_chart(10);
    for (int i = 0; i < 48; ++i) {
        add_tap(chart, i % 10, i * 75);
    }

    const OsuDifficultyMetrics metrics = calculate_osu_mania_difficulty(chart);
    CHECK(metrics.circus_rating == doctest::Approx(4.074886037));
    CHECK(metrics.revive_level == 8);
    CHECK(metrics.peak_nps == doctest::Approx(13.0));
    CHECK(metrics.average_nps == doctest::Approx(13.617021277));
}

TEST_CASE("10k difficulty matches python jack reference") {
    auto chart = make_chart(10);
    for (int i = 0; i < 64; ++i) {
        add_tap(chart, 4, i * 70);
    }

    const OsuDifficultyMetrics metrics = calculate_osu_mania_difficulty(chart);
    CHECK(metrics.circus_rating == doctest::Approx(6.118601555));
    CHECK(metrics.revive_level == 13);
    CHECK(metrics.peak_nps == doctest::Approx(15.0));
    CHECK(metrics.average_nps == doctest::Approx(14.512471655));
}

TEST_CASE("10k difficulty matches python mixed hold reference") {
    auto chart = make_chart(10);
    for (int i = 0; i < 32; ++i) {
        add_tap(chart, i % 10, i * 160);
        if (i % 8 == 0) {
            add_hold(chart, 2, i * 160 + 40, i * 160 + 280);
        }
    }

    const OsuDifficultyMetrics metrics = calculate_osu_mania_difficulty(chart);
    CHECK(metrics.circus_rating == doctest::Approx(3.022832854));
    CHECK(metrics.revive_level == 6);
    CHECK(metrics.peak_nps == doctest::Approx(9.0));
    CHECK(metrics.average_nps == doctest::Approx(8.064516129));
}

TEST_CASE("layout options distinguish 5+1 from 6k") {
    auto chart = make_chart(6);
    for (int i = 0; i < 24; ++i) {
        add_tap(chart, i % 6, i * 110);
    }

    ManiaDifficultyOptions scratch_options;
    scratch_options.preset = DifficultyPreset::QwilightBmsEz;
    scratch_options.mode_name = "5+1";

    ManiaDifficultyOptions six_key_options;
    six_key_options.preset = DifficultyPreset::QwilightBmsEz;
    six_key_options.mode_name = "6K";

    const OsuDifficultyMetrics scratch_metrics = calculate_osu_mania_difficulty(chart, scratch_options);
    const OsuDifficultyMetrics six_key_metrics = calculate_osu_mania_difficulty(chart, six_key_options);
    CHECK(scratch_metrics.circus_rating == doctest::Approx(3.05134503));
    CHECK(scratch_metrics.revive_level == 6);
    CHECK(six_key_metrics.circus_rating == doctest::Approx(2.955337324));
    CHECK(six_key_metrics.revive_level == 6);
    CHECK(scratch_metrics.circus_rating != doctest::Approx(six_key_metrics.circus_rating));
}

TEST_CASE("layout options distinguish 14+2 from 16k") {
    auto chart = make_chart(16);
    for (int i = 0; i < 32; ++i) {
        add_tap(chart, i % 16, i * 130);
    }

    ManiaDifficultyOptions scratch_options;
    scratch_options.preset = DifficultyPreset::QwilightBmsEz;
    scratch_options.mode_name = "DP16";

    ManiaDifficultyOptions full_options;
    full_options.preset = DifficultyPreset::QwilightBmsEz;
    full_options.mode_name = "16K";

    const OsuDifficultyMetrics scratch_metrics = calculate_osu_mania_difficulty(chart, scratch_options);
    const OsuDifficultyMetrics full_metrics = calculate_osu_mania_difficulty(chart, full_options);
    CHECK(scratch_metrics.circus_rating == doctest::Approx(3.203575929));
    CHECK(scratch_metrics.revive_level == 6);
    CHECK(full_metrics.circus_rating == doctest::Approx(3.105113229));
    CHECK(full_metrics.revive_level == 6);
    CHECK(scratch_metrics.circus_rating != doctest::Approx(full_metrics.circus_rating));
}
