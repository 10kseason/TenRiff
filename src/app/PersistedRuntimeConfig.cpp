#include "app/PersistedRuntimeConfig.h"

#include "app/ModeManager.h"

namespace tenriff::app {

bool is_session_only_mode_mod(std::string_view token) {
    const auto* descriptor = find_mode_mod_descriptor(token);
    if (!descriptor) {
        return false;
    }
    return descriptor->token == "judge_easy" || descriptor->token == "judge_hard" ||
           descriptor->token == "judge_ex";
}

bool strip_session_only_mode_mods(config::RuntimeConfig& config) {
    const auto normalized = normalize_mode_mod_tokens(config.mode.mods);
    std::vector<std::string> persisted;
    persisted.reserve(normalized.size());
    bool removed_any = false;
    for (const auto& token : normalized) {
        if (is_session_only_mode_mod(token)) {
            removed_any = true;
            continue;
        }
        persisted.push_back(token);
    }
    if (!removed_any && persisted == normalized) {
        return false;
    }
    config.mode.mods = std::move(persisted);
    return true;
}

config::RuntimeConfig build_persisted_runtime_config(const config::RuntimeConfig& config) {
    config::RuntimeConfig persisted = config;
    static_cast<void>(strip_session_only_mode_mods(persisted));
    return persisted;
}

void apply_session_only_mode_mods(config::RuntimeConfig& config,
                                 const std::vector<std::string>& selected_mods) {
    // Normalize the source before editing config, including when it aliases
    // config.mode.mods. A Normal selection explicitly removes an old choice.
    const auto selected = normalize_mode_mod_tokens(selected_mods);
    static_cast<void>(strip_session_only_mode_mods(config));
    for (const auto& token : selected) {
        if (is_session_only_mode_mod(token)) config.mode.mods.push_back(token);
    }
    config.mode.mods = normalize_mode_mod_tokens(config.mode.mods);
}

config::RuntimeConfig build_persisted_input_backend_config(const config::RuntimeConfig& persisted_base,
                                                           const config::RuntimeConfig& runtime_source) {
    config::RuntimeConfig persisted = build_persisted_runtime_config(persisted_base);
    persisted.input = runtime_source.input;
    return persisted;
}

}  // namespace tenriff::app
