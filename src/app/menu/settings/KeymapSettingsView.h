#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/menu/settings/KeymapSettingsController.h"
#include "config/Keymap.h"

namespace tenriff::app::menu::settings {

struct KeymapViewRow {
    std::string label;
    std::string value;
    bool selected = false;
    std::optional<KeymapActionId> action;
};

struct KeymapSettingsViewModel {
    std::vector<KeymapViewRow> rows;
    std::vector<std::string> footer_notes;
    int footer_reserved_lines = 0;
};

class KeymapSettingsView {
public:
    [[nodiscard]] static KeymapSettingsViewModel build(
        const KeymapSettingsController& controller,
        const config::Keymap& working_keymap,
        std::optional<int> selected_chart_key_count,
        std::string_view runtime_key_mode,
        std::string backend_status,
        std::int64_t now_ns,
        bool use_korean);

    [[nodiscard]] static KeymapSettingsViewModel build_nkro_test(
        const KeymapSettingsController& controller,
        const config::Keymap& working_keymap,
        const std::unordered_set<std::uint32_t>& pressed_keys,
        std::string backend_status,
        bool use_korean);
};

}  // namespace tenriff::app::menu::settings
