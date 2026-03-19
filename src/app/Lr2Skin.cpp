#include "app/Lr2Skin.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
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

struct Lr2SourceSlice {
    int group_index = -1;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
};

struct Lr2DestinationNote {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
};

struct CustomFileSelection {
    std::string wildcard_path;
    std::string default_name;
};

struct Lr2ParseState {
    fs::path skin_dir;
    std::unordered_map<int, std::string> image_paths;
    std::unordered_map<int, Lr2SourceSlice> note_slices;
    std::unordered_map<int, Lr2SourceSlice> hold_head_slices;
    std::unordered_map<int, Lr2SourceSlice> hold_body_slices;
    std::unordered_map<int, Lr2SourceSlice> hold_tail_slices;
    std::unordered_map<int, Lr2DestinationNote> dst_notes;
    std::unordered_map<std::string, CustomFileSelection> custom_files;
    std::set<int> active_options;
    std::set<std::string> visited_files;
};

struct ConditionalFrame {
    bool parent_active = true;
    bool branch_taken = false;
    bool current_active = true;
};

fs::path relativize_theme_path(const fs::path& skin_dir, const fs::path& candidate);

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

bool parse_lr2_file(const fs::path& file_path, Lr2ParseState& state) {
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
    int next_image_index = 0;
    for (const auto& [index, _] : state.image_paths) {
        next_image_index = (std::max)(next_image_index, index + 1);
    }

    auto current_active = [&]() {
        return conditionals.empty() ? true : conditionals.back().current_active;
    };

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
            if (const auto include_path =
                    resolve_existing_lr2_path(state.skin_dir, current_dir, tokens[1u]);
                include_path.has_value()) {
                parse_lr2_file(*include_path, state);
            }
            continue;
        }
        if (command == "#image") {
            if (tokens.size() < 2u) {
                continue;
            }
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
                state.image_paths[next_image_index++] = resolved_path;
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
            destination[*lane] = Lr2SourceSlice{*group_index, *x, *y, *width, *height, true};
        };

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

void resize_lane_assets(std::vector<ImportedSkinImageAsset>& assets, int lane_count) {
    if (lane_count <= 0) {
        assets.clear();
        return;
    }
    assets.resize(static_cast<std::size_t>(lane_count));
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
        if (definition.key_images[index].path.empty()) {
            definition.key_images[index] = definition.note_images[index];
        }
        if (definition.key_pressed_images[index].path.empty()) {
            definition.key_pressed_images[index] =
                !definition.hold_head_images[index].path.empty() ? definition.hold_head_images[index]
                                                                  : definition.note_images[index];
        }
    }
}

Lr2PlaySkinDefinition build_lr2_definition(const Lr2ParseState& state) {
    Lr2PlaySkinDefinition definition;
    const std::vector<int> lane_order = collect_lr2_lane_order(state);
    definition.keys = static_cast<int>(lane_order.size());
    if (definition.keys <= 0) {
        return definition;
    }

    definition.found = true;
    definition.note_images.resize(static_cast<std::size_t>(definition.keys));
    definition.hold_head_images.resize(static_cast<std::size_t>(definition.keys));
    definition.hold_body_images.resize(static_cast<std::size_t>(definition.keys));
    definition.hold_tail_images.resize(static_cast<std::size_t>(definition.keys));
    for (int lane = 0; lane < definition.keys; ++lane) {
        const int raw_lane = lane_order[static_cast<std::size_t>(lane)];
        definition.note_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.note_slices, raw_lane);
        definition.hold_head_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.hold_head_slices, raw_lane);
        definition.hold_body_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.hold_body_slices, raw_lane);
        definition.hold_tail_images[static_cast<std::size_t>(lane)] =
            make_asset_from_slice(state.image_paths, state.hold_tail_slices, raw_lane);
    }

    float width_sum = 0.0f;
    float height_sum = 0.0f;
    int width_count = 0;
    std::vector<std::pair<int, Lr2DestinationNote>> ordered_dst;
    ordered_dst.reserve(state.dst_notes.size());
    for (const auto& [index, dst] : state.dst_notes) {
        if (!dst.valid) {
            continue;
        }
        ordered_dst.emplace_back(index, dst);
        if (dst.width > 0.0f) {
            width_sum += dst.width;
            ++width_count;
        }
        if (dst.height > 0.0f) {
            height_sum += dst.height;
        }
    }
    std::sort(ordered_dst.begin(), ordered_dst.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second.x != rhs.second.x) {
            return lhs.second.x < rhs.second.x;
        }
        if (lhs.second.y != rhs.second.y) {
            return lhs.second.y < rhs.second.y;
        }
        return lhs.first < rhs.first;
    });

    if (width_count > 0) {
        const float average_width = width_sum / static_cast<float>(width_count);
        definition.imported_note_width_ratio =
            std::clamp(average_width / kDefaultLr2NoteWidth, 0.25f, 4.0f);
        definition.imported_note_height_ratio =
            std::clamp((height_sum / static_cast<float>(width_count)) / kDefaultLr2NoteHeight, 0.25f, 4.0f);
    }

    if (ordered_dst.size() >= 2u) {
        definition.lane_divider_widths.reserve(ordered_dst.size() - 1u);
        for (std::size_t i = 1; i < ordered_dst.size(); ++i) {
            const auto& lhs = ordered_dst[i - 1u].second;
            const auto& rhs = ordered_dst[i].second;
            const float divider = std::max(0.0f, rhs.x - (lhs.x + lhs.width));
            definition.lane_divider_widths.push_back(divider);
        }
    }

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
                                            int keys) {
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
        if (!parse_lr2_file(skin_file, state)) {
            continue;
        }
        Lr2PlaySkinDefinition candidate = build_lr2_definition(state);
        if (!candidate.found || candidate.keys <= 0) {
            continue;
        }
        const int delta = (keys > 0) ? std::abs(candidate.keys - keys) : 0;
        const int exact_bonus = (keys > 0 && candidate.keys == keys) ? 1000 : 0;
        const int asset_bonus = static_cast<int>(candidate.note_images.size()) * 10 +
                                static_cast<int>(candidate.hold_body_images.size());
        const int file_hint_bonus = score_lr2_skin_file_hint(skin_file, keys);
        const int score = exact_bonus - delta * 25 + asset_bonus + file_hint_bonus;
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
