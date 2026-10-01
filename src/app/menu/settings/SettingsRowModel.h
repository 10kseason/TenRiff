#pragma once

#include <optional>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace tenriff::app::menu::settings {

enum class SettingsRowKind {
    Action,
    Toggle,
    Choice,
    Numeric,
    Slider,
};

// Pointer snapshots carry stable enum IDs, whereas keyboard navigation uses
// display positions. Validate the ID before casting, including narrow enums.
template <typename RowId, typename IndexLookup>
[[nodiscard]] std::optional<RowId> settings_id_from_hit(int hit_id, IndexLookup index_lookup) noexcept {
    using Underlying = std::underlying_type_t<RowId>;
    if (hit_id < 0 || static_cast<std::uint64_t>(hit_id) >
                          static_cast<std::uint64_t>(std::numeric_limits<Underlying>::max())) {
        return std::nullopt;
    }
    const auto id = static_cast<RowId>(hit_id);
    return index_lookup(id).has_value() ? std::optional<RowId>{id} : std::nullopt;
}

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
    std::string category;
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
