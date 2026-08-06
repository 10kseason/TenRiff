#include "app/Lr2Skin.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

namespace fs = std::filesystem;

constexpr float kDefaultLr2NoteWidth = 30.0f;
constexpr float kDefaultLr2NoteHeight = 22.0f;
// A canvas backdrop has to sit in the corner and cover most of the screen. LR2
// authors at 640x480, 1280x720 or 1920x1080, so anything smaller than this is a
// panel rather than the canvas.
constexpr float kLr2CanvasOriginTolerance = 4.0f;
constexpr float kLr2MinCanvasWidth = 400.0f;
constexpr float kLr2MinCanvasHeight = 300.0f;

struct Lr2SourceSlice {
    int group_index = -1;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
    // div_x * div_y from the source command. Anything above one is a sprite sheet
    // the skin cycles through, not a single still frame.
    int divisions = 1;
};

struct Lr2DestinationNote {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
};

struct Lr2StaticImageLayer {
    Lr2SourceSlice source;
    Lr2DestinationNote destination;
    // Layers declared inside a Gear-named file are authored gear art. Layers found
    // anywhere else still qualify, they just lose every tie against these.
    bool from_gear_file = false;
    // False when the destination is gated on a timer or an option, which is how
    // LR2 declares transient art such as the full-combo burst or the stage shutter.
    bool always_visible = true;
};

struct CustomFileSelection {
    std::string wildcard_path;
    std::string default_name;
};

struct Lr2ParseState {
    fs::path skin_dir;
    std::unordered_map<int, std::string> image_paths;
    int next_image_index = 0;
    std::unordered_map<int, Lr2SourceSlice> note_slices;
    std::unordered_map<int, Lr2SourceSlice> hold_head_slices;
    std::unordered_map<int, Lr2SourceSlice> hold_body_slices;
    std::unordered_map<int, Lr2SourceSlice> hold_tail_slices;
    std::unordered_map<int, Lr2DestinationNote> dst_notes;
    // #SRC_IMAGE slots are global in LR2: a slot declared in one file stays
    // addressable by a #DST_IMAGE in an included file.
    std::unordered_map<int, Lr2SourceSlice> static_image_slices;
    std::vector<Lr2StaticImageLayer> gear_layers;
    std::unordered_map<std::string, CustomFileSelection> custom_files;
    std::set<int> active_options;
    std::set<std::string> visited_files;
    std::optional<Lr2ResolutionFamily> explicit_resolution_family;
    // Largest corner-anchored backdrop seen, used to recognise the authoring canvas.
    float canvas_width_hint = 0.0f;
    float canvas_height_hint = 0.0f;
    bool top_level_header_open = false;
};

struct ConditionalFrame {
    bool parent_active = true;
    bool branch_taken = false;
    bool current_active = true;
};

struct Lr2OrderedDestination {
    int lane = 0;
    Lr2DestinationNote dst;
};

struct Lr2RawPlayLayout {
    std::vector<int> lane_order;
    std::vector<Lr2OrderedDestination> ordered_destinations;
    float average_width = kDefaultLr2NoteWidth;
    float average_height = kDefaultLr2NoteHeight;
    float max_x = 0.0f;
    int size_sample_count = 0;
    bool has_destinations = false;
};

struct Lr2NormalizedPlayMetrics {
    int keys = 0;
    Lr2ResolutionFamily resolution_family = Lr2ResolutionFamily::Sd;
    float imported_note_width_ratio = 1.0f;
    float imported_note_height_ratio = 1.0f;
    std::vector<float> lane_divider_widths;
};

struct Lr2CandidateInfo {
    fs::path skin_file;
    int requested_keys = 0;
    int parsed_keys = 0;
    int file_hint_bonus = 0;
    int score = 0;
    Lr2ResolutionFamily resolution_family = Lr2ResolutionFamily::Sd;
};

fs::path relativize_theme_path(const fs::path& skin_dir, const fs::path& candidate);
fs::path relativize_theme_root_path(const fs::path& skin_dir, const fs::path& candidate);

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
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

std::string normalize_lr2_token(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2u) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = trim_copy(std::string_view(value).substr(1, value.size() - 2u));
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

std::vector<fs::path> build_lr2_path_candidates(const fs::path& skin_dir,
                                                const fs::path& current_dir,
                                                std::string_view raw_path) {
    std::vector<fs::path> candidates;
    const std::string normalized = normalize_lr2_token(std::string(raw_path));
    if (normalized.empty()) {
        return candidates;
    }

    const fs::path token_path = util::path_from_utf8_lossy(normalized);
    if (token_path.is_absolute()) {
        candidates.push_back(token_path);
        return candidates;
    }

    candidates.push_back(current_dir / token_path);
    candidates.push_back(skin_dir / token_path);
    const fs::path theme_relative = relativize_theme_path(skin_dir, token_path);
    if (theme_relative != token_path) {
        candidates.push_back(theme_relative);
    }
    const fs::path theme_root_relative = relativize_theme_root_path(skin_dir, token_path);
    if (theme_root_relative != token_path) {
        candidates.push_back(theme_root_relative);
    }
    return candidates;
}

bool has_lr2_skin_assets(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return false;
    }
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        if (to_lower_ascii(it->path().extension().u8string()) == ".lr2skin") {
            return true;
        }
    }
    return false;
}

std::vector<std::string> split_lr2_command(std::string_view line) {
    std::vector<std::string> tokens;
    std::string current;
    const auto flush = [&]() {
        tokens.push_back(trim_copy(current));
        current.clear();
    };

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '/' && i + 1u < line.size() && line[i + 1u] == '/') {
            break;
        }
        if (ch == ',' || ch == '\t') {
            flush();
            continue;
        }
        current.push_back(ch);
    }
    flush();
    while (!tokens.empty() && tokens.back().empty()) {
        tokens.pop_back();
    }
    return tokens;
}

std::optional<int> parse_int_token(const std::vector<std::string>& tokens, std::size_t index) {
    if (index >= tokens.size()) {
        return std::nullopt;
    }
    try {
        return std::stoi(tokens[index]);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> parse_float_token(const std::vector<std::string>& tokens, std::size_t index) {
    if (index >= tokens.size()) {
        return std::nullopt;
    }
    try {
        const float value = std::stof(tokens[index]);
        if (std::isfinite(value)) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<Lr2ResolutionFamily> parse_lr2_resolution_value(int value) {
    switch (value) {
        case 1:
            return Lr2ResolutionFamily::Hd;
        case 2:
            return Lr2ResolutionFamily::Fhd;
        default:
            return std::nullopt;
    }
}

std::optional<Lr2ResolutionFamily> parse_lr2_resolution_override_token(std::string_view token) {
    const std::string normalized = to_lower_ascii(trim_copy(token));
    if (normalized.empty() || normalized == "auto") {
        return std::nullopt;
    }
    if (normalized == "sd" || normalized == "640x480" || normalized == "lr2") {
        return Lr2ResolutionFamily::Sd;
    }
    if (normalized == "hd" || normalized == "1280x720" || normalized == "720p") {
        return Lr2ResolutionFamily::Hd;
    }
    if (normalized == "fhd" || normalized == "1920x1080" || normalized == "1080p") {
        return Lr2ResolutionFamily::Fhd;
    }
    return std::nullopt;
}

std::string lr2_resolution_family_label(Lr2ResolutionFamily family) {
    switch (family) {
        case Lr2ResolutionFamily::Hd:
            return "hd";
        case Lr2ResolutionFamily::Fhd:
            return "fhd";
        case Lr2ResolutionFamily::Sd:
        default:
            return "sd";
    }
}

std::pair<float, float> lr2_resolution_family_scale(Lr2ResolutionFamily family) {
    switch (family) {
        case Lr2ResolutionFamily::Hd:
            return {2.0f, 1.5f};
        case Lr2ResolutionFamily::Fhd:
            return {3.0f, 2.25f};
        case Lr2ResolutionFamily::Sd:
        default:
            return {1.0f, 1.0f};
    }
}

bool evaluate_lr2_if(const std::set<int>& active_options, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2u) {
        return false;
    }
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        std::string expr = trim_copy(tokens[i]);
        if (expr.empty()) {
            continue;
        }
        bool negate = false;
        if (expr.front() == '!') {
            negate = true;
            expr.erase(expr.begin());
        }

        int op = 0;
        try {
            op = std::stoi(expr);
        } catch (...) {
            return false;
        }

        const bool active = active_options.find(op) != active_options.end();
        if (negate ? active : !active) {
            return false;
        }
    }
    return true;
}

fs::path relativize_theme_path(const fs::path& skin_dir, const fs::path& candidate) {
    std::vector<fs::path> parts;
    for (const auto& part : candidate) {
        parts.push_back(part);
    }
    std::size_t start = 0u;
    while (start < parts.size() && parts[start].u8string() == ".") {
        ++start;
    }
    if (parts.size() >= start + 4u &&
        to_lower_ascii(parts[start].u8string()) == "lr2files" &&
        to_lower_ascii(parts[start + 1u].u8string()) == "theme") {
        fs::path relative;
        for (std::size_t i = start + 3u; i < parts.size(); ++i) {
            relative /= parts[i];
        }
        if (!relative.empty()) {
            return skin_dir / relative;
        }
    }
    return candidate;
}

fs::path relativize_theme_root_path(const fs::path& skin_dir, const fs::path& candidate) {
    std::vector<fs::path> parts;
    for (const auto& part : candidate) {
        parts.push_back(part);
    }
    std::size_t start = 0u;
    while (start < parts.size() && parts[start].u8string() == ".") {
        ++start;
    }
    if (parts.size() >= start + 3u &&
        to_lower_ascii(parts[start].u8string()) == "lr2files" &&
        to_lower_ascii(parts[start + 1u].u8string()) == "theme") {
        fs::path relative;
        // Keep the skin-name component so references to a sibling theme stay
        // inside the imported Theme namespace instead of being folded into the
        // currently selected skin directory.
        for (std::size_t i = start + 2u; i < parts.size(); ++i) {
            relative /= parts[i];
        }
        if (!relative.empty()) {
            return skin_dir.parent_path() / relative;
        }
    }
    return candidate;
}

std::optional<fs::path> resolve_existing_lr2_path(const fs::path& skin_dir,
                                                  const fs::path& current_dir,
                                                  std::string_view raw_path) {
    std::error_code ec;
    for (auto candidate : build_lr2_path_candidates(skin_dir, current_dir, raw_path)) {
        candidate = candidate.lexically_normal();
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
        ec.clear();
    }
    return std::nullopt;
}

bool wildcard_match(std::string_view candidate, std::string_view pattern) {
    const std::size_t star = pattern.find('*');
    if (star == std::string_view::npos) {
        return to_lower_ascii(std::string(candidate)) == to_lower_ascii(std::string(pattern));
    }
    const std::string prefix = to_lower_ascii(std::string(pattern.substr(0, star)));
    const std::string suffix = to_lower_ascii(std::string(pattern.substr(star + 1u)));
    const std::string lower_candidate = to_lower_ascii(std::string(candidate));
    if (lower_candidate.size() < prefix.size() + suffix.size()) {
        return false;
    }
    if (!prefix.empty() && lower_candidate.rfind(prefix, 0) != 0) {
        return false;
    }
    if (!suffix.empty() &&
        lower_candidate.substr(lower_candidate.size() - suffix.size()) != suffix) {
        return false;
    }
    return true;
}

std::string replace_first_wildcard(std::string value,
                                   std::string_view replacement,
                                   std::size_t start_index = 0u) {
    if (replacement.empty()) {
        return value;
    }
    const std::size_t star = value.find('*', start_index);
    if (star == std::string::npos) {
        return value;
    }
    value.replace(star, 1u, replacement);
    return value;
}

bool custom_file_matches_image(std::string_view image_token, std::string_view wildcard_path) {
    const std::size_t wildcard = wildcard_path.find('*');
    if (wildcard == std::string_view::npos) {
        return false;
    }
    const std::string_view prefix = wildcard_path.substr(0, wildcard);
    if (image_token.size() < prefix.size() ||
        image_token.substr(0, prefix.size()) != prefix) {
        return false;
    }
    const std::size_t image_wildcard = image_token.find('*', prefix.size());
    if (image_wildcard == std::string_view::npos) {
        return false;
    }
    const std::string_view suffix = wildcard_path.substr(wildcard + 1u);
    return suffix.empty() || image_token.find(suffix, image_wildcard + 1u) != std::string_view::npos;
}

std::string apply_custom_file_selection(const Lr2ParseState& state, std::string_view image_token) {
    std::string best = std::string(image_token);
    std::size_t best_prefix = 0u;
    for (const auto& [_, selection] : state.custom_files) {
        if (selection.default_name.empty() ||
            !custom_file_matches_image(image_token, selection.wildcard_path)) {
            continue;
        }
        const std::size_t wildcard = selection.wildcard_path.find('*');
        if (wildcard == std::string::npos || wildcard < best_prefix) {
            continue;
        }
        best = replace_first_wildcard(std::string(image_token), selection.default_name, wildcard);
        best_prefix = wildcard;
    }
    return best;
}

std::vector<fs::path> expand_lr2_pattern_path(const fs::path& pattern_path) {
    std::vector<fs::path> frontier;
    if (pattern_path.is_absolute()) {
        frontier.push_back(pattern_path.root_path());
    } else {
        frontier.push_back(fs::path{});
    }

    const fs::path segments = pattern_path.is_absolute() ? pattern_path.relative_path() : pattern_path;
    for (const auto& part : segments) {
        const std::string segment = part.u8string();
        if (segment.empty() || segment == ".") {
            continue;
        }

        std::vector<fs::path> next;
        if (segment.find('*') == std::string::npos) {
            next.reserve(frontier.size());
            for (const auto& prefix : frontier) {
                next.push_back((prefix / part).lexically_normal());
            }
        } else {
            std::error_code ec;
            for (const auto& prefix : frontier) {
                if (!fs::is_directory(prefix, ec)) {
                    ec.clear();
                    continue;
                }
                for (fs::directory_iterator it(prefix, ec), end; !ec && it != end; it.increment(ec)) {
                    if (wildcard_match(it->path().filename().u8string(), segment)) {
                        next.push_back(it->path());
                    }
                }
                ec.clear();
            }
        }
        frontier = std::move(next);
        if (frontier.empty()) {
            break;
        }
    }
    return frontier;
}

std::optional<fs::path> resolve_lr2_wildcard_path(const fs::path& skin_dir,
                                                  const fs::path& current_dir,
                                                  std::string_view wildcard_path,
                                                  std::string_view default_name) {
    const std::string normalized = normalize_lr2_token(std::string(wildcard_path));
    if (normalized.find('*') == std::string::npos) {
        return resolve_existing_lr2_path(skin_dir, current_dir, normalized);
    }

    std::vector<fs::path> matches;
    for (auto pattern_path : build_lr2_path_candidates(skin_dir, current_dir, normalized)) {
        pattern_path = pattern_path.lexically_normal();
        for (const auto& candidate : expand_lr2_pattern_path(pattern_path)) {
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                matches.push_back(candidate);
            }
        }
    }
    if (matches.empty()) {
        return std::nullopt;
    }

    std::sort(matches.begin(), matches.end(), [](const fs::path& lhs, const fs::path& rhs) {
        return to_lower_ascii(lhs.u8string()) < to_lower_ascii(rhs.u8string());
    });
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

    const std::string preferred = to_lower_ascii(normalize_lr2_token(std::string(default_name)));
    if (!preferred.empty()) {
        for (const auto& candidate : matches) {
            const std::string stem = to_lower_ascii(candidate.stem().u8string());
            const std::string filename = to_lower_ascii(candidate.filename().u8string());
            const std::string parent = to_lower_ascii(candidate.parent_path().filename().u8string());
            if (stem == preferred || filename == preferred || parent == preferred) {
                return candidate;
            }
        }
    }
    return matches.front();
}

ImportedSkinImageAsset make_asset_from_slice(const std::unordered_map<int, std::string>& image_paths,
                                             const std::unordered_map<int, Lr2SourceSlice>& slices,
                                             int lane_index) {
    ImportedSkinImageAsset asset;
    const auto slice_it = slices.find(lane_index);
    if (slice_it == slices.end() || !slice_it->second.valid) {
        return asset;
    }
    const auto path_it = image_paths.find(slice_it->second.group_index);
    if (path_it == image_paths.end() || path_it->second.empty()) {
        return asset;
    }
    asset.path = path_it->second;
    asset.source_x = slice_it->second.x;
    asset.source_y = slice_it->second.y;
    asset.source_width = slice_it->second.width;
    asset.source_height = slice_it->second.height;
    asset.has_source_rect = slice_it->second.width > 0.0f && slice_it->second.height > 0.0f;
    return asset;
}

bool parse_lr2_file(const fs::path& file_path, Lr2ParseState& state, bool is_top_level = false) {
    std::error_code canonical_ec;
    const fs::path canonical_path = fs::weakly_canonical(file_path, canonical_ec);
    const std::string visited_key = normalize_path_utf8(canonical_path.empty() ? file_path : canonical_path);
    if (!state.visited_files.insert(visited_key).second) {
        return true;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return false;
    }

    const fs::path current_dir = file_path.parent_path();
    std::vector<ConditionalFrame> conditionals;
    // A #SRC_IMAGE followed by one or more #DST_IMAGE lines is one animated layer.
    // The final keyframe is where the art comes to rest, so later keyframes in the
    // same run overwrite the layer instead of appending a new one.
    std::size_t open_gear_layer = std::numeric_limits<std::size_t>::max();
    int open_gear_layer_source = -1;
    // Only the first destination line of a run carries the timer and option gates;
    // the keyframes after it leave those columns blank.
    bool open_group_always_visible = true;
    bool open_group_gates_read = false;
    const std::string generic_file_path = to_lower_ascii(file_path.generic_u8string());
    const bool gear_file = generic_file_path.find("/gear/") != std::string::npos ||
                           to_lower_ascii(file_path.stem().u8string()) == "gear";

    auto current_active = [&]() {
        return conditionals.empty() ? true : conditionals.back().current_active;
    };

    if (is_top_level) {
        state.top_level_header_open = true;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() >= 3u &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3u);
        }

        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.rfind("//", 0) == 0 || trimmed.front() != '#') {
            continue;
        }

        const auto tokens = split_lr2_command(trimmed);
        if (tokens.empty()) {
            continue;
        }
        const std::string command = to_lower_ascii(tokens.front());

        if (is_top_level && state.top_level_header_open) {
            if (command == "#resolution" && !state.explicit_resolution_family.has_value()) {
                if (const auto resolution_value = parse_int_token(tokens, 1u); resolution_value.has_value()) {
                    state.explicit_resolution_family = parse_lr2_resolution_value(*resolution_value);
                }
                continue;
            }
            if (command == "#endofheader") {
                state.top_level_header_open = false;
                continue;
            }
        }

        if (command == "#if") {
            const bool parent_active = current_active();
            const bool branch_active = parent_active && evaluate_lr2_if(state.active_options, tokens);
            conditionals.push_back({parent_active, branch_active, branch_active});
            continue;
        }
        if (command == "#elseif") {
            if (conditionals.empty()) {
                continue;
            }
            auto& frame = conditionals.back();
            const bool branch_active =
                frame.parent_active && !frame.branch_taken && evaluate_lr2_if(state.active_options, tokens);
            frame.current_active = branch_active;
            frame.branch_taken = frame.branch_taken || branch_active;
            continue;
        }
        if (command == "#else") {
            if (conditionals.empty()) {
                continue;
            }
            auto& frame = conditionals.back();
            frame.current_active = frame.parent_active && !frame.branch_taken;
            frame.branch_taken = true;
            continue;
        }
        if (command == "#endif") {
            if (!conditionals.empty()) {
                conditionals.pop_back();
            }
            continue;
        }

        if (!current_active()) {
            continue;
        }

        if (command == "#customoption") {
            if (const auto op = parse_int_token(tokens, 2u); op.has_value()) {
                state.active_options.insert(*op);
            }
            continue;
        }
        if (command == "#setoption") {
            if (const auto op = parse_int_token(tokens, 1u); op.has_value()) {
                const auto value = parse_int_token(tokens, 2u);
                if (!value.has_value() || *value != 0) {
                    state.active_options.insert(*op);
                } else {
                    state.active_options.erase(*op);
                }
            }
            continue;
        }
        if (command == "#customfile") {
            if (tokens.size() >= 4u) {
                CustomFileSelection selection;
                selection.wildcard_path = normalize_lr2_token(tokens[2u]);
                selection.default_name = normalize_lr2_token(tokens[3u]);
                state.custom_files[to_lower_ascii(selection.wildcard_path)] = selection;
            }
            continue;
        }
        if (command == "#include") {
            if (tokens.size() < 2u) {
                continue;
            }
            const std::string include_token = normalize_lr2_token(tokens[1u]);
            const std::string customized_token = apply_custom_file_selection(state, include_token);
            auto include_path = resolve_lr2_wildcard_path(
                state.skin_dir, current_dir, customized_token, {});
            if (!include_path.has_value() && customized_token != include_token) {
                include_path = resolve_lr2_wildcard_path(
                    state.skin_dir, current_dir, include_token, {});
            }
            if (include_path.has_value()) {
                parse_lr2_file(*include_path, state, false);
            }
            continue;
        }
        if (command == "#image") {
            if (tokens.size() < 2u) {
                continue;
            }
            // LR2 source commands address images by declaration order. Reserve the slot even
            // when its file is missing, and share the counter across includes so a caller cannot
            // reuse an index that an included file already consumed.
            const int image_index = state.next_image_index++;
            const std::string image_token = normalize_lr2_token(tokens[1u]);
            const std::string customized_token = apply_custom_file_selection(state, image_token);
            std::string resolved_path;
            if (const auto resolved =
                    resolve_lr2_wildcard_path(state.skin_dir, current_dir, customized_token, {});
                resolved.has_value()) {
                resolved_path = normalize_path_utf8(*resolved);
            } else if (customized_token != image_token) {
                if (const auto resolved =
                        resolve_lr2_wildcard_path(state.skin_dir, current_dir, image_token, {});
                    resolved.has_value()) {
                    resolved_path = normalize_path_utf8(*resolved);
                }
            }
            if (!resolved_path.empty()) {
                state.image_paths[image_index] = resolved_path;
            }
            continue;
        }

        auto parse_source_slice = [&](std::unordered_map<int, Lr2SourceSlice>& destination) {
            const auto lane = parse_int_token(tokens, 1u);
            const auto group_index = parse_int_token(tokens, 2u);
            const auto x = parse_float_token(tokens, 3u);
            const auto y = parse_float_token(tokens, 4u);
            const auto width = parse_float_token(tokens, 5u);
            const auto height = parse_float_token(tokens, 6u);
            if (!lane.has_value() || !group_index.has_value() || !x.has_value() || !y.has_value() ||
                !width.has_value() || !height.has_value()) {
                return;
            }
            const int div_x = std::max(1, parse_int_token(tokens, 7u).value_or(1));
            const int div_y = std::max(1, parse_int_token(tokens, 8u).value_or(1));
            destination[*lane] =
                Lr2SourceSlice{*group_index, *x, *y, *width, *height, true, div_x * div_y};
        };

        if (command == "#src_image") {
            parse_source_slice(state.static_image_slices);
            open_gear_layer = std::numeric_limits<std::size_t>::max();
            open_gear_layer_source = -1;
            open_group_always_visible = true;
            open_group_gates_read = false;
            continue;
        }
        if (command == "#dst_image") {
            const auto source_index = parse_int_token(tokens, 1u);
            const auto x = parse_float_token(tokens, 3u);
            const auto y = parse_float_token(tokens, 4u);
            const auto width = parse_float_token(tokens, 5u);
            const auto height = parse_float_token(tokens, 6u);
            if (!source_index.has_value() || !x.has_value() || !y.has_value() ||
                !width.has_value() || !height.has_value()) {
                continue;
            }
            // A backdrop pinned to the top-left corner is the skin telling us its
            // canvas size. Every keyframe counts, including ones whose art is
            // missing or hidden, but only corner-anchored full-size panels: layers
            // parked off-screen for a slide-in are the wrong shape for this.
            if (std::abs(*x) <= kLr2CanvasOriginTolerance &&
                std::abs(*y) <= kLr2CanvasOriginTolerance &&
                std::abs(*width) >= kLr2MinCanvasWidth && std::abs(*height) >= kLr2MinCanvasHeight) {
                state.canvas_width_hint = std::max(state.canvas_width_hint, std::abs(*width));
                state.canvas_height_hint = std::max(state.canvas_height_hint, std::abs(*height));
            }
            if (!open_group_gates_read) {
                // Timer 0 is the scene timer, which runs for the whole song. Any other
                // timer is an event cue such as the full-combo burst or the shutter.
                const bool scene_timer = parse_int_token(tokens, 17u).value_or(0) == 0;
                // A positive option shows the layer while that option is set and a
                // negative one while it is clear, matching #IF.
                auto option_holds = [&](std::size_t token_index) {
                    const int op = parse_int_token(tokens, token_index).value_or(0);
                    if (op == 0) {
                        return true;
                    }
                    const bool active = state.active_options.find(std::abs(op)) != state.active_options.end();
                    return op > 0 ? active : !active;
                };
                open_group_always_visible = scene_timer && option_holds(18u) &&
                                            option_holds(19u) && option_holds(20u);
                open_group_gates_read = true;
            }
            const auto source_it = state.static_image_slices.find(*source_index);
            if (source_it == state.static_image_slices.end() || !source_it->second.valid) {
                continue;
            }
            // A run that ends fully transparent is a fade-out, not gear art.
            const auto alpha = parse_int_token(tokens, 8u);
            if (alpha.has_value() && *alpha <= 0) {
                if (open_gear_layer < state.gear_layers.size() && open_gear_layer_source == *source_index) {
                    state.gear_layers[open_gear_layer].destination.valid = false;
                }
                continue;
            }
            // LR2 encodes flips as negative extents. Normalise to a plain rectangle
            // so the layer can be compared against the lane block.
            Lr2DestinationNote destination{*x, *y, *width, *height, true};
            if (destination.width < 0.0f) {
                destination.x += destination.width;
                destination.width = -destination.width;
            }
            if (destination.height < 0.0f) {
                destination.y += destination.height;
                destination.height = -destination.height;
            }
            if (open_gear_layer < state.gear_layers.size() && open_gear_layer_source == *source_index) {
                state.gear_layers[open_gear_layer].destination = destination;
                continue;
            }
            open_gear_layer = state.gear_layers.size();
            open_gear_layer_source = *source_index;
            state.gear_layers.push_back(Lr2StaticImageLayer{
                source_it->second, destination, gear_file, open_group_always_visible});
            continue;
        }
        if (command == "#src_note") {
            parse_source_slice(state.note_slices);
            continue;
        }
        if (command == "#src_ln_start") {
            parse_source_slice(state.hold_head_slices);
            continue;
        }
        if (command == "#src_ln_body") {
            parse_source_slice(state.hold_body_slices);
            continue;
        }
        if (command == "#src_ln_end") {
            parse_source_slice(state.hold_tail_slices);
            continue;
        }
        if (command == "#dst_note") {
            const auto lane = parse_int_token(tokens, 1u);
            const auto x = parse_float_token(tokens, 3u);
            const auto y = parse_float_token(tokens, 4u);
            const auto width = parse_float_token(tokens, 5u);
            const auto height = parse_float_token(tokens, 6u);
            if (!lane.has_value() || !x.has_value() || !y.has_value() || !width.has_value() || !height.has_value()) {
                continue;
            }
            state.dst_notes[*lane] = Lr2DestinationNote{*x, *y, *width, *height, true};
            continue;
        }
    }

    return true;
}

fs::path normalized_absolute_path(const fs::path& path) {
    std::error_code ec;
    fs::path normalized = fs::weakly_canonical(path, ec);
    if (!ec && !normalized.empty()) {
        return normalized;
    }
    ec.clear();
    normalized = fs::absolute(path, ec);
    return (!ec && !normalized.empty()) ? normalized.lexically_normal() : path.lexically_normal();
}

bool path_component_equal(const fs::path& lhs, const fs::path& rhs) {
#ifdef _WIN32
    return to_lower_ascii(lhs.u8string()) == to_lower_ascii(rhs.u8string());
#else
    return lhs == rhs;
#endif
}

bool path_is_same_or_child(const fs::path& child, const fs::path& parent) {
    const fs::path normalized_child = normalized_absolute_path(child);
    const fs::path normalized_parent = normalized_absolute_path(parent);
    auto child_it = normalized_child.begin();
    auto parent_it = normalized_parent.begin();
    for (; parent_it != normalized_parent.end(); ++parent_it, ++child_it) {
        if (child_it == normalized_child.end() || !path_component_equal(*child_it, *parent_it)) {
            return false;
        }
    }
    return true;
}

bool paths_equivalent_or_equal(const fs::path& lhs, const fs::path& rhs) {
    std::error_code ec;
    if (fs::equivalent(lhs, rhs, ec) && !ec) {
        return true;
    }
    return path_is_same_or_child(lhs, rhs) && path_is_same_or_child(rhs, lhs);
}

std::string portable_lr2_skin_name(std::string value) {
    value = util::sanitize_ui_text(value);
    if (value.empty()) {
        value = "Imported Skin";
    }
    for (char& ch : value) {
        switch (ch) {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                ch = '_';
                break;
            default:
                break;
        }
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) {
        value.pop_back();
    }
    if (value.empty() || value == "." || value == "..") {
        value = "Imported Skin";
    }

    const std::string lower = to_lower_ascii(value.substr(0, value.find('.')));
    const bool reserved = lower == "con" || lower == "prn" || lower == "aux" || lower == "nul" ||
                          (lower.size() == 4u &&
                           ((lower.rfind("com", 0) == 0 || lower.rfind("lpt", 0) == 0) &&
                            lower[3] >= '1' && lower[3] <= '9'));
    if (reserved) {
        value += '_';
    }
    return value;
}

std::optional<fs::path> standard_lr2_theme_root(const fs::path& source) {
    std::error_code ec;
    if (to_lower_ascii(source.filename().u8string()) == "theme" && fs::is_directory(source, ec)) {
        return source;
    }
    ec.clear();
    if (to_lower_ascii(source.filename().u8string()) == "lr2files") {
        const fs::path theme = source / "Theme";
        if (fs::is_directory(theme, ec)) {
            return theme;
        }
    }
    ec.clear();
    const fs::path nested_theme = source / "LR2files" / "Theme";
    if (fs::is_directory(nested_theme, ec)) {
        return nested_theme;
    }
    return std::nullopt;
}

bool excluded_lr2_theme_name(const fs::path& path) {
    return to_lower_ascii(path.filename().u8string()) == "iidx";
}

bool lr2_text_references_excluded_iidx(const fs::path& path) {
    const std::string extension = to_lower_ascii(path.extension().u8string());
    if (extension != ".lr2skin" && extension != ".csv") {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::string line;
    constexpr std::string_view kPrefix = "lr2files/theme/iidx";
    while (std::getline(file, line)) {
        line = to_lower_ascii(normalize_lr2_token(std::move(line)));
        const std::size_t match = line.find(kPrefix);
        if (match == std::string::npos) {
            continue;
        }
        const std::size_t boundary = match + kPrefix.size();
        if (boundary == line.size() || line[boundary] == '/' || line[boundary] == ',' ||
            std::isspace(static_cast<unsigned char>(line[boundary])) != 0) {
            return true;
        }
    }
    return false;
}

bool lr2_theme_depends_on_excluded_iidx(const fs::path& path) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && lr2_text_references_excluded_iidx(it->path())) {
            return true;
        }
        ec.clear();
    }
    return false;
}

std::vector<fs::path> collect_child_lr2_skins(const fs::path& root,
                                              std::vector<std::string>& warnings) {
    std::vector<fs::path> candidates;
    std::error_code ec;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec) || !has_lr2_skin_assets(it->path())) {
            ec.clear();
            continue;
        }
        if (excluded_lr2_theme_name(it->path())) {
            warnings.push_back("Skipped excluded LR2 theme: " + it->path().filename().u8string());
        } else if (lr2_theme_depends_on_excluded_iidx(it->path())) {
            warnings.push_back("Skipped LR2 theme that depends on excluded IIDX assets: " +
                               it->path().filename().u8string());
        } else {
            candidates.push_back(it->path());
        }
        ec.clear();
    }
    std::sort(candidates.begin(), candidates.end(), [](const fs::path& lhs, const fs::path& rhs) {
        return to_lower_ascii(lhs.filename().u8string()) < to_lower_ascii(rhs.filename().u8string());
    });
    return candidates;
}

std::vector<fs::path> collect_lr2_import_candidates(const fs::path& source,
                                                    const fs::path& destination_root,
                                                    std::vector<std::string>& warnings) {
    if (paths_equivalent_or_equal(source, destination_root)) {
        return collect_child_lr2_skins(source, warnings);
    }
    if (const auto theme_root = standard_lr2_theme_root(source); theme_root.has_value()) {
        return collect_child_lr2_skins(*theme_root, warnings);
    }
    if (excluded_lr2_theme_name(source)) {
        warnings.push_back("Skipped excluded LR2 theme: " + source.filename().u8string());
        return {};
    }
    if (has_lr2_skin_assets(source)) {
        if (lr2_theme_depends_on_excluded_iidx(source)) {
            warnings.push_back("Skipped LR2 theme that depends on excluded IIDX assets: " +
                               source.filename().u8string());
            return {};
        }
        return {source};
    }
    return {};
}

fs::path next_lr2_import_destination(const fs::path& root, std::string_view requested_name) {
    const std::string base_name = portable_lr2_skin_name(std::string(requested_name));
    fs::path candidate = root / util::path_from_utf8_lossy(base_name);
    std::error_code ec;
    int suffix = 2;
    while (fs::exists(candidate, ec) && !ec) {
        candidate = root / util::path_from_utf8_lossy(base_name + " (" + std::to_string(suffix) + ")");
        ++suffix;
    }
    return ec ? fs::path{} : candidate;
}

bool safe_relative_import_path(const fs::path& relative) {
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const auto& part : relative) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

bool copy_lr2_skin_portable(const fs::path& source,
                            const fs::path& destination,
                            Lr2SkinImportResult& result) {
    std::error_code ec;
    if (!fs::create_directory(destination, ec) || ec) {
        result.warnings.push_back("Failed to create LR2 import folder: " + destination.u8string());
        return false;
    }

    const auto fail_and_cleanup = [&](std::string message) {
        result.warnings.push_back(std::move(message));
        std::error_code cleanup_ec;
        fs::remove_all(destination, cleanup_ec);
        if (cleanup_ec) {
            result.warnings.push_back("Failed to remove partial LR2 import: " + destination.u8string());
        }
        return false;
    };

    std::size_t copied_files = 0;
    std::uintmax_t copied_bytes = 0;
    fs::recursive_directory_iterator it(source, fs::directory_options::none, ec), end;
    if (ec) {
        return fail_and_cleanup("Failed to enumerate LR2 skin: " + source.u8string());
    }
    while (it != end) {
        const fs::path entry_path = it->path();
        const fs::file_status status = it->symlink_status(ec);
        if (ec) {
            return fail_and_cleanup("Failed to inspect LR2 skin entry: " + entry_path.u8string());
        }
        const fs::path relative = entry_path.lexically_relative(source);
        if (!safe_relative_import_path(relative)) {
            return fail_and_cleanup("Unsafe LR2 skin entry path: " + entry_path.u8string());
        }
        const fs::path target = destination / relative;

        if (fs::is_symlink(status)) {
            if (fs::is_directory(entry_path, ec)) {
                it.disable_recursion_pending();
            }
            ec.clear();
            result.warnings.push_back("Skipped non-portable LR2 symlink: " + entry_path.u8string());
        } else if (fs::is_directory(status)) {
            fs::create_directories(target, ec);
            if (ec) {
                return fail_and_cleanup("Failed to create LR2 import subfolder: " + target.u8string());
            }
        } else if (fs::is_regular_file(status)) {
            fs::create_directories(target.parent_path(), ec);
            if (ec) {
                return fail_and_cleanup("Failed to create LR2 asset folder: " + target.parent_path().u8string());
            }
            fs::copy_file(entry_path, target, fs::copy_options::none, ec);
            if (ec) {
                return fail_and_cleanup("Failed to copy LR2 asset: " + entry_path.u8string());
            }
            ++copied_files;
            const std::uintmax_t size = fs::file_size(entry_path, ec);
            if (!ec) {
                copied_bytes += size;
            }
            ec.clear();
        } else {
            result.warnings.push_back("Skipped unsupported LR2 filesystem entry: " + entry_path.u8string());
        }

        it.increment(ec);
        if (ec) {
            return fail_and_cleanup("Failed while enumerating LR2 skin: " + source.u8string());
        }
    }

    result.copied_files += copied_files;
    result.copied_bytes += copied_bytes;
    return true;
}

}  // namespace

std::string find_default_lr2_skin_test_root() {
    const std::vector<fs::path> candidates = {
        fs::current_path() / "build" / "Release" / "test-skins-lr2",
        fs::current_path() / "build-dist" / "Release" / "test-skins-lr2",
        fs::current_path() / "test-skins-lr2",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::is_directory(candidate, ec)) {
            return normalize_path_utf8(candidate);
        }
    }
    return {};
}

bool is_lr2_skin_directory(std::string_view path_utf8) {
    if (path_utf8.empty()) {
        return false;
    }
    const fs::path path = util::path_from_utf8_lossy(path_utf8);
    return has_lr2_skin_assets(path);
}

std::vector<std::string> list_lr2_skin_names(std::string_view root_utf8) {
    std::vector<std::string> names;
    if (root_utf8.empty()) {
        return names;
    }
    const fs::path root = util::path_from_utf8_lossy(root_utf8);
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        return names;
    }
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) {
            continue;
        }
        if (has_lr2_skin_assets(it->path())) {
            names.push_back(it->path().filename().u8string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

Lr2SkinImportResult import_lr2_skin_tree(std::string_view source_utf8,
                                         std::string_view destination_root_utf8) {
    Lr2SkinImportResult result;
    if (source_utf8.empty() || destination_root_utf8.empty()) {
        result.warnings.push_back("LR2 import source and destination must not be empty.");
        return result;
    }

    const fs::path source = util::path_from_utf8_lossy(source_utf8);
    const fs::path destination_root = util::path_from_utf8_lossy(destination_root_utf8);
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        result.warnings.push_back("LR2 import source is not a directory: " + source.u8string());
        return result;
    }
    ec.clear();
    fs::create_directories(destination_root, ec);
    if (ec) {
        result.warnings.push_back("Failed to create LR2 import root: " + destination_root.u8string());
        return result;
    }

    const auto candidates = collect_lr2_import_candidates(source, destination_root, result.warnings);
    if (candidates.empty()) {
        result.warnings.push_back("No supported non-IIDX LR2 playskin folders were found below: " + source.u8string());
        return result;
    }

    for (const auto& candidate : candidates) {
        const fs::path normalized_candidate = normalized_absolute_path(candidate);
        const fs::path normalized_destination_root = normalized_absolute_path(destination_root);
        if (paths_equivalent_or_equal(normalized_candidate.parent_path(), normalized_destination_root)) {
            result.skin_names.push_back(normalized_candidate.filename().u8string());
            continue;
        }
        if (path_is_same_or_child(normalized_destination_root, normalized_candidate)) {
            result.warnings.push_back("Refused recursive LR2 import into its own source: " + candidate.u8string());
            continue;
        }

        const fs::path destination = next_lr2_import_destination(
            destination_root, candidate.filename().u8string());
        if (destination.empty()) {
            result.warnings.push_back("Failed to choose LR2 import destination for: " + candidate.u8string());
            continue;
        }
        if (!copy_lr2_skin_portable(candidate, destination, result)) {
            continue;
        }
        result.skin_names.push_back(destination.filename().u8string());
    }

    return result;
}

namespace {

std::vector<int> collect_lr2_lane_order(const Lr2ParseState& state) {
    std::set<int> indices;
    const auto collect = [&](const auto& source) {
        for (const auto& [index, value] : source) {
            if (!value.valid) {
                continue;
            }
            indices.insert(index);
        }
    };
    collect(state.note_slices);
    collect(state.hold_head_slices);
    collect(state.hold_body_slices);
    collect(state.hold_tail_slices);
    collect(state.dst_notes);

    std::vector<int> order(indices.begin(), indices.end());
    std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        const auto lhs_it = state.dst_notes.find(lhs);
        const auto rhs_it = state.dst_notes.find(rhs);
        const bool lhs_valid = lhs_it != state.dst_notes.end() && lhs_it->second.valid;
        const bool rhs_valid = rhs_it != state.dst_notes.end() && rhs_it->second.valid;
        if (lhs_valid && rhs_valid) {
            if (lhs_it->second.x != rhs_it->second.x) {
                return lhs_it->second.x < rhs_it->second.x;
            }
            if (lhs_it->second.y != rhs_it->second.y) {
                return lhs_it->second.y < rhs_it->second.y;
            }
        } else if (lhs_valid != rhs_valid) {
            return lhs_valid;
        }
        return lhs < rhs;
    });
    return order;
}

Lr2RawPlayLayout collect_lr2_raw_play_layout(const Lr2ParseState& state) {
    Lr2RawPlayLayout layout;
    layout.lane_order = collect_lr2_lane_order(state);
    layout.ordered_destinations.reserve(state.dst_notes.size());

    float width_sum = 0.0f;
    float height_sum = 0.0f;
    for (const auto& [lane, dst] : state.dst_notes) {
        if (!dst.valid) {
            continue;
        }
        layout.has_destinations = true;
        layout.max_x = (std::max)(layout.max_x, dst.x);
        layout.ordered_destinations.push_back({lane, dst});
        if (dst.width > 0.0f && dst.height > 0.0f) {
            width_sum += dst.width;
            height_sum += dst.height;
            ++layout.size_sample_count;
        }
    }

    std::sort(layout.ordered_destinations.begin(),
              layout.ordered_destinations.end(),
              [](const Lr2OrderedDestination& lhs, const Lr2OrderedDestination& rhs) {
                  if (lhs.dst.x != rhs.dst.x) {
                      return lhs.dst.x < rhs.dst.x;
                  }
                  if (lhs.dst.y != rhs.dst.y) {
                      return lhs.dst.y < rhs.dst.y;
                  }
                  return lhs.lane < rhs.lane;
              });

    if (layout.size_sample_count > 0) {
        layout.average_width = width_sum / static_cast<float>(layout.size_sample_count);
        layout.average_height = height_sum / static_cast<float>(layout.size_sample_count);
    }
    return layout;
}

Lr2ResolutionFamily detect_lr2_lane_resolution_family(const Lr2RawPlayLayout& layout) {
    if (!layout.has_destinations) {
        return Lr2ResolutionFamily::Sd;
    }
    if (layout.max_x <= 960.0f) {
        return Lr2ResolutionFamily::Sd;
    }
    if (layout.max_x <= 1600.0f) {
        return Lr2ResolutionFamily::Hd;
    }
    return Lr2ResolutionFamily::Fhd;
}

Lr2ResolutionFamily detect_lr2_canvas_resolution_family(float canvas_width, float canvas_height) {
    // Midpoints between the three canvases LR2 supports: 640x480, 1280x720, 1920x1080.
    const auto axis_family = [](float value, float sd_to_hd, float hd_to_fhd) {
        if (value <= sd_to_hd) {
            return Lr2ResolutionFamily::Sd;
        }
        return value <= hd_to_fhd ? Lr2ResolutionFamily::Hd : Lr2ResolutionFamily::Fhd;
    };
    return std::max(axis_family(canvas_width, 960.0f, 1600.0f),
                    axis_family(canvas_height, 600.0f, 900.0f));
}

Lr2ResolutionFamily detect_lr2_auto_resolution_family(const Lr2ParseState& state,
                                                      const Lr2RawPlayLayout& layout) {
    // Lane positions alone under-report: a 1280x720 skin whose lanes sit left of
    // x=960 reads as SD and its note art then imports at twice the intended size.
    // The backdrop the skin draws behind the playfield is the reliable signal, so
    // take whichever of the two reads higher.
    const Lr2ResolutionFamily lane_family = detect_lr2_lane_resolution_family(layout);
    if (state.canvas_width_hint <= 0.0f || state.canvas_height_hint <= 0.0f) {
        return lane_family;
    }
    return std::max(lane_family,
                    detect_lr2_canvas_resolution_family(state.canvas_width_hint,
                                                        state.canvas_height_hint));
}

Lr2ResolutionFamily resolve_lr2_resolution_family(const Lr2ParseState& state,
                                                  const Lr2RawPlayLayout& layout,
                                                  std::string_view override_token,
                                                  const fs::path& skin_file) {
    const Lr2ResolutionFamily auto_family = detect_lr2_auto_resolution_family(state, layout);
    if (const auto override_family = parse_lr2_resolution_override_token(override_token); override_family.has_value()) {
        return *override_family;
    }
    if (state.explicit_resolution_family.has_value()) {
        if (*state.explicit_resolution_family != auto_family) {
            std::cerr << "[warn] LR2 playskin resolution mismatch for " << skin_file.u8string()
                      << ": explicit=" << lr2_resolution_family_label(*state.explicit_resolution_family)
                      << " auto=" << lr2_resolution_family_label(auto_family)
                      << ". Using explicit resolution." << std::endl;
        }
        return *state.explicit_resolution_family;
    }
    return auto_family;
}

void resize_lane_assets(std::vector<ImportedSkinImageAsset>& assets, int lane_count) {
    if (lane_count <= 0) {
        assets.clear();
        return;
    }
    assets.resize(static_cast<std::size_t>(lane_count));
}

struct Lr2LaneBlock {
    float left = 0.0f;
    float right = 0.0f;
    float judgement_y = 0.0f;
    bool valid = false;

    [[nodiscard]] float width() const { return right - left; }
};

Lr2LaneBlock collect_lr2_lane_block(const Lr2RawPlayLayout& layout) {
    Lr2LaneBlock block;
    for (const auto& entry : layout.ordered_destinations) {
        if (!entry.dst.valid || entry.dst.width <= 0.0f) {
            continue;
        }
        const float lane_right = entry.dst.x + entry.dst.width;
        if (!block.valid) {
            block.left = entry.dst.x;
            block.right = lane_right;
            block.judgement_y = entry.dst.y;
            block.valid = true;
            continue;
        }
        block.left = std::min(block.left, entry.dst.x);
        block.right = std::max(block.right, lane_right);
        // Lanes share one judgement line; take the lowest so a decorative lane
        // parked higher up cannot drag the anchor off the play line.
        block.judgement_y = std::max(block.judgement_y, entry.dst.y);
    }
    if (block.valid && block.width() <= 0.0f) {
        block.valid = false;
    }
    return block;
}

void populate_gear_key_assets(Lr2PlaySkinDefinition& definition,
                              const Lr2ParseState& state,
                              const Lr2RawPlayLayout& layout) {
    const Lr2LaneBlock lane_block = collect_lr2_lane_block(layout);
    const Lr2StaticImageLayer* best_layer = nullptr;
    int best_coverage = 0;
    bool best_from_gear_file = false;
    float best_width_error = 0.0f;
    float best_height = 0.0f;
    for (const auto& layer : state.gear_layers) {
        if (!layer.source.valid || !layer.destination.valid || !layer.always_visible ||
            layer.source.divisions > 1 ||
            layer.destination.width <= 0.0f || layer.destination.height <= 0.0f) {
            continue;
        }
        const auto image_it = state.image_paths.find(layer.source.group_index);
        if (image_it == state.image_paths.end() || image_it->second.empty()) {
            continue;
        }
        const float right = layer.destination.x + layer.destination.width;
        const float bottom = layer.destination.y + layer.destination.height;
        int coverage = 0;
        for (int raw_lane : layout.lane_order) {
            const auto dst_it = state.dst_notes.find(raw_lane);
            if (dst_it == state.dst_notes.end() || !dst_it->second.valid) {
                continue;
            }
            const auto& dst = dst_it->second;
            const float center_x = dst.x + dst.width * 0.5f;
            if (center_x >= layer.destination.x && center_x <= right &&
                dst.y >= layer.destination.y && dst.y < bottom) {
                ++coverage;
            }
        }
        if (coverage <= 0) {
            continue;
        }
        // Gear art authored in a Gear file always wins. Otherwise take the layer
        // covering the most lanes, then the one framing them most tightly, then the
        // tallest — that is the lane panel rather than a stripe or a wall of scenery.
        const float width_error =
            lane_block.valid ? std::abs(layer.destination.width - lane_block.width()) : 0.0f;
        const bool ranks_higher =
            coverage > best_coverage ||
            (coverage == best_coverage &&
             (width_error < best_width_error ||
              (width_error == best_width_error && layer.destination.height > best_height)));
        const bool better =
            best_layer == nullptr ||
            (layer.from_gear_file != best_from_gear_file && layer.from_gear_file) ||
            (layer.from_gear_file == best_from_gear_file && ranks_higher);
        if (better) {
            best_layer = &layer;
            best_coverage = coverage;
            best_from_gear_file = layer.from_gear_file;
            best_width_error = width_error;
            best_height = layer.destination.height;
        }
    }
    if (!best_layer) {
        return;
    }

    const auto image_it = state.image_paths.find(best_layer->source.group_index);
    if (image_it == state.image_paths.end() || image_it->second.empty()) {
        return;
    }

    // Keep the complete static Gear frame as one asset. Per-lane stretching
    // distorts logos and text because LR2 Gear panels are authored as a single
    // destination rectangle with their own aspect ratio.
    const bool has_source_rect =
        best_layer->source.width > 0.0f && best_layer->source.height > 0.0f;
    definition.gear_overlay_image.path = image_it->second;
    definition.gear_overlay_image.source_x = best_layer->source.x;
    definition.gear_overlay_image.source_y = best_layer->source.y;
    definition.gear_overlay_image.source_width = best_layer->source.width;
    definition.gear_overlay_image.source_height = best_layer->source.height;
    definition.gear_overlay_image.has_source_rect = has_source_rect;

    const float dst_left = best_layer->destination.x;
    const float dst_top = best_layer->destination.y;
    const float dst_right = dst_left + best_layer->destination.width;
    const float dst_bottom = dst_top + best_layer->destination.height;

    // Record the panel in lane-block units so the renderer can reproduce the
    // authored offset from the lanes instead of guessing a bottom-anchored fit.
    if (lane_block.valid) {
        const float unit = lane_block.width();
        definition.gear_placement.offset_x = (dst_left - lane_block.left) / unit;
        definition.gear_placement.offset_y = (dst_top - lane_block.judgement_y) / unit;
        definition.gear_placement.width = best_layer->destination.width / unit;
        definition.gear_placement.height = best_layer->destination.height / unit;
        definition.gear_placement.valid = true;
    }

    resize_lane_assets(definition.key_images, definition.keys);
    resize_lane_assets(definition.key_pressed_images, definition.keys);
    bool populated = false;
    // Slicing per-lane receptor art out of the panel only makes sense when the
    // panel actually reaches below the judgement line. A lane backdrop that stops
    // at the line would otherwise yield a few-pixel smear.
    const bool panel_covers_receptors =
        has_source_rect && lane_block.valid &&
        (dst_bottom - lane_block.judgement_y) >= lane_block.width() * 0.1f;
    if (!panel_covers_receptors) {
        return;
    }
    for (int lane = 0; lane < definition.keys; ++lane) {
        const int raw_lane = layout.lane_order[static_cast<std::size_t>(lane)];
        const auto dst_it = state.dst_notes.find(raw_lane);
        if (dst_it == state.dst_notes.end() || !dst_it->second.valid) {
            continue;
        }
        const auto& lane_dst = dst_it->second;
        const float lane_left = std::max(dst_left, lane_dst.x);
        const float lane_right = std::min(dst_right, lane_dst.x + lane_dst.width);
        const float gear_top = std::clamp(lane_dst.y, dst_top, dst_bottom);
        if (lane_right <= lane_left || dst_bottom <= gear_top) {
            continue;
        }

        ImportedSkinImageAsset asset;
        asset.path = image_it->second;
        asset.source_x = best_layer->source.x +
                         ((lane_left - dst_left) / best_layer->destination.width) * best_layer->source.width;
        asset.source_y = best_layer->source.y +
                         ((gear_top - dst_top) / best_layer->destination.height) * best_layer->source.height;
        asset.source_width = ((lane_right - lane_left) / best_layer->destination.width) * best_layer->source.width;
        asset.source_height = ((dst_bottom - gear_top) / best_layer->destination.height) * best_layer->source.height;
        asset.has_source_rect = asset.source_width > 0.0f && asset.source_height > 0.0f;
        if (!asset.has_source_rect) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(lane);
        definition.key_images[index] = asset;
        definition.key_pressed_images[index] = asset;
        populated = true;
    }
    definition.use_full_lane_receptor_layout = populated;
}

void finalize_key_fallbacks(Lr2PlaySkinDefinition& definition) {
    resize_lane_assets(definition.note_images, definition.keys);
    resize_lane_assets(definition.hold_head_images, definition.keys);
    resize_lane_assets(definition.hold_body_images, definition.keys);
    resize_lane_assets(definition.hold_tail_images, definition.keys);
    resize_lane_assets(definition.key_images, definition.keys);
    resize_lane_assets(definition.key_pressed_images, definition.keys);
    for (int lane = 0; lane < definition.keys; ++lane) {
        const std::size_t index = static_cast<std::size_t>(lane);
        if (definition.key_pressed_images[index].path.empty() &&
            !definition.key_images[index].path.empty()) {
            definition.key_pressed_images[index] = definition.key_images[index];
        }
    }
}

Lr2NormalizedPlayMetrics normalize_lr2_play_metrics(const Lr2RawPlayLayout& layout,
                                                    Lr2ResolutionFamily family) {
    Lr2NormalizedPlayMetrics metrics;
    metrics.keys = static_cast<int>(layout.lane_order.size());
    metrics.resolution_family = family;
    if (metrics.keys <= 0) {
        return metrics;
    }

    const auto [x_scale, y_scale] = lr2_resolution_family_scale(family);
    if (layout.size_sample_count > 0) {
        metrics.imported_note_width_ratio =
            std::clamp((layout.average_width / x_scale) / kDefaultLr2NoteWidth, 0.25f, 4.0f);
        metrics.imported_note_height_ratio =
            std::clamp((layout.average_height / y_scale) / kDefaultLr2NoteHeight, 0.25f, 4.0f);
    }

    if (layout.ordered_destinations.size() >= 2u) {
        metrics.lane_divider_widths.reserve(layout.ordered_destinations.size() - 1u);
        for (std::size_t i = 1; i < layout.ordered_destinations.size(); ++i) {
            const auto& lhs = layout.ordered_destinations[i - 1u].dst;
            const auto& rhs = layout.ordered_destinations[i].dst;
            const float divider = std::max(0.0f, (rhs.x - (lhs.x + lhs.width)) / x_scale);
            metrics.lane_divider_widths.push_back(divider);
        }
    }
    return metrics;
}

Lr2PlaySkinDefinition build_lr2_definition(const Lr2ParseState& state,
                                           const Lr2RawPlayLayout& layout,
                                           const Lr2NormalizedPlayMetrics& metrics) {
    Lr2PlaySkinDefinition definition;
    definition.keys = metrics.keys;
    if (definition.keys <= 0) {
        return definition;
    }

    definition.found = true;
    definition.resolution_family = metrics.resolution_family;
    definition.imported_note_width_ratio = metrics.imported_note_width_ratio;
    definition.imported_note_height_ratio = metrics.imported_note_height_ratio;
    definition.lane_divider_widths = metrics.lane_divider_widths;
    definition.note_images.resize(static_cast<std::size_t>(definition.keys));
    definition.hold_head_images.resize(static_cast<std::size_t>(definition.keys));
    definition.hold_body_images.resize(static_cast<std::size_t>(definition.keys));
    definition.hold_tail_images.resize(static_cast<std::size_t>(definition.keys));
    for (int lane = 0; lane < definition.keys; ++lane) {
        const int raw_lane = layout.lane_order[static_cast<std::size_t>(lane)];
        definition.note_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.note_slices, raw_lane);
        definition.hold_head_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.hold_head_slices, raw_lane);
        definition.hold_body_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.hold_body_slices, raw_lane);
        definition.hold_tail_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.hold_tail_slices, raw_lane);
    }

    populate_gear_key_assets(definition, state, layout);
    finalize_key_fallbacks(definition);
    return definition;
}

std::vector<fs::path> collect_lr2_skin_files(const fs::path& dir) {
    std::vector<fs::path> files;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return files;
    }
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        if (to_lower_ascii(it->path().extension().u8string()) == ".lr2skin") {
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end(), [](const fs::path& lhs, const fs::path& rhs) {
        return to_lower_ascii(lhs.u8string()) < to_lower_ascii(rhs.u8string());
    });
    return files;
}

int preferred_lr2_file_key_hint(int keys) {
    switch (keys) {
        case 6:
            return 5;
        case 8:
            return 7;
        default:
            return keys;
    }
}

int score_lr2_skin_file_hint(const fs::path& skin_file, int requested_keys) {
    const int hinted_keys = preferred_lr2_file_key_hint(requested_keys);
    if (hinted_keys <= 0) {
        return 0;
    }

    const std::string lower = to_lower_ascii(skin_file.stem().u8string());
    const std::string hinted = std::to_string(hinted_keys);
    if (lower == "play_" + hinted || lower.find("play_" + hinted) != std::string::npos) {
        return 250;
    }
    if (lower.find(hinted + "key") != std::string::npos ||
        lower.find("_" + hinted) != std::string::npos) {
        return 100;
    }
    return 0;
}

Lr2PlaySkinDefinition trim_or_expand_definition(Lr2PlaySkinDefinition definition, int keys) {
    if (keys <= 0 || !definition.found || definition.keys <= 0) {
        return definition;
    }
    definition.keys = keys;
    resize_lane_assets(definition.note_images, keys);
    resize_lane_assets(definition.hold_head_images, keys);
    resize_lane_assets(definition.hold_body_images, keys);
    resize_lane_assets(definition.hold_tail_images, keys);
    resize_lane_assets(definition.key_images, keys);
    resize_lane_assets(definition.key_pressed_images, keys);
    if (keys > 1) {
        definition.lane_divider_widths.resize(static_cast<std::size_t>(keys - 1), 0.0f);
    } else {
        definition.lane_divider_widths.clear();
    }
    finalize_key_fallbacks(definition);
    return definition;
}

}  // namespace

Lr2PlaySkinDefinition resolve_lr2_play_skin(std::string_view root_utf8,
                                            std::string_view skin_name,
                                            int keys,
                                            std::string_view resolution_override) {
    Lr2PlaySkinDefinition best;
    if (root_utf8.empty() || skin_name.empty()) {
        return best;
    }

    const fs::path root = util::path_from_utf8_lossy(root_utf8);
    const fs::path skin_dir = root / util::path_from_utf8_lossy(std::string(skin_name));
    std::error_code ec;
    if (!fs::is_directory(skin_dir, ec)) {
        return best;
    }

    int best_score = -1000000;
    for (const auto& skin_file : collect_lr2_skin_files(skin_dir)) {
        Lr2ParseState state;
        state.skin_dir = skin_dir;
        if (!parse_lr2_file(skin_file, state, true)) {
            continue;
        }
        const Lr2RawPlayLayout raw_layout = collect_lr2_raw_play_layout(state);
        if (raw_layout.lane_order.empty()) {
            continue;
        }
        const Lr2ResolutionFamily resolution_family =
            resolve_lr2_resolution_family(state, raw_layout, resolution_override, skin_file);
        const Lr2NormalizedPlayMetrics metrics = normalize_lr2_play_metrics(raw_layout, resolution_family);
        Lr2PlaySkinDefinition candidate = build_lr2_definition(state, raw_layout, metrics);
        if (!candidate.found || candidate.keys <= 0) {
            continue;
        }
        const int delta = (keys > 0) ? std::abs(candidate.keys - keys) : 0;
        const int exact_bonus = (keys > 0 && candidate.keys == keys) ? 1000 : 0;
        const int asset_bonus = static_cast<int>(candidate.note_images.size()) * 10 +
                                static_cast<int>(candidate.hold_body_images.size());
        const int file_hint_bonus = score_lr2_skin_file_hint(skin_file, keys);
        Lr2CandidateInfo info;
        info.skin_file = skin_file;
        info.requested_keys = keys;
        info.parsed_keys = candidate.keys;
        info.file_hint_bonus = file_hint_bonus;
        info.resolution_family = candidate.resolution_family;
        info.score = exact_bonus - delta * 25 + asset_bonus + file_hint_bonus;
        const int score = info.score;
        if (score > best_score) {
            best_score = score;
            best = std::move(candidate);
        }
    }

    if (!best.found) {
        return best;
    }
    return trim_or_expand_definition(std::move(best), keys > 0 ? keys : best.keys);
}

}  // namespace tenriff::app
