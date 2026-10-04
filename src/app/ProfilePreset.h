#pragma once

#include <string>
#include <string_view>

#include "config/Config.h"
#include "config/Keymap.h"

namespace tenriff::app {

struct ProfilePresetResult {
    std::string error;
    std::string path;
    std::string backup_path;
    config::RuntimeConfig config;
    config::Keymap keymap;
    [[nodiscard]] bool success() const { return error.empty(); }
};

// Portable settings include key bindings and an embedded .trskin. Accounts,
// library/history paths, external model paths and personal images stay local.
[[nodiscard]] ProfilePresetResult export_profile_preset(
    std::string_view destination, const config::RuntimeConfig& runtime,
    const config::Keymap& keymap, std::string_view active_skin_root);

// Validates in a fresh staging directory before changing the profile. Previous
// config/keymap files remain in settings-backups for recovery after import.
[[nodiscard]] ProfilePresetResult import_profile_preset(
    std::string_view source, std::string_view profile_dir,
    const config::RuntimeConfig& current);

} // namespace tenriff::app
