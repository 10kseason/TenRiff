#include "app/menu/settings/CalibrationSettingsController.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace tenriff::app::menu::settings {
namespace {

constexpr std::array<int, 4> kAdjustmentStepOptions{1, 5, 10, 25};
constexpr NumericSettingRange kInputOffsetRange{-250.0, 250.0, 1.0};
constexpr NumericSettingRange kVisualOffsetRange{
    config::kVisualOffsetMin, config::kVisualOffsetMax, 1.0};
constexpr NumericSettingRange kSoundOffsetRange{
    config::kSoundOffsetMin, config::kSoundOffsetMax, 1.0};

double snap_to_step(double value, double minimum, double maximum, double step) {
    if (!std::isfinite(value)) {
        return minimum;
    }
    const double clamped = std::clamp(value, minimum, maximum);
    const double steps = std::round((clamped - minimum) / step);
    return std::clamp(minimum + steps * step, minimum, maximum);
}

bool adjust_value(double& destination, int direction, int step, double minimum, double maximum) {
    const double next = snap_to_step(
        destination + static_cast<double>(direction * step),
        minimum,
        maximum,
        static_cast<double>(step));
    if (destination == next) {
        return false;
    }
    destination = next;
    return true;
}

}  // namespace

std::optional<std::size_t> calibration_setting_index(CalibrationSettingId id) noexcept {
    for (std::size_t index = 0; index < kCalibrationSettingOrder.size(); ++index) {
        if (kCalibrationSettingOrder[index] == id) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<CalibrationSettingId> calibration_setting_id_at(std::size_t index) noexcept {
    if (index >= kCalibrationSettingOrder.size()) {
        return std::nullopt;
    }
    return kCalibrationSettingOrder[index];
}

std::optional<NumericSettingRange> calibration_setting_numeric_range(
    CalibrationSettingId id) noexcept {
    switch (id) {
        case CalibrationSettingId::InputOffset:
            return kInputOffsetRange;
        case CalibrationSettingId::VisualOffset:
            return kVisualOffsetRange;
        case CalibrationSettingId::SoundOffset:
            return kSoundOffsetRange;
        case CalibrationSettingId::AdjustmentStep:
        case CalibrationSettingId::ResetOffsets:
        case CalibrationSettingId::Back:
            return std::nullopt;
    }
    return std::nullopt;
}

CalibrationSettingId CalibrationSettingsController::selected_id() const noexcept {
    return selected_id_;
}

int CalibrationSettingsController::adjustment_step_ms() const noexcept {
    return adjustment_step_ms_;
}

void CalibrationSettingsController::reset(CalibrationSettingId selected) noexcept {
    selected_id_ = calibration_setting_index(selected).has_value()
        ? selected
        : CalibrationSettingId::AdjustmentStep;
}

MenuEffectFlags CalibrationSettingsController::select(CalibrationSettingId target) noexcept {
    if (!calibration_setting_index(target).has_value() || selected_id_ == target) {
        return {};
    }
    selected_id_ = target;
    return MenuEffectFlags{true};
}

MenuEffectFlags CalibrationSettingsController::handle(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    std::optional<CalibrationSettingId> target) {
    if (action.kind == MenuActionKind::Back) {
        return leave_screen();
    }
    if (action.kind == MenuActionKind::Move) {
        return move_selection(action.direction);
    }

    MenuEffectFlags effects;
    if (target.has_value()) {
        if (!calibration_setting_index(*target).has_value()) {
            return effects;
        }
        effects.merge(select(*target));
    }
    effects.merge(apply_selected_action(action, runtime));
    return effects;
}

MenuEffectFlags CalibrationSettingsController::move_selection(int direction) noexcept {
    if (direction == 0) {
        return {};
    }
    const auto current = calibration_setting_index(selected_id_);
    if (!current.has_value()) {
        selected_id_ = CalibrationSettingId::AdjustmentStep;
        return MenuEffectFlags{true};
    }
    const std::size_t next = direction < 0
        ? (*current == 0 ? 0 : *current - 1)
        : std::min(*current + 1, kCalibrationSettingOrder.size() - 1);
    if (next == *current) {
        return {};
    }
    selected_id_ = kCalibrationSettingOrder[next];
    return MenuEffectFlags{true};
}

MenuEffectFlags CalibrationSettingsController::apply_selected_action(
    const MenuAction& action,
    config::RuntimeConfig& runtime) {
    const bool is_adjust = action.kind == MenuActionKind::Adjust && action.direction != 0;
    const bool is_activate = action.kind == MenuActionKind::Activate;
    if (selected_id_ == CalibrationSettingId::Back && is_activate) {
        return leave_screen();
    }
    if (selected_id_ == CalibrationSettingId::ResetOffsets && is_activate) {
        const bool changed = runtime.input_offset_ms != 0.0 ||
            runtime.visual_offset_ms != 0.0 || runtime.sound_offset_ms != 0.0;
        if (!changed) {
            return {};
        }
        runtime.input_offset_ms = 0.0;
        runtime.visual_offset_ms = 0.0;
        runtime.sound_offset_ms = 0.0;
        return MenuEffectFlags{true, true};
    }
    if (!is_adjust) {
        return {};
    }

    bool changed = false;
    switch (selected_id_) {
        case CalibrationSettingId::AdjustmentStep: {
            const auto current = std::find(
                kAdjustmentStepOptions.begin(), kAdjustmentStepOptions.end(), adjustment_step_ms_);
            std::size_t index = current == kAdjustmentStepOptions.end()
                ? 0
                : static_cast<std::size_t>(current - kAdjustmentStepOptions.begin());
            if (action.direction < 0) {
                index = index == 0 ? kAdjustmentStepOptions.size() - 1 : index - 1;
            } else {
                index = (index + 1) % kAdjustmentStepOptions.size();
            }
            adjustment_step_ms_ = kAdjustmentStepOptions[index];
            return MenuEffectFlags{true};
        }
        case CalibrationSettingId::InputOffset:
            changed = adjust_value(
                runtime.input_offset_ms, action.direction, adjustment_step_ms_, -250.0, 250.0);
            break;
        case CalibrationSettingId::VisualOffset:
            changed = adjust_value(
                runtime.visual_offset_ms,
                action.direction,
                adjustment_step_ms_,
                config::kVisualOffsetMin,
                config::kVisualOffsetMax);
            break;
        case CalibrationSettingId::SoundOffset:
            changed = adjust_value(
                runtime.sound_offset_ms,
                action.direction,
                adjustment_step_ms_,
                config::kSoundOffsetMin,
                config::kSoundOffsetMax);
            break;
        case CalibrationSettingId::ResetOffsets:
        case CalibrationSettingId::Back:
            break;
    }
    return changed ? MenuEffectFlags{true, true} : MenuEffectFlags{};
}

MenuEffectFlags CalibrationSettingsController::leave_screen() noexcept {
    selected_id_ = CalibrationSettingId::AdjustmentStep;
    MenuEffectFlags effects;
    effects.render_changed = true;
    effects.navigate_back = true;
    return effects;
}

}  // namespace tenriff::app::menu::settings
