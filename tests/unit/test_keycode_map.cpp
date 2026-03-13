#include "doctest/doctest.h"

#include "config/KeycodeMap.h"

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
