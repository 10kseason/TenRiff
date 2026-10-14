#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "config/KeycodeMap.h"
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

// F5/F6 now belong to the assignable coarse Hi-Speed actions.
[[nodiscard]] inline bool hispeed_key_reserved(std::uint32_t keycode) {
    if (keycode == config::KeycodeMap::to_keycode("F5").value_or(0) ||
        keycode == config::KeycodeMap::to_keycode("F6").value_or(0)) return keycode == 0;
    return instant_restart_key_reserved(keycode);
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
