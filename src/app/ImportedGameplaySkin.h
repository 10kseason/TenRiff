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

// Where an imported gear panel sits relative to the lanes it was authored around.
// The origin is the left edge of the lane block on the judgement line and one unit
// is the lane block width, so the placement survives any source resolution and any
// playfield width the renderer ends up using.
struct ImportedSkinGearPlacement {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
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
    ImportedSkinImageAsset gear_overlay_image;
    ImportedSkinGearPlacement gear_placement;
    std::vector<float> lane_divider_widths;
    std::vector<float> column_widths;
    std::vector<float> column_spacings;
    // Per-lane sprite rotation in degrees clockwise about the sprite centre, so
    // one arrow image can serve every lane. Empty means no rotation.
    std::vector<float> note_rotations;
    std::vector<float> key_rotations;
    // "stretch" | "contain" | "width". Empty means the skin said nothing and the
    // player's Image Aspect option decides. Arrow skins want "width", which sizes
    // the sprite by lane width and derives its height from the image aspect.
    std::string note_aspect;
    float hit_position = 0.0f;
    bool has_hit_position = false;
    float imported_note_width_ratio = 1.0f;
    float imported_note_height_ratio = 1.0f;
    bool use_full_lane_receptor_layout = false;
};

[[nodiscard]] ImportedGameplaySkinDefinition resolve_imported_gameplay_skin(std::string_view source_token,
                                                                            std::string_view root_utf8,
                                                                            std::string_view skin_name,
                                                                            int keys,
                                                                            std::string_view lr2_resolution_override = {});

}  // namespace tenriff::app
