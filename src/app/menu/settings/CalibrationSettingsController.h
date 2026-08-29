#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/SettingsRowModel.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

enum class CalibrationSettingId : std::uint8_t {
    AdjustmentStep = 0,
    InputOffset = 1,
    VisualOffset = 2,
    SoundOffset = 3,
    ResetOffsets = 4,
    Back = 5,
};

inline constexpr std::array<CalibrationSettingId, 6> kCalibrationSettingOrder{
    CalibrationSettingId::AdjustmentStep,
    CalibrationSettingId::InputOffset,
    CalibrationSettingId::VisualOffset,
    CalibrationSettingId::SoundOffset,
    CalibrationSettingId::ResetOffsets,
    CalibrationSettingId::Back,
};

[[nodiscard]] std::optional<std::size_t> calibration_setting_index(
    CalibrationSettingId id) noexcept;
[[nodiscard]] std::optional<CalibrationSettingId> calibration_setting_id_at(
    std::size_t index) noexcept;
[[nodiscard]] std::optional<NumericSettingRange> calibration_setting_numeric_range(
    CalibrationSettingId id) noexcept;

class CalibrationSettingsController {
public:
    [[nodiscard]] CalibrationSettingId selected_id() const noexcept;
    [[nodiscard]] int adjustment_step_ms() const noexcept;

    void reset(CalibrationSettingId selected = CalibrationSettingId::AdjustmentStep) noexcept;
    [[nodiscard]] MenuEffectFlags select(CalibrationSettingId target) noexcept;
    [[nodiscard]] MenuEffectFlags handle(
        const MenuAction& action,
        config::RuntimeConfig& runtime,
        std::optional<CalibrationSettingId> target = std::nullopt);

private:
    [[nodiscard]] MenuEffectFlags move_selection(int direction) noexcept;
    [[nodiscard]] MenuEffectFlags apply_selected_action(
        const MenuAction& action,
        config::RuntimeConfig& runtime);
    [[nodiscard]] MenuEffectFlags leave_screen() noexcept;

    CalibrationSettingId selected_id_ = CalibrationSettingId::AdjustmentStep;
    int adjustment_step_ms_ = 1;
};

}  // namespace tenriff::app::menu::settings
