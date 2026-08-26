#include "doctest/doctest.h"

#include "render/TextFit.h"

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
