#include "app/menu/settings/InputSettingsController.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace tenriff::app::menu::settings {
namespace {

constexpr std::array<int, 4> kPollingOptions{1000, 2000, 4000, 8000};
constexpr std::array<int, 7> kDebounceMsOptions{0, 2, 4, 6, 8, 10, 12};

template <std::size_t Size>
int cycle_nearest_option(const std::array<int, Size>& options, int current, int direction) {
    std::size_t index = 0;
    int best_distance = std::abs(options.front() - current);
    for (std::size_t candidate = 0; candidate < options.size(); ++candidate) {
        const int distance = std::abs(options[candidate] - current);
        if (distance < best_distance) {
            best_distance = distance;
            index = candidate;
        }
        if (distance == 0) {
            index = candidate;
            break;
        }
    }
    if (direction < 0) {
        index = index == 0 ? options.size() - 1 : index - 1;
    } else {
        index = (index + 1) % options.size();
    }
    return options[index];
}

}  // namespace

std::optional<std::size_t> input_setting_index(InputSettingId id) noexcept {
    for (std::size_t index = 0; index < kInputSettingOrder.size(); ++index) {
        if (kInputSettingOrder[index] == id) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<InputSettingId> input_setting_id_at(std::size_t index) noexcept {
    if (index >= kInputSettingOrder.size()) {
        return std::nullopt;
    }
    return kInputSettingOrder[index];
}

InputSettingId InputSettingsController::selected_id() const noexcept {
    return selected_id_;
}

bool InputSettingsController::dirty() const noexcept {
    return dirty_;
}

bool InputSettingsController::backend_dirty() const noexcept {
    return backend_dirty_;
}

void InputSettingsController::reset(InputSettingId selected) noexcept {
    selected_id_ = input_setting_index(selected).has_value() ? selected : InputSettingId::Backend;
    dirty_ = false;
    backend_dirty_ = false;
}

MenuEffectFlags InputSettingsController::select(InputSettingId target) noexcept {
    if (!input_setting_index(target).has_value() || selected_id_ == target) {
        return {};
    }
    selected_id_ = target;
    return MenuEffectFlags{true};
}

MenuEffectFlags InputSettingsController::handle(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    bool is_polling_fallback_latched,
    std::optional<InputSettingId> target) {
    if (action.kind == MenuActionKind::Back) {
        return leave_screen();
    }
    if (action.kind == MenuActionKind::Move) {
        return move_selection(action.direction);
    }

    MenuEffectFlags effects;
    if (target.has_value()) {
        if (!input_setting_index(*target).has_value()) {
            return effects;
        }
        effects.merge(select(*target));
    }
    effects.merge(apply_selected_action(action, runtime, is_polling_fallback_latched));
    return effects;
}

MenuEffectFlags InputSettingsController::move_selection(int direction) noexcept {
    if (direction == 0) {
        return {};
    }
    const auto current = input_setting_index(selected_id_);
    if (!current.has_value()) {
        selected_id_ = InputSettingId::Backend;
        return MenuEffectFlags{true};
    }
    const std::size_t next = direction < 0
        ? (*current == 0 ? 0 : *current - 1)
        : std::min(*current + 1, kInputSettingOrder.size() - 1);
    if (next == *current) {
        return {};
    }
    selected_id_ = kInputSettingOrder[next];
    return MenuEffectFlags{true};
}

MenuEffectFlags InputSettingsController::apply_selected_action(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    bool is_polling_fallback_latched) {
    const bool is_adjust = action.kind == MenuActionKind::Adjust && action.direction != 0;
    const bool is_activate = action.kind == MenuActionKind::Activate;
    if (selected_id_ == InputSettingId::Back && is_activate) {
        return leave_screen();
    }
    if (!is_adjust) {
        return {};
    }

    switch (selected_id_) {
        case InputSettingId::Backend: {
            const bool requested_rawinput = action.direction > 0;
            const bool backend_changed = runtime.input.rawinput != requested_rawinput;
            const bool retry_rawinput = requested_rawinput && is_polling_fallback_latched;
            if (!backend_changed && !retry_rawinput) {
                return {};
            }
            runtime.input.rawinput = requested_rawinput;
            runtime.input.backend = requested_rawinput ? "rawinput" : "polling";
            return mark_changed(true);
        }
        case InputSettingId::PollingHz: {
            const int next = cycle_nearest_option(
                kPollingOptions, runtime.input.polling_hz, action.direction);
            if (next == runtime.input.polling_hz) {
                return {};
            }
            runtime.input.polling_hz = next;
            return mark_changed(false);
        }
        case InputSettingId::Debounce: {
            const int current = static_cast<int>(std::llround(runtime.input.debounce_ms));
            const int next = cycle_nearest_option(kDebounceMsOptions, current, action.direction);
            if (runtime.input.debounce_ms == static_cast<double>(next)) {
                return {};
            }
            runtime.input.debounce_ms = static_cast<double>(next);
            return mark_changed(false);
        }
        case InputSettingId::Back:
            return {};
    }
    return {};
}

MenuEffectFlags InputSettingsController::leave_screen() noexcept {
    MenuEffectFlags effects;
    effects.render_changed = true;
    effects.navigate_back = true;
    if (dirty_) {
        effects.persist_config = true;
        effects.restart_input = true;
        effects.reinitialize_input_backend = backend_dirty_;
    }
    selected_id_ = InputSettingId::Backend;
    dirty_ = false;
    backend_dirty_ = false;
    return effects;
}

MenuEffectFlags InputSettingsController::mark_changed(bool backend_changed) noexcept {
    dirty_ = true;
    backend_dirty_ = backend_dirty_ || backend_changed;
    return MenuEffectFlags{true};
}

}  // namespace tenriff::app::menu::settings
