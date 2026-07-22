#include "app/OsuSkin.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

namespace fs = std::filesystem;

struct ManiaSection {
    int keys = 0;
    std::unordered_map<std::string, std::string> values;
};

constexpr float kMaxManiaMetric = 4096.0f;

std::string to_lower_ascii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string normalize_asset_token(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = trim_copy(std::string_view(value).substr(1, value.size() - 2));
        }
    }
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

std::string normalize_path_utf8(const fs::path& path) {
    std::error_code ec;
    fs::path normalized = path;
    if (!normalized.is_absolute()) {
        const fs::path absolute = fs::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        } else {
            ec.clear();
        }
    }
    const fs::path canonical = fs::weakly_canonical(normalized, ec);
    if (!ec && !canonical.empty()) {
        normalized = canonical;
    } else {
        normalized = normalized.lexically_normal();
    }
    return normalized.u8string();
}

bool path_stays_within(const fs::path& root, const fs::path& candidate) {
    std::error_code ec;
    const fs::path canonical_root = fs::weakly_canonical(root, ec);
    if (ec || canonical_root.empty()) {
        return false;
    }
    ec.clear();
    const fs::path canonical_candidate = fs::weakly_canonical(candidate, ec);
    if (ec || canonical_candidate.empty()) {
        return false;
    }

    const fs::path relative = canonical_candidate.lexically_relative(canonical_root);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.has_root_directory()) {
        return false;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool is_safe_relative_path(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return false;
    }
    for (const auto& component : path) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool has_osu_skin_assets(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return false;
    }
    if (fs::exists(dir / "skin.ini", ec)) {
        return true;
    }
    ec.clear();
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        const std::string filename = to_lower_ascii(it->path().filename().u8string());
        if (filename.rfind("mania-note", 0) == 0 || filename.rfind("mania-key", 0) == 0) {
            return true;
        }
    }
    return false;
}

std::optional<std::pair<std::string, std::string>> parse_key_value(std::string_view line) {
    const std::size_t colon = line.find(':');
    const std::size_t equals = line.find('=');
    std::size_t split = std::string_view::npos;
    if (colon == std::string_view::npos) {
        split = equals;
    } else if (equals == std::string_view::npos) {
        split = colon;
    } else {
        split = std::min(colon, equals);
    }
    if (split == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string key = trim_copy(line.substr(0, split));
    const std::string value = trim_copy(line.substr(split + 1));
    if (key.empty()) {
        return std::nullopt;
    }
    return std::make_pair(key, value);
}

std::unordered_map<int, ManiaSection> parse_skin_ini(const fs::path& skin_ini_path) {
    std::unordered_map<int, ManiaSection> sections;
    std::ifstream file(skin_ini_path, std::ios::binary);
    if (!file) {
        return sections;
    }

    bool in_mania = false;
    ManiaSection current;
    auto commit = [&]() {
        if (in_mania && current.keys > 0) {
            sections[current.keys] = current;
        }
        current = {};
    };

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }

        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.rfind("//", 0) == 0 || trimmed.rfind(";", 0) == 0) {
            continue;
        }
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            commit();
            const std::string section_name = to_lower_ascii(trim_copy(
                std::string_view(trimmed).substr(1, trimmed.size() - 2)));
            in_mania = (section_name == "mania");
            continue;
        }
        if (!in_mania) {
            continue;
        }
        const auto key_value = parse_key_value(trimmed);
        if (!key_value.has_value()) {
            continue;
        }
        const std::string key = to_lower_ascii(key_value->first);
        const std::string value = key_value->second;
        if (key == "keys") {
            try {
                current.keys = std::max(0, std::stoi(value));
            } catch (...) {
                current.keys = 0;
            }
            continue;
        }
        current.values[key] = value;
    }

    commit();
    return sections;
}

std::string resolve_existing_asset(const fs::path& skin_dir, const std::string& token) {
    if (token.empty()) {
        return {};
    }
    const fs::path relative = util::path_from_utf8_lossy(token);
    if (!is_safe_relative_path(relative)) {
        return {};
    }
    std::vector<fs::path> candidates;
    if (to_lower_ascii(relative.extension().u8string()) == ".png") {
        const std::string stem = relative.stem().u8string();
        if (stem.size() < 3u || to_lower_ascii(stem.substr(stem.size() - 3u)) != "@2x") {
            fs::path high_dpi = relative.parent_path() / relative.stem();
            high_dpi += "@2x.png";
            candidates.push_back(std::move(high_dpi));
        }
        candidates.push_back(relative);
    } else if (!relative.has_extension()) {
        candidates.push_back(relative);
        auto push_with_suffix = [&](std::string_view suffix) {
            fs::path candidate = relative;
            candidate += suffix;
            candidates.push_back(std::move(candidate));
        };
        push_with_suffix("@2x.png");
        push_with_suffix(".png");
        push_with_suffix(".jpg");
        push_with_suffix(".jpeg");
    } else {
        candidates.push_back(relative);
    }
    for (const auto& candidate : candidates) {
        std::error_code ec;
        const fs::path full = skin_dir / candidate;
        if (!path_stays_within(skin_dir, full)) {
            continue;
        }
        if (fs::exists(full, ec) && fs::is_regular_file(full, ec)) {
            return normalize_path_utf8(full);
        }
    }
    return {};
}

const ManiaSection* find_exact_section(const std::unordered_map<int, ManiaSection>& sections, int keys) {
    const auto it = sections.find(keys);
    if (it == sections.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<float> parse_number_csv(std::string_view value) {
    std::vector<float> numbers;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        const std::size_t end = (comma == std::string_view::npos) ? value.size() : comma;
        const std::string token = trim_copy(value.substr(start, end - start));
        if (!token.empty()) {
            try {
                const float parsed = std::stof(token);
                if (std::isfinite(parsed)) {
                    numbers.push_back(std::clamp(parsed, 0.0f, kMaxManiaMetric));
                }
            } catch (...) {
            }
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return numbers;
}

std::vector<float> resolve_lane_divider_widths(const ManiaSection& section, int keys) {
    if (keys <= 1) {
        return {};
    }
    const auto it = section.values.find("columnlinewidth");
    if (it == section.values.end()) {
        return {};
    }
    const std::vector<float> parsed = parse_number_csv(it->second);
    if (parsed.empty()) {
        return {};
    }

    const std::size_t internal_count = static_cast<std::size_t>(keys - 1);
    std::vector<float> widths(internal_count, 2.0f);
    if (parsed.size() == static_cast<std::size_t>(keys + 1)) {
        for (std::size_t divider = 0; divider < internal_count; ++divider) {
            widths[divider] = parsed[divider + 1];
        }
        return widths;
    }
    if (parsed.size() >= internal_count) {
        for (std::size_t divider = 0; divider < internal_count; ++divider) {
            widths[divider] = parsed[divider];
        }
        return widths;
    }
    for (std::size_t divider = 0; divider < parsed.size(); ++divider) {
        widths[divider] = parsed[divider];
    }
    return widths;
}

std::vector<float> resolve_column_widths(const ManiaSection& section, int keys) {
    if (keys <= 0) {
        return {};
    }
    const auto it = section.values.find("columnwidth");
    if (it == section.values.end()) {
        return {};
    }
    const std::vector<float> parsed = parse_number_csv(it->second);
    if (parsed.empty()) {
        return {};
    }
    std::vector<float> widths(static_cast<std::size_t>(keys), 30.0f);
    const std::size_t count = (std::min)(widths.size(), parsed.size());
    for (std::size_t i = 0; i < count; ++i) {
        widths[i] = parsed[i];
    }
    return widths;
}

std::vector<float> resolve_column_spacings(const ManiaSection& section, int keys) {
    if (keys <= 1) {
        return {};
    }
    const auto it = section.values.find("columnspacing");
    if (it == section.values.end()) {
        return {};
    }
    const std::vector<float> parsed = parse_number_csv(it->second);
    if (parsed.empty()) {
        return {};
    }

    std::vector<float> spacings(static_cast<std::size_t>(keys - 1), 0.0f);
    const std::size_t count = (std::min)(spacings.size(), parsed.size());
    std::copy_n(parsed.begin(), count, spacings.begin());
    return spacings;
}

std::optional<float> resolve_hit_position(const ManiaSection& section) {
    const auto it = section.values.find("hitposition");
    if (it == section.values.end()) {
        return std::nullopt;
    }
    try {
        const float value = std::stof(it->second);
        if (std::isfinite(value)) {
            return std::clamp(value, 0.0f, kMaxManiaMetric);
        }
    } catch (...) {
    }
    return std::nullopt;
}

float resolve_width_for_note_height_scale(const ManiaSection& section) {
    const auto it = section.values.find("widthfornoteheightscale");
    if (it == section.values.end()) {
        return 0.0f;
    }
    try {
        const float value = std::stof(it->second);
        return std::isfinite(value) ? std::clamp(value, 0.0f, kMaxManiaMetric) : 0.0f;
    } catch (...) {
        return 0.0f;
    }
}

float average_positive_value(const std::vector<float>& values) {
    double sum = 0.0;
    std::size_t count = 0;
    for (float value : values) {
        if (!std::isfinite(value) || value <= 0.0f) {
            continue;
        }
        sum += value;
        ++count;
    }
    if (count == 0u) {
        return 0.0f;
    }
    return static_cast<float>(sum / static_cast<double>(count));
}

float minimum_positive_value(const std::vector<float>& values) {
    float minimum = 0.0f;
    for (float value : values) {
        if (!std::isfinite(value) || value <= 0.0f) {
            continue;
        }
        if (minimum <= 0.0f || value < minimum) {
            minimum = value;
        }
    }
    return minimum;
}

std::vector<std::string> fallback_lane_families(int keys) {
    switch (keys) {
        case 4: return {"1", "2", "2", "1"};
        case 5: return {"1", "2", "S", "2", "1"};
        case 6: return {"1", "2", "1", "1", "2", "1"};
        case 7: return {"1", "2", "1", "S", "1", "2", "1"};
        case 8: return {"1", "2", "1", "2", "2", "1", "2", "1"};
        case 9: return {"1", "2", "1", "2", "S", "2", "1", "2", "1"};
        // osu! defines fallback families through 9K. TenRiff composes its extended
        // layouts from two standard halves instead of inventing another family.
        case 10: return {"1", "2", "S", "2", "1", "1", "2", "S", "2", "1"};
        case 16: return {"1", "2", "1", "2", "2", "1", "2", "1",
                         "1", "2", "1", "2", "2", "1", "2", "1"};
        default: break;
    }
    std::vector<std::string> families;
    families.reserve(static_cast<std::size_t>(std::max(keys, 0)));
    for (int lane = 0; lane < keys; ++lane) {
        families.push_back((lane % 2 == 0) ? "1" : "2");
    }
    return families;
}

std::string resolve_family_asset(const fs::path& skin_dir,
                                 std::string_view base_token,
                                 std::string_view family,
                                 std::string_view suffix) {
    const std::vector<std::string> family_order = {
        std::string(family), "3", "S", "1", "2"
    };
    std::string previous;
    for (const auto& candidate_family : family_order) {
        if (candidate_family.empty() || candidate_family == previous) {
            continue;
        }
        previous = candidate_family;
        const std::string token = std::string(base_token) + candidate_family + std::string(suffix);
        if (std::string path = resolve_existing_asset(skin_dir, token); !path.empty()) {
            return path;
        }
    }
    return {};
}

}  // namespace

std::string find_default_osu_skin_test_root() {
    const std::vector<fs::path> candidates = {
        fs::current_path() / "build" / "Release" / "test-skins-osu",
        fs::current_path() / "build-dist" / "Release" / "test-skins-osu",
        fs::current_path() / "test-skins-osu",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::is_directory(candidate, ec)) {
            return normalize_path_utf8(candidate);
        }
    }
    return {};
}

bool is_osu_skin_directory(std::string_view path_utf8) {
    if (path_utf8.empty()) {
        return false;
    }
    const fs::path path = util::path_from_utf8_lossy(path_utf8);
    return has_osu_skin_assets(path);
}

std::vector<std::string> list_osu_skin_names(std::string_view root_utf8) {
    std::vector<std::string> names;
    if (root_utf8.empty()) {
        return names;
    }
    std::error_code ec;
    const fs::path root = util::path_from_utf8_lossy(root_utf8);
    if (!fs::is_directory(root, ec)) {
        return names;
    }
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) {
            continue;
        }
        if (!has_osu_skin_assets(it->path())) {
            continue;
        }
        names.push_back(util::ensure_utf8_text(it->path().filename().u8string()));
    }
    std::sort(names.begin(), names.end());
    return names;
}

OsuManiaSkinDefinition resolve_osu_mania_skin(std::string_view root_utf8,
                                              std::string_view skin_name,
                                              int keys) {
    OsuManiaSkinDefinition definition;
    definition.keys = std::max(keys, 0);
    if (definition.keys <= 0 || root_utf8.empty() || skin_name.empty()) {
        return definition;
    }

    const fs::path root = util::path_from_utf8_lossy(root_utf8);
    const fs::path relative_skin = util::path_from_utf8_lossy(std::string(skin_name));
    if (!is_safe_relative_path(relative_skin)) {
        return definition;
    }
    const fs::path skin_dir = root / relative_skin;
    std::error_code ec;
    if (!fs::is_directory(skin_dir, ec) || !path_stays_within(root, skin_dir)) {
        return definition;
    }

    const fs::path skin_ini = skin_dir / "skin.ini";
    ec.clear();
    const bool has_skin_ini = fs::is_regular_file(skin_ini, ec) &&
                              path_stays_within(skin_dir, skin_ini);
    const auto sections = has_skin_ini
                              ? parse_skin_ini(skin_ini)
                              : std::unordered_map<int, ManiaSection>{};
    const ManiaSection* section = find_exact_section(sections, definition.keys);
    // A declared skin.ini is authoritative: applying a different key-count section can
    // silently map the wrong receptors and notes. Legacy asset-only skins still use the
    // conventional mania-note/mania-key family fallback below.
    if (has_skin_ini && section == nullptr) {
        return definition;
    }
    definition.found = (section != nullptr);

    const std::size_t lane_count = static_cast<std::size_t>(definition.keys);
    definition.note_images.resize(lane_count);
    definition.hold_head_images.resize(lane_count);
    definition.hold_body_images.resize(lane_count);
    definition.hold_tail_images.resize(lane_count);
    definition.key_images.resize(lane_count);
    definition.key_pressed_images.resize(lane_count);
    definition.lane_divider_widths.clear();
    definition.column_widths.clear();
    definition.column_spacings.clear();
    definition.hit_position = 0.0f;
    definition.has_hit_position = false;
    definition.width_for_note_height_scale = 0.0f;
    definition.imported_note_width_ratio = 1.0f;
    definition.imported_note_height_ratio = 1.0f;

    if (section != nullptr) {
        definition.lane_divider_widths = resolve_lane_divider_widths(*section, definition.keys);
        definition.column_widths = resolve_column_widths(*section, definition.keys);
        definition.column_spacings = resolve_column_spacings(*section, definition.keys);
        if (const auto hit_position = resolve_hit_position(*section); hit_position.has_value()) {
            definition.hit_position = *hit_position;
            definition.has_hit_position = true;
        }
        definition.width_for_note_height_scale = resolve_width_for_note_height_scale(*section);
        for (std::size_t lane = 0; lane < lane_count; ++lane) {
            const std::string lane_suffix = std::to_string(lane);
            const std::string note_image_key = "noteimage" + lane_suffix;
            const std::string hold_head_key = note_image_key + "h";
            const std::string hold_body_key = note_image_key + "l";
            const std::string hold_tail_key = note_image_key + "t";
            const std::string key_image_key = "keyimage" + lane_suffix;
            const std::string key_pressed_key = key_image_key + "d";
            const auto find_value = [&](std::string_view key) -> std::string {
                const auto it = section->values.find(to_lower_ascii(std::string(key)));
                if (it == section->values.end()) {
                    return {};
                }
                return normalize_asset_token(it->second);
            };

            definition.note_images[lane] =
                resolve_existing_asset(skin_dir, find_value(note_image_key));
            definition.hold_head_images[lane] =
                resolve_existing_asset(skin_dir, find_value(hold_head_key));
            definition.hold_body_images[lane] =
                resolve_existing_asset(skin_dir, find_value(hold_body_key));
            definition.hold_tail_images[lane] =
                resolve_existing_asset(skin_dir, find_value(hold_tail_key));
            definition.key_images[lane] =
                resolve_existing_asset(skin_dir, find_value(key_image_key));
            definition.key_pressed_images[lane] =
                resolve_existing_asset(skin_dir, find_value(key_pressed_key));
        }

        const float imported_base_width = average_positive_value(definition.column_widths);
        if (imported_base_width > 0.0f) {
            definition.imported_note_width_ratio = imported_base_width / 30.0f;
            const float height_scale_width =
                (definition.width_for_note_height_scale > 0.0f)
                    ? definition.width_for_note_height_scale
                    : minimum_positive_value(definition.column_widths);
            if (height_scale_width > 0.0f) {
                definition.imported_note_height_ratio = height_scale_width / imported_base_width;
            }
        }
    }

    const auto fallback_families = fallback_lane_families(definition.keys);
    for (std::size_t lane = 0; lane < lane_count && lane < fallback_families.size(); ++lane) {
        const std::string& family = fallback_families[lane];
        if (definition.note_images[lane].empty()) {
            definition.note_images[lane] = resolve_family_asset(skin_dir, "mania-note", family, "");
        }
        if (definition.hold_head_images[lane].empty()) {
            definition.hold_head_images[lane] = resolve_family_asset(skin_dir, "mania-note", family, "H");
        }
        if (definition.hold_body_images[lane].empty()) {
            definition.hold_body_images[lane] = resolve_family_asset(skin_dir, "mania-note", family, "L");
        }
        if (definition.hold_tail_images[lane].empty()) {
            definition.hold_tail_images[lane] = resolve_family_asset(skin_dir, "mania-note", family, "T");
        }
        if (definition.key_images[lane].empty()) {
            definition.key_images[lane] = resolve_family_asset(skin_dir, "mania-key", family, "");
        }
        if (definition.key_pressed_images[lane].empty()) {
            definition.key_pressed_images[lane] = resolve_family_asset(skin_dir, "mania-key", family, "D");
        }
    }

    if (!has_skin_ini) {
        const auto has_asset = [](const std::vector<std::string>& assets) {
            return std::any_of(assets.begin(), assets.end(), [](const std::string& path) {
                return !path.empty();
            });
        };
        definition.found = has_asset(definition.note_images) ||
                           has_asset(definition.hold_head_images) ||
                           has_asset(definition.hold_body_images) ||
                           has_asset(definition.hold_tail_images) ||
                           has_asset(definition.key_images) ||
                           has_asset(definition.key_pressed_images);
    }

    return definition;
}

}  // namespace tenriff::app
