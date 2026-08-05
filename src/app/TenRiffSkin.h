#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "app/ImportedGameplaySkin.h"

namespace tenriff::app {

inline constexpr int kTenRiffSkinFormatVersion = 1;

// Menu rect override, in the 1920x1080 base coordinate space the renderer uses.
struct SkinLayoutRect {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

// Every rect a skin may move, as "<screen>.<slot>". Unknown keys are rejected
// with a warning so a typo never silently does nothing.
inline constexpr std::array<std::string_view, 13> kTenRiffSkinLayoutSlots = {
    "title.spectrum",
    "title.logo",
    "title.buttons",
    "title.guide",
    "title.footer",
    "song_select.top_bar",
    "song_select.logo",
    "song_select.nav",
    "song_select.profile",
    "song_select.left_panel",
    "song_select.center_panel",
    "song_select.right_panel",
    "song_select.bottom_bar",
};

[[nodiscard]] bool is_tenriff_skin_layout_slot(std::string_view key);

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
    std::unordered_map<std::string, SkinLayoutRect> layout_rects;
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
