#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "app/menu/MenuAction.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

enum class InputSettingId : std::uint8_t {
    Backend = 0,
    PollingHz = 1,
    Debounce = 2,
    Back = 3,
};

inline constexpr std::array<InputSettingId, 4> kInputSettingOrder{
    InputSettingId::Backend,
    InputSettingId::PollingHz,
    InputSettingId::Debounce,
    InputSettingId::Back,
};

[[nodiscard]] std::optional<std::size_t> input_setting_index(InputSettingId id) noexcept;
[[nodiscard]] std::optional<InputSettingId> input_setting_id_at(std::size_t index) noexcept;

class InputSettingsController {
public:
    [[nodiscard]] InputSettingId selected_id() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] bool backend_dirty() const noexcept;

    void reset(InputSettingId selected = InputSettingId::Backend) noexcept;
    [[nodiscard]] MenuEffectFlags select(InputSettingId target) noexcept;
    [[nodiscard]] MenuEffectFlags handle(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        bool is_polling_fallback_latched,
        std::optional<InputSettingId> target = std::nullopt);

private:
    [[nodiscard]] MenuEffectFlags move_selection(int direction) noexcept;
    [[nodiscard]] MenuEffectFlags apply_selected_action(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        bool is_polling_fallback_latched);
    [[nodiscard]] MenuEffectFlags leave_screen() noexcept;
    [[nodiscard]] MenuEffectFlags mark_changed(bool backend_changed) noexcept;

    InputSettingId selected_id_ = InputSettingId::Backend;
    bool dirty_ = false;
    bool backend_dirty_ = false;
};

}  // namespace tenriff::app::menu::settings
