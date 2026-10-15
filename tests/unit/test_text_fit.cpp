#include "doctest/doctest.h"

#include "render/TextFit.h"
#include "render/DifficultyLabel.h"

TEST_CASE("song and result difficulty display retain only the Aery five badge") {
    using tenriff::render::difficulty_display_label;
    CHECK(difficulty_display_label("\xE2\x91\xA4LEVEL 12", true) == "\xE2\x91\xA4LEVEL 12");
    CHECK(difficulty_display_label("\xE2\x91\xA4LEVEL 12", false) == "LEVEL 12");
    for (const auto* label : {"LV 12", "4UE18+", "6UE4", "8UE?!", "\xE2\x93\x87" "10", "st12", ""}) {
        CHECK(difficulty_display_label(label, false) == label);
    }
}

TEST_CASE("difficulty prefixes color the complete registered mode marker only") {
    using tenriff::render::difficulty_label_prefix;
    const auto four = difficulty_label_prefix(L"4UE18+", false, 4);
    CHECK(four.key_count == 4);
    CHECK(four.length == 3);
    const auto six = difficulty_label_prefix(L"6UE4", false, 6);
    CHECK(six.key_count == 6);
    CHECK(six.length == 3);
    const auto eight = difficulty_label_prefix(L"8UE?!", false, 8);
    CHECK(eight.key_count == 8);
    CHECK(eight.length == 3);
    const auto ten = difficulty_label_prefix(L"\u24c710", false, 10);
    CHECK(ten.key_count == 10);
    CHECK(ten.length == 1);
    const auto five = difficulty_label_prefix(L"\u2464LEVEL 17+", true, 0);
    CHECK(five.key_count == 5);
    CHECK(five.length == 1);
}

TEST_CASE("difficulty prefix colors do not infer modes from levels or unrelated tables") {
    using tenriff::render::difficulty_label_prefix;
    for (const auto* label : {L"4", L"6", L"8", L"10", L"18+", L"st4", L"sl10", L""}) {
        for (const int mode : {0, 4, 6, 8, 10}) {
            CHECK(difficulty_label_prefix(label, false, mode).length == 0);
        }
    }
    CHECK(difficulty_label_prefix(L"4UE18", false, 0).length == 0);
    CHECK(difficulty_label_prefix(L"4UE18", false, 6).length == 0);
    CHECK(difficulty_label_prefix(L"\u24c710", false, 0).length == 0);
    CHECK(difficulty_label_prefix(L"\u2464LEVEL 17+", false, 0).length == 0);
}

TEST_CASE("skin menu text fit preserves short labels and shrinks long labels") {
    CHECK(tenriff::render::estimate_single_line_text_scale(L"PLAY", 34.0f, 600.0f, 100.0f) ==
          doctest::Approx(1.0f));

    const float korean_scale = tenriff::render::estimate_single_line_text_scale(
        L"아주 긴 한국어 메뉴 설명이 패널 밖으로 나가면 안 됩니다", 34.0f, 360.0f, 54.0f);
    CHECK(korean_scale < 1.0f);
    CHECK(korean_scale >= 0.45f);
}

TEST_CASE("skin menu text fit also respects a short row height") {
    const float scale = tenriff::render::estimate_single_line_text_scale(
        L"GUIDE", 30.0f, 500.0f, 20.0f);
    CHECK(scale < 0.7f);
    CHECK(scale >= 0.45f);
}
