#include "app/menu/MenuNavigator.h"

namespace tenriff::app::menu {

MenuNavigator::MenuNavigator(Screen root_screen) noexcept
    : current_(root_screen) {}

Screen MenuNavigator::current() const noexcept {
    return current_;
}

Screen MenuNavigator::root() const noexcept {
    return history_.empty() ? current_ : history_.front();
}

std::optional<Screen> MenuNavigator::parent() const noexcept {
    if (history_.empty()) {
        return std::nullopt;
    }
    return history_.back();
}

void MenuNavigator::reset(Screen root_screen) noexcept {
    current_ = root_screen;
    history_.clear();
}

void MenuNavigator::push(Screen screen) {
    history_.push_back(current_);
    current_ = screen;
}

void MenuNavigator::replace(Screen screen) noexcept {
    current_ = screen;
}

bool MenuNavigator::back() noexcept {
    if (history_.empty()) {
        return false;
    }

    current_ = history_.back();
    history_.pop_back();
    return true;
}

bool MenuNavigator::can_back() const noexcept {
    return !history_.empty();
}

std::size_t MenuNavigator::depth() const noexcept {
    return history_.size() + 1;
}

}  // namespace tenriff::app::menu
