#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "config/Config.h"

namespace tenriff::app {

// A portable .trskin stores only SkinConfig and the active skin's own assets.
// Input/audio/timing/account settings are deliberately outside this format.
struct SkinPresetResult {
    std::string error;
    std::string path;
    config::SkinConfig skin;
    std::size_t file_count = 0;
    std::uint64_t asset_bytes = 0;
    [[nodiscard]] bool success() const { return error.empty(); }
};

[[nodiscard]] SkinPresetResult export_skin_preset(
    std::string_view destination_utf8,
    const config::SkinConfig& skin,
    std::string_view active_skin_root_utf8);

// Installs into a newly reserved folder beneath profile/skins/{source}; existing
// folders and the caller's active config are never replaced by this operation.
[[nodiscard]] SkinPresetResult import_skin_preset(
    std::string_view source_utf8,
    std::string_view profile_dir_utf8);

}  // namespace tenriff::app
