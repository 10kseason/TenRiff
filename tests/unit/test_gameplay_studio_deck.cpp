#include "doctest/doctest.h"
#include "render/GameplayStudioDeck.h"

#include <limits>
#include <numeric>

using namespace tenriff::render;

TEST_CASE("Studio deck applies only to native solo gameplay") {
    CHECK(gameplay_studio_deck_enabled("studio", false, false, false));
    CHECK_FALSE(gameplay_studio_deck_enabled("classic", false, false, false));
    CHECK_FALSE(gameplay_studio_deck_enabled("studio", true, false, false));
    CHECK_FALSE(gameplay_studio_deck_enabled("studio", false, true, false));
    CHECK_FALSE(gameplay_studio_deck_enabled("studio", false, false, true));
}

TEST_CASE("Studio deck columns are anchored to the field") {
    const auto wide = compute_gameplay_studio_deck_layout(470.0f, 1450.0f);
    CHECK(wide.map_x == doctest::Approx(390.0f).epsilon(1e-3));
    CHECK(wide.right_x == doctest::Approx(1550.0f).epsilon(1e-3));
    CHECK(wide.left_width == doctest::Approx(260.0f).epsilon(1e-3));
    CHECK(wide.right_width == doctest::Approx(306.0f).epsilon(1e-3));
    CHECK(wide.map_visible);
    CHECK(wide.map_times_visible);
    CHECK(wide.right_hud_visible);
    // A narrow 4K field opens both columns; maximum-width 16K follows the same
    // field-relative rule without depending on lane count or hidden scratches.
    const auto narrow = compute_gameplay_studio_deck_layout(660.0f, 1260.0f);
    CHECK(narrow.map_x == doctest::Approx(580.0f).epsilon(1e-3));
    CHECK(narrow.left_width == doctest::Approx(426.0f).epsilon(1e-3));
    CHECK(narrow.right_x == doctest::Approx(1360.0f).epsilon(1e-3));
    CHECK(narrow.right_width > wide.right_width);
}

TEST_CASE("Studio deck dragged field visibility recovers at exact boundaries") {
    CHECK_FALSE(compute_gameplay_studio_deck_layout(383.0f, 1363.0f).map_visible);
    CHECK(compute_gameplay_studio_deck_layout(384.0f, 1364.0f).map_visible);
    CHECK_FALSE(compute_gameplay_studio_deck_layout(441.0f, 1421.0f).map_times_visible);
    CHECK(compute_gameplay_studio_deck_layout(442.0f, 1422.0f).map_times_visible);
    CHECK_FALSE(compute_gameplay_studio_deck_layout(577.0f, 1557.0f).right_hud_visible);
    CHECK(compute_gameplay_studio_deck_layout(576.0f, 1556.0f).right_hud_visible);
    CHECK_FALSE(compute_gameplay_studio_deck_layout(470.0f, 1450.0f, 1749.0f).right_hud_visible);
    const auto overlay = compute_gameplay_studio_deck_layout(470.0f, 1450.0f, 1760.0f);
    CHECK(overlay.right_width == doctest::Approx(210.0f).epsilon(1e-3));
    CHECK(overlay.right_hud_visible);
    CHECK(compute_gameplay_studio_deck_layout(900.0f, 1880.0f).right_width == 0.0f);
}

TEST_CASE("Studio gauge marker stays inside its authored vertical limits") {
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 0.0) == doctest::Approx(855.0f).epsilon(1e-3));
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 0.5) == doctest::Approx(438.0f).epsilon(1e-3));
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 1.0) == doctest::Approx(8.0f).epsilon(1e-3));
    CHECK(gameplay_studio_gauge_marker_y(900.0f, -1.0) == 855.0f);
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 2.0) == 8.0f);
    CHECK(gameplay_studio_gauge_marker_y(20.0f, 0.5) == 8.0f);
    CHECK(gameplay_studio_gauge_marker_y(900.0f, std::numeric_limits<double>::quiet_NaN()) == 855.0f);
}

TEST_CASE("Studio score counter retains grouping and grows beyond native maximum") {
    const auto zero = make_gameplay_studio_score_counter(0);
    CHECK(zero.padded == L"00,000");
    CHECK(zero.significant == L"0");
    const auto partial = make_gameplay_studio_score_counter(9500);
    CHECK(partial.padded == L"09,500");
    CHECK(partial.significant == L"9,500");
    CHECK(make_gameplay_studio_score_counter(9999).padded == L"09,999");
    CHECK(make_gameplay_studio_score_counter(10000).padded == L"10,000");
    CHECK(make_gameplay_studio_score_counter(100000).padded == L"100,000");
    CHECK(make_gameplay_studio_score_counter(9500, 8).padded == L"00,009,500");
    CHECK(make_gameplay_studio_score_counter(-1).significant == L"0");
    CHECK(make_gameplay_studio_score_counter(std::numeric_limits<int64_t>::max()).padded == L"9,223,372,036,854,775,807");
}

TEST_CASE("Studio judgement ratios handle empty and large counts without overflow") {
    const auto empty = gameplay_studio_judgement_ratios({0, 0, 0, 0, 0});
    CHECK(std::accumulate(empty.begin(), empty.end(), 0.0f) == 0.0f);
    const auto ratios = gameplay_studio_judgement_ratios({50, 25, 15, 9, 1});
    CHECK(ratios[0] == doctest::Approx(0.5).epsilon(1e-3));
    CHECK(ratios[4] == doctest::Approx(0.01).epsilon(1e-3));
    CHECK(std::accumulate(ratios.begin(), ratios.end(), 0.0f) == doctest::Approx(1.0).epsilon(1e-3));
    const int large = std::numeric_limits<int>::max();
    const auto large_ratios = gameplay_studio_judgement_ratios({large, large, large, large, large});
    CHECK(large_ratios[0] == doctest::Approx(0.2).epsilon(1e-3));
    const auto invalid = gameplay_studio_judgement_ratios({-1, 1, 0, 0, 0});
    CHECK(invalid[0] == 0.0f);
    CHECK(invalid[1] == 1.0f);
}
