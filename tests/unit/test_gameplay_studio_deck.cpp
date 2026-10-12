#include "doctest/doctest.h"
#include "render/GameplayStudioDeck.h"
#include "render/GameplaySongTitle.h"

#include <limits>
#include <numeric>
#include <utility>

using namespace tenriff::render;

TEST_CASE("Song titles stay outside resized and dragged solo fields") {
    for (const bool studio : {false, true}) {
        for (const float width : {490.0f, 980.0f, 1372.0f}) {
            for (float left = 12.0f; left + width <= 1800.0f; left += 17.0f) {
                const float right = left + width;
                const auto title = studio
                    ? gameplay_studio_song_title_layout(compute_gameplay_studio_deck_layout(left, right))
                    : compute_gameplay_song_title_layout(left, right, left - 32.0f, false);
                CHECK(title.right > title.left);
                CHECK(title.left >= 24.0f);
                CHECK(title.right <= 1856.0f);
                CHECK(title.bottom > title.top);
                CHECK((title.right <= left - 32.0f || title.left >= right + (studio ? 32.0f : 124.0f)));
            }
        }
    }
}

TEST_CASE("Studio title stays in the upper information card as side space changes") {
    const auto left = gameplay_studio_song_title_layout(compute_gameplay_studio_deck_layout(12, 992));
    CHECK(left.left >= 1100);
    CHECK(left.top == 52);
    const auto restored = gameplay_studio_song_title_layout(compute_gameplay_studio_deck_layout(470, 1450));
    CHECK(restored.left == 1558);
    CHECK(restored.top == 52);
    CHECK(restored.right == 1856);
    const auto wide = gameplay_studio_song_title_layout(compute_gameplay_studio_deck_layout(250, 1622));
    CHECK(wide.left == 24);
    CHECK(wide.right <= 218);
}

TEST_CASE("Full title fitting follows both measured dimensions without a truncating font floor") {
    for (const float width : {20.0f, 240.0f, 1000.0f, 100000.0f}) {
        for (const float height : {20.0f, 40.0f, 120.0f}) {
            const float scale = gameplay_song_title_fit_scale(width, height, 234, 34);
            CHECK(scale > 0);
            CHECK(scale <= 1);
            CHECK(width * scale <= 234.001f);
            CHECK(height * scale <= 34.001f);
        }
    }
    CHECK(gameplay_song_title_fit_scale(200, 30, 250, 34) == 1);
    CHECK(gameplay_song_title_fit_scale(10000, 30, 100, 34) < 0.05f);
    CHECK(gameplay_song_title_fit_scale(100, 30, 0, 34) == 0);
    CHECK(gameplay_song_title_fit_scale(std::numeric_limits<float>::quiet_NaN(), 30, 200, 34) == 0);
}

TEST_CASE("Studio deck supports native and imported LR2 solo HUDs without changing battle fallback") {
    for (const auto source : {"native", "lr2"}) {
        CHECK(gameplay_studio_deck_enabled("studio", source, false, false));
        CHECK_FALSE(gameplay_studio_deck_enabled("classic", source, false, false));
        CHECK_FALSE(gameplay_studio_deck_enabled("studio", source, true, false));
        CHECK_FALSE(gameplay_studio_deck_enabled("studio", source, false, true));
    }
    CHECK_FALSE(gameplay_studio_deck_enabled("studio", "tenriff", false, false));
}

TEST_CASE("Studio riff map switch hides bars and clocks without moving the remaining HUD") {
    const auto visible = compute_gameplay_studio_deck_layout(470.0f, 1450.0f);
    const auto hidden = compute_gameplay_studio_deck_layout(470.0f, 1450.0f, 1856.0f, false);
    REQUIRE(visible.map_visible);
    REQUIRE(visible.map_times_visible);
    CHECK_FALSE(hidden.map_visible);
    CHECK_FALSE(hidden.map_times_visible);
    CHECK(hidden.info_x == visible.info_x);
    CHECK(hidden.info_width == visible.info_width);
    CHECK(hidden.score_x == visible.score_x);
    CHECK(hidden.score_width == visible.score_width);
    CHECK(hidden.score_y == visible.score_y);
    CHECK(hidden.score_visible == visible.score_visible);
    const auto fallback_visible = compute_gameplay_studio_deck_layout(12, 992);
    const auto fallback_hidden = compute_gameplay_studio_deck_layout(12, 992, 1856, false);
    CHECK(fallback_hidden.score_y == fallback_visible.score_y);
}

TEST_CASE("Studio deck places information upper right score lower left and a horizontal map lower right") {
    const auto wide = compute_gameplay_studio_deck_layout(470.0f, 1450.0f);
    CHECK(wide.info_x == 1558);
    CHECK(wide.info_y == 52);
    CHECK(wide.info_width == 298);
    CHECK(wide.score_x == 24);
    CHECK(wide.score_y == 576);
    CHECK(wide.score_width == 414);
    CHECK(wide.map_x == wide.info_x);
    CHECK(wide.map_width > wide.map_height);
    CHECK(wide.map_scale == 1);
    CHECK(wide.map_height == 96);
    CHECK(wide.map_top == 924);
    CHECK(wide.map_visible);
    CHECK(wide.map_times_visible);
    CHECK(wide.score_visible);
    // A narrow 4K field opens both columns without depending on lane count.
    const auto narrow = compute_gameplay_studio_deck_layout(660.0f, 1260.0f);
    CHECK(narrow.info_x == 1368);
    CHECK(narrow.score_width == 600);
    CHECK(narrow.info_width > wide.info_width);
}

TEST_CASE("Studio map clocks stay full size in ordinary gaps and scale within narrow gaps") {
    for (const float width : {280.0f, 298.0f, 414.0f}) {
        const auto deck = compute_gameplay_studio_deck_layout(100, 1748.0f - width);
        REQUIRE(deck.map_width == width);
        CHECK(deck.map_scale == 1);
        CHECK(deck.map_height == 96);
        CHECK(deck.map_top == 924);
    }
    for (const float width : {279.0f, 220.0f, 160.0f, 140.0f}) {
        const auto deck = compute_gameplay_studio_deck_layout(100, 1748.0f - width);
        REQUIRE(deck.map_width == width);
        CHECK(deck.map_scale == doctest::Approx(width / 280.0f));
        CHECK(deck.map_scale < 1);
        CHECK(deck.map_scale > 0);
        // Each end clock has a 76px local box; the boxes must stay disjoint.
        CHECK(2 * 76.0f * deck.map_scale < deck.map_width);
        CHECK(deck.map_times_visible == (width >= 160));
        CHECK(deck.map_top + deck.map_height == doctest::Approx(1020));
    }
}

TEST_CASE("Studio deck keeps cards outside moved fields and separates shared fallback columns") {
    for (const float width : {490.0f, 600.0f, 980.0f, 1372.0f}) {
        for (float left = 12.0f; left + width <= 1900.0f; left += 17.0f) {
            const float right = left + width;
            for (const bool performance : {false, true}) {
                const auto deck = compute_gameplay_studio_deck_layout(left, right,
                    gameplay_studio_header_safe_right(performance));
                if (!performance) REQUIRE(deck.info_visible);
                if (!deck.info_visible) {
                    // A full-width field and the large diagnostic overlay can
                    // consume both upper side gaps. Never cover the note field.
                    CHECK(performance);
                    CHECK(left <= 56);
                    CHECK(right + 108 >= gameplay_studio_header_safe_right(true));
                }
                REQUIRE(deck.score_visible);
                REQUIRE(deck.map_visible);
                for (const auto pair : {std::pair<float, float>{deck.info_x, deck.info_width},
                                       {deck.score_x, deck.score_width}, {deck.map_x, deck.map_width}}) {
                    if (pair.second <= 0) continue;
                    CHECK(pair.first >= 24);
                    CHECK(pair.first + pair.second <= 1856);
                    CHECK((pair.first + pair.second <= left - 32 || pair.first >= right + 32));
                }
                CHECK(deck.info_x + deck.info_width <= gameplay_studio_header_safe_right(performance));
                CHECK(deck.info_scale >= 0);
                CHECK(deck.info_scale <= 1);
                CHECK(deck.score_scale > 0);
                CHECK(deck.score_scale <= 1);
                CHECK(deck.score_y >= deck.info_y + 210 * deck.info_scale + 12);
                CHECK(deck.score_y + 444 * deck.score_scale <= 1020.001f);
                CHECK(deck.map_top + deck.map_height == doctest::Approx(1020).epsilon(1e-3));
                if (deck.score_x < deck.map_x + deck.map_width && deck.map_x < deck.score_x + deck.score_width)
                    CHECK(deck.score_y + 444 * deck.score_scale + 12 <= deck.map_top - 32 * deck.map_scale);
            }
        }
    }
    const auto far_right = compute_gameplay_studio_deck_layout(900, 1880);
    CHECK(far_right.gauge_marker_x + 86 < 900);
    CHECK(far_right.info_x + far_right.info_width < far_right.gauge_marker_x);
    CHECK_FALSE(compute_gameplay_studio_deck_layout(0, 0).info_visible);
    CHECK_FALSE(compute_gameplay_studio_deck_layout(std::numeric_limits<float>::quiet_NaN(), 980).map_visible);
}

TEST_CASE("Studio performance overlay moves only the upper information card and restores its position") {
    const auto normal = compute_gameplay_studio_deck_layout(470, 1450, gameplay_studio_header_safe_right(false));
    const auto overlay = compute_gameplay_studio_deck_layout(470, 1450, gameplay_studio_header_safe_right(true));
    CHECK(normal.info_x > 1450);
    CHECK(overlay.info_x + overlay.info_width <= 438);
    CHECK(normal.score_x == overlay.score_x);
    CHECK(normal.score_y == overlay.score_y);
    CHECK(normal.map_x == overlay.map_x);
    CHECK(normal.map_top == overlay.map_top);
    const auto restored = compute_gameplay_studio_deck_layout(470, 1450, gameplay_studio_header_safe_right(false));
    CHECK(restored.info_x == normal.info_x);
    CHECK(restored.info_width == normal.info_width);
}

TEST_CASE("Studio gauge marker stays inside its authored vertical limits") {
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 0.0) == doctest::Approx(846.0f).epsilon(1e-3));
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 0.5) == doctest::Approx(434.0f).epsilon(1e-3));
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 1.0) == doctest::Approx(8.0f).epsilon(1e-3));
    CHECK(gameplay_studio_gauge_marker_y(900.0f, -1.0) == 846.0f);
    CHECK(gameplay_studio_gauge_marker_y(900.0f, 2.0) == 8.0f);
    CHECK(gameplay_studio_gauge_marker_y(20.0f, 0.5) == 8.0f);
    CHECK(gameplay_studio_gauge_marker_y(900.0f, std::numeric_limits<double>::quiet_NaN()) == 846.0f);
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
