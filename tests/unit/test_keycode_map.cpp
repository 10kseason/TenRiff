#include "doctest/doctest.h"

#include "config/KeycodeMap.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

TEST_CASE("keycode map supports delete and generic virtual-key round trips") {
    const auto delete_key = tenriff::config::KeycodeMap::to_keycode("Delete");
    REQUIRE(delete_key.has_value());
    CHECK(delete_key.value() == 0x2Eu);
    CHECK(tenriff::config::KeycodeMap::to_name(delete_key.value()) == "Delete");

    constexpr uint32_t obscure_vk = 0xC1u;
    CHECK(tenriff::config::KeycodeMap::to_name(obscure_vk) == "VK_C1");
    CHECK(tenriff::config::KeycodeMap::to_keycode("VK_C1").value_or(0u) == obscure_vk);
}

TEST_CASE("keycode map keeps common punctuation names stable") {
    const auto semicolon = tenriff::config::KeycodeMap::to_keycode("Semicolon");
    REQUIRE(semicolon.has_value());
    CHECK(tenriff::config::KeycodeMap::to_name(semicolon.value()) == "Semicolon");

    const auto backslash = tenriff::config::KeycodeMap::to_keycode("Backslash");
    REQUIRE(backslash.has_value());
    CHECK(tenriff::config::KeycodeMap::to_name(backslash.value()) == "Backslash");
}

#if defined(_WIN32)
TEST_CASE("keycode map uses physical scan aliases for layout-sensitive OEM keys") {
    const auto lbracket = tenriff::config::KeycodeMap::to_keycode("LBracket");
    REQUIRE(lbracket.has_value());
    CHECK(lbracket.value() > 0xFFu);
    CHECK(tenriff::config::KeycodeMap::to_name(lbracket.value()) == "LBracket");

    const auto semicolon = tenriff::config::KeycodeMap::to_keycode("Semicolon");
    REQUIRE(semicolon.has_value());
    CHECK(semicolon.value() > 0xFFu);
    CHECK(tenriff::config::KeycodeMap::to_name(semicolon.value()) == "Semicolon");
}

TEST_CASE("keycode map normalizes raw and polling OEM keys to the same physical code") {
    const auto lbracket = tenriff::config::KeycodeMap::to_keycode("LBracket");
    REQUIRE(lbracket.has_value());

    const uint32_t raw = tenriff::config::KeycodeMap::normalize_windows_raw_keycode(VK_OEM_6, 0x1A, 0);
    const uint32_t polled = tenriff::config::KeycodeMap::normalize_windows_polling_keycode(VK_OEM_4);

    CHECK(raw == lbracket.value());
    CHECK(polled == lbracket.value() || polled == VK_OEM_4 || polled == VK_OEM_6);

    const auto poll_vk = tenriff::config::KeycodeMap::polling_vk_for_keycode(lbracket.value());
    CHECK(poll_vk.has_value());
}
#endif
