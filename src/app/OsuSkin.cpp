#include "app/OsuSkin.h"

#include <algorithm>
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
    std::vector<fs::path> candidates;
    candidates.push_back(relative);
    if (!relative.has_extension()) {
        candidates.push_back(relative.string() + ".png");
        candidates.push_back(relative.string() + "@2x.png");
        candidates.push_back(relative.string() + ".jpg");
        candidates.push_back(relative.string() + ".jpeg");
    }
    for (const auto& candidate : candidates) {
        std::error_code ec;
        const fs::path full = skin_dir / candidate;
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

std::vector<std::string> fallback_lane_families(int keys) {
    switch (keys) {
        case 4: return {"1", "2", "2", "1"};
        case 5: return {"1", "2", "3", "2", "1"};
        case 6: return {"1", "2", "3", "3", "2", "1"};
        case 7: return {"1", "2", "1", "3", "1", "2", "1"};
        case 8: return {"1", "2", "1", "S", "S", "1", "2", "1"};
        case 9: return {"1", "2", "1", "S", "3", "S", "1", "2", "1"};
        case 10: return {"1", "2", "3", "2", "1", "1", "2", "3", "2", "1"};
        case 16: return {"1", "2", "1", "S", "S", "1", "2", "1",
                         "1", "2", "1", "S", "S", "1", "2", "1"};
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
    const fs::path skin_dir = root / util::path_from_utf8_lossy(std::string(skin_name));
    std::error_code ec;
    if (!fs::is_directory(skin_dir, ec)) {
        return definition;
    }

    definition.found = true;
    const auto sections = parse_skin_ini(skin_dir / "skin.ini");
    const ManiaSection* section = find_exact_section(sections, definition.keys);

    const std::size_t lane_count = static_cast<std::size_t>(definition.keys);
    definition.note_images.resize(lane_count);
    definition.hold_head_images.resize(lane_count);
    definition.hold_body_images.resize(lane_count);
    definition.hold_tail_images.resize(lane_count);
    definition.key_images.resize(lane_count);
    definition.key_pressed_images.resize(lane_count);

    if (section != nullptr) {
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

    return definition;
}

}  // namespace tenriff::app
