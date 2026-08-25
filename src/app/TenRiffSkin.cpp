#include "app/TenRiffSkin.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <unordered_set>

#include "config/SimpleJson.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

namespace fs = std::filesystem;

constexpr std::uintmax_t kMaxManifestBytes = 1024u * 1024u;

fs::path path_from_utf8(std::string_view value) {
    try {
        return util::path_from_utf8_lossy(value);
    } catch (...) {
        return {};
    }
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const config::JsonValue* object_value(const config::JsonObject& object, std::string_view key) {
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

const config::JsonObject* child_object(const config::JsonObject& object, std::string_view key) {
    const auto* value = object_value(object, key);
    return value ? value->as_object() : nullptr;
}

std::string string_value(const config::JsonObject& object,
                         std::string_view key,
                         std::string fallback = {}) {
    const auto* value = object_value(object, key);
    return value && value->is_string() ? value->as_string() : std::move(fallback);
}

double number_value(const config::JsonObject& object,
                    std::string_view key,
                    double fallback) {
    const auto* value = object_value(object, key);
    return value && value->is_number() ? value->as_number(fallback) : fallback;
}

bool bool_value(const config::JsonObject& object, std::string_view key, bool fallback) {
    const auto* value = object_value(object, key);
    return value && value->is_bool() ? value->as_bool(fallback) : fallback;
}

bool is_supported_image_extension(const fs::path& path) {
    const std::string ext = lower_ascii(path.extension().u8string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
}

bool is_within_root(const fs::path& root, const fs::path& candidate) {
    std::error_code ec;
    const fs::path canonical_root = fs::weakly_canonical(root, ec);
    if (ec) {
        return false;
    }
    const fs::path canonical_candidate = fs::weakly_canonical(candidate, ec);
    if (ec) {
        return false;
    }
    const fs::path relative = fs::relative(canonical_candidate, canonical_root, ec);
    if (ec || relative.empty() || relative.is_absolute() || relative.has_root_name()) {
        return false;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

std::optional<std::string> resolve_asset_path(const fs::path& root,
                                              std::string_view raw,
                                              std::vector<std::string>& warnings,
                                              std::string_view field) {
    if (raw.empty()) {
        return std::nullopt;
    }
    const fs::path relative = path_from_utf8(raw);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.has_root_directory()) {
        warnings.push_back(std::string(field) + " must use a path relative to skin.json.");
        return std::nullopt;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            warnings.push_back(std::string(field) + " cannot leave the skin folder.");
            return std::nullopt;
        }
    }
    const fs::path candidate = root / relative;
    std::error_code ec;
    if (!fs::is_regular_file(candidate, ec) || ec || !is_supported_image_extension(candidate) ||
        !is_within_root(root, candidate)) {
        warnings.push_back(std::string(field) + " image was not found or is unsupported: " +
                           std::string(raw));
        return std::nullopt;
    }
    return candidate.u8string();
}

void add_reference(TenRiffSkinDefinition& definition, const std::string& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(definition.referenced_asset_paths.begin(),
                  definition.referenced_asset_paths.end(),
                  path) == definition.referenced_asset_paths.end()) {
        definition.referenced_asset_paths.push_back(path);
    }
}

template <std::size_t N>
void warn_unknown_keys(const config::JsonObject& object,
                       const std::array<std::string_view, N>& allowed,
                       std::string_view prefix,
                       TenRiffSkinDefinition& definition) {
    for (const auto& [key, value] : object) {
        (void)value;
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            definition.warnings.push_back(std::string(prefix) + key + " is not a supported field.");
        }
    }
}

std::optional<bool> optional_bool(const config::JsonObject& object,
                                  std::string_view key,
                                  TenRiffSkinDefinition& definition,
                                  std::string_view prefix) {
    const auto* value = object_value(object, key);
    if (!value) {
        return std::nullopt;
    }
    if (!value->is_bool()) {
        definition.warnings.push_back(std::string(prefix) + std::string(key) + " must be true or false.");
        return std::nullopt;
    }
    return value->as_bool();
}

std::optional<float> optional_number(const config::JsonObject& object,
                                     std::string_view key,
                                     float minimum,
                                     float maximum,
                                     TenRiffSkinDefinition& definition,
                                     std::string_view prefix) {
    const auto* value = object_value(object, key);
    if (!value) {
        return std::nullopt;
    }
    if (!value->is_number()) {
        definition.warnings.push_back(std::string(prefix) + std::string(key) + " must be a number.");
        return std::nullopt;
    }
    const double number = value->as_number();
    if (!std::isfinite(number) || number < minimum || number > maximum) {
        definition.warnings.push_back(std::string(prefix) + std::string(key) + " must be between " +
                                      std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
        return std::nullopt;
    }
    return static_cast<float>(number);
}

std::optional<std::string> optional_enum(const config::JsonObject& object,
                                         std::string_view key,
                                         std::initializer_list<std::string_view> allowed,
                                         TenRiffSkinDefinition& definition,
                                         std::string_view prefix) {
    const auto* value = object_value(object, key);
    if (!value) {
        return std::nullopt;
    }
    if (!value->is_string()) {
        definition.warnings.push_back(std::string(prefix) + std::string(key) + " must be a string.");
        return std::nullopt;
    }
    const std::string normalized = lower_ascii(value->as_string());
    if (std::find(allowed.begin(), allowed.end(), normalized) == allowed.end()) {
        definition.warnings.push_back(std::string(prefix) + std::string(key) + " has an unsupported value.");
        return std::nullopt;
    }
    return normalized;
}

std::optional<std::array<float, 4>> parse_hex_color(std::string_view raw) {
    if (!raw.empty() && raw.front() == '#') {
        raw.remove_prefix(1);
    }
    if (raw.size() != 6u && raw.size() != 8u) {
        return std::nullopt;
    }
    auto nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    std::array<unsigned int, 4> bytes{0u, 0u, 0u, 255u};
    for (std::size_t i = 0; i < raw.size() / 2u; ++i) {
        const int hi = nibble(raw[i * 2u]);
        const int lo = nibble(raw[i * 2u + 1u]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        bytes[i] = static_cast<unsigned int>(hi * 16 + lo);
    }
    return std::array<float, 4>{
        static_cast<float>(bytes[0]) / 255.0f,
        static_cast<float>(bytes[1]) / 255.0f,
        static_cast<float>(bytes[2]) / 255.0f,
        static_cast<float>(bytes[3]) / 255.0f,
    };
}

std::vector<std::string> parse_lane_map(const config::JsonObject& gameplay,
                                        int keys,
                                        TenRiffSkinDefinition& definition) {
    std::vector<std::string> lanes;
    const auto* value = object_value(gameplay, "lane_map");
    if (!value) {
        return lanes;
    }
    const auto* array = value->as_array();
    if (!array) {
        definition.warnings.push_back("gameplay.lane_map must be an array of lane names.");
        return lanes;
    }
    lanes.reserve(std::min<std::size_t>(array->size(), 16u));
    for (std::size_t i = 0; i < array->size() && i < 16u; ++i) {
        if (!(*array)[i].is_string() || (*array)[i].as_string().empty()) {
            definition.warnings.push_back("gameplay.lane_map entries must be non-empty strings.");
            lanes.emplace_back();
        } else {
            lanes.push_back((*array)[i].as_string());
        }
    }
    if (!lanes.empty() && lanes.size() < static_cast<std::size_t>(keys)) {
        definition.warnings.push_back("gameplay.lane_map has fewer entries than the active key mode.");
    }
    return lanes;
}

void replace_all(std::string& value, std::string_view token, std::string_view replacement) {
    std::size_t offset = 0;
    while ((offset = value.find(token, offset)) != std::string::npos) {
        value.replace(offset, token.size(), replacement);
        offset += replacement.size();
    }
}

std::string expand_lane_pattern(std::string value,
                                std::string_view lane,
                                std::size_t index) {
    replace_all(value, "{lane}", lane);
    replace_all(value, "{index}", std::to_string(index + 1u));
    const std::string padded = index + 1u < 10u ? "0" + std::to_string(index + 1u)
                                                : std::to_string(index + 1u);
    replace_all(value, "{index:02}", padded);
    return value;
}

std::optional<std::string> conventional_asset_path(const fs::path& root,
                                                   std::string_view relative_stem) {
    static constexpr std::array<std::string_view, 4> kExtensions = {
        ".png", ".jpg", ".jpeg", ".bmp"
    };
    for (const auto extension : kExtensions) {
        const std::string relative = std::string(relative_stem) + std::string(extension);
        std::error_code ec;
        if (fs::is_regular_file(root / path_from_utf8(relative), ec) && !ec) {
            return relative;
        }
    }
    return std::nullopt;
}

std::vector<ImportedSkinImageAsset> parse_asset_list(const config::JsonObject& object,
                                                     std::string_view key,
                                                     const fs::path& root,
                                                     TenRiffSkinDefinition& definition,
                                                     const std::vector<std::string>& lane_map,
                                                     int keys,
                                                     std::string_view conventional_stem) {
    std::vector<ImportedSkinImageAsset> assets;
    const config::JsonValue* value = object_value(object, key);
    std::optional<config::JsonValue> conventional_value;
    if (!value && !conventional_stem.empty()) {
        if (const auto detected = conventional_asset_path(root, conventional_stem)) {
            conventional_value.emplace(*detected);
            value = &*conventional_value;
        }
    }
    if (!value) return assets;
    auto append = [&](const std::string& raw, std::size_t index) {
        const std::string field = std::string("gameplay.") + std::string(key) +
                                  (index == 0u ? std::string{} : "[" + std::to_string(index) + "]");
        const auto resolved = resolve_asset_path(root, raw, definition.warnings, field);
        if (!resolved.has_value()) {
            assets.emplace_back();
            return;
        }
        ImportedSkinImageAsset asset;
        asset.path = *resolved;
        add_reference(definition, asset.path);
        assets.push_back(std::move(asset));
    };
    if (value->is_string()) {
        const std::string raw = value->as_string();
        const bool patterned = raw.find("{lane}") != std::string::npos ||
                               raw.find("{index}") != std::string::npos ||
                               raw.find("{index:02}") != std::string::npos;
        if (patterned) {
            if (raw.find("{lane}") != std::string::npos && lane_map.empty()) {
                definition.warnings.push_back("gameplay." + std::string(key) +
                                              " uses {lane} but gameplay.lane_map is empty.");
                return assets;
            }
            assets.reserve(static_cast<std::size_t>(keys));
            for (int lane_index = 0; lane_index < keys; ++lane_index) {
                const std::string_view lane = static_cast<std::size_t>(lane_index) < lane_map.size()
                                                  ? std::string_view(lane_map[static_cast<std::size_t>(lane_index)])
                                                  : std::string_view{};
                append(expand_lane_pattern(raw, lane, static_cast<std::size_t>(lane_index)),
                       static_cast<std::size_t>(lane_index));
            }
        } else {
            append(raw, 0u);
        }
    } else if (const auto* array = value->as_array()) {
        assets.reserve(array->size());
        for (std::size_t i = 0; i < array->size(); ++i) {
            if (!(*array)[i].is_string()) {
                definition.warnings.push_back("gameplay." + std::string(key) +
                                              " entries must be image paths.");
                assets.emplace_back();
                continue;
            }
            append((*array)[i].as_string(), i);
        }
    } else {
        definition.warnings.push_back("gameplay." + std::string(key) +
                                      " must be an image path or path array.");
    }
    return assets;
}

std::vector<float> parse_positive_number_array(const config::JsonObject& object,
                                               std::string_view key,
                                               std::size_t max_count) {
    std::vector<float> values;
    const config::JsonValue* value = object_value(object, key);
    const auto* array = value ? value->as_array() : nullptr;
    if (!array) {
        return values;
    }
    values.reserve(std::min(array->size(), max_count));
    for (std::size_t i = 0; i < array->size() && i < max_count; ++i) {
        const double number = (*array)[i].as_number(0.0);
        values.push_back(std::isfinite(number) && number > 0.0
                             ? static_cast<float>(number)
                             : 0.0f);
    }
    return values;
}

// Degrees clockwise, wrapped into [0, 360). Any finite value is legal, so this
// cannot reuse parse_positive_number_array.
std::vector<float> parse_rotation_array(const config::JsonObject& object,
                                        std::string_view key,
                                        std::size_t max_count) {
    std::vector<float> values;
    const config::JsonValue* value = object_value(object, key);
    const auto* array = value ? value->as_array() : nullptr;
    if (!array) {
        return values;
    }
    values.reserve(std::min(array->size(), max_count));
    for (std::size_t i = 0; i < array->size() && i < max_count; ++i) {
        const double degrees = (*array)[i].as_number(0.0);
        if (!std::isfinite(degrees)) {
            values.push_back(0.0f);
            continue;
        }
        double wrapped = std::fmod(degrees, 360.0);
        if (wrapped < 0.0) {
            wrapped += 360.0;
        }
        values.push_back(static_cast<float>(wrapped));
    }
    return values;
}

constexpr double kMaxLayoutCoordinate = 8192.0;

bool parse_layout_rect(const config::JsonValue& value, SkinLayoutRect& out) {
    const auto* array = value.as_array();
    if (!array || array->size() != 4u) {
        return false;
    }
    std::array<float, 4> edges{};
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (!(*array)[i].is_number()) {
            return false;
        }
        const double number = (*array)[i].as_number(0.0);
        if (!std::isfinite(number) || std::abs(number) > kMaxLayoutCoordinate) {
            return false;
        }
        edges[i] = static_cast<float>(number);
    }
    if (edges[2] <= edges[0] || edges[3] <= edges[1]) {
        return false;
    }
    out = SkinLayoutRect{edges[0], edges[1], edges[2], edges[3]};
    return true;
}

void parse_layout_section(const config::JsonObject& layout, TenRiffSkinDefinition& definition) {
    for (const auto& [screen, screen_value] : layout) {
        const auto* slots = screen_value.as_object();
        if (!slots) {
            definition.warnings.push_back("layout." + screen +
                                          " must be an object of rect entries.");
            continue;
        }
        for (const auto& [slot, rect_value] : *slots) {
            const std::string key = screen + "." + slot;
            if (!is_tenriff_skin_layout_slot(key)) {
                definition.warnings.push_back("layout." + key + " is not a layout slot.");
                continue;
            }
            SkinLayoutRect rect;
            if (!parse_layout_rect(rect_value, rect)) {
                definition.warnings.push_back(
                    "layout." + key +
                    " must be [left, top, right, bottom] with right > left and bottom > top.");
                continue;
            }
            definition.layout_rects[key] = rect;
        }
    }
}

config::JsonObject effective_gameplay_object(const config::JsonObject& gameplay,
                                             int keys,
                                             TenRiffSkinDefinition& definition) {
    config::JsonObject effective = gameplay;
    effective.erase("modes");
    const auto* modes_value = object_value(gameplay, "modes");
    if (!modes_value) {
        return effective;
    }
    const auto* modes = modes_value->as_object();
    if (!modes) {
        definition.warnings.push_back("gameplay.modes must be an object keyed by 1k through 16k.");
        return effective;
    }
    for (const auto& [mode, value] : *modes) {
        bool valid_mode = false;
        if (mode.size() >= 2u && mode.back() == 'k') {
            const std::string digits = mode.substr(0, mode.size() - 1u);
            try {
                const bool all_digits = !digits.empty() &&
                    std::all_of(digits.begin(), digits.end(), [](unsigned char ch) {
                        return std::isdigit(ch) != 0;
                    });
                const int count = all_digits ? std::stoi(digits) : 0;
                valid_mode = all_digits && count >= 1 && count <= 16;
            } catch (...) {
                valid_mode = false;
            }
        }
        if (!valid_mode || !value.is_object()) {
            definition.warnings.push_back("gameplay.modes." + mode +
                                          " must be an object using a 1k through 16k key.");
        }
    }
    const std::string active_mode = std::to_string(std::clamp(keys, 1, 16)) + "k";
    const auto it = modes->find(active_mode);
    if (it == modes->end()) {
        return effective;
    }
    const auto* override_object = it->second.as_object();
    if (!override_object) {
        return effective;
    }
    for (const auto& [key, value] : *override_object) {
        if (key == "modes") {
            definition.warnings.push_back("gameplay.modes." + active_mode + ".modes cannot be nested.");
            continue;
        }
        effective.insert_or_assign(key, value);
    }
    return effective;
}

void parse_theme_section(const config::JsonObject& theme, TenRiffSkinDefinition& definition) {
    static constexpr std::array<std::string_view, 14> kThemeKeys = {
        "accent", "text", "muted", "card", "panel", "footer", "button",
        "button_selected", "border", "judgement_line", "lane_divider",
        "scene_primary", "scene_secondary", "scene_background"
    };
    warn_unknown_keys(theme, kThemeKeys, "theme.", definition);
    for (const auto key : kThemeKeys) {
        const auto* value = object_value(theme, key);
        if (!value) continue;
        if (!value->is_string()) {
            definition.warnings.push_back("theme." + std::string(key) +
                                          " must be #RRGGBB or #RRGGBBAA.");
            continue;
        }
        const auto color = parse_hex_color(value->as_string());
        if (!color.has_value()) {
            definition.warnings.push_back("theme." + std::string(key) +
                                          " must be #RRGGBB or #RRGGBBAA.");
            continue;
        }
        definition.theme_colors[std::string(key)] = *color;
    }
}

void parse_screen_backgrounds(const config::JsonObject& lobby,
                              const fs::path& root,
                              TenRiffSkinDefinition& definition) {
    const auto* backgrounds_value = object_value(lobby, "screen_backgrounds");
    if (backgrounds_value) {
        const auto* backgrounds = backgrounds_value->as_object();
        if (!backgrounds) {
            definition.warnings.push_back("lobby.screen_backgrounds must be an object.");
        } else {
            warn_unknown_keys(*backgrounds, kTenRiffSkinScreenIds, "lobby.screen_backgrounds.", definition);
            for (const auto key : kTenRiffSkinScreenIds) {
                const auto* value = object_value(*backgrounds, key);
                if (!value) continue;
                if (!value->is_string()) {
                    definition.warnings.push_back("lobby.screen_backgrounds." + std::string(key) +
                                                  " must be an image path.");
                    continue;
                }
                if (const auto path = resolve_asset_path(
                        root, value->as_string(), definition.warnings,
                        "lobby.screen_backgrounds." + std::string(key))) {
                    definition.screen_background_paths[std::string(key)] = *path;
                    add_reference(definition, *path);
                }
            }
        }
    }
    for (const auto key : kTenRiffSkinScreenIds) {
        if (definition.screen_background_paths.find(std::string(key)) !=
            definition.screen_background_paths.end()) {
            continue;
        }
        auto detected = conventional_asset_path(root, "lobby/screens/" + std::string(key));
        if (!detected.has_value()) {
            detected = conventional_asset_path(root, "lobby/" + std::string(key));
        }
        if (detected.has_value()) {
            if (const auto path = resolve_asset_path(
                    root, *detected, definition.warnings,
                    "lobby.screen_backgrounds." + std::string(key))) {
                definition.screen_background_paths[std::string(key)] = *path;
                add_reference(definition, *path);
            }
        }
    }
    const auto* opacities_value = object_value(lobby, "screen_opacities");
    if (!opacities_value) return;
    const auto* opacities = opacities_value->as_object();
    if (!opacities) {
        definition.warnings.push_back("lobby.screen_opacities must be an object.");
        return;
    }
    warn_unknown_keys(*opacities, kTenRiffSkinScreenIds, "lobby.screen_opacities.", definition);
    for (const auto key : kTenRiffSkinScreenIds) {
        if (const auto opacity = optional_number(*opacities, key, 0.0f, 1.0f,
                                                 definition, "lobby.screen_opacities.")) {
            definition.screen_background_opacities[std::string(key)] = *opacity;
        }
    }
}

std::optional<std::uint32_t> parse_rgb_color(std::string_view raw) {
    const auto color = parse_hex_color(raw);
    if (!color.has_value()) return std::nullopt;
    const auto byte = [](float value) -> std::uint32_t {
        return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return (byte((*color)[0]) << 16u) | (byte((*color)[1]) << 8u) | byte((*color)[2]);
}

void parse_gameplay_style(const config::JsonObject& gameplay,
                          TenRiffSkinDefinition& definition) {
    auto& style = definition.gameplay_style;
    style.show_lane_dividers = optional_bool(gameplay, "show_lane_dividers", definition, "gameplay.");
    style.show_judgement_line = optional_bool(gameplay, "show_judgement_line", definition, "gameplay.");
    style.show_timing_feedback = optional_bool(gameplay, "show_timing_feedback", definition, "gameplay.");
    style.show_gear_boundary_line = optional_bool(gameplay, "show_gear_boundary_line", definition, "gameplay.");
    style.show_hold_tail = optional_bool(gameplay, "show_hold_tail", definition, "gameplay.");
    style.hold_tail_taper_enabled = optional_bool(gameplay, "hold_tail_taper", definition, "gameplay.");
    style.judgement_line_glow_enabled = optional_bool(gameplay, "judgement_line_glow", definition, "gameplay.");
    style.key_pulse_enabled = optional_bool(gameplay, "key_pulse", definition, "gameplay.");
    style.note_border_enabled = optional_bool(gameplay, "note_border", definition, "gameplay.");
    style.black_playfield_enabled = optional_bool(gameplay, "black_playfield", definition, "gameplay.");
    style.key_pulse_brightness = optional_number(gameplay, "key_pulse_brightness", 0.0f, 1.0f,
                                                 definition, "gameplay.");
    style.lane_background_opacity = optional_number(gameplay, "lane_background_opacity", 0.0f, 0.45f,
                                                     definition, "gameplay.");
    style.visual_opacity = optional_number(gameplay, "visual_opacity", 0.20f, 1.0f,
                                           definition, "gameplay.");
    style.note_outline_opacity = optional_number(gameplay, "note_outline_opacity", 0.0f, 1.0f,
                                                 definition, "gameplay.");
    style.hold_body_opacity = optional_number(gameplay, "hold_body_opacity", 0.05f, 1.0f,
                                              definition, "gameplay.");
    style.hit_burst_style = optional_enum(gameplay, "hit_burst_style", {"prism", "ring", "spark"},
                                          definition, "gameplay.");
    style.key_label_position = optional_enum(gameplay, "key_label_position", {"off", "top", "bottom"},
                                             definition, "gameplay.");
    style.note_shape = optional_enum(gameplay, "note_shape", {"rect", "circle", "diamond", "hex"},
                                     definition, "gameplay.");

    const auto* lane_colors = object_value(gameplay, "lane_colors");
    if (!lane_colors) return;
    const auto* array = lane_colors->as_array();
    if (!array || array->empty() || array->size() > 16u) {
        definition.warnings.push_back("gameplay.lane_colors must contain 1 to 16 hex colors.");
        return;
    }
    style.lane_colors.reserve(array->size());
    for (const auto& value : *array) {
        if (!value.is_string()) {
            definition.warnings.push_back("gameplay.lane_colors entries must be hex colors.");
            style.lane_colors.push_back(0u);
            continue;
        }
        const auto color = parse_rgb_color(value.as_string());
        if (!color.has_value()) {
            definition.warnings.push_back("gameplay.lane_colors entries must be #RRGGBB colors.");
            style.lane_colors.push_back(0u);
        } else {
            style.lane_colors.push_back(*color);
        }
    }
}

std::string safe_folder_name(std::string value) {
    constexpr std::size_t kMaxFolderNameBytes = 96u;
    value = util::sanitize_ui_text(value);
    std::string out;
    out.reserve(std::min(value.size(), kMaxFolderNameBytes));
    for (unsigned char ch : value) {
        if (ch >= 0x80u || std::isalnum(ch) != 0 || ch == '-' || ch == '_') {
            out.push_back(static_cast<char>(ch));
        } else if ((ch == ' ' || ch == '.') && !out.empty() && out.back() != '-') {
            out.push_back('-');
        } else if (ch != '<' && ch != '>' && ch != ':' && ch != '"' && ch != '/' &&
                   ch != '\\' && ch != '|' && ch != '?' && ch != '*') {
            out.push_back('-');
        }
    }
    if (out.size() > kMaxFolderNameBytes) {
        std::size_t cut = kMaxFolderNameBytes;
        while (cut > 0u &&
               (static_cast<unsigned char>(out[cut]) & 0xC0u) == 0x80u) {
            --cut;
        }
        out.resize(cut);
    }
    while (!out.empty() && (out.back() == '-' || out.back() == '.' || out.back() == ' ')) {
        out.pop_back();
    }
    return out.empty() ? "custom-skin" : out;
}
}  // namespace

bool is_tenriff_skin_layout_slot(std::string_view key) {
    if (std::find(kTenRiffSkinLayoutSlots.begin(), kTenRiffSkinLayoutSlots.end(), key) !=
        kTenRiffSkinLayoutSlots.end()) {
        return true;
    }
    const auto dot = key.find('.');
    if (dot == std::string_view::npos) return false;
    const std::string_view screen = key.substr(0, dot);
    const std::string_view slot = key.substr(dot + 1u);
    return screen != "title" && screen != "song_select" && screen != "result" &&
           (slot == "content" || slot == "preview") &&
           std::find(kTenRiffSkinScreenIds.begin(), kTenRiffSkinScreenIds.end(), screen) !=
               kTenRiffSkinScreenIds.end();
}

TenRiffSkinDefinition load_tenriff_skin_folder(std::string_view folder_utf8, int keys) {
    TenRiffSkinDefinition definition;
    const fs::path root = path_from_utf8(folder_utf8);
    if (root.empty()) {
        definition.warnings.push_back("TenRiff skin folder path is empty.");
        return definition;
    }
    const fs::path manifest_path = root / "skin.json";
    std::error_code ec;
    const std::uintmax_t manifest_size = fs::file_size(manifest_path, ec);
    if (ec || manifest_size > kMaxManifestBytes) {
        definition.warnings.push_back("skin.json is missing or larger than 1 MiB.");
        return definition;
    }
    std::ifstream file(manifest_path, std::ios::binary);
    if (!file) {
        definition.warnings.push_back("Could not open skin.json.");
        return definition;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto parsed = config::parse_json(buffer.str());
    const auto* manifest = parsed.success() && parsed.root.has_value()
                               ? parsed.root->as_object()
                               : nullptr;
    if (!manifest) {
        definition.warnings.push_back(parsed.error.empty() ? "skin.json root must be an object."
                                                           : parsed.error);
        return definition;
    }
    if (lower_ascii(string_value(*manifest, "format")) != "tenriff-skin") {
        definition.warnings.push_back("skin.json format must be tenriff-skin.");
        return definition;
    }
    const int version = static_cast<int>(std::lround(number_value(*manifest, "version", 0.0)));
    if (version != kTenRiffSkinFormatVersion) {
        definition.warnings.push_back("Unsupported TenRiff skin format version: " +
                                      std::to_string(version));
        return definition;
    }

    static constexpr std::array<std::string_view, 9> kManifestKeys = {
        "$schema", "format", "version", "name", "author", "lobby", "layout", "theme", "gameplay"
    };
    warn_unknown_keys(*manifest, kManifestKeys, "", definition);

    definition.found = true;
    definition.format_version = version;
    definition.root_path = root.u8string();
    definition.folder_name = root.filename().u8string();
    const auto* name_value = object_value(*manifest, "name");
    definition.name = name_value && name_value->is_string()
                          ? name_value->as_string()
                          : definition.folder_name;
    if (!name_value || !name_value->is_string() || definition.name.empty()) {
        definition.name = definition.folder_name;
        definition.warnings.push_back("name must be a non-empty string; the folder name is being used.");
    }
    const auto* author_value = object_value(*manifest, "author");
    if (author_value && !author_value->is_string()) {
        definition.warnings.push_back("author must be a string.");
    } else if (author_value) {
        definition.author = author_value->as_string();
    }
    definition.gameplay.found = true;
    definition.gameplay.keys = std::clamp(keys, 1, 16);

    if (const auto* lobby = child_object(*manifest, "lobby")) {
        static constexpr std::array<std::string_view, 5> kLobbyKeys = {
            "background", "logo", "background_opacity", "screen_backgrounds", "screen_opacities"
        };
        warn_unknown_keys(*lobby, kLobbyKeys, "lobby.", definition);
        std::string lobby_background = string_value(*lobby, "background");
        if (const auto* value = object_value(*lobby, "background"); value && !value->is_string()) {
            definition.warnings.push_back("lobby.background must be an image path.");
        }
        if (!object_value(*lobby, "background")) {
            lobby_background = conventional_asset_path(root, "lobby/background").value_or(std::string{});
        }
        if (const auto path = resolve_asset_path(root, lobby_background,
                                                 definition.warnings, "lobby.background")) {
            definition.lobby_background_path = *path;
            add_reference(definition, *path);
        }
        std::string lobby_logo = string_value(*lobby, "logo");
        if (const auto* value = object_value(*lobby, "logo"); value && !value->is_string()) {
            definition.warnings.push_back("lobby.logo must be an image path.");
        }
        if (!object_value(*lobby, "logo")) {
            lobby_logo = conventional_asset_path(root, "lobby/logo").value_or(std::string{});
        }
        if (const auto path = resolve_asset_path(root, lobby_logo,
                                                 definition.warnings, "lobby.logo")) {
            definition.lobby_logo_path = *path;
            add_reference(definition, *path);
        }
        if (const auto opacity = optional_number(*lobby, "background_opacity", 0.0f, 1.0f,
                                                 definition, "lobby.")) {
            definition.lobby_background_opacity = *opacity;
        }
        parse_screen_backgrounds(*lobby, root, definition);
    } else if (object_value(*manifest, "lobby")) {
        definition.warnings.push_back("lobby must be an object.");
    } else {
        if (const auto detected = conventional_asset_path(root, "lobby/background")) {
            if (const auto path = resolve_asset_path(root, *detected, definition.warnings, "lobby.background")) {
                definition.lobby_background_path = *path;
                add_reference(definition, *path);
            }
        }
        if (const auto detected = conventional_asset_path(root, "lobby/logo")) {
            if (const auto path = resolve_asset_path(root, *detected, definition.warnings, "lobby.logo")) {
                definition.lobby_logo_path = *path;
                add_reference(definition, *path);
            }
        }
        config::JsonObject empty_lobby;
        parse_screen_backgrounds(empty_lobby, root, definition);
    }

    if (const auto* layout = child_object(*manifest, "layout")) {
        parse_layout_section(*layout, definition);
    } else if (object_value(*manifest, "layout")) {
        definition.warnings.push_back("layout must be an object.");
    }

    if (const auto* theme = child_object(*manifest, "theme")) {
        parse_theme_section(*theme, definition);
    } else if (object_value(*manifest, "theme")) {
        definition.warnings.push_back("theme must be an object of hex colors.");
    }

    if (const auto* gameplay_manifest = child_object(*manifest, "gameplay")) {
        config::JsonObject gameplay = effective_gameplay_object(*gameplay_manifest, definition.gameplay.keys,
                                                                 definition);
        static constexpr std::array<std::string_view, 40> kGameplayKeys = {
            "background", "background_opacity", "gear", "note", "hold_head", "hold_body",
            "hold_tail", "key_idle", "key_pressed", "note_width_ratio", "note_height_ratio",
            "note_aspect", "note_rotations", "key_rotations", "judgement_line_position",
            "full_lane_receptors", "column_widths", "column_spacings", "lane_map",
            "show_lane_dividers", "show_judgement_line", "show_timing_feedback",
            "show_gear_boundary_line", "show_hold_tail", "hold_tail_taper", "judgement_line_glow",
            "key_pulse", "key_pulse_brightness", "hit_burst_style", "key_label_position",
            "note_border", "note_shape", "lane_colors", "lane_background_opacity",
            "black_playfield", "visual_opacity", "note_outline_opacity", "hold_body_opacity",
            "modes", "description"
        };
        warn_unknown_keys(gameplay, kGameplayKeys, "gameplay.", definition);
        const std::vector<std::string> lane_map = parse_lane_map(gameplay, definition.gameplay.keys,
                                                                 definition);
        definition.gameplay.note_images = parse_asset_list(
            gameplay, "note", root, definition, lane_map, definition.gameplay.keys, "gameplay/note");
        definition.gameplay.hold_head_images = parse_asset_list(
            gameplay, "hold_head", root, definition, lane_map, definition.gameplay.keys, "gameplay/hold-head");
        definition.gameplay.hold_body_images = parse_asset_list(
            gameplay, "hold_body", root, definition, lane_map, definition.gameplay.keys, "gameplay/hold-body");
        definition.gameplay.hold_tail_images = parse_asset_list(
            gameplay, "hold_tail", root, definition, lane_map, definition.gameplay.keys, "gameplay/hold-tail");
        definition.gameplay.key_images = parse_asset_list(
            gameplay, "key_idle", root, definition, lane_map, definition.gameplay.keys, "gameplay/key-idle");
        definition.gameplay.key_pressed_images = parse_asset_list(
            gameplay, "key_pressed", root, definition, lane_map, definition.gameplay.keys, "gameplay/key-pressed");
        std::string gear_path = string_value(gameplay, "gear");
        if (const auto* value = object_value(gameplay, "gear"); value && !value->is_string()) {
            definition.warnings.push_back("gameplay.gear must be an image path.");
        }
        if (!object_value(gameplay, "gear")) {
            gear_path = conventional_asset_path(root, "gameplay/gear").value_or(std::string{});
        }
        if (const auto path = resolve_asset_path(root, gear_path,
                                                 definition.warnings, "gameplay.gear")) {
            definition.gameplay.gear_overlay_image.path = *path;
            add_reference(definition, *path);
        }
        std::string gameplay_background = string_value(gameplay, "background");
        if (const auto* value = object_value(gameplay, "background"); value && !value->is_string()) {
            definition.warnings.push_back("gameplay.background must be an image path.");
        }
        if (!object_value(gameplay, "background")) {
            gameplay_background = conventional_asset_path(root, "gameplay/background").value_or(std::string{});
        }
        if (const auto path = resolve_asset_path(root, gameplay_background,
                                                 definition.warnings, "gameplay.background")) {
            definition.gameplay_background_path = *path;
            add_reference(definition, *path);
        }
        if (const auto opacity = optional_number(gameplay, "background_opacity", 0.0f, 1.0f,
                                                 definition, "gameplay.")) {
            definition.gameplay_background_opacity = *opacity;
        }
        if (const auto ratio = optional_number(gameplay, "note_width_ratio", 0.1f, 4.0f,
                                               definition, "gameplay.")) {
            definition.gameplay.imported_note_width_ratio = *ratio;
        }
        if (const auto ratio = optional_number(gameplay, "note_height_ratio", 0.1f, 4.0f,
                                               definition, "gameplay.")) {
            definition.gameplay.imported_note_height_ratio = *ratio;
        }
        definition.gameplay.column_widths =
            parse_positive_number_array(gameplay, "column_widths", 16u);
        definition.gameplay.column_spacings =
            parse_positive_number_array(gameplay, "column_spacings", 15u);
        definition.gameplay.note_rotations =
            parse_rotation_array(gameplay, "note_rotations", 16u);
        definition.gameplay.key_rotations =
            parse_rotation_array(gameplay, "key_rotations", 16u);
        const std::string note_aspect = lower_ascii(string_value(gameplay, "note_aspect"));
        if (note_aspect == "stretch" || note_aspect == "contain" || note_aspect == "width") {
            definition.gameplay.note_aspect = note_aspect;
        } else if (!note_aspect.empty()) {
            definition.warnings.push_back(
                "gameplay.note_aspect must be stretch, contain, or width.");
        }
        if (const auto judgement_line = optional_number(gameplay, "judgement_line_position", 0.05f,
                                                        0.98f, definition, "gameplay.")) {
            definition.gameplay.has_hit_position = true;
            definition.gameplay.hit_position = *judgement_line * 480.0f;
        }
        if (const auto full_lane = optional_bool(gameplay, "full_lane_receptors", definition,
                                                 "gameplay.")) {
            definition.gameplay.use_full_lane_receptor_layout = *full_lane;
        }
        parse_gameplay_style(gameplay, definition);
    } else if (object_value(*manifest, "gameplay")) {
        definition.warnings.push_back("gameplay must be an object.");
    } else {
        config::JsonObject empty_gameplay;
        const std::vector<std::string> empty_lane_map;
        definition.gameplay.note_images = parse_asset_list(
            empty_gameplay, "note", root, definition, empty_lane_map, definition.gameplay.keys, "gameplay/note");
        definition.gameplay.hold_head_images = parse_asset_list(
            empty_gameplay, "hold_head", root, definition, empty_lane_map, definition.gameplay.keys, "gameplay/hold-head");
        definition.gameplay.hold_body_images = parse_asset_list(
            empty_gameplay, "hold_body", root, definition, empty_lane_map, definition.gameplay.keys, "gameplay/hold-body");
        definition.gameplay.hold_tail_images = parse_asset_list(
            empty_gameplay, "hold_tail", root, definition, empty_lane_map, definition.gameplay.keys, "gameplay/hold-tail");
        definition.gameplay.key_images = parse_asset_list(
            empty_gameplay, "key_idle", root, definition, empty_lane_map, definition.gameplay.keys, "gameplay/key-idle");
        definition.gameplay.key_pressed_images = parse_asset_list(
            empty_gameplay, "key_pressed", root, definition, empty_lane_map, definition.gameplay.keys, "gameplay/key-pressed");
        if (const auto detected = conventional_asset_path(root, "gameplay/gear")) {
            if (const auto path = resolve_asset_path(root, *detected, definition.warnings, "gameplay.gear")) {
                definition.gameplay.gear_overlay_image.path = *path;
                add_reference(definition, *path);
            }
        }
        if (const auto detected = conventional_asset_path(root, "gameplay/background")) {
            if (const auto path = resolve_asset_path(root, *detected, definition.warnings, "gameplay.background")) {
                definition.gameplay_background_path = *path;
                add_reference(definition, *path);
            }
        }
    }

    return definition;
}

TenRiffSkinDefinition resolve_tenriff_skin(std::string_view root_utf8,
                                           std::string_view skin_name,
                                           int keys) {
    if (root_utf8.empty() || skin_name.empty()) {
        return {};
    }
    const fs::path root = path_from_utf8(root_utf8);
    const fs::path name = path_from_utf8(skin_name);
    if (root.empty() || name.empty() || name.is_absolute() || name.has_root_name() ||
        name.has_root_directory()) {
        return {};
    }
    for (const auto& component : name) {
        if (component == "..") {
            return {};
        }
    }
    return load_tenriff_skin_folder((root / name).u8string(), keys);
}

std::vector<std::string> list_tenriff_skin_names(std::string_view root_utf8) {
    std::vector<std::string> names;
    const fs::path root = path_from_utf8(root_utf8);
    std::error_code ec;
    if (root.empty() || !fs::is_directory(root, ec) || ec) {
        return names;
    }
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec) || ec) {
            ec.clear();
            continue;
        }
        const std::string folder = it->path().filename().u8string();
        if (load_tenriff_skin_folder(it->path().u8string(), 10).found) {
            names.push_back(folder);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

TenRiffSkinImportResult import_tenriff_skin(std::string_view source_utf8,
                                            std::string_view import_root_utf8) {
    TenRiffSkinImportResult result;
    fs::path source = path_from_utf8(source_utf8);
    if (lower_ascii(source.filename().u8string()) == "skin.json") {
        source = source.parent_path();
    }
    TenRiffSkinDefinition definition = load_tenriff_skin_folder(source.u8string(), 10);
    if (!definition.found) {
        result.warnings = definition.warnings;
        return result;
    }
    result.warnings = definition.warnings;
    // A package can override assets per key mode. Import every referenced file,
    // not only the 10K view used for the initial manifest validity check.
    for (int keys = 1; keys <= 16; ++keys) {
        const auto mode_definition = load_tenriff_skin_folder(source.u8string(), keys);
        for (const auto& path : mode_definition.referenced_asset_paths) {
            add_reference(definition, path);
        }
    }

    const fs::path import_root = path_from_utf8(import_root_utf8);
    std::error_code ec;
    fs::create_directories(import_root, ec);
    if (ec) {
        result.warnings.push_back("Could not create the TenRiff skin import folder.");
        return result;
    }
    const std::string base_name = safe_folder_name(
        definition.name.empty() ? source.filename().u8string() : definition.name);
    std::string install_name = base_name;
    fs::path destination = import_root / path_from_utf8(install_name);
    for (int suffix = 2; fs::exists(destination, ec) && !ec; ++suffix) {
        install_name = base_name + "-" + std::to_string(suffix);
        destination = import_root / path_from_utf8(install_name);
    }
    if (ec) {
        result.warnings.push_back("Could not choose a destination for the imported skin.");
        return result;
    }
    fs::create_directories(destination, ec);
    if (ec) {
        result.warnings.push_back("Could not create the imported skin directory.");
        return result;
    }

    auto copy_one = [&](const fs::path& from, const fs::path& relative) -> bool {
        const fs::path to = destination / relative;
        fs::create_directories(to.parent_path(), ec);
        if (ec) {
            return false;
        }
        fs::copy_file(from, to, fs::copy_options::none, ec);
        if (ec) {
            return false;
        }
        ++result.copied_files;
        result.copied_bytes += fs::file_size(to, ec);
        if (ec) {
            ec.clear();
        }
        return true;
    };

    if (!copy_one(source / "skin.json", fs::path("skin.json"))) {
        result.warnings.push_back("Could not copy skin.json.");
        return result;
    }
    static constexpr std::array<std::string_view, 4> kCompanionFiles = {
        "tenriff-skin.schema.json", "skin-format.md", "README.md", "LICENSE.txt"
    };
    for (const auto companion_name : kCompanionFiles) {
        const fs::path companion = source / path_from_utf8(companion_name);
        std::error_code companion_ec;
        if (fs::is_regular_file(companion, companion_ec) && !companion_ec &&
            !copy_one(companion, path_from_utf8(companion_name))) {
            result.warnings.push_back("Could not copy skin companion file: " +
                                      std::string(companion_name));
            return result;
        }
    }
    for (const auto& path_utf8 : definition.referenced_asset_paths) {
        const fs::path asset = path_from_utf8(path_utf8);
        const fs::path relative = fs::relative(asset, source, ec);
        if (ec || relative.empty() || relative.is_absolute()) {
            result.warnings.push_back("Skipped an asset outside the selected skin folder.");
            ec.clear();
            continue;
        }
        if (!copy_one(asset, relative)) {
            result.warnings.push_back("Could not copy skin asset: " + relative.u8string());
            return result;
        }
    }

    result.skin_name = install_name;
    result.install_root = import_root.u8string();
    return result;
}

TenRiffSkinCreateResult create_tenriff_skin_template(std::string_view import_root_utf8) {
    TenRiffSkinCreateResult result;
    const fs::path import_root = path_from_utf8(import_root_utf8);
    if (import_root.empty()) {
        result.warnings.push_back("The TenRiff skin folder path is empty.");
        return result;
    }
    std::error_code ec;
    fs::create_directories(import_root, ec);
    if (ec) {
        result.warnings.push_back("Could not create the TenRiff skin folder.");
        return result;
    }

    std::string folder_name = "My-TenRiff-Skin";
    fs::path destination = import_root / folder_name;
    for (int suffix = 2; fs::exists(destination, ec) && !ec; ++suffix) {
        folder_name = "My-TenRiff-Skin-" + std::to_string(suffix);
        destination = import_root / folder_name;
    }
    if (ec) {
        result.warnings.push_back("Could not choose a folder for the new skin.");
        return result;
    }
    fs::create_directories(destination / "lobby" / "screens", ec);
    if (!ec) fs::create_directories(destination / "gameplay", ec);
    if (ec) {
        result.warnings.push_back("Could not create the new skin directories.");
        return result;
    }

    const std::string display_name = folder_name == "My-TenRiff-Skin"
                                         ? "My TenRiff Skin"
                                         : folder_name;
    const std::string manifest =
        "{\n"
        "  \"$schema\": \"https://raw.githubusercontent.com/10kseason/TenRiff/main/docs/tenriff-skin.schema.json\",\n"
        "  \"format\": \"tenriff-skin\",\n"
        "  \"version\": 1,\n"
        "  \"name\": \"" + display_name + "\",\n"
        "  \"author\": \"Your Name\",\n"
        "  \"lobby\": {},\n"
        "  \"theme\": {\n"
        "    \"accent\": \"#6EE7F2\"\n"
        "  },\n"
        "  \"gameplay\": {}\n"
        "}\n";
    std::ofstream manifest_file(destination / "skin.json", std::ios::binary);
    if (!manifest_file) {
        result.warnings.push_back("Could not write skin.json.");
        return result;
    }
    manifest_file << manifest;
    manifest_file.close();
    if (!manifest_file) {
        result.warnings.push_back("Could not finish writing skin.json.");
        return result;
    }

    std::ofstream readme_file(destination / "README.md", std::ios::binary);
    if (readme_file) {
        readme_file <<
            "# My TenRiff Skin\n\n"
            "Drop images into the folders below and press F5 on TenRiff's Skin Settings screen.\n\n"
            "- lobby/background.png or .jpg\n"
            "- lobby/logo.png\n"
            "- lobby/screens/<screen-id>.png for per-screen backgrounds\n"
            "- gameplay/background.png or .jpg\n"
            "- gameplay/gear.png\n"
            "- gameplay/note.png\n"
            "- gameplay/hold-head.png, hold-body.png, hold-tail.png\n"
            "- gameplay/key-idle.png, key-pressed.png\n\n"
            "Files using these standard names are detected automatically; skin.json only needs entries for overrides.\n";
    }

    result.skin_name = folder_name;
    result.folder_path = destination.u8string();
    return result;
}

}  // namespace tenriff::app
