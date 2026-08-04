#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "app/ImportedGameplaySkin.h"

namespace tenriff::app {

inline constexpr int kTenRiffSkinFormatVersion = 1;

struct TenRiffSkinDefinition {
    bool found = false;
    int format_version = 0;
    std::string folder_name;
    std::string name;
    std::string author;
    std::string root_path;
    std::string lobby_background_path;
    std::string lobby_logo_path;
    float lobby_background_opacity = 0.72f;
    std::string gameplay_background_path;
    float gameplay_background_opacity = 0.66f;
    ImportedGameplaySkinDefinition gameplay;
    std::vector<std::string> referenced_asset_paths;
    std::vector<std::string> warnings;
};

struct TenRiffSkinImportResult {
    std::string skin_name;
    std::string install_root;
    std::size_t copied_files = 0;
    std::uintmax_t copied_bytes = 0;
    std::vector<std::string> warnings;

    [[nodiscard]] bool success() const { return !skin_name.empty(); }
};

[[nodiscard]] TenRiffSkinDefinition load_tenriff_skin_folder(std::string_view folder_utf8,
                                                              int keys);
[[nodiscard]] TenRiffSkinDefinition resolve_tenriff_skin(std::string_view root_utf8,
                                                         std::string_view skin_name,
                                                         int keys);
[[nodiscard]] std::vector<std::string> list_tenriff_skin_names(std::string_view root_utf8);
[[nodiscard]] TenRiffSkinImportResult import_tenriff_skin(std::string_view source_utf8,
                                                          std::string_view import_root_utf8);

}  // namespace tenriff::app
