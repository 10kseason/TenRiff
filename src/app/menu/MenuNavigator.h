#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "app/menu/MenuScreen.h"

namespace tenriff::app::menu {

class MenuNavigator {
public:
    explicit MenuNavigator(Screen root_screen = Screen::Title) noexcept;

    [[nodiscard]] Screen current() const noexcept;
    [[nodiscard]] Screen root() const noexcept;
    [[nodiscard]] std::optional<Screen> parent() const noexcept;

    // Starts a new root flow. No screen from the previous flow remains reachable.
    void reset(Screen root_screen) noexcept;

    // Enters a nested screen and remembers the current screen for back().
    void push(Screen screen);

    // Changes only the current step, retaining any deliberate parent history.
    void replace(Screen screen) noexcept;

    // Returns false at a root and leaves the current screen unchanged.
    [[nodiscard]] bool back() noexcept;

    [[nodiscard]] bool can_back() const noexcept;

    // Includes the current screen, so every navigator has a minimum depth of one.
    [[nodiscard]] std::size_t depth() const noexcept;

private:
    Screen current_;
    std::vector<Screen> history_;
};

}  // namespace tenriff::app::menu
