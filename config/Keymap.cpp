#include "config/Keymap.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "config/SimpleJson.h"

namespace tenriff::config {

namespace {

const JsonObject* get_object(const JsonObject& object, std::string_view key) {
    auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return it->second.as_object();
}

std::string get_string(const JsonObject& object, std::string_view key, std::string fallback) {
    auto it = object.find(std::string(key));
    if (it == object.end()) {
        return fallback;
    }
    return it->second.as_string(std::move(fallback));
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const std::unordered_map<std::string, std::vector<std::string>>& default_mode_bindings() {
    static const std::unordered_map<std::string, std::vector<std::string>> kBindings = {
        {"4k", {"D", "F", "L", "Semicolon"}},
        {"5k", {"D", "F", "K", "L", "Semicolon"}},
        {"6k", {"S", "D", "F", "J", "K", "L"}},
        {"7k", {"W", "E", "R", "M", "I", "O", "P"}},
        {"8k", {"W", "E", "R", "V", "M", "I", "O", "P"}},
        {"9k", {"A", "S", "D", "F", "Space", "H", "J", "K", "L"}},
        {"10k", {"Q", "W", "E", "R", "V", "M", "I", "O", "P", "LBracket"}},
        {"16k", {"Q", "W", "E", "R", "A", "S", "D", "F",
                 "U", "I", "O", "P", "J", "K", "L", "Semicolon"}},
    };
    return kBindings;
}

std::unordered_map<std::string, std::string> bindings_from_vector(const std::vector<std::string>& keys) {
    std::unordered_map<std::string, std::string> bindings;
    bindings.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        bindings.emplace("lane" + std::to_string(i + 1), keys[i]);
    }
    return bindings;
}

}  // namespace

Keymap KeymapManager::default_keymap() const {
    Keymap keymap;
    keymap.layout = "multi";
    for (const auto& [mode, keys] : default_mode_bindings()) {
        keymap.mode_bindings.emplace(mode, bindings_from_vector(keys));
    }
    keymap.bindings = keymap.mode_bindings["10k"];
    return keymap;
}

std::string KeymapManager::normalize_mode_token(std::string_view key_mode) const {
    std::string normalized = to_lower_ascii(std::string(key_mode));
    if (normalized == "4" || normalized == "4key" || normalized == "keys4") {
        return "4k";
    }
    if (normalized == "5" || normalized == "5key" || normalized == "keys5") {
        return "5k";
    }
    if (normalized == "6" || normalized == "6key" || normalized == "keys6") {
        return "6k";
    }
    if (normalized == "7" || normalized == "7key" || normalized == "keys7") {
        return "7k";
    }
    if (normalized == "8" || normalized == "8key" || normalized == "keys8") {
        return "8k";
    }
    if (normalized == "9" || normalized == "9key" || normalized == "keys9") {
        return "9k";
    }
    if (normalized == "10" || normalized == "10key" || normalized == "keys10") {
        return "10k";
    }
    if (normalized == "16" || normalized == "16key" || normalized == "keys16") {
        return "16k";
    }
    if (normalized == "4k" || normalized == "5k" || normalized == "6k" || normalized == "7k" ||
        normalized == "8k" || normalized == "9k" || normalized == "10k" || normalized == "16k") {
        return normalized;
    }
    return "10k";
}

std::vector<std::string> KeymapManager::supported_mode_tokens() const {
    return {"4k", "5k", "6k", "7k", "8k", "9k", "10k", "16k"};
}

std::vector<std::string> KeymapManager::lane_ids_for_mode(std::string_view key_mode) const {
    const auto bindings = bindings_for_mode(default_keymap(), key_mode);
    std::vector<std::string> lanes;
    lanes.reserve(bindings.size());
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        lanes.push_back("lane" + std::to_string(i + 1));
    }
    return lanes;
}

std::unordered_map<std::string, std::string> KeymapManager::bindings_for_mode(const Keymap& keymap,
                                                                               std::string_view key_mode) const {
    const std::string normalized = normalize_mode_token(key_mode);
    auto it = keymap.mode_bindings.find(normalized);
    if (it != keymap.mode_bindings.end()) {
        return it->second;
    }

    Keymap defaults = default_keymap();
    auto defaults_it = defaults.mode_bindings.find(normalized);
    if (defaults_it != defaults.mode_bindings.end()) {
        return defaults_it->second;
    }
    return {};
}

void KeymapManager::reset_mode_bindings(Keymap& keymap, std::string_view key_mode) const {
    const std::string normalized = normalize_mode_token(key_mode);
    Keymap defaults = default_keymap();
    keymap.mode_bindings[normalized] = defaults.mode_bindings[normalized];
    if (normalized == "10k") {
        keymap.bindings = keymap.mode_bindings[normalized];
    }
}

KeymapLoadResult KeymapManager::load_profile(std::string_view profile_dir) const {
    KeymapLoadResult result;
    result.keymap = default_keymap();

    std::filesystem::path path(profile_dir);
    path /= "keymap.json";

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.used_defaults = true;
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto parse = parse_json(buffer.str());
    if (!parse.success() || !parse.root.has_value()) {
        result.error = parse.error.empty() ? "Failed to parse keymap.json." : parse.error;
        return result;
    }

    const auto* root = parse.root->as_object();
    if (!root) {
        result.error = "keymap.json root must be an object.";
        return result;
    }

    result.keymap.layout = get_string(*root, "layout", result.keymap.layout);

    if (auto* modes = get_object(*root, "modes")) {
        for (const auto& [mode, value] : *modes) {
            const auto* bindings = value.as_object();
            if (!bindings) {
                continue;
            }
            auto& target = result.keymap.mode_bindings[normalize_mode_token(mode)];
            for (const auto& [lane, lane_value] : *bindings) {
                if (!lane_value.is_string()) {
                    continue;
                }
                target[lane] = lane_value.as_string();
            }
        }
    }

    if (auto* bindings = get_object(*root, "bindings")) {
        for (const auto& [lane, value] : *bindings) {
            if (!value.is_string()) {
                continue;
            }
            result.keymap.bindings[lane] = value.as_string();
            result.keymap.mode_bindings["10k"][lane] = value.as_string();
        }
    }

    if (result.keymap.mode_bindings.find("10k") != result.keymap.mode_bindings.end()) {
        result.keymap.bindings = result.keymap.mode_bindings["10k"];
    }

    return result;
}

bool KeymapManager::save_profile(std::string_view profile_dir, const Keymap& keymap, std::string* error) const {
    std::filesystem::path path(profile_dir);
    std::filesystem::create_directories(path);
    path /= "keymap.json";

    JsonObject root;
    root.emplace("layout", JsonValue{keymap.layout});

    JsonObject modes;
    for (const auto& mode : supported_mode_tokens()) {
        JsonObject bindings;
        const auto mode_bindings = bindings_for_mode(keymap, mode);
        for (const auto& [lane, key] : mode_bindings) {
            bindings.emplace(lane, JsonValue{key});
        }
        modes.emplace(mode, JsonValue{std::move(bindings)});
    }
    root.emplace("modes", JsonValue{std::move(modes)});

    JsonObject bindings;
    const auto ten_key_bindings = bindings_for_mode(keymap, "10k");
    for (const auto& [lane, key] : ten_key_bindings) {
        bindings.emplace(lane, JsonValue{key});
    }
    root.emplace("bindings", JsonValue{std::move(bindings)});

    JsonValue root_value{std::move(root)};
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) {
            *error = "Failed to open keymap.json for writing.";
        }
        return false;
    }

    out << json_stringify(root_value, 2);
    return true;
}

std::vector<std::string> KeymapManager::validate_unique_bindings(const Keymap& keymap) const {
    std::vector<std::string> duplicates;
    std::unordered_map<std::string, std::string> used;

    for (const auto& mode : supported_mode_tokens()) {
        used.clear();
        for (const auto& [lane, key] : bindings_for_mode(keymap, mode)) {
            if (key.empty()) {
                continue;
            }
            auto it = used.find(key);
            if (it != used.end()) {
                duplicates.push_back(mode + ":" + key);
            } else {
                used.emplace(key, lane);
            }
        }
    }

    return duplicates;
}

}  // namespace tenriff::config
