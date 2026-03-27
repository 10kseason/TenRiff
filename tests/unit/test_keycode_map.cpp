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

TEST_CASE("keycode map canonicalizes legacy OEM tokens and punctuation aliases") {
#if defined(_WIN32)
    const auto semicolon = tenriff::config::KeycodeMap::to_keycode("VK_OEM_1");
    REQUIRE(semicolon.has_value());
    CHECK(tenriff::config::KeycodeMap::to_name(semicolon.value()) == "Semicolon");

    const auto lbracket = tenriff::config::KeycodeMap::to_keycode("[");
    REQUIRE(lbracket.has_value());
    CHECK(tenriff::config::KeycodeMap::to_name(lbracket.value()) == "LBracket");
#else
    SUCCEED();
#endif
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
    CHECK(polled == lbracket.value());

    const auto poll_vk = tenriff::config::KeycodeMap::polling_vk_for_keycode(lbracket.value());
    CHECK(poll_vk.has_value());
    CHECK(poll_vk.value() == static_cast<uint32_t>(VK_OEM_4));
}

TEST_CASE("keycode map keeps layout-sensitive polling round trips stable") {
    struct Case {
        const char* name;
        uint32_t vkey;
    };
    const Case cases[] = {
        {"Semicolon", VK_OEM_1},
        {"Plus", VK_OEM_PLUS},
        {"Comma", VK_OEM_COMMA},
        {"Minus", VK_OEM_MINUS},
        {"Period", VK_OEM_PERIOD},
        {"Slash", VK_OEM_2},
        {"Grave", VK_OEM_3},
        {"LBracket", VK_OEM_4},
        {"Backslash", VK_OEM_5},
        {"RBracket", VK_OEM_6},
        {"Apostrophe", VK_OEM_7},
    };

    for (const auto& item : cases) {
        const auto keycode = tenriff::config::KeycodeMap::to_keycode(item.name);
        REQUIRE(keycode.has_value());
        CHECK(tenriff::config::KeycodeMap::normalize_windows_polling_keycode(item.vkey) == keycode.value());
        const auto poll_vk = tenriff::config::KeycodeMap::polling_vk_for_keycode(keycode.value());
        REQUIRE(poll_vk.has_value());
        CHECK(poll_vk.value() == item.vkey);
    }
}

TEST_CASE("keycode map recovers raw process keys from scan code under IME-like input") {
    const auto a_key = tenriff::config::KeycodeMap::to_keycode("A");
    REQUIRE(a_key.has_value());

    const uint32_t raw = tenriff::config::KeycodeMap::normalize_windows_raw_keycode(VK_PROCESSKEY, 0x1Eu, 0);
    CHECK(raw == a_key.value());
}
#endif
