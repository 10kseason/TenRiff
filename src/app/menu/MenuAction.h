#pragma once

namespace tenriff::app::menu {

// Platform input is normalized to this small action vocabulary before it
// reaches a screen controller.
enum class MenuActionKind {
    Move,
    Activate,
    Adjust,
    SetRatio,
    Back,
};

struct MenuAction {
    MenuActionKind kind = MenuActionKind::Activate;
    int direction = 0;
    double ratio = 0.0;

    [[nodiscard]] static constexpr MenuAction move(int raw_direction) noexcept {
        return MenuAction{MenuActionKind::Move, normalize_direction(raw_direction), 0.0};
    }

    [[nodiscard]] static constexpr MenuAction activate() noexcept {
        return MenuAction{MenuActionKind::Activate, 0, 0.0};
    }

    [[nodiscard]] static constexpr MenuAction adjust(int raw_direction) noexcept {
        return MenuAction{MenuActionKind::Adjust, normalize_direction(raw_direction), 0.0};
    }

    [[nodiscard]] static constexpr MenuAction set_ratio(double value) noexcept {
        return MenuAction{MenuActionKind::SetRatio, 0, value};
    }

    [[nodiscard]] static constexpr MenuAction back() noexcept {
        return MenuAction{MenuActionKind::Back, 0, 0.0};
    }

private:
    [[nodiscard]] static constexpr int normalize_direction(int direction) noexcept {
        return direction < 0 ? -1 : (direction > 0 ? 1 : 0);
    }
};

// Controllers describe boundary work; they never perform persistence, thread
// restarts, navigation, or rendering themselves.
struct MenuEffectFlags {
    bool render_changed = false;
    bool persist_config = false;
    bool restart_audio = false;
    bool navigate_back = false;
    bool restart_input = false;
    bool reinitialize_input_backend = false;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return !render_changed && !persist_config && !restart_audio && !navigate_back &&
               !restart_input && !reinitialize_input_backend;
    }

    constexpr void merge(const MenuEffectFlags& other) noexcept {
        render_changed = render_changed || other.render_changed;
        persist_config = persist_config || other.persist_config;
        restart_audio = restart_audio || other.restart_audio;
        navigate_back = navigate_back || other.navigate_back;
        restart_input = restart_input || other.restart_input;
        reinitialize_input_backend =
            reinitialize_input_backend || other.reinitialize_input_backend;
    }
};

}  // namespace tenriff::app::menu
