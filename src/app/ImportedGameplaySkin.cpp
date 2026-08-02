#include "app/ImportedGameplaySkin.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "app/Lr2Skin.h"

namespace tenriff::app {

namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void apply_asset_fallbacks(ImportedGameplaySkinDefinition& definition) {
    const std::size_t lane_count = (std::max)({
        static_cast<std::size_t>(std::max(definition.keys, 0)),
        definition.note_images.size(),
        definition.hold_head_images.size(),
        definition.hold_tail_images.size(),
        definition.key_images.size(),
        definition.key_pressed_images.size(),
    });
    const auto resize_preserving_broadcast = [lane_count](
                                                  std::vector<ImportedSkinImageAsset>& assets) {
        if (assets.size() == 1u && lane_count > 1u) {
            const ImportedSkinImageAsset broadcast = assets.front();
            assets.resize(lane_count, broadcast);
            return;
        }
        assets.resize(lane_count);
    };
    resize_preserving_broadcast(definition.hold_tail_images);
    resize_preserving_broadcast(definition.key_images);
    resize_preserving_broadcast(definition.key_pressed_images);

    const auto asset_at = [](const std::vector<ImportedSkinImageAsset>& assets,
                             std::size_t lane) -> const ImportedSkinImageAsset* {
        if (assets.empty()) {
            return nullptr;
        }
        const std::size_t index = (assets.size() == 1u) ? 0u : lane;
        if (index >= assets.size() || assets[index].path.empty()) {
            return nullptr;
        }
        return &assets[index];
    };

    for (std::size_t lane = 0; lane < lane_count; ++lane) {
        const ImportedSkinImageAsset* hold_head = asset_at(definition.hold_head_images, lane);

        if (definition.hold_tail_images[lane].path.empty() && hold_head != nullptr) {
            definition.hold_tail_images[lane] = *hold_head;
        }
        // Falling-note art is not a receptor. Reusing it here leaves a note or
        // LN head parked on the judgement line for the duration of a hold.
        if (definition.key_pressed_images[lane].path.empty() &&
            !definition.key_images[lane].path.empty()) {
            definition.key_pressed_images[lane] = definition.key_images[lane];
        }
    }
}

}  // namespace

ImportedGameplaySkinDefinition resolve_imported_gameplay_skin(std::string_view source_token,
                                                              std::string_view root_utf8,
                                                              std::string_view skin_name,
                                                              int keys,
                                                              std::string_view lr2_resolution_override) {
    ImportedGameplaySkinDefinition definition;
    const std::string source = to_lower_ascii(std::string(source_token));
    if (source == "lr2") {
        const auto lr2 = resolve_lr2_play_skin(root_utf8, skin_name, keys, lr2_resolution_override);
        if (!lr2.found) {
            return definition;
        }
        definition.found = true;
        definition.keys = lr2.keys;
        definition.note_images = lr2.note_images;
        definition.hold_head_images = lr2.hold_head_images;
        definition.hold_body_images = lr2.hold_body_images;
        definition.hold_tail_images = lr2.hold_tail_images;
        definition.key_images = lr2.key_images;
        definition.key_pressed_images = lr2.key_pressed_images;
        definition.gear_overlay_image = lr2.gear_overlay_image;
        definition.lane_divider_widths = lr2.lane_divider_widths;
        definition.imported_note_width_ratio = lr2.imported_note_width_ratio;
        definition.imported_note_height_ratio = lr2.imported_note_height_ratio;
        definition.use_full_lane_receptor_layout = lr2.use_full_lane_receptor_layout;
        apply_asset_fallbacks(definition);
        return definition;
    }

    return definition;
}

}  // namespace tenriff::app
