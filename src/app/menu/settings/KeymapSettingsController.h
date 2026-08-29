#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/MenuAction.h"

namespace tenriff::app {

[[nodiscard]] std::string resolve_keymap_edit_mode_for_menu(
    std::optional<int> selected_chart_key_count,
    std::string_view runtime_key_mode);

namespace menu::settings {

inline constexpr std::int64_t kKeymapCaptureTimeoutNs = 5'000'000'000LL;
inline constexpr std::int64_t kKeymapStatusTimeoutNs = 2'000'000'000LL;

enum class KeymapActionId : std::uint8_t {
    Reset = 0,
    NkroTest = 1,
    Back = 2,
};

struct KeymapSettingsEffects {
    MenuEffectFlags menu{};
    bool refresh_input_scope = false;

    [[nodiscard]] bool empty() const noexcept;
    void merge(const KeymapSettingsEffects& other) noexcept;
};

class KeymapSettingsController {
public:
    [[nodiscard]] int selected_row() const noexcept;
    [[nodiscard]] std::string_view edit_mode() const noexcept;
    [[nodiscard]] const std::vector<std::string>& lane_ids() const noexcept;
    [[nodiscard]] bool capture_active() const noexcept;
    [[nodiscard]] std::int64_t capture_deadline_ns() const noexcept;
    [[nodiscard]] std::string_view status_message() const noexcept;
    [[nodiscard]] bool status_visible(std::int64_t now_ns) const noexcept;
    [[nodiscard]] std::optional<std::string_view> selected_lane() const noexcept;

    void reset(
        std::optional<int> selected_chart_key_count,
        std::string_view runtime_key_mode);
    [[nodiscard]] KeymapSettingsEffects handle(
        const MenuAction& action,
        std::int64_t now_ns);
    [[nodiscard]] KeymapSettingsEffects cancel_capture() noexcept;
    [[nodiscard]] KeymapSettingsEffects update_capture_timeout(
        std::int64_t now_ns) noexcept;
    [[nodiscard]] KeymapSettingsEffects finish_capture(
        std::string status_message,
        std::int64_t now_ns);
    void show_status(std::string message, std::int64_t now_ns);
    void clear_status() noexcept;

private:
    void refresh_lanes();
    [[nodiscard]] KeymapSettingsEffects move_selection(int direction) noexcept;
    [[nodiscard]] KeymapSettingsEffects cycle_mode(int direction);
    [[nodiscard]] KeymapSettingsEffects begin_capture(std::int64_t now_ns) noexcept;
    [[nodiscard]] KeymapSettingsEffects leave_screen() noexcept;

    std::string edit_mode_ = "10k";
    std::vector<std::string> lane_ids_{};
    int selected_row_ = 0;
    bool capture_active_ = false;
    std::int64_t capture_deadline_ns_ = 0;
    std::string status_message_{};
    std::int64_t status_deadline_ns_ = 0;
};

}  // namespace menu::settings
}  // namespace tenriff::app
