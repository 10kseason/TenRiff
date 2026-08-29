#include "app/menu/settings/KeymapSettingsController.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "config/Keymap.h"

namespace tenriff::app {
namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_concrete_keymap_mode(std::string_view token) {
    return token == "4k" || token == "5k" || token == "6k" || token == "7k" ||
           token == "8k" || token == "9k" || token == "10k" || token == "12k" ||
           token == "14k" || token == "16k";
}

}  // namespace

std::string resolve_keymap_edit_mode_for_menu(
    std::optional<int> selected_chart_key_count,
    std::string_view runtime_key_mode) {
    config::KeymapManager manager;
    if (selected_chart_key_count.has_value()) {
        const int key_count = *selected_chart_key_count;
        if ((key_count >= 4 && key_count <= 10) || key_count == 12 ||
            key_count == 14 || key_count == 16) {
            return manager.normalize_mode_token(std::to_string(key_count) + "k");
        }
    }

    const std::string normalized_runtime =
        to_lower_ascii(std::string(runtime_key_mode));
    if (is_concrete_keymap_mode(normalized_runtime)) {
        return manager.normalize_mode_token(normalized_runtime);
    }
    return "10k";
}

namespace menu::settings {

bool KeymapSettingsEffects::empty() const noexcept {
    return menu.empty() && !refresh_input_scope;
}

void KeymapSettingsEffects::merge(const KeymapSettingsEffects& other) noexcept {
    menu.merge(other.menu);
    refresh_input_scope = refresh_input_scope || other.refresh_input_scope;
}

int KeymapSettingsController::selected_row() const noexcept {
    return selected_row_;
}

std::string_view KeymapSettingsController::edit_mode() const noexcept {
    return edit_mode_;
}

const std::vector<std::string>& KeymapSettingsController::lane_ids() const noexcept {
    return lane_ids_;
}

bool KeymapSettingsController::capture_active() const noexcept {
    return capture_active_;
}

std::int64_t KeymapSettingsController::capture_deadline_ns() const noexcept {
    return capture_deadline_ns_;
}

std::string_view KeymapSettingsController::status_message() const noexcept {
    return status_message_;
}

bool KeymapSettingsController::status_visible(std::int64_t now_ns) const noexcept {
    return !status_message_.empty() && now_ns < status_deadline_ns_;
}

std::optional<std::string_view> KeymapSettingsController::selected_lane() const noexcept {
    const int lane_index = selected_row_ - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(lane_ids_.size())) {
        return std::nullopt;
    }
    return lane_ids_[static_cast<std::size_t>(lane_index)];
}

void KeymapSettingsController::reset(
    std::optional<int> selected_chart_key_count,
    std::string_view runtime_key_mode) {
    edit_mode_ = resolve_keymap_edit_mode_for_menu(
        selected_chart_key_count, runtime_key_mode);
    selected_row_ = 0;
    capture_active_ = false;
    capture_deadline_ns_ = 0;
    clear_status();
    refresh_lanes();
}

KeymapSettingsEffects KeymapSettingsController::handle(
    const MenuAction& action,
    std::int64_t now_ns) {
    if (action.kind == MenuActionKind::Back) {
        return leave_screen();
    }
    if (action.kind == MenuActionKind::Move) {
        return move_selection(action.direction);
    }
    if (selected_row_ == 0 &&
        (action.kind == MenuActionKind::Adjust ||
         action.kind == MenuActionKind::Activate)) {
        const int direction = action.kind == MenuActionKind::Adjust
            ? action.direction
            : 1;
        return cycle_mode(direction);
    }
    if (action.kind == MenuActionKind::Activate) {
        return begin_capture(now_ns);
    }
    return {};
}

KeymapSettingsEffects KeymapSettingsController::cancel_capture() noexcept {
    if (!capture_active_) {
        return {};
    }
    capture_active_ = false;
    capture_deadline_ns_ = 0;
    KeymapSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.refresh_input_scope = true;
    return effects;
}

KeymapSettingsEffects KeymapSettingsController::update_capture_timeout(
    std::int64_t now_ns) noexcept {
    if (!capture_active_ || now_ns < capture_deadline_ns_) {
        return {};
    }
    return cancel_capture();
}

KeymapSettingsEffects KeymapSettingsController::finish_capture(
    std::string status_message,
    std::int64_t now_ns) {
    capture_active_ = false;
    capture_deadline_ns_ = 0;
    show_status(std::move(status_message), now_ns);
    KeymapSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.refresh_input_scope = true;
    return effects;
}

void KeymapSettingsController::show_status(
    std::string message,
    std::int64_t now_ns) {
    status_message_ = std::move(message);
    status_deadline_ns_ = now_ns + kKeymapStatusTimeoutNs;
}

void KeymapSettingsController::clear_status() noexcept {
    status_message_.clear();
    status_deadline_ns_ = 0;
}

void KeymapSettingsController::refresh_lanes() {
    config::KeymapManager manager;
    edit_mode_ = manager.normalize_mode_token(edit_mode_);
    lane_ids_ = manager.lane_ids_for_mode(edit_mode_);
    selected_row_ = std::clamp(
        selected_row_, 0, static_cast<int>(lane_ids_.size()));
}

KeymapSettingsEffects KeymapSettingsController::move_selection(
    int direction) noexcept {
    if (direction == 0) {
        return {};
    }
    const int next = std::clamp(
        selected_row_ + (direction < 0 ? -1 : 1),
        0,
        static_cast<int>(lane_ids_.size()));
    if (next == selected_row_) {
        return {};
    }
    selected_row_ = next;
    KeymapSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

KeymapSettingsEffects KeymapSettingsController::cycle_mode(int direction) {
    if (direction == 0) {
        return {};
    }
    config::KeymapManager manager;
    const auto modes = manager.supported_mode_tokens();
    auto current = std::find(modes.begin(), modes.end(),
                             manager.normalize_mode_token(edit_mode_));
    std::size_t index = current == modes.end()
        ? 0
        : static_cast<std::size_t>(std::distance(modes.begin(), current));
    index = direction < 0
        ? (index == 0 ? modes.size() - 1 : index - 1)
        : (index + 1) % modes.size();
    edit_mode_ = modes[index];
    refresh_lanes();
    KeymapSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.refresh_input_scope = true;
    return effects;
}

KeymapSettingsEffects KeymapSettingsController::begin_capture(
    std::int64_t now_ns) noexcept {
    if (!selected_lane().has_value()) {
        return {};
    }
    capture_active_ = true;
    capture_deadline_ns_ = now_ns + kKeymapCaptureTimeoutNs;
    KeymapSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.refresh_input_scope = true;
    return effects;
}

KeymapSettingsEffects KeymapSettingsController::leave_screen() noexcept {
    capture_active_ = false;
    capture_deadline_ns_ = 0;
    clear_status();
    KeymapSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.menu.navigate_back = true;
    effects.refresh_input_scope = true;
    return effects;
}

}  // namespace menu::settings
}  // namespace tenriff::app
