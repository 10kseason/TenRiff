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
};

}  // namespace tenriff::config
