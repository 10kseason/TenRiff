#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "app/ImportedGameplaySkin.h"

namespace tenriff::app {

enum class Lr2ResolutionFamily {
    Sd,
    Hd,
    Fhd,
};

struct Lr2PlaySkinDefinition {
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
    Lr2ResolutionFamily resolution_family = Lr2ResolutionFamily::Sd;
};

[[nodiscard]] std::string find_default_lr2_skin_test_root();
[[nodiscard]] bool is_lr2_skin_directory(std::string_view path_utf8);
[[nodiscard]] std::vector<std::string> list_lr2_skin_names(std::string_view root_utf8);
[[nodiscard]] Lr2PlaySkinDefinition resolve_lr2_play_skin(std::string_view root_utf8,
                                                          std::string_view skin_name,
                                                          int keys,
                                                          std::string_view resolution_override = {});

}  // namespace tenriff::app
