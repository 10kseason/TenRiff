#pragma once

#include <string_view>

namespace tenriff::app {

void install_crash_handlers() noexcept;
void write_current_exception_log(std::string_view context = {}) noexcept;

}  // namespace tenriff::app
