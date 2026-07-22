#include "app/OsuArchiveImport.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "miniz.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {
namespace {

namespace fs = std::filesystem;

constexpr mz_uint16 kZipMethodStored = 0;
constexpr mz_uint16 kZipMethodDeflate = 8;
constexpr mz_uint16 kZipEncryptedFlag = 1u << 0u;
constexpr mz_uint16 kZipUtf8FilenameFlag = 1u << 11u;
constexpr mz_uint16 kZipHostUnix = 3;
constexpr mz_uint32 kUnixTypeMask = 0170000u;
constexpr mz_uint32 kUnixRegular = 0100000u;
constexpr mz_uint32 kUnixDirectory = 0040000u;
constexpr mz_uint32 kUnixSymlink = 0120000u;

constexpr std::uint32_t kZipEndOfCentralDirectorySignature = 0x06054B50u;
constexpr std::uint32_t kZip64EndOfCentralDirectorySignature = 0x06064B50u;
constexpr std::uint32_t kZip64EndOfCentralDirectoryLocatorSignature = 0x07064B50u;
constexpr std::uint32_t kZipCentralDirectoryHeaderSignature = 0x02014B50u;
constexpr std::uint64_t kZipEndOfCentralDirectorySize = 22;
constexpr std::uint64_t kZip64EndOfCentralDirectorySize = 56;
constexpr std::uint64_t kZip64EndOfCentralDirectoryLocatorSize = 20;
constexpr std::uint64_t kZipCentralDirectoryHeaderSize = 46;
constexpr std::uint64_t kZipMaximumCommentSize = 65535;
// osu! archives do not need unbounded per-entry comments or extra fields. This
// allowance keeps the miniz central-directory allocation tied to the caller's
// entry/path limits while leaving ample room for ordinary ZIP metadata.
constexpr std::uint64_t kCentralMetadataAllowancePerEntry = 4096;
constexpr std::uint64_t kCentralMetadataArchiveAllowance = 4096;

struct ArchiveEntry {
    mz_uint index = 0;
    std::string raw_name;
    std::vector<std::string> components;
    fs::path relative_path;
    std::string canonical_key;
    bool is_directory = false;
    std::uint64_t compressed_size = 0;
    std::uint64_t uncompressed_size = 0;
    std::uint32_t crc32 = 0;
};

struct PreflightResult {
    bool success = false;
    std::string error;
    std::vector<ArchiveEntry> entries;
    fs::path install_source_relative;
    std::string preferred_name;
    std::size_t file_count = 0;
    std::uint64_t total_bytes = 0;
};

struct InputStreamContext {
    std::ifstream stream;
};

struct ZipDirectoryEnvelope {
    std::uint64_t entry_count = 0;
    std::uint64_t central_directory_size = 0;
    std::uint64_t central_directory_offset = 0;
    std::uint64_t central_directory_boundary = 0;
};

struct OutputStreamContext {
    std::ofstream stream;
    std::uint64_t expected_size = 0;
    std::uint64_t written = 0;
    mz_ulong crc = MZ_CRC32_INIT;
    bool failed = false;
};

struct ZipReaderGuard {
    mz_zip_archive* archive = nullptr;
    bool initialized = false;

    ~ZipReaderGuard() {
        if (initialized && archive != nullptr) {
            mz_zip_reader_end(archive);
        }
    }
};

struct StagingGuard {
    fs::path path;
    bool committed = false;

    ~StagingGuard() {
        if (!committed && !path.empty()) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

std::string ascii_lower(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::string casefold_utf8(std::string_view value) {
#ifdef _WIN32
    const std::wstring wide = util::wide_from_multibyte(value, CP_UTF8, MB_ERR_INVALID_CHARS);
    if (!wide.empty()) {
        const int required = LCMapStringEx(LOCALE_NAME_INVARIANT,
                                           LCMAP_LOWERCASE,
                                           wide.data(),
                                           static_cast<int>(wide.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr,
                                           0);
        if (required > 0) {
            std::wstring lowered(static_cast<std::size_t>(required), L'\0');
            if (LCMapStringEx(LOCALE_NAME_INVARIANT,
                              LCMAP_LOWERCASE,
                              wide.data(),
                              static_cast<int>(wide.size()),
                              lowered.data(),
                              required,
                              nullptr,
                              nullptr,
                              0) > 0) {
                const std::string utf8 = util::utf8_from_wide_lossy(lowered);
                if (!utf8.empty()) {
                    return utf8;
                }
            }
        }
    }
#endif
    return ascii_lower(value);
}

bool is_valid_utf8(std::string_view value) {
    std::size_t index = 0;
    while (index < value.size()) {
        const unsigned char first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7Fu) {
            ++index;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t codepoint = 0;
        if (first >= 0xC2u && first <= 0xDFu) {
            continuation_count = 1;
            codepoint = first & 0x1Fu;
        } else if (first >= 0xE0u && first <= 0xEFu) {
            continuation_count = 2;
            codepoint = first & 0x0Fu;
        } else if (first >= 0xF0u && first <= 0xF4u) {
            continuation_count = 3;
            codepoint = first & 0x07u;
        } else {
            return false;
        }

        if (index + continuation_count >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const unsigned char next = static_cast<unsigned char>(value[index + offset]);
            if ((next & 0xC0u) != 0x80u) {
                return false;
            }
            codepoint = (codepoint << 6u) | (next & 0x3Fu);
        }

        if ((continuation_count == 2 && codepoint < 0x800u) ||
            (continuation_count == 3 && codepoint < 0x10000u) ||
            codepoint > 0x10FFFFu ||
            (codepoint >= 0xD800u && codepoint <= 0xDFFFu)) {
            return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

bool normalize_archive_filename(std::string& value, bool declared_utf8, std::string& error) {
    if (is_valid_utf8(value)) {
        return true;
    }
    if (declared_utf8) {
        error = "archive entry has an invalid UTF-8 name";
        return false;
    }
#ifdef _WIN32
    // Older Japanese osu! skins commonly stored CP932 names without ZIP's
    // UTF-8 flag. CP437 is the ZIP legacy fallback if CP932 cannot decode.
    for (const UINT code_page : {static_cast<UINT>(932), static_cast<UINT>(437)}) {
        const std::wstring wide = util::wide_from_multibyte(value, code_page);
        if (wide.empty()) {
            continue;
        }
        const std::string converted = util::utf8_from_wide_lossy(wide);
        if (!converted.empty() && is_valid_utf8(converted)) {
            value = converted;
            return true;
        }
    }
#endif
    error = "archive entry name is neither UTF-8 nor a supported legacy encoding";
    return false;
}

bool is_windows_reserved_component(std::string_view component) {
    std::string base(component.substr(0, component.find('.')));
    while (!base.empty() && (base.back() == '.' || base.back() == ' ')) {
        base.pop_back();
    }
    base = ascii_lower(base);
    if (base == "con" || base == "prn" || base == "aux" || base == "nul") {
        return true;
    }
    if (base.size() == 4 && (base.rfind("com", 0) == 0 || base.rfind("lpt", 0) == 0) &&
        base[3] >= '1' && base[3] <= '9') {
        return true;
    }
    return false;
}

bool validate_component(std::string_view component, std::string& error) {
    if (component.empty() || component == "." || component == "..") {
        error = "archive contains an empty or relative path component";
        return false;
    }
    if (component.back() == '.' || component.back() == ' ') {
        error = "archive path component ends in a dot or space";
        return false;
    }
    if (is_windows_reserved_component(component)) {
        error = "archive path uses a reserved Windows device name";
        return false;
    }
#ifdef _WIN32
    const std::wstring wide_component =
        util::wide_from_multibyte(component, CP_UTF8, MB_ERR_INVALID_CHARS);
    if (wide_component.empty() || wide_component.size() > 255) {
        error = "archive path component exceeds the Windows filename limit";
        return false;
    }
#else
    if (component.size() > 255) {
        error = "archive path component exceeds the filename limit";
        return false;
    }
#endif
    for (unsigned char ch : component) {
        if (ch < 0x20u || ch == 0x7Fu || ch == ':' || ch == '<' || ch == '>' ||
            ch == '"' || ch == '|' || ch == '?' || ch == '*') {
            error = "archive path contains a control or Windows-reserved character";
            return false;
        }
    }
    return true;
}

bool parse_entry_path(std::string raw_name,
                      bool declared_utf8,
                      const OsuArchiveImportLimits& limits,
                      ArchiveEntry& entry,
                      std::string& error) {
    if (raw_name.empty() || raw_name.size() > limits.max_path_bytes) {
        error = "archive entry has an empty or oversized name";
        return false;
    }
    if (!normalize_archive_filename(raw_name, declared_utf8, error)) {
        return false;
    }
    if (raw_name.size() > limits.max_path_bytes) {
        error = "archive entry name exceeds the path-size limit after decoding";
        return false;
    }
    std::replace(raw_name.begin(), raw_name.end(), '\\', '/');
    if (raw_name.front() == '/' || raw_name.rfind("//", 0) == 0) {
        error = "archive entry uses an absolute path";
        return false;
    }

    const bool trailing_slash = raw_name.back() == '/';
    if (trailing_slash) {
        raw_name.pop_back();
    }
    if (raw_name.empty()) {
        error = "archive contains an invalid root directory entry";
        return false;
    }

    std::size_t start = 0;
    while (start <= raw_name.size()) {
        const std::size_t slash = raw_name.find('/', start);
        const std::size_t end = slash == std::string::npos ? raw_name.size() : slash;
        const std::string component = raw_name.substr(start, end - start);
        if (!validate_component(component, error)) {
            return false;
        }
        entry.components.push_back(component);
        entry.relative_path /= fs::u8path(component);
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    if (entry.components.empty() || entry.components.size() > limits.max_path_components) {
        error = "archive entry path is too deep";
        return false;
    }

    entry.raw_name = std::move(raw_name);
    entry.canonical_key.clear();
    for (const std::string& component : entry.components) {
        if (!entry.canonical_key.empty()) {
            entry.canonical_key.push_back('/');
        }
        entry.canonical_key += casefold_utf8(component);
    }
    entry.is_directory = trailing_slash;
    return true;
}

std::string get_full_filename(mz_zip_archive& archive, mz_uint index, std::string& error) {
    const mz_uint required = mz_zip_reader_get_filename(&archive, index, nullptr, 0);
    if (required == 0) {
        error = "could not read an archive entry name";
        return {};
    }
    std::vector<char> buffer(static_cast<std::size_t>(required));
    const mz_uint written = mz_zip_reader_get_filename(&archive, index, buffer.data(), required);
    if (written == 0 || buffer.back() != '\0') {
        error = "could not read a complete archive entry name";
        return {};
    }
    return std::string(buffer.data(), buffer.size() - 1);
}

bool validate_entry_type(const mz_zip_archive_file_stat& stat, bool is_directory, std::string& error) {
    if (stat.m_is_encrypted || (stat.m_bit_flag & kZipEncryptedFlag) != 0u) {
        error = "encrypted ZIP entries are not supported";
        return false;
    }
    if (!stat.m_is_supported || (stat.m_method != kZipMethodStored && stat.m_method != kZipMethodDeflate)) {
        error = "archive uses an unsupported ZIP compression method or feature";
        return false;
    }

    const mz_uint16 host = static_cast<mz_uint16>(stat.m_version_made_by >> 8u);
    if (host == kZipHostUnix) {
        const mz_uint32 unix_type = (stat.m_external_attr >> 16u) & kUnixTypeMask;
        if (unix_type == kUnixSymlink) {
            error = "archive contains a symbolic link";
            return false;
        }
        if (unix_type != 0u && unix_type != kUnixRegular && unix_type != kUnixDirectory) {
            error = "archive contains a non-regular filesystem entry";
            return false;
        }
        if ((unix_type == kUnixDirectory) != is_directory) {
            error = "archive directory metadata is inconsistent";
            return false;
        }
    }
    return true;
}

bool path_is_within(std::string_view child, std::string_view parent) {
    return parent.empty() || child == parent ||
           (child.size() > parent.size() && child.compare(0, parent.size(), parent) == 0 &&
            child[parent.size()] == '/');
}

std::string parent_key(const ArchiveEntry& entry) {
    const std::size_t slash = entry.canonical_key.rfind('/');
    return slash == std::string::npos ? std::string{} : entry.canonical_key.substr(0, slash);
}

bool has_extension(const ArchiveEntry& entry, std::string_view expected) {
    if (entry.components.empty()) {
        return false;
    }
    const std::string& leaf = entry.components.back();
    const std::size_t dot = leaf.rfind('.');
    return dot != std::string::npos && ascii_lower(std::string_view(leaf).substr(dot)) == expected;
}

bool is_skin_ini(const ArchiveEntry& entry) {
    return !entry.is_directory && !entry.components.empty() &&
           ascii_lower(entry.components.back()) == "skin.ini";
}

bool is_mania_asset(const ArchiveEntry& entry) {
    if (entry.is_directory || entry.components.empty()) {
        return false;
    }
    const std::string leaf = ascii_lower(entry.components.back());
    const bool supported_image = has_extension(entry, ".png") || has_extension(entry, ".jpg") ||
                                 has_extension(entry, ".jpeg");
    return supported_image && leaf.rfind("mania-", 0) == 0;
}

fs::path relative_path_from_components(const std::vector<std::string>& components, std::size_t count) {
    fs::path result;
    for (std::size_t i = 0; i < count; ++i) {
        result /= fs::u8path(components[i]);
    }
    return result;
}

std::string preferred_name_for_root(const std::vector<ArchiveEntry>& entries,
                                    std::string_view root_key,
                                    const fs::path& archive_path,
                                    fs::path& source_relative) {
    if (!root_key.empty()) {
        for (const ArchiveEntry& entry : entries) {
            if (entry.canonical_key == root_key || path_is_within(entry.canonical_key, root_key)) {
                const std::size_t component_count =
                    static_cast<std::size_t>(std::count(root_key.begin(), root_key.end(), '/')) + 1;
                source_relative = relative_path_from_components(entry.components, component_count);
                return entry.components[component_count - 1];
            }
        }
    }
    source_relative.clear();
    return archive_path.stem().u8string();
}

bool validate_osk_semantics(PreflightResult& result, const fs::path& archive_path) {
    std::set<std::string> skin_ini_roots;
    std::set<std::string> mania_roots;
    for (const ArchiveEntry& entry : result.entries) {
        if (is_skin_ini(entry)) {
            skin_ini_roots.insert(parent_key(entry));
        }
        if (is_mania_asset(entry)) {
            mania_roots.insert(parent_key(entry));
        }
    }

    const std::set<std::string>& candidates = skin_ini_roots.empty() ? mania_roots : skin_ini_roots;
    if (candidates.size() != 1) {
        result.error = "OSK must contain exactly one skin root with skin.ini or a mania asset";
        return false;
    }
    const std::string root_key = *candidates.begin();
    if (!root_key.empty() && root_key.find('/') != std::string::npos) {
        result.error = "OSK skin root may have at most one common wrapper directory";
        return false;
    }
    if (!root_key.empty()) {
        for (const ArchiveEntry& entry : result.entries) {
            if (!path_is_within(entry.canonical_key, root_key)) {
                result.error = "OSK contains files outside its single skin wrapper";
                return false;
            }
        }
    }

    result.preferred_name =
        preferred_name_for_root(result.entries, root_key, archive_path, result.install_source_relative);
    return true;
}

bool validate_osz_semantics(PreflightResult& result, const fs::path& archive_path) {
    bool has_osu = false;
    std::string common_top;
    bool can_unwrap = true;
    for (const ArchiveEntry& entry : result.entries) {
        if (entry.is_directory) {
            continue;
        }
        has_osu = has_osu || has_extension(entry, ".osu");
        if (entry.components.size() < 2) {
            can_unwrap = false;
            continue;
        }
        const std::string top = casefold_utf8(entry.components.front());
        if (common_top.empty()) {
            common_top = top;
        } else if (common_top != top) {
            can_unwrap = false;
        }
    }
    if (!has_osu) {
        result.error = "OSZ must contain at least one .osu beatmap";
        return false;
    }

    if (can_unwrap && !common_top.empty()) {
        result.preferred_name =
            preferred_name_for_root(result.entries, common_top, archive_path, result.install_source_relative);
    } else {
        result.install_source_relative.clear();
        result.preferred_name = archive_path.stem().u8string();
    }
    return true;
}

PreflightResult preflight_archive(mz_zip_archive& archive,
                                  const fs::path& archive_path,
                                  OsuArchiveKind kind,
                                  const OsuArchiveImportLimits& limits) {
    PreflightResult result;
    const mz_uint entry_count = mz_zip_reader_get_num_files(&archive);
    if (entry_count == 0 || static_cast<std::size_t>(entry_count) > limits.max_entries) {
        result.error = "archive is empty or exceeds the entry-count limit";
        return result;
    }

    std::set<std::string> files;
    std::set<std::string> directories;
    std::set<std::string> explicit_entries;
    std::map<std::string, std::string> directory_spelling;
    result.entries.reserve(entry_count);

    for (mz_uint index = 0; index < entry_count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
            result.error = "could not read ZIP entry metadata";
            return result;
        }

        ArchiveEntry entry;
        entry.index = index;
        std::string filename_error;
        std::string raw_name = get_full_filename(archive, index, filename_error);
        if (!filename_error.empty()) {
            result.error = std::move(filename_error);
            return result;
        }
        if (!parse_entry_path(std::move(raw_name),
                              (stat.m_bit_flag & kZipUtf8FilenameFlag) != 0u,
                              limits,
                              entry,
                              result.error)) {
            return result;
        }

        const bool metadata_directory = stat.m_is_directory != 0 ||
                                        mz_zip_reader_is_file_a_directory(&archive, index) != 0;
        if (entry.is_directory != metadata_directory) {
            result.error = "archive directory name and metadata disagree";
            return result;
        }
        if (!validate_entry_type(stat, entry.is_directory, result.error)) {
            return result;
        }

        entry.compressed_size = stat.m_comp_size;
        entry.uncompressed_size = stat.m_uncomp_size;
        entry.crc32 = stat.m_crc32;
        if (entry.is_directory && entry.uncompressed_size != 0) {
            result.error = "archive directory entry contains file data";
            return result;
        }
        if (entry.uncompressed_size > limits.max_file_bytes ||
            entry.uncompressed_size > limits.max_total_uncompressed_bytes ||
            result.total_bytes > limits.max_total_uncompressed_bytes - entry.uncompressed_size) {
            result.error = "archive exceeds the configured uncompressed-size limits";
            return result;
        }
        if (!entry.is_directory && entry.uncompressed_size > 1024u * 1024u) {
            if (entry.compressed_size == 0 ||
                static_cast<double>(entry.uncompressed_size) /
                        static_cast<double>(entry.compressed_size) >
                    limits.max_compression_ratio) {
                result.error = "archive entry exceeds the compression-ratio limit";
                return result;
            }
        }

        if (explicit_entries.count(entry.canonical_key) != 0) {
            result.error = "archive contains a duplicate or colliding path";
            return result;
        }
        explicit_entries.insert(entry.canonical_key);

        std::string prefix;
        std::string raw_prefix;
        for (std::size_t component_index = 0; component_index + 1 < entry.components.size();
             ++component_index) {
            if (!prefix.empty()) {
                prefix.push_back('/');
                raw_prefix.push_back('/');
            }
            prefix += casefold_utf8(entry.components[component_index]);
            raw_prefix += entry.components[component_index];
            const auto [spelling, inserted] = directory_spelling.emplace(prefix, raw_prefix);
            if (!inserted && spelling->second != raw_prefix) {
                result.error = "archive contains case-fold aliases for the same directory";
                return result;
            }
            if (files.count(prefix) != 0) {
                result.error = "archive contains a file/directory prefix collision";
                return result;
            }
            directories.insert(prefix);
        }
        if (entry.is_directory) {
            const auto [spelling, inserted] =
                directory_spelling.emplace(entry.canonical_key, entry.raw_name);
            if (!inserted && spelling->second != entry.raw_name) {
                result.error = "archive contains case-fold aliases for the same directory";
                return result;
            }
            if (files.count(entry.canonical_key) != 0) {
                result.error = "archive contains a duplicate or colliding path";
                return result;
            }
            directories.insert(entry.canonical_key);
        } else {
            if (files.count(entry.canonical_key) != 0 || directories.count(entry.canonical_key) != 0) {
                result.error = "archive contains a duplicate or colliding path";
                return result;
            }
            files.insert(entry.canonical_key);
            ++result.file_count;
            result.total_bytes += entry.uncompressed_size;
        }
        result.entries.push_back(std::move(entry));
    }

    if (kind == OsuArchiveKind::Osk) {
        if (!validate_osk_semantics(result, archive_path)) {
            return result;
        }
    } else if (!validate_osz_semantics(result, archive_path)) {
        return result;
    }
    result.success = true;
    return result;
}

size_t archive_read_callback(void* opaque, mz_uint64 file_offset, void* buffer, size_t bytes) {
    auto* context = static_cast<InputStreamContext*>(opaque);
    if (context == nullptr || file_offset > static_cast<mz_uint64>((std::numeric_limits<std::streamoff>::max)()) ||
        bytes > static_cast<size_t>((std::numeric_limits<std::streamsize>::max)())) {
        return 0;
    }
    context->stream.clear();
    context->stream.seekg(static_cast<std::streamoff>(file_offset), std::ios::beg);
    if (!context->stream.good()) {
        return 0;
    }
    context->stream.read(static_cast<char*>(buffer), static_cast<std::streamsize>(bytes));
    return static_cast<size_t>(context->stream.gcount());
}

std::uint16_t read_le16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8u);
}

std::uint32_t read_le32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

std::uint64_t read_le64(const std::uint8_t* bytes) {
    return static_cast<std::uint64_t>(read_le32(bytes)) |
           (static_cast<std::uint64_t>(read_le32(bytes + 4)) << 32u);
}

bool checked_add_u64(std::uint64_t left, std::uint64_t right, std::uint64_t& result) {
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
        return false;
    }
    result = left + right;
    return true;
}

bool checked_multiply_u64(std::uint64_t left,
                          std::uint64_t right,
                          std::uint64_t& result) {
    if (left != 0 && right > (std::numeric_limits<std::uint64_t>::max)() / left) {
        return false;
    }
    result = left * right;
    return true;
}

bool read_archive_exact(InputStreamContext& input,
                        std::uint64_t archive_bytes,
                        std::uint64_t offset,
                        void* buffer,
                        std::size_t bytes) {
    if (offset > archive_bytes || static_cast<std::uint64_t>(bytes) > archive_bytes - offset) {
        return false;
    }
    return archive_read_callback(&input, offset, buffer, bytes) == bytes;
}

bool locate_end_of_central_directory(InputStreamContext& input,
                                     std::uint64_t archive_bytes,
                                     std::uint64_t& eocd_offset,
                                     std::string& error) {
    if (archive_bytes < kZipEndOfCentralDirectorySize) {
        error = "ZIP is too small to contain an end-of-central-directory record";
        return false;
    }
    const std::uint64_t maximum_search =
        kZipMaximumCommentSize + kZipEndOfCentralDirectorySize;
    const std::uint64_t tail_bytes = std::min(archive_bytes, maximum_search);
    std::vector<std::uint8_t> tail(static_cast<std::size_t>(tail_bytes));
    const std::uint64_t tail_offset = archive_bytes - tail_bytes;
    if (!read_archive_exact(input, archive_bytes, tail_offset, tail.data(), tail.size())) {
        error = "could not read the ZIP end-of-central-directory window";
        return false;
    }

    std::size_t cursor = tail.size() - static_cast<std::size_t>(kZipEndOfCentralDirectorySize);
    for (;;) {
        if (read_le32(tail.data() + cursor) == kZipEndOfCentralDirectorySignature) {
            eocd_offset = tail_offset + cursor;
            return true;
        }
        if (cursor == 0) {
            break;
        }
        --cursor;
    }
    error = "could not find the ZIP end-of-central-directory record";
    return false;
}

bool validate_zip_directory_envelope(InputStreamContext& input,
                                     std::uint64_t archive_bytes,
                                     const OsuArchiveImportLimits& limits,
                                     ZipDirectoryEnvelope& envelope,
                                     std::string& error) {
    std::uint64_t eocd_offset = 0;
    if (!locate_end_of_central_directory(input, archive_bytes, eocd_offset, error)) {
        return false;
    }

    std::uint8_t eocd[static_cast<std::size_t>(kZipEndOfCentralDirectorySize)]{};
    if (!read_archive_exact(input, archive_bytes, eocd_offset, eocd, sizeof(eocd))) {
        error = "could not read the ZIP end-of-central-directory record";
        return false;
    }
    const std::uint16_t comment_size = read_le16(eocd + 20);
    std::uint64_t eocd_end = 0;
    if (!checked_add_u64(eocd_offset, kZipEndOfCentralDirectorySize, eocd_end) ||
        !checked_add_u64(eocd_end, comment_size, eocd_end) || eocd_end > archive_bytes) {
        error = "ZIP end-of-central-directory comment exceeds the archive";
        return false;
    }

    const std::uint16_t classic_disk = read_le16(eocd + 4);
    const std::uint16_t classic_central_disk = read_le16(eocd + 6);
    const std::uint16_t classic_entries_on_disk = read_le16(eocd + 8);
    const std::uint16_t classic_entries = read_le16(eocd + 10);
    const std::uint32_t classic_central_size = read_le32(eocd + 12);
    const std::uint32_t classic_central_offset = read_le32(eocd + 16);

    bool has_zip64 = false;
    std::uint8_t locator[static_cast<std::size_t>(kZip64EndOfCentralDirectoryLocatorSize)]{};
    std::uint64_t locator_offset = 0;
    if (eocd_offset >= kZip64EndOfCentralDirectoryLocatorSize) {
        locator_offset = eocd_offset - kZip64EndOfCentralDirectoryLocatorSize;
        if (!read_archive_exact(input,
                                archive_bytes,
                                locator_offset,
                                locator,
                                sizeof(locator))) {
            error = "could not read the ZIP64 end-of-central-directory locator";
            return false;
        }
        has_zip64 = read_le32(locator) == kZip64EndOfCentralDirectoryLocatorSignature;
    }

    if (has_zip64) {
        if (classic_disk != 0 || classic_central_disk != 0 || read_le32(locator + 4) != 0 ||
            read_le32(locator + 16) != 1) {
            error = "multi-disk ZIP archives are not supported";
            return false;
        }

        std::uint64_t zip64_offset = 0;
        std::uint8_t zip64[static_cast<std::size_t>(kZip64EndOfCentralDirectorySize)]{};
        const std::uint64_t fixed_offset =
            locator_offset >= kZip64EndOfCentralDirectorySize
                ? locator_offset - kZip64EndOfCentralDirectorySize
                : 0;
        bool fixed_signature = false;
        if (locator_offset >= kZip64EndOfCentralDirectorySize &&
            read_archive_exact(input,
                               archive_bytes,
                               fixed_offset,
                               zip64,
                               sizeof(zip64))) {
            fixed_signature = read_le32(zip64) == kZip64EndOfCentralDirectorySignature;
        }
        if (fixed_signature) {
            // Match miniz's lookup priority so the record we validate is the
            // same record it will consume before allocating its directory.
            zip64_offset = fixed_offset;
        } else {
            zip64_offset = read_le64(locator + 8);
            if (!read_archive_exact(input,
                                    archive_bytes,
                                    zip64_offset,
                                    zip64,
                                    sizeof(zip64)) ||
                read_le32(zip64) != kZip64EndOfCentralDirectorySignature) {
                error = "ZIP64 locator does not reference a valid ZIP64 record";
                return false;
            }
        }

        const std::uint64_t zip64_record_payload_size = read_le64(zip64 + 4);
        std::uint64_t zip64_record_end = 0;
        if (zip64_record_payload_size < 44 ||
            !checked_add_u64(zip64_offset, 12, zip64_record_end) ||
            !checked_add_u64(zip64_record_end,
                             zip64_record_payload_size,
                             zip64_record_end) ||
            zip64_record_end != locator_offset) {
            error = "ZIP64 end-of-central-directory record has an invalid size or offset";
            return false;
        }
        if (read_le32(zip64 + 16) != 0 || read_le32(zip64 + 20) != 0) {
            error = "multi-disk ZIP archives are not supported";
            return false;
        }

        envelope.entry_count = read_le64(zip64 + 32);
        if (read_le64(zip64 + 24) != envelope.entry_count) {
            error = "multi-disk ZIP archives are not supported";
            return false;
        }
        envelope.central_directory_size = read_le64(zip64 + 40);
        envelope.central_directory_offset = read_le64(zip64 + 48);
        envelope.central_directory_boundary = zip64_offset;
    } else {
        if (classic_disk != 0 || classic_central_disk != 0 ||
            classic_entries_on_disk != classic_entries) {
            error = "multi-disk ZIP archives are not supported";
            return false;
        }
        if (classic_entries == (std::numeric_limits<std::uint16_t>::max)() ||
            classic_central_size == (std::numeric_limits<std::uint32_t>::max)() ||
            classic_central_offset == (std::numeric_limits<std::uint32_t>::max)()) {
            error = "ZIP64 sentinel values require a ZIP64 locator";
            return false;
        }
        envelope.entry_count = classic_entries;
        envelope.central_directory_size = classic_central_size;
        envelope.central_directory_offset = classic_central_offset;
        envelope.central_directory_boundary = eocd_offset;
    }

    if (envelope.entry_count > static_cast<std::uint64_t>(limits.max_entries)) {
        error = "archive exceeds the entry-count limit";
        return false;
    }
    if (envelope.entry_count > (std::numeric_limits<std::uint32_t>::max)() ||
        envelope.central_directory_size > (std::numeric_limits<std::uint32_t>::max)() ||
        envelope.central_directory_size > static_cast<std::uint64_t>(
                                              (std::numeric_limits<std::size_t>::max)())) {
        error = "ZIP central-directory metadata exceeds supported limits";
        return false;
    }

    std::uint64_t minimum_central_size = 0;
    if (!checked_multiply_u64(envelope.entry_count,
                              kZipCentralDirectoryHeaderSize,
                              minimum_central_size) ||
        envelope.central_directory_size < minimum_central_size) {
        error = "ZIP central directory is too small for its declared entry count";
        return false;
    }

    std::uint64_t reasonable_entry_size = 0;
    std::uint64_t reasonable_central_size = 0;
    if (!checked_add_u64(kZipCentralDirectoryHeaderSize,
                         static_cast<std::uint64_t>(limits.max_path_bytes),
                         reasonable_entry_size) ||
        !checked_add_u64(reasonable_entry_size,
                         kCentralMetadataAllowancePerEntry,
                         reasonable_entry_size) ||
        !checked_multiply_u64(envelope.entry_count,
                              reasonable_entry_size,
                              reasonable_central_size) ||
        !checked_add_u64(reasonable_central_size,
                         kCentralMetadataArchiveAllowance,
                         reasonable_central_size)) {
        error = "archive import limits overflow ZIP metadata accounting";
        return false;
    }
    if (envelope.central_directory_size > reasonable_central_size) {
        error = "ZIP central directory exceeds the bounded metadata allowance";
        return false;
    }

    std::uint64_t central_end = 0;
    if (!checked_add_u64(envelope.central_directory_offset,
                         envelope.central_directory_size,
                         central_end) ||
        central_end > archive_bytes || central_end > envelope.central_directory_boundary) {
        error = "ZIP central-directory size or offset is outside the archive";
        return false;
    }
    if (envelope.entry_count != 0) {
        std::uint8_t signature[4]{};
        if (!read_archive_exact(input,
                                archive_bytes,
                                envelope.central_directory_offset,
                                signature,
                                sizeof(signature)) ||
            read_le32(signature) != kZipCentralDirectoryHeaderSignature) {
            error = "ZIP central-directory offset does not reference a directory header";
            return false;
        }
    }
    return true;
}

size_t archive_write_callback(void* opaque, mz_uint64 file_offset, const void* buffer, size_t bytes) {
    auto* context = static_cast<OutputStreamContext*>(opaque);
    if (context == nullptr || context->failed || file_offset != context->written ||
        context->written > context->expected_size || bytes > context->expected_size - context->written ||
        bytes > static_cast<size_t>((std::numeric_limits<std::streamsize>::max)())) {
        if (context != nullptr) {
            context->failed = true;
        }
        return 0;
    }
    context->stream.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(bytes));
    if (!context->stream.good()) {
        context->failed = true;
        return 0;
    }
    context->crc = mz_crc32(context->crc, static_cast<const unsigned char*>(buffer), bytes);
    context->written += bytes;
    return bytes;
}

bool ensure_owned_directory(const fs::path& path, std::string& error) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (!ec && fs::exists(status)) {
        if (fs::is_symlink(status) || !fs::is_directory(status)) {
            error = "import destination contains a symlink or non-directory control path";
            return false;
        }
        return true;
    }
    ec.clear();
    if (!fs::create_directories(path, ec) && ec) {
        error = "could not create the import destination";
        return false;
    }
    const fs::file_status created_status = fs::symlink_status(path, ec);
    if (ec || fs::is_symlink(created_status) || !fs::is_directory(created_status)) {
        error = "import destination is not a safe directory";
        return false;
    }
    return true;
}

fs::path create_unique_staging_directory(const fs::path& staging_root, std::string& error) {
    std::random_device random;
    const std::uint64_t time_bits = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        const std::uint64_t token = time_bits ^
                                    (static_cast<std::uint64_t>(random()) << 32u) ^
                                    static_cast<std::uint64_t>(random()) ^ attempt;
        std::ostringstream name;
        name << "import-" << std::hex << token;
        const fs::path candidate = staging_root / name.str();
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
        if (ec && ec != std::errc::file_exists) {
            error = "could not create a private import staging directory";
            return {};
        }
    }
    error = "could not allocate a unique import staging directory";
    return {};
}

std::string sanitize_install_name(std::string name, OsuArchiveKind kind) {
    if (!is_valid_utf8(name)) {
        name.clear();
    }
    for (char& ch : name) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte < 0x20u || byte == 0x7Fu || ch == ':' || ch == '<' || ch == '>' ||
            ch == '"' || ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*') {
            ch = '_';
        }
    }
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
        name.pop_back();
    }
    while (!name.empty() && name.front() == ' ') {
        name.erase(name.begin());
    }
    if (name.empty()) {
        name = kind == OsuArchiveKind::Osk ? "Imported osu skin" : "Imported osu beatmap";
    }
    if (is_windows_reserved_component(name)) {
        name.insert(name.begin(), '_');
    }
    return name;
}

std::set<std::string> existing_child_names(const fs::path& root) {
    std::set<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end;
         it.increment(ec)) {
        names.insert(casefold_utf8(it->path().filename().u8string()));
    }
    return names;
}

std::pair<fs::path, std::string> choose_install_path(const fs::path& root, std::string base_name) {
    const std::set<std::string> existing = existing_child_names(root);
    for (unsigned suffix = 1; suffix < 100000; ++suffix) {
        const std::string display = suffix == 1 ? base_name : base_name + " (" + std::to_string(suffix) + ")";
        if (existing.count(casefold_utf8(display)) == 0) {
            return {root / fs::u8path(display), display};
        }
    }
    return {};
}

bool extract_entries(mz_zip_archive& archive,
                     const std::vector<ArchiveEntry>& entries,
                     const fs::path& payload_root,
                     std::string& error) {
    std::error_code ec;
    if (!fs::create_directory(payload_root, ec) || ec) {
        error = "could not create the extraction payload directory";
        return false;
    }

    for (const ArchiveEntry& entry : entries) {
        const fs::path destination = payload_root / entry.relative_path;
        if (entry.is_directory) {
            if (!fs::create_directories(destination, ec) && ec) {
                error = "could not create an archive directory";
                return false;
            }
            continue;
        }
        ec.clear();
        if (!fs::create_directories(destination.parent_path(), ec) && ec) {
            error = "could not create an archive file parent directory";
            return false;
        }

        OutputStreamContext output;
        output.expected_size = entry.uncompressed_size;
        output.stream.open(destination, std::ios::binary | std::ios::trunc);
        if (!output.stream.good()) {
            error = "could not create an extracted archive file";
            return false;
        }
        if (!mz_zip_reader_extract_to_callback(
                &archive, entry.index, archive_write_callback, &output, 0)) {
            const char* detail = mz_zip_get_error_string(mz_zip_peek_last_error(&archive));
            error = std::string("ZIP extraction failed") + (detail ? ": " + std::string(detail) : std::string{});
            return false;
        }
        output.stream.flush();
        if (output.failed || !output.stream.good() || output.written != entry.uncompressed_size ||
            static_cast<std::uint32_t>(output.crc) != entry.crc32) {
            error = "extracted file failed its size or CRC check";
            return false;
        }
    }
    return true;
}

}  // namespace

static OsuArchiveImportResult import_osu_archive_impl(std::string_view archive_path_utf8,
                                                       std::string_view destination_root_utf8,
                                                       OsuArchiveKind kind,
                                                       const OsuArchiveImportLimits& limits) {
    OsuArchiveImportResult result;
    if (archive_path_utf8.empty() || destination_root_utf8.empty()) {
        result.error = "archive path and destination root are required";
        return result;
    }
    if (limits.max_entries == 0 || limits.max_path_bytes == 0 || limits.max_path_components == 0 ||
        limits.max_archive_bytes == 0 || limits.max_file_bytes == 0 ||
        limits.max_total_uncompressed_bytes == 0 || limits.max_compression_ratio <= 0.0) {
        result.error = "archive import limits are invalid";
        return result;
    }

    const fs::path archive_path = util::path_from_utf8_lossy(archive_path_utf8);
    const fs::path destination_root = util::path_from_utf8_lossy(destination_root_utf8);
    const std::string expected_extension = kind == OsuArchiveKind::Osk ? ".osk" : ".osz";
    if (ascii_lower(archive_path.extension().u8string()) != expected_extension) {
        result.error = "archive extension does not match the requested import kind";
        return result;
    }

    std::error_code ec;
    const fs::file_status archive_status = fs::symlink_status(archive_path, ec);
    if (ec || fs::is_symlink(archive_status) || !fs::is_regular_file(archive_status)) {
        result.error = "archive path is missing, a symlink, or not a regular file";
        return result;
    }
    const std::uint64_t archive_bytes = fs::file_size(archive_path, ec);
    if (ec || archive_bytes == 0 || archive_bytes > limits.max_archive_bytes ||
        archive_bytes > static_cast<std::uint64_t>((std::numeric_limits<std::streamoff>::max)())) {
        result.error = "archive is empty, unreadable, or exceeds the archive-size limit";
        return result;
    }

    InputStreamContext input;
    input.stream.open(archive_path, std::ios::binary);
    if (!input.stream.good()) {
        result.error = "could not open the archive";
        return result;
    }

    // miniz materializes the complete central directory during reader init.
    // Bound its entry array and directory buffer from raw EOCD/ZIP64 metadata
    // first, using only fixed-size reads plus a <=64 KiB tail window.
    ZipDirectoryEnvelope directory_envelope;
    if (!validate_zip_directory_envelope(
            input, archive_bytes, limits, directory_envelope, result.error)) {
        return result;
    }

    mz_zip_archive archive{};
    mz_zip_zero_struct(&archive);
    archive.m_pRead = archive_read_callback;
    archive.m_pIO_opaque = &input;
    ZipReaderGuard archive_guard{&archive, false};
    if (!mz_zip_reader_init(&archive, archive_bytes, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY)) {
        const char* detail = mz_zip_get_error_string(mz_zip_peek_last_error(&archive));
        result.error = std::string("could not parse the ZIP archive") +
                       (detail ? ": " + std::string(detail) : std::string{});
        return result;
    }
    archive_guard.initialized = true;
    result.archive_entry_count = mz_zip_reader_get_num_files(&archive);

    PreflightResult preflight = preflight_archive(archive, archive_path, kind, limits);
    if (!preflight.success) {
        result.error = std::move(preflight.error);
        return result;
    }

    if (!ensure_owned_directory(destination_root, result.error)) {
        return result;
    }
    const fs::path metadata_root = destination_root / ".tenriff";
    const fs::path staging_root = metadata_root / "import-staging";
    if (!ensure_owned_directory(metadata_root, result.error) ||
        !ensure_owned_directory(staging_root, result.error)) {
        return result;
    }

    StagingGuard staging;
    staging.path = create_unique_staging_directory(staging_root, result.error);
    if (staging.path.empty()) {
        return result;
    }
    const fs::path payload_root = staging.path / "payload";
    if (!extract_entries(archive, preflight.entries, payload_root, result.error)) {
        return result;
    }

    const fs::path install_source = preflight.install_source_relative.empty()
                                        ? payload_root
                                        : payload_root / preflight.install_source_relative;
    const fs::file_status source_status = fs::symlink_status(install_source, ec);
    if (ec || fs::is_symlink(source_status) || !fs::is_directory(source_status)) {
        result.error = "validated archive payload did not produce an installable directory";
        return result;
    }

    const std::string base_name = sanitize_install_name(preflight.preferred_name, kind);
    auto [install_path, display_name] = choose_install_path(destination_root, base_name);
    if (install_path.empty()) {
        result.error = "could not choose a collision-free installation name";
        return result;
    }
    if (fs::exists(fs::symlink_status(install_path, ec))) {
        result.error = "installation target appeared during import; retry the import";
        return result;
    }
    ec.clear();
    fs::rename(install_source, install_path, ec);
    if (ec) {
        result.error = "could not commit the imported archive";
        return result;
    }

    // The committed directory no longer belongs to staging; remove only the
    // private token directory and leave the shared staging parent in place.
    fs::remove_all(staging.path, ec);
    staging.committed = true;
    result.success = true;
    result.installed_path = install_path.u8string();
    result.display_name = std::move(display_name);
    result.extracted_file_count = preflight.file_count;
    result.extracted_bytes = preflight.total_bytes;
    return result;
}

OsuArchiveImportResult import_osu_archive(std::string_view archive_path_utf8,
                                          std::string_view destination_root_utf8,
                                          OsuArchiveKind kind,
                                          const OsuArchiveImportLimits& limits) {
    try {
        return import_osu_archive_impl(archive_path_utf8, destination_root_utf8, kind, limits);
    } catch (const std::exception& exception) {
        OsuArchiveImportResult result;
        result.error = std::string("archive import failed: ") + exception.what();
        return result;
    } catch (...) {
        OsuArchiveImportResult result;
        result.error = "archive import failed with an unknown error";
        return result;
    }
}

}  // namespace tenriff::app
