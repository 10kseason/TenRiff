#include "app/ImportedGameplaySkin.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "app/Lr2Skin.h"
#include "app/OsuSkin.h"

namespace tenriff::app {

namespace {

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

ImportedSkinImageAsset make_asset(std::string path) {
    ImportedSkinImageAsset asset;
    asset.path = std::move(path);
    return asset;
}

void copy_osu_assets(const std::vector<std::string>& source,
                     std::vector<ImportedSkinImageAsset>& destination) {
    destination.clear();
    destination.reserve(source.size());
    for (const auto& path : source) {
        destination.push_back(make_asset(path));
    }
}

void apply_key_fallbacks(ImportedGameplaySkinDefinition& definition) {
    if (definition.key_images.empty() && !definition.note_images.empty()) {
        definition.key_images = definition.note_images;
    }
    if (definition.key_pressed_images.empty()) {
        if (!definition.hold_head_images.empty()) {
            definition.key_pressed_images = definition.hold_head_images;
        } else if (!definition.note_images.empty()) {
            definition.key_pressed_images = definition.note_images;
        }
    }
}

}  // namespace

ImportedGameplaySkinDefinition resolve_imported_gameplay_skin(std::string_view source_token,
                                                              std::string_view root_utf8,
                                                              std::string_view skin_name,
                                                              int keys) {
    ImportedGameplaySkinDefinition definition;
    const std::string source = to_lower_ascii(std::string(source_token));
    if (source == "osu") {
        const auto osu = resolve_osu_mania_skin(root_utf8, skin_name, keys);
        if (!osu.found) {
            return definition;
        }

        definition.found = true;
        definition.keys = osu.keys;
        definition.lane_divider_widths = osu.lane_divider_widths;
        definition.imported_note_width_ratio = osu.imported_note_width_ratio;
        definition.imported_note_height_ratio = osu.imported_note_height_ratio;
        definition.use_full_lane_receptor_layout = true;
        copy_osu_assets(osu.note_images, definition.note_images);
        copy_osu_assets(osu.hold_head_images, definition.hold_head_images);
        copy_osu_assets(osu.hold_body_images, definition.hold_body_images);
        copy_osu_assets(osu.hold_tail_images, definition.hold_tail_images);
        copy_osu_assets(osu.key_images, definition.key_images);
        copy_osu_assets(osu.key_pressed_images, definition.key_pressed_images);
        apply_key_fallbacks(definition);
        return definition;
    }

    if (source == "lr2") {
        const auto lr2 = resolve_lr2_play_skin(root_utf8, skin_name, keys);
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
        definition.lane_divider_widths = lr2.lane_divider_widths;
        definition.imported_note_width_ratio = lr2.imported_note_width_ratio;
        definition.imported_note_height_ratio = lr2.imported_note_height_ratio;
        definition.use_full_lane_receptor_layout = false;
        apply_key_fallbacks(definition);
        return definition;
    }

    return definition;
}

}  // namespace tenriff::app
