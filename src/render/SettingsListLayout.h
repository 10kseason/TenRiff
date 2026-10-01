#pragma once

#include <algorithm>
#include <cstddef>

namespace tenriff::render {

struct SettingsListWindow { int start = 0; int count = 0; };

template <typename Rows>
bool settings_category_heading(const Rows& rows, int index, int start) {
    return !rows[index].category.empty() &&
        (index == start || rows[index].category != rows[index - 1].category);
}

// Headings consume space but never become selectable rows or pointer targets.
// Always keep the keyboard-selected row in the visible window.
template <typename Rows>
SettingsListWindow settings_list_window(const Rows& rows, int selected, float available,
                                       float row_step, float heading_height) {
    const int size = static_cast<int>(rows.size());
    if (size == 0 || row_step <= 0) return {};
    selected = std::clamp(selected, 0, size - 1);
    const auto height = [&](int start, int end) {
        float total = 0;
        for (int i = start; i < end; ++i)
            total += row_step + (settings_category_heading(rows, i, start) ? heading_height : 0);
        return total;
    };
    int start = std::max(0, selected - std::max(1, static_cast<int>(available / row_step)) / 2);
    while (start < selected && height(start, selected + 1) > available) ++start;
    int end = start;
    float used = 0;
    while (end < size) {
        const float next = row_step + (settings_category_heading(rows, end, start) ? heading_height : 0);
        if (end > selected && used + next > available) break;
        used += next;
        ++end;
    }
    if (end == size) {
        while (start > 0 && height(start - 1, end) <= available) --start;
    }
    return {start, end - start};
}

}  // namespace tenriff::render
