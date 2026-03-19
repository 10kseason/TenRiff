#pragma once

#include <string_view>

#include "config/Config.h"

namespace tenriff::app {

[[nodiscard]] bool is_session_only_mode_mod(std::string_view token);
bool strip_session_only_mode_mods(config::RuntimeConfig& config);
[[nodiscard]] config::RuntimeConfig build_persisted_runtime_config(const config::RuntimeConfig& config);

}  // namespace tenriff::app
