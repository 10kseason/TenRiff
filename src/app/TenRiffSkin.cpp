#include "app/TenRiffSkin.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
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

std::vector<ImportedSkinImageAsset> parse_asset_list(const config::JsonObject& object,
                                                     std::string_view key,
                                                     const fs::path& root,
                                                     TenRiffSkinDefinition& definition) {
    std::vector<ImportedSkinImageAsset> assets;
    const config::JsonValue* value = object_value(object, key);
    if (!value) {
        return assets;
    }
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
        append(value->as_string(), 0u);
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

    definition.found = true;
    definition.format_version = version;
    definition.root_path = root.u8string();
    definition.folder_name = root.filename().u8string();
    definition.name = string_value(*manifest, "name", definition.folder_name);
    definition.author = string_value(*manifest, "author");
    definition.gameplay.found = true;
    definition.gameplay.keys = std::clamp(keys, 1, 16);

    if (const auto* lobby = child_object(*manifest, "lobby")) {
        if (const auto path = resolve_asset_path(root, string_value(*lobby, "background"),
                                                 definition.warnings, "lobby.background")) {
            definition.lobby_background_path = *path;
            add_reference(definition, *path);
        }
        if (const auto path = resolve_asset_path(root, string_value(*lobby, "logo"),
                                                 definition.warnings, "lobby.logo")) {
            definition.lobby_logo_path = *path;
            add_reference(definition, *path);
        }
        definition.lobby_background_opacity = static_cast<float>(std::clamp(
            number_value(*lobby, "background_opacity", definition.lobby_background_opacity),
            0.0, 1.0));
    }

    if (const auto* gameplay = child_object(*manifest, "gameplay")) {
        definition.gameplay.note_images = parse_asset_list(*gameplay, "note", root, definition);
        definition.gameplay.hold_head_images = parse_asset_list(*gameplay, "hold_head", root, definition);
        definition.gameplay.hold_body_images = parse_asset_list(*gameplay, "hold_body", root, definition);
        definition.gameplay.hold_tail_images = parse_asset_list(*gameplay, "hold_tail", root, definition);
        definition.gameplay.key_images = parse_asset_list(*gameplay, "key_idle", root, definition);
        definition.gameplay.key_pressed_images = parse_asset_list(*gameplay, "key_pressed", root, definition);
        if (const auto path = resolve_asset_path(root, string_value(*gameplay, "gear"),
                                                 definition.warnings, "gameplay.gear")) {
            definition.gameplay.gear_overlay_image.path = *path;
            add_reference(definition, *path);
        }
        if (const auto path = resolve_asset_path(root, string_value(*gameplay, "background"),
                                                 definition.warnings, "gameplay.background")) {
            definition.gameplay_background_path = *path;
            add_reference(definition, *path);
        }
        definition.gameplay_background_opacity = static_cast<float>(std::clamp(
            number_value(*gameplay, "background_opacity", definition.gameplay_background_opacity),
            0.0, 1.0));
        definition.gameplay.imported_note_width_ratio = static_cast<float>(std::clamp(
            number_value(*gameplay, "note_width_ratio", 1.0), 0.1, 4.0));
        definition.gameplay.imported_note_height_ratio = static_cast<float>(std::clamp(
            number_value(*gameplay, "note_height_ratio", 1.0), 0.1, 4.0));
        definition.gameplay.column_widths =
            parse_positive_number_array(*gameplay, "column_widths", 16u);
        definition.gameplay.column_spacings =
            parse_positive_number_array(*gameplay, "column_spacings", 15u);
        const double judgement_line = number_value(*gameplay, "judgement_line_position", -1.0);
        if (std::isfinite(judgement_line) && judgement_line >= 0.05 && judgement_line <= 0.98) {
            definition.gameplay.has_hit_position = true;
            definition.gameplay.hit_position = static_cast<float>(judgement_line * 480.0);
        }
        definition.gameplay.use_full_lane_receptor_layout =
            bool_value(*gameplay, "full_lane_receptors", false);
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
    result.warnings = definition.warnings;
    if (!definition.found) {
        return result;
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

}  // namespace tenriff::app
