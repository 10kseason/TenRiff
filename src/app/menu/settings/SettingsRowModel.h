#pragma once

#include <optional>
#include <string>
#include <vector>

namespace tenriff::app::menu::settings {

enum class SettingsRowKind {
    Action,
    Toggle,
    Choice,
    Numeric,
    Slider,
};

struct NumericSettingRange {
    double minimum = 0.0;
    double maximum = 0.0;
    double step = 0.0;

    friend constexpr bool operator==(
        const NumericSettingRange& lhs,
        const NumericSettingRange& rhs) noexcept {
        return lhs.minimum == rhs.minimum &&
               lhs.maximum == rhs.maximum &&
               lhs.step == rhs.step;
    }

    friend constexpr bool operator!=(
        const NumericSettingRange& lhs,
        const NumericSettingRange& rhs) noexcept {
        return !(lhs == rhs);
    }
};

// This is intentionally a value-only render model. RowId is a stable screen-
// specific enum; callbacks and references to mutable application state do not
// cross the snapshot boundary.
template <typename RowId>
struct SettingsRowModel {
    RowId id{};
    SettingsRowKind kind = SettingsRowKind::Action;
    std::string label;
    std::string value;
    bool selected = false;
    bool activatable = false;
    bool adjustable = false;
    std::optional<NumericSettingRange> numeric_range;
    std::optional<double> slider_ratio;
};

template <typename RowId>
struct SettingsViewModel {
    std::vector<SettingsRowModel<RowId>> rows;
    std::vector<std::string> notes;
};

}  // namespace tenriff::app::menu::settings
