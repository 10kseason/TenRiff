#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::config {

class KeycodeMap {
public:
    [[nodiscard]] static std::optional<uint32_t> to_keycode(std::string_view name);
    [[nodiscard]] static std::string to_name(uint32_t keycode);
#ifdef _WIN32
    [[nodiscard]] static uint32_t normalize_windows_raw_keycode(uint32_t vkey, uint16_t make_code, uint16_t flags);
    [[nodiscard]] static uint32_t normalize_windows_polling_keycode(uint32_t vkey);
    [[nodiscard]] static std::optional<uint32_t> polling_vk_for_keycode(uint32_t keycode);
#endif
};

}  // namespace tenriff::config
