#include "doctest/doctest.h"
#include "render/NativeSkinOverrides.h"

#include <limits>

using tenriff::app::NativeMenuSkinStyle;
using tenriff::render::native_skin_number;
using tenriff::render::native_skin_rect;
using tenriff::render::native_skin_metric_minimum;

TEST_CASE("native skin absent and zero rectangle overrides preserve exact defaults") {
    NativeMenuSkinStyle style;
    // Progress and clipped decorations can legitimately start with no area.
    // Saving the catalog's zero defaults must not turn them into visible pixels.
    for (const std::array<float, 4> base : {
             std::array<float, 4>{10, 20, 110, 60}, {10, 20, 10, 20},
             {10, 20, 10.25f, 20.5f}}) {
        CHECK(native_skin_rect(style, "menu.row", base) == base);
        style.rects["menu.row"] = {0, 0, 0, 0};
        CHECK(native_skin_rect(style, "menu.row", base) == base);
        CHECK(native_skin_rect(style, "menu.other", base) == base);
    }
}

TEST_CASE("native skin rectangle deltas preserve repeated row spacing") {
    NativeMenuSkinStyle style;
    style.rects["menu.row"] = {12, -5, 30, -8};
    const auto first = native_skin_rect(style, "menu.row", {40, 100, 440, 160});
    const auto second = native_skin_rect(style, "menu.row", {40, 174, 440, 234});
    const std::array<float, 4> expected_first = {52, 95, 482, 147};
    CHECK(first == expected_first);
    CHECK(second[1] - first[1] == doctest::Approx(74));
    CHECK(second[3] - first[3] == doctest::Approx(74));
    CHECK(second[2] - second[0] == doctest::Approx(430));
    CHECK(second[3] - second[1] == doctest::Approx(52));
}

TEST_CASE("native skin rectangle resizing stays usable and rejects nonfinite deltas") {
    NativeMenuSkinStyle style;
    const std::array<float, 4> base = {40, 100, 440, 160};
    style.rects["menu.row"] = {0, 0, -8192, -8192};
    const auto collapsed = native_skin_rect(style, "menu.row", base);
    CHECK(collapsed[2] - collapsed[0] >= 1);
    CHECK(collapsed[3] - collapsed[1] >= 1);
    for (const float invalid : {std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        for (std::size_t index = 0; index < base.size(); ++index) {
            style.rects["menu.row"] = {1, 2, 3, 4};
            style.rects["menu.row"][index] = invalid;
            CHECK(native_skin_rect(style, "menu.row", base) == base);
        }
    }
}

TEST_CASE("native skin numeric slots honor consumer limits and invalid values keep defaults") {
    std::unordered_map<std::string, float> values;
    CHECK(native_skin_number(values, "speed", 1, 0, 4) == 1);
    values["unrelated"] = 3;
    CHECK(native_skin_number(values, "speed", 1, 0, 4) == 1);
    values["speed"] = 0;
    CHECK(native_skin_number(values, "speed", 1, 0, 4) == 0);
    values["speed"] = 120;
    CHECK(native_skin_number(values, "speed", 1, 0, 4) == 4);
    values["font.body.size"] = -8192;
    CHECK(native_skin_number(values, "font.body.size", 18, 8, 180) == 8);
    values["font.body.size"] = 8192;
    CHECK(native_skin_number(values, "font.body.size", 18, 8, 180) == 180);
    for (const float invalid : {std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        values["speed"] = invalid;
        CHECK(native_skin_number(values, "speed", 1, 0, 4) == 1);
    }
}

TEST_CASE("native skin row dimensions cannot make help pagination divide by zero") {
    std::unordered_map<std::string, float> values;
    for (const float invalid : {0.0f, -8192.0f}) {
        values["generic_help.line_height"] = invalid;
        const float line_height = native_skin_number(values, "generic_help.line_height", 26,
            native_skin_metric_minimum("generic_help.line_height"), 8192);
        REQUIRE(line_height > 0);
        const float page_height = std::floor((408.0f - 140.0f) / line_height) * line_height;
        CHECK(std::isfinite(page_height));
        CHECK(page_height >= line_height);
    }
    values["songselect.card_gap"] = -40;
    CHECK(native_skin_number(values, "songselect.card_gap", 8,
        native_skin_metric_minimum("songselect.card_gap"), 8192) == 0);
    values["songselect.card_height"] = 0;
    CHECK(native_skin_number(values, "songselect.card_height", 86,
        native_skin_metric_minimum("songselect.card_height"), 8192) >= 1);
}
