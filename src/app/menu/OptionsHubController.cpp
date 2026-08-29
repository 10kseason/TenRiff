#include "app/menu/OptionsHubController.h"

#include <algorithm>
#include <cstdint>

namespace tenriff::app::menu {
namespace {

constexpr std::size_t kColumnCount = 4;
constexpr std::size_t kRowCount = 2;
static_assert(kOptionsItemRoutes.size() == kColumnCount * kRowCount);

[[nodiscard]] std::size_t clamp_axis_move(
    std::size_t position,
    int direction,
    std::size_t upper_bound) noexcept {
    const auto candidate = static_cast<std::int64_t>(position) + direction;
    return static_cast<std::size_t>(std::clamp<std::int64_t>(
        candidate,
        0,
        static_cast<std::int64_t>(upper_bound)));
}

}  // namespace

std::optional<std::size_t> options_item_index(OptionsItemId id) noexcept {
    for (std::size_t index = 0; index < kOptionsItemRoutes.size(); ++index) {
        if (kOptionsItemRoutes[index].id == id) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<OptionsItemId> options_item_id_at(std::size_t index) noexcept {
    if (index >= kOptionsItemRoutes.size()) {
        return std::nullopt;
    }
    return kOptionsItemRoutes[index].id;
}

OptionsHubController::OptionsHubController(OptionsItemId initial_cursor) noexcept {
    (void)set_cursor(initial_cursor);
}

void OptionsHubController::move_horizontal(int direction) noexcept {
    const std::size_t row = cursor_index_ / kColumnCount;
    const std::size_t column = cursor_index_ % kColumnCount;
    const std::size_t next_column = clamp_axis_move(column, direction, kColumnCount - 1);
    cursor_index_ = row * kColumnCount + next_column;
}

void OptionsHubController::move_vertical(int direction) noexcept {
    const std::size_t row = cursor_index_ / kColumnCount;
    const std::size_t column = cursor_index_ % kColumnCount;
    const std::size_t next_row = clamp_axis_move(row, direction, kRowCount - 1);
    cursor_index_ = next_row * kColumnCount + column;
}

bool OptionsHubController::set_cursor(OptionsItemId cursor) noexcept {
    const std::optional<std::size_t> index = options_item_index(cursor);
    if (!index.has_value()) {
        return false;
    }
    cursor_index_ = *index;
    return true;
}

OptionsItemId OptionsHubController::cursor() const noexcept {
    return kOptionsItemRoutes[cursor_index_].id;
}

Screen OptionsHubController::activate() const noexcept {
    return kOptionsItemRoutes[cursor_index_].destination;
}

}  // namespace tenriff::app::menu
