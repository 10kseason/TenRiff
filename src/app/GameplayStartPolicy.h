#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "config/KeycodeMap.h"
#include "config/Config.h"
#include "input/GamepadInput.h"

namespace tenriff::app {

// This controls the visible opening countdown only. The chart's canonical
// lead-in and resume countdown stay unchanged for replay/ghost compatibility.
[[nodiscard]] inline constexpr int gameplay_start_countdown_seconds(
    bool skip_start_countdown, bool peer_battle, bool instant_restart) {
    return !peer_battle && (skip_start_countdown || instant_restart) ? 0 : 3;
}

[[nodiscard]] inline bool instant_restart_key_reserved(std::uint32_t keycode) {
    if (input::gamepad::is_gamepad_keycode(keycode)) return false;
#ifdef _WIN32
    const auto vk = config::KeycodeMap::polling_vk_for_keycode(keycode);
    if (!vk) return true;
    keycode = *vk;
#endif
    // Menu/navigation keys, modifier keys and existing F1-F11 controls are not
    // reassigned. F12 and otherwise unused ordinary keys remain assignable.
    return keycode == 0 || keycode < 0x20 || (keycode >= 0x21 && keycode <= 0x2f) ||
           (keycode >= 0x5b && keycode <= 0x5d) || (keycode >= 0x70 && keycode <= 0x7a) ||
           (keycode >= 0x90 && keycode <= 0x91) || (keycode >= 0xa0 && keycode <= 0xa5);
}

// Function-key actions can move to another F key. Conflicts with other actions
// are checked against the complete profile by the UI and runtime resolver.
[[nodiscard]] inline bool function_key_reserved(std::uint32_t keycode) {
    for (int number = 1; number <= 24; ++number)
        if (keycode == config::KeycodeMap::to_keycode("F" + std::to_string(number)).value_or(0)) return keycode == 0;
    return instant_restart_key_reserved(keycode) || keycode == 0x20;
}

// Legacy resolver name retained for callers of the profile speed actions.
[[nodiscard]] inline bool hispeed_key_reserved(std::uint32_t keycode) {
    return function_key_reserved(keycode);
}

[[nodiscard]] inline std::array<std::uint32_t, 11> resolve_function_keys(
    const config::UiConfig& ui, const std::unordered_map<std::uint32_t, int>& lane_keys = {}) {
    std::array<std::uint32_t, 11> raw{}, result{};
    for (int number = 1; number <= 11; ++number)
        raw[number - 1] = config::KeycodeMap::to_keycode(config::function_key(ui, number)).value_or(0);
    const auto restart = config::KeycodeMap::to_keycode(ui.instant_restart_key).value_or(0);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (function_key_reserved(raw[i]) || lane_keys.count(raw[i]) || raw[i] == restart) continue;
        bool duplicate = false;
        for (std::size_t j = 0; j < raw.size(); ++j) duplicate |= i != j && raw[i] == raw[j];
        if (!duplicate) result[i] = raw[i];
    }
    return result;
}

[[nodiscard]] inline std::uint32_t resolve_hispeed_key(
    std::string_view name, const std::unordered_map<std::uint32_t, int>& lane_keys,
    std::uint32_t restart_key) {
    const auto key = config::KeycodeMap::to_keycode(name);
    return key && !hispeed_key_reserved(*key) && lane_keys.count(*key) == 0 &&
        *key != restart_key ? *key : 0;
}

[[nodiscard]] inline std::uint32_t resolve_instant_restart_key(
    std::string_view name, const std::unordered_map<std::uint32_t, int>& lane_keys) {
    const auto key = config::KeycodeMap::to_keycode(name);
    return key && !instant_restart_key_reserved(*key) && lane_keys.count(*key) == 0 ? *key : 0;
}

}  // namespace tenriff::app
