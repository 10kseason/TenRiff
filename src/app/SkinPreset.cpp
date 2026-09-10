#include "app/SkinPreset.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "app/Lr2Skin.h"
#include "app/TenRiffSkin.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {
namespace {
namespace fs = std::filesystem;
constexpr std::array<char, 8> kMagic{'T', 'R', 'S', 'K', 'I', 'N', '\r', '\n'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaxFiles = 4096;
constexpr std::uint32_t kMaxPath = 1024;
constexpr std::uint32_t kMaxSettings = 1024 * 1024;
constexpr std::uint64_t kMaxFileBytes = 128ull * 1024 * 1024;
constexpr std::uint64_t kMaxAssetBytes = 512ull * 1024 * 1024;
constexpr std::uint64_t kMaxPackageBytes = kMaxAssetBytes + kMaxSettings +
    static_cast<std::uint64_t>(kMaxFiles) * (kMaxPath + 16) + 32;

void require(bool condition, const char* error) {
    if (!condition) throw std::runtime_error(error);
}

std::string lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

// Use an identical conservative namespace on Windows and other platforms:
// no device names, ADS, rooted paths, empty components, or Win32 aliases.
bool safe_relative_name(std::string_view name) {
    if (name.empty() || name.size() > kMaxPath || name.front() == '/') return false;
    for (unsigned char c : name) {
        if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') return false;
    }
    std::size_t begin = 0;
    while (begin < name.size()) {
        const auto slash = name.find('/', begin);
        const auto part = name.substr(begin, slash == name.npos ? name.size() - begin : slash - begin);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        const std::string stem = lower(std::string(part.substr(0, part.find('.'))));
        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
            stem == "conin$" || stem == "conout$" ||
            (stem.size() == 4 && (stem.substr(0, 3) == "com" || stem.substr(0, 3) == "lpt") &&
             stem.back() >= '1' && stem.back() <= '9')) return false;
        if (slash == name.npos) break;
        begin = slash + 1;
        if (begin == name.size()) return false;
    }
    try {
        // Do not accept invalid UTF-8 names that a lossy decoder could collapse.
        const fs::path path = fs::u8path(name);
        return path.generic_u8string() == name && !path.has_root_path();
    } catch (...) { return false; }
}

bool is_link(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec) return false;
    if (fs::is_symlink(status)) return true;
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return false;
#endif
}

void reject_link_ancestors(const fs::path& path) {
    fs::path current;
    for (const auto& component : fs::absolute(path)) {
        current /= component;
        require(!is_link(current), "Skin preset paths cannot use symbolic links or junctions.");
    }
}

void ensure_within(const fs::path& root, const fs::path& child) {
    const auto relative = fs::weakly_canonical(child).lexically_relative(fs::weakly_canonical(root));
    require(safe_relative_name(relative.generic_u8string()), "Skin asset escapes its source folder.");
    reject_link_ancestors(child);
}

struct OwnedDirectory {
    fs::path path;
    fs::path parent;
    bool keep = false;
    ~OwnedDirectory() {
        if (keep || path.empty()) return;
        try {
            // Cleanup is restricted to the exact newly reserved direct child.
            if (fs::weakly_canonical(path).parent_path() == fs::weakly_canonical(parent) &&
                !is_link(path)) {
                std::error_code ec;
                fs::remove_all(path, ec);
            }
        } catch (...) {}
    }
};

fs::path reserve_directory(const fs::path& root, const std::string& requested_name) {
    reject_link_ancestors(root);
    fs::create_directories(root);
    std::string name = requested_name;
    if (!safe_relative_name(name) || name.find('/') != name.npos) name = "Imported Preset";
    for (int suffix = 1; suffix <= 10000; ++suffix) {
        const auto candidate = root / fs::u8path(name + (suffix == 1 ? "" : "-" + std::to_string(suffix)));
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) return candidate;
        require(!ec || ec == std::errc::file_exists, "Could not create skin preset destination.");
    }
    throw std::runtime_error("Could not reserve a new skin preset folder.");
}

void write_u32(std::ostream& out, std::uint32_t value) {
    for (int n = 0; n < 4; ++n) out.put(static_cast<char>((value >> (n * 8)) & 255));
}
void write_u64(std::ostream& out, std::uint64_t value) {
    write_u32(out, static_cast<std::uint32_t>(value));
    write_u32(out, static_cast<std::uint32_t>(value >> 32));
}
std::uint32_t read_u32(std::istream& in) {
    std::uint32_t result = 0;
    for (int n = 0; n < 4; ++n) {
        const int value = in.get();
        require(value != std::char_traits<char>::eof(), "Truncated skin preset.");
        result |= static_cast<std::uint32_t>(value) << (n * 8);
    }
    return result;
}
std::uint64_t read_u64(std::istream& in) {
    const auto low = read_u32(in);
    return low | (static_cast<std::uint64_t>(read_u32(in)) << 32);
}
std::string read_string(std::istream& in, std::uint32_t length, std::uint32_t limit) {
    require(length > 0 && length <= limit, "Skin preset string exceeds its size limit.");
    std::string value(length, '\0');
    in.read(value.data(), static_cast<std::streamsize>(length));
    require(in.good(), "Truncated skin preset string.");
    return value;
}

void validate_json_depth(std::string_view json) {
    int depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (char c : json) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') require(++depth <= 32, "Skin JSON nesting exceeds its limit.");
        else if (c == '}' || c == ']') --depth;
    }
}

void validate_manifest_size(const fs::path& file) {
    reject_link_ancestors(file);
    require(fs::is_regular_file(file) && fs::file_size(file) <= kMaxSettings,
            "Skin manifest is missing or exceeds its size limit.");
    std::ifstream in(file, std::ios::binary);
    const auto text = read_string(in, static_cast<std::uint32_t>(fs::file_size(file)), kMaxSettings);
    validate_json_depth(text);
}

std::uint32_t crc_update(std::uint32_t crc, const char* bytes, std::size_t count) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t value = n;
            for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
            result[n] = value;
        }
        return result;
    }();
    for (std::size_t n = 0; n < count; ++n)
        crc = table[(crc ^ static_cast<unsigned char>(bytes[n])) & 255] ^ (crc >> 8);
    return crc;
}

std::uint32_t copy_bytes(std::istream& in, std::ostream& out, std::uint64_t size) {
    std::array<char, 64 * 1024> buffer{};
    std::uint32_t crc = 0xffffffffu;
    while (size > 0) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(size, buffer.size()));
        in.read(buffer.data(), static_cast<std::streamsize>(count));
        require(in.good(), "Truncated or unreadable skin asset.");
        out.write(buffer.data(), static_cast<std::streamsize>(count));
        require(out.good(), "Could not write skin asset.");
        crc = crc_update(crc, buffer.data(), count);
        size -= count;
    }
    return crc ^ 0xffffffffu;
}

bool lr2_file_allowed(const fs::path& file) {
    const auto ext = lower(file.extension().u8string());
    static const std::set<std::string> allowed{
        ".lr2skin", ".csv", ".txt", ".png", ".jpg", ".jpeg", ".bmp", ".gif",
        ".tga", ".dds", ".webp", ".json", ".md"};
    return allowed.count(ext) != 0;
}

bool lr2_pattern_matches(std::string pattern, std::string name) {
    pattern = lower(std::move(pattern));
    name = lower(std::move(name));
    std::istringstream patterns(pattern), names(name);
    std::string p, n;
    while (std::getline(patterns, p, '/')) {
        if (!std::getline(names, n, '/')) return false;
        const auto star = p.find('*');
        if (star == p.npos) { if (p != n) return false; }
        else {
            const auto prefix = p.substr(0, star);
            const auto suffix = p.substr(star + 1);
            if (n.size() < prefix.size() + suffix.size() || n.compare(0, prefix.size(), prefix) != 0 ||
                n.compare(n.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
        }
    }
    return !std::getline(names, n, '/');
}

bool local_lr2_pattern_exists(const fs::path& root, const std::string& pattern) {
    std::size_t visited = 0;
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
        require(++visited <= kMaxFiles * 4, "LR2 pattern search exceeds its directory limit.");
        require(!is_link(it->path()), "LR2 skin contains a symbolic link or junction.");
        if (it->is_regular_file() && lr2_file_allowed(it->path()) &&
            lr2_pattern_matches(pattern, it->path().lexically_relative(root).generic_u8string())) return true;
    }
    return false;
}

// Check command paths before invoking the LR2 parser, whose legacy resolver also
// supports absolute paths. Portable files must never depend on another folder.
void validate_lr2_text(const fs::path& file, const fs::path& root, std::size_t& include_count) {
    const auto ext = lower(file.extension().u8string());
    if (ext != ".lr2skin" && ext != ".csv" && ext != ".txt") return;
    require(fs::file_size(file) <= kMaxSettings * 8ull, "LR2 text file is too large.");
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        require(line.find('\0') == line.npos &&
                    !(line.size() >= 2 &&
                      ((static_cast<unsigned char>(line[0]) == 0xff && static_cast<unsigned char>(line[1]) == 0xfe) ||
                       (static_cast<unsigned char>(line[0]) == 0xfe && static_cast<unsigned char>(line[1]) == 0xff))),
                "UTF-16 LR2 files are not supported; convert the skin text to UTF-8 first.");
        // LR2 command splitting treats // as a comment, including inline comments.
        const auto comment = line.find("//");
        if (comment != line.npos) line.resize(comment);
        std::vector<std::string> fields;
        std::size_t begin = 0;
        while (begin <= line.size()) {
            auto end = line.find_first_of(",\t", begin);
            auto field = line.substr(begin, end == line.npos ? line.size() - begin : end - begin);
            const auto first = field.find_first_not_of(" \r\n\"'");
            const auto last = field.find_last_not_of(" \r\n\"'");
            fields.push_back(first == field.npos ? "" : field.substr(first, last - first + 1));
            if (end == line.npos) break;
            begin = end + 1;
        }
        if (fields.empty()) continue;
        auto command = lower(fields[0]);
        if (command.size() >= 3 && command.compare(0, 3, "\xef\xbb\xbf") == 0) command.erase(0, 3);
        const auto index = command == "#customfile" ? 2u : 1u;
        if (command != "#image" && command != "#include" && command != "#customfile") continue;
        if (command == "#include")
            require(++include_count <= 128, "LR2 preset has too many includes for safe portable loading.");
        if (fields.size() <= index || fields[index].empty()) continue;
        auto path = fields[index];
        std::replace(path.begin(), path.end(), '\\', '/');
        // Match the existing LR2 loader's Windows UTF-8/CP932 path fallback.
        path = util::ensure_utf8_text(path);
        while (path.rfind("./", 0) == 0) path.erase(0, 2);
        const auto raw_path = path;
        // Wildcard selection is local and remains valid after moving the folder.
        std::replace(path.begin(), path.end(), '*', 'x');
        std::replace(path.begin(), path.end(), '?', 'x');
        require(!fs::u8path(path).has_root_path() && path.find(':') == path.npos,
                "LR2 preset has an absolute asset reference; make the skin self-contained first.");
        // ../ is allowed when it resolves from this command file into this skin.
        // Root-relative and LR2files/Theme/... are the legacy loader's fallbacks.
        const bool wildcard = raw_path.find('*') != raw_path.npos;
        const auto raw_components = fs::u8path(raw_path);
        if (wildcard) {
            for (const auto& component : raw_components)
                require(component != "..", "LR2 wildcard reference has an external fallback; use a local root-relative pattern.");
            require(lower(raw_path).rfind("lr2files/theme/", 0) != 0,
                    "LR2 wildcard theme reference can reach sibling skins; use a local root-relative pattern.");
        }
        std::vector<fs::path> candidates{file.parent_path() / raw_components, root / raw_components};
        const auto theme = lower(path);
        if (theme.rfind("lr2files/theme/", 0) == 0) {
            const auto slash = path.find('/', 15);
            if (slash != path.npos) candidates.push_back(root / fs::u8path(raw_path.substr(slash + 1)));
        }
        bool local = false;
        for (const auto& candidate : candidates) {
            const auto relative = candidate.lexically_normal().lexically_relative(root).generic_u8string();
            auto safe_pattern = relative;
            std::replace(safe_pattern.begin(), safe_pattern.end(), '*', 'x');
            if (safe_relative_name(safe_pattern)) {
                if (wildcard ? local_lr2_pattern_exists(root, relative) : fs::is_regular_file(candidate)) local = true;
            } else if (!wildcard && fs::is_regular_file(candidate)) {
                require(false, "LR2 preset references an asset outside its selected skin folder.");
            }
            if (local && !wildcard) break;
        }
        require(local, "LR2 preset has a missing or external asset reference.");
        if (command == "#include") {
            const auto include_extension = lower(fs::u8path(path).extension().u8string());
            require(include_extension == ".lr2skin" || include_extension == ".csv" || include_extension == ".txt",
                    "LR2 preset include has an unsupported extension.");
        }
        if (command == "#customfile" && fields.size() > 3) {
            // A default selection can be substituted into a wildcard path.
            auto selection = fields[3];
            std::replace(selection.begin(), selection.end(), '\\', '/');
            selection = util::ensure_utf8_text(selection);
            require(selection.empty() || safe_relative_name(selection), "Unsafe LR2 custom file selection.");
        }
    }
    require(!in.bad(), "Could not read LR2 skin text.");
}

std::vector<fs::path> collect_assets(const fs::path& folder, const std::string& source) {
    reject_link_ancestors(folder);
    require(fs::is_directory(folder), "The active skin folder is missing.");
    std::set<fs::path> assets;
    if (source == "tenriff") {
        validate_manifest_size(folder / "skin.json");
        for (int mode = 1; mode <= 17; ++mode) {
            const auto skin = load_tenriff_skin_folder(folder.u8string(), mode == 17 ? 8 : mode,
                                                      mode == 17 ? "7+1" : "");
            require(skin.found, "Skin preset has no valid TenRiff skin.json.");
            require(skin.warnings.empty(), "Skin manifest contains missing or invalid assets; fix its warnings before export/import.");
            for (const auto& asset : skin.referenced_asset_paths) assets.insert(fs::u8path(asset));
        }
        assets.insert(folder / "skin.json");
        for (const auto* companion : {"LICENSE", "LICENSE.txt", "README.md"})
            if (fs::is_regular_file(folder / companion)) assets.insert(folder / companion);
    } else {
        require(source == "lr2", "Unsupported skin preset source.");
        std::size_t include_count = 0;
        std::error_code ec;
        fs::recursive_directory_iterator it(folder, fs::directory_options::none, ec), end;
        require(!ec, "Could not enumerate skin assets.");
        while (it != end) {
            require(!is_link(it->path()), "LR2 skin contains a symbolic link or junction.");
            if (it->is_regular_file() && lr2_file_allowed(it->path())) {
                validate_lr2_text(it->path(), folder, include_count);
                assets.insert(it->path());
            }
            require(assets.size() <= kMaxFiles, "Skin has too many assets.");
            it.increment(ec);
            require(!ec, "Could not enumerate skin assets.");
        }
        require(is_lr2_skin_directory(folder.u8string()), "Skin preset has no LR2 playskin.");
        // The actual parser resolves wildcard/custom-file choices. Check each
        // rendered mode and resolution so a sibling theme cannot leak through.
        for (int keys = 1; keys <= 16; ++keys) {
            for (const auto* resolution : {"auto", "sd", "hd", "fhd"}) {
                const auto skin = resolve_lr2_play_skin(folder.parent_path().u8string(),
                                                         folder.filename().u8string(), keys, resolution);
                require(skin.found, "LR2 preset has no usable playskin.");
                const auto check_images = [&](const auto& images) {
                    for (const auto& image : images) if (!image.path.empty()) ensure_within(folder, fs::u8path(image.path));
                };
                check_images(skin.note_images);
                check_images(skin.hold_head_images);
                check_images(skin.hold_body_images);
                check_images(skin.hold_tail_images);
                check_images(skin.key_images);
                check_images(skin.key_pressed_images);
                if (!skin.gear_overlay_image.path.empty()) ensure_within(folder, fs::u8path(skin.gear_overlay_image.path));
            }
        }
    }
    std::uint64_t bytes = 0;
    require(assets.size() <= kMaxFiles, "Skin has too many assets.");
    for (const auto& asset : assets) {
        ensure_within(folder, asset);
        const auto size = fs::file_size(asset);
        require(size <= kMaxFileBytes && size <= kMaxAssetBytes - bytes, "Skin assets exceed the portable size limit.");
        bytes += size;
    }
    return {assets.begin(), assets.end()};
}
}  // namespace

SkinPresetResult export_skin_preset(std::string_view destination_utf8,
                                    const config::SkinConfig& settings,
                                    std::string_view active_skin_root_utf8) {
    SkinPresetResult result;
    try {
        const auto destination = fs::absolute(fs::u8path(destination_utf8));
        reject_link_ancestors(destination);
        require(!fs::exists(destination), "The preset file already exists; choose a new filename.");
        require(fs::is_directory(destination.parent_path()), "The preset destination folder does not exist.");
        auto skin = settings;
        skin.source = config::normalize_skin_source_token(skin.source);
        std::vector<fs::path> assets;
        fs::path source_folder;
        if (skin.source != "native") {
            const auto name = skin.source == "lr2" ? skin.lr2_skin_name : skin.tenriff_skin_name;
            require(safe_relative_name(name) && name.find('/') == name.npos, "Invalid active skin folder name.");
            source_folder = fs::absolute(fs::u8path(active_skin_root_utf8) / fs::u8path(name));
            assets = collect_assets(source_folder, skin.source);
        }
        // Inactive catalog choices may contain machine-specific names; omit them.
        if (skin.source != "lr2") skin.lr2_skin_name.clear();
        if (skin.source != "tenriff") skin.tenriff_skin_name.clear();
        const auto json = config::serialize_skin_config(skin);
        require(!json.empty() && json.size() <= kMaxSettings, "Skin settings exceed the preset size limit.");
        OwnedDirectory scratch;
        scratch.parent = destination.parent_path();
        scratch.path = reserve_directory(scratch.parent, ".tenriff-preset-export");
        const auto temporary = scratch.path / "preset.tmp";
        std::ofstream out(temporary, std::ios::binary);
        require(out.good(), "Could not create preset output.");
        out.write(kMagic.data(), kMagic.size());
        write_u32(out, kVersion);
        write_u32(out, static_cast<std::uint32_t>(json.size()));
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        write_u32(out, static_cast<std::uint32_t>(assets.size()));
        for (const auto& asset : assets) {
            ensure_within(source_folder, asset);
            const auto relative = asset.lexically_relative(source_folder).generic_u8string();
            require(safe_relative_name(relative), "Unsafe skin asset filename.");
            const auto size = fs::file_size(asset);
            require(size <= kMaxFileBytes && size <= kMaxAssetBytes - result.asset_bytes,
                    "Skin assets exceed the portable size limit.");
            write_u32(out, static_cast<std::uint32_t>(relative.size()));
            out.write(relative.data(), static_cast<std::streamsize>(relative.size()));
            write_u64(out, size);
            std::ifstream in(asset, std::ios::binary);
            require(in.good(), "Could not open skin asset.");
            write_u32(out, copy_bytes(in, out, size));
            result.asset_bytes += size;
        }
        out.close();
        require(out.good(), "Could not finish skin preset output.");
        // Commit only the fully written file and never replace an existing file.
#ifdef _WIN32
        require(MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != 0,
                "Could not save preset; choose a new filename in a writable folder.");
#else
        fs::create_hard_link(temporary, destination);
#endif
        result.path = destination.u8string();
        result.file_count = assets.size();
        result.skin = std::move(skin);
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

SkinPresetResult import_skin_preset(std::string_view source_utf8,
                                    std::string_view profile_dir_utf8) {
    SkinPresetResult result;
    OwnedDirectory installed;
    try {
        const auto source = fs::absolute(fs::u8path(source_utf8));
        reject_link_ancestors(source);
        require(fs::is_regular_file(source) && fs::file_size(source) <= kMaxPackageBytes,
                "Skin preset is missing or exceeds its size limit.");
        std::ifstream in(source, std::ios::binary);
        std::array<char, 8> magic{};
        in.read(magic.data(), magic.size());
        require(in.good() && magic == kMagic, "This is not a TenRiff .trskin preset.");
        require(read_u32(in) == kVersion, "Unsupported skin preset version.");
        const auto json = read_string(in, read_u32(in), kMaxSettings);
        validate_json_depth(json);
        config::SkinConfig skin;
        std::string parse_error;
        require(config::deserialize_skin_config(json, skin, &parse_error), "Invalid skin preset settings.");
        const auto count = read_u32(in);
        require(count <= kMaxFiles, "Skin preset contains too many files.");
        require(skin.source != "native" || count == 0, "Native skin presets cannot include files.");
        if (skin.source != "native") {
            require(skin.source == "lr2" || skin.source == "tenriff", "Unsupported skin preset source.");
            auto name = skin.source == "lr2" ? skin.lr2_skin_name : skin.tenriff_skin_name;
            require(safe_relative_name(name) && name.find('/') == name.npos, "Invalid preset skin name.");
            installed.parent = fs::absolute(fs::u8path(profile_dir_utf8)) / "skins" / skin.source;
            installed.path = reserve_directory(installed.parent, name);
        }
        std::set<std::string> names;
        for (std::uint32_t file = 0; file < count; ++file) {
            const auto relative = read_string(in, read_u32(in), kMaxPath);
            require(safe_relative_name(relative), "Unsafe path in skin preset.");
            require(names.insert(lower(relative)).second, "Duplicate filename in skin preset.");
            const auto size = read_u64(in);
            require(size <= kMaxFileBytes && size <= kMaxAssetBytes - result.asset_bytes,
                    "Skin preset asset exceeds its size limit.");
            const auto destination = installed.path / fs::u8path(relative);
            ensure_within(installed.path, destination);
            fs::create_directories(destination.parent_path());
            require(!fs::exists(destination), "Conflicting skin preset entries.");
            std::ofstream out(destination, std::ios::binary);
            require(out.good(), "Could not create imported skin asset.");
            const auto crc = copy_bytes(in, out, size);
            require(read_u32(in) == crc, "Skin preset asset checksum mismatch.");
            out.close();
            require(out.good(), "Could not finish imported skin asset.");
            result.asset_bytes += size;
        }
        require(in.peek() == std::char_traits<char>::eof(), "Unexpected trailing skin preset data.");
        if (skin.source != "native") {
            // Validate every key mode only after safe extraction, before activation.
            static_cast<void>(collect_assets(installed.path, skin.source));
            if (skin.source == "lr2") skin.lr2_skin_name = installed.path.filename().u8string();
            else skin.tenriff_skin_name = installed.path.filename().u8string();
        }
        if (skin.source != "lr2") skin.lr2_skin_name.clear();
        if (skin.source != "tenriff") skin.tenriff_skin_name.clear();
        result.path = installed.path.u8string();
        result.skin = std::move(skin);
        result.file_count = count;
        installed.keep = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}
}  // namespace tenriff::app
