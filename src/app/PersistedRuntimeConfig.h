#pragma once

#include <string_view>

#include "config/Config.h"

namespace tenriff::app {

[[nodiscard]] bool is_session_only_mode_mod(std::string_view token);
bool strip_session_only_mode_mods(config::RuntimeConfig& config);
// Restore only the current menu's transient judgement choice after loading a
// profile. Other mode settings continue to come from the saved profile.
void apply_session_only_mode_mods(config::RuntimeConfig& config,
                                 const std::vector<std::string>& selected_mods);
[[nodiscard]] config::RuntimeConfig build_persisted_runtime_config(const config::RuntimeConfig& config);
[[nodiscard]] config::RuntimeConfig build_persisted_input_backend_config(
    const config::RuntimeConfig& persisted_base,
    const config::RuntimeConfig& runtime_source);

}  // namespace tenriff::app
