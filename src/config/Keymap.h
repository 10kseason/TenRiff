#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tenriff::config {

struct Keymap {
    std::string layout = "multi";
    std::unordered_map<std::string, std::string> bindings;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> mode_bindings;
};

struct KeymapLoadResult {
    Keymap keymap;
    std::string error;
    bool used_defaults = false;

    [[nodiscard]] bool success() const { return error.empty(); }
};

class KeymapManager {
public:
    [[nodiscard]] Keymap default_keymap() const;
    [[nodiscard]] std::string normalize_mode_token(std::string_view key_mode) const;
    [[nodiscard]] std::vector<std::string> supported_mode_tokens() const;
    [[nodiscard]] std::vector<std::string> lane_ids_for_mode(std::string_view key_mode) const;
    [[nodiscard]] std::unordered_map<std::string, std::string> bindings_for_mode(const Keymap& keymap,
                                                                                  std::string_view key_mode) const;
    void reset_mode_bindings(Keymap& keymap, std::string_view key_mode) const;

    [[nodiscard]] KeymapLoadResult load_profile(std::string_view profile_dir) const;

    bool save_profile(std::string_view profile_dir, const Keymap& keymap, std::string* error = nullptr) const;

    [[nodiscard]] std::vector<std::string> validate_unique_bindings(const Keymap& keymap) const;
};

}  // namespace tenriff::config
