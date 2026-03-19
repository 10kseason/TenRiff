#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

struct ImportedSkinImageAsset {
    std::string path;
    float source_x = 0.0f;
    float source_y = 0.0f;
    float source_width = 0.0f;
    float source_height = 0.0f;
    bool has_source_rect = false;
};

struct ImportedGameplaySkinDefinition {
    bool found = false;
    int keys = 0;
    std::vector<ImportedSkinImageAsset> note_images;
    std::vector<ImportedSkinImageAsset> hold_head_images;
    std::vector<ImportedSkinImageAsset> hold_body_images;
    std::vector<ImportedSkinImageAsset> hold_tail_images;
    std::vector<ImportedSkinImageAsset> key_images;
    std::vector<ImportedSkinImageAsset> key_pressed_images;
    std::vector<float> lane_divider_widths;
    float imported_note_width_ratio = 1.0f;
    float imported_note_height_ratio = 1.0f;
    bool use_full_lane_receptor_layout = false;
};

[[nodiscard]] ImportedGameplaySkinDefinition resolve_imported_gameplay_skin(std::string_view source_token,
                                                                            std::string_view root_utf8,
                                                                            std::string_view skin_name,
                                                                            int keys);

}  // namespace tenriff::app
