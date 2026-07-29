#include "app/DifficultyTable.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

#include "config/SimpleJson.h"

namespace tenriff::app {

namespace {

constexpr std::uintmax_t kMaxDifficultyTableJsonBytes = 128u * 1024u * 1024u;

struct ParsedTableEntry {
    std::string md5;
    std::string sha256;
    std::string level;
};

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1u])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

bool is_hex_digest(std::string_view value, std::size_t expected_size) {
    if (value.size() != expected_size) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= static_cast<unsigned char>('0') && ch <= static_cast<unsigned char>('9')) ||
               (ch >= static_cast<unsigned char>('a') && ch <= static_cast<unsigned char>('f')) ||
               (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('F'));
    });
}

std::string normalize_digest(std::string_view value, std::size_t expected_size) {
    std::string normalized = trim_copy(value);
    if (!is_hex_digest(normalized, expected_size)) {
        return {};
    }
    return lower_ascii(std::move(normalized));
}

const config::JsonValue* find_value(const config::JsonObject& object, std::string_view key) {
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

std::optional<std::string> json_string_field(const config::JsonObject& object, std::string_view key) {
    const config::JsonValue* value = find_value(object, key);
    if (!value || !value->is_string()) {
        return std::nullopt;
    }
    return trim_copy(value->as_string());
}

std::optional<std::string> level_value_to_string(const config::JsonValue& value) {
    if (value.is_null()) {
        return std::string("0");
    }
    if (value.is_string()) {
        std::string level = trim_copy(value.as_string());
        return level.empty() ? std::optional<std::string>("0")
                             : std::optional<std::string>(std::move(level));
    }
    if (!value.is_number()) {
        return std::nullopt;
    }

    const double number = value.as_number();
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    const double integral = std::trunc(number);
    if (integral == number &&
        integral >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
        integral <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return std::to_string(static_cast<std::int64_t>(integral));
    }

    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << number;
    return out.str();
}

bool parse_level_order(const config::JsonValue* value,
                       std::vector<std::string>& levels,
                       std::vector<std::string>& warnings,
                       std::string& error) {
    levels.clear();
    if (!value) {
        return true;
    }
    const config::JsonArray* array = value->as_array();
    if (!array) {
        error = "Difficulty-table level_order must be an array.";
        return false;
    }

    for (std::size_t i = 0; i < array->size(); ++i) {
        const auto normalized = level_value_to_string((*array)[i]);
        if (!normalized.has_value()) {
            warnings.push_back("Ignored non-string/non-number level_order item " + std::to_string(i + 1u) + ".");
            continue;
        }
        if (std::find(levels.begin(), levels.end(), *normalized) != levels.end()) {
            warnings.push_back("Ignored duplicate difficulty-table level_order value '" + *normalized + "'.");
            continue;
        }
        levels.push_back(*normalized);
    }
    return true;
}

bool parse_data_array(const config::JsonArray& array,
                      std::vector<ParsedTableEntry>& entries,
                      std::vector<std::string>& warnings,
                      std::string& error) {
    entries.clear();
    entries.reserve(array.size());
    for (std::size_t i = 0; i < array.size(); ++i) {
        const config::JsonObject* object = array[i].as_object();
        if (!object) {
            warnings.push_back("Ignored non-object difficulty-table entry " + std::to_string(i + 1u) + ".");
            continue;
        }

        ParsedTableEntry entry;
        if (const auto md5 = json_string_field(*object, "md5"); md5.has_value()) {
            if (md5->empty()) {
                entry.md5.clear();
            } else {
                entry.md5 = normalize_digest(*md5, 32u);
                if (entry.md5.empty()) {
                    warnings.push_back("Ignored invalid MD5 in difficulty-table entry " +
                                       std::to_string(i + 1u) + ".");
                }
            }
        }
        if (const auto sha256 = json_string_field(*object, "sha256"); sha256.has_value()) {
            if (sha256->empty()) {
                entry.sha256.clear();
            } else {
                entry.sha256 = normalize_digest(*sha256, 64u);
                if (entry.sha256.empty()) {
                    warnings.push_back("Ignored invalid SHA-256 in difficulty-table entry " +
                                       std::to_string(i + 1u) + ".");
                }
            }
        }
        if (entry.md5.empty() && entry.sha256.empty()) {
            warnings.push_back("Ignored difficulty-table entry " + std::to_string(i + 1u) +
                               " because it has no valid MD5 or SHA-256 hash.");
            continue;
        }

        if (const config::JsonValue* level = find_value(*object, "level")) {
            const auto normalized = level_value_to_string(*level);
            if (!normalized.has_value()) {
                warnings.push_back("Difficulty-table entry " + std::to_string(i + 1u) +
                                   " has an invalid level; using 0.");
                entry.level = "0";
            } else {
                entry.level = *normalized;
            }
        } else {
            entry.level = "0";
        }
        entries.push_back(std::move(entry));
    }

    static_cast<void>(error);
    return true;
}

bool read_json_file(const std::filesystem::path& path, config::JsonValue& root, std::string& error) {
    std::error_code ec;
    const std::uintmax_t file_size = std::filesystem::file_size(path, ec);
    if (!ec && file_size > kMaxDifficultyTableJsonBytes) {
        error = "Difficulty-table JSON exceeds the 128 MiB safety limit.";
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open difficulty-table JSON: " + path.u8string();
        return false;
    }

    std::string content;
    if (!ec) {
        content.reserve(static_cast<std::size_t>(file_size));
    }
    std::array<char, 64u * 1024u> buffer{};
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count <= 0) {
            continue;
        }
        if (content.size() > static_cast<std::size_t>(kMaxDifficultyTableJsonBytes) -
                                 static_cast<std::size_t>(count)) {
            error = "Difficulty-table JSON exceeds the 128 MiB safety limit.";
            return false;
        }
        content.append(buffer.data(), static_cast<std::size_t>(count));
    }
    if (!file.eof()) {
        error = "Could not read difficulty-table JSON completely: " + path.u8string();
        return false;
    }

    if (content.size() >= 3u && static_cast<unsigned char>(content[0]) == 0xefu &&
        static_cast<unsigned char>(content[1]) == 0xbbu &&
        static_cast<unsigned char>(content[2]) == 0xbfu) {
        content.erase(0, 3u);
    }

    auto parsed = config::parse_json(content);
    if (!parsed.success() || !parsed.root.has_value()) {
        error = parsed.error.empty() ? "Could not parse difficulty-table JSON."
                                     : "Could not parse difficulty-table JSON: " + parsed.error;
        return false;
    }
    root = std::move(*parsed.root);
    return true;
}

bool is_local_relative_data_url(std::string_view raw_value) {
    const std::string value = trim_copy(raw_value);
    if (value.empty() || value.find("://") != std::string::npos || value.front() == '/' ||
        value.front() == '\\') {
        return false;
    }
    if (value.size() >= 2u && std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
        value[1] == ':') {
        return false;
    }
    try {
        const std::filesystem::path path = std::filesystem::u8path(value.begin(), value.end());
        return !path.is_absolute() && !path.has_root_name() && !path.has_root_directory();
    } catch (...) {
        return false;
    }
}

std::unordered_map<std::string, int> build_level_order_lookup(const std::vector<std::string>& levels) {
    std::unordered_map<std::string, int> lookup;
    lookup.reserve(levels.size());
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const std::string normalized = trim_copy(levels[i]);
        if (normalized.empty() || lookup.find(normalized) != lookup.end()) {
            continue;
        }
        lookup.emplace(normalized, static_cast<int>(i));
    }
    return lookup;
}

}  // namespace

std::optional<DifficultyTableMatch> DifficultyTable::lookup(const ChartFileHashes& hashes) const {
    return lookup(hashes.md5, hashes.sha256);
}

std::optional<DifficultyTableMatch> DifficultyTable::lookup(std::string_view md5,
                                                            std::string_view sha256) const {
    std::size_t entry_index = entries_.size();
    const std::string normalized_sha256 = normalize_digest(sha256, 64u);
    if (!normalized_sha256.empty()) {
        const auto sha_it = by_sha256_.find(normalized_sha256);
        if (sha_it != by_sha256_.end()) {
            entry_index = sha_it->second;
        }
    }
    if (entry_index == entries_.size()) {
        const std::string normalized_md5 = normalize_digest(md5, 32u);
        if (!normalized_md5.empty()) {
            const auto md5_it = by_md5_.find(normalized_md5);
            if (md5_it != by_md5_.end()) {
                entry_index = md5_it->second;
            }
        }
    }
    if (entry_index >= entries_.size()) {
        return std::nullopt;
    }

    const Entry& entry = entries_[entry_index];
    return DifficultyTableMatch{name_, symbol_, entry.level, entry.order};
}

DifficultyTableLoadResult load_difficulty_table(const std::filesystem::path& path,
                                                const DifficultyTableLoadOptions& options) {
    DifficultyTableLoadResult result;
    if (path.empty()) {
        result.error = "Difficulty-table path is empty.";
        return result;
    }

    config::JsonValue selected_root;
    if (!read_json_file(path, selected_root, result.error)) {
        return result;
    }

    std::string table_name;
    std::string table_symbol;
    std::vector<std::string> level_order;
    const config::JsonArray* data_array = selected_root.as_array();
    config::JsonValue referenced_root;
    if (data_array) {
        table_name = trim_copy(options.name);
        table_symbol = trim_copy(options.symbol);
        level_order = options.level_order;
        if (table_name.empty() || table_symbol.empty()) {
            result.error = "Direct difficulty-table data arrays require non-empty name and symbol options.";
            return result;
        }
    } else {
        const config::JsonObject* header = selected_root.as_object();
        if (!header) {
            result.error = "Difficulty-table JSON must be a data array or a local header object.";
            return result;
        }

        const auto name = json_string_field(*header, "name");
        const auto symbol = json_string_field(*header, "symbol");
        const auto data_url = json_string_field(*header, "data_url");
        if (!name.has_value() || name->empty() || !symbol.has_value() || symbol->empty() ||
            !data_url.has_value() || data_url->empty()) {
            result.error = "Difficulty-table header requires non-empty name, symbol, and data_url strings.";
            return result;
        }
        if (!is_local_relative_data_url(*data_url)) {
            result.error = "Difficulty-table data_url must be a local relative path; network and absolute URLs are not fetched.";
            return result;
        }

        table_name = *name;
        table_symbol = *symbol;
        if (!parse_level_order(find_value(*header, "level_order"), level_order, result.warnings, result.error)) {
            return result;
        }
        if (find_value(*header, "level_order") == nullptr) {
            level_order = options.level_order;
        }

        std::filesystem::path relative_data_path;
        try {
            relative_data_path = std::filesystem::u8path(data_url->begin(), data_url->end());
        } catch (...) {
            result.error = "Difficulty-table data_url is not valid UTF-8.";
            return result;
        }
        const std::filesystem::path data_path = (path.parent_path() / relative_data_path).lexically_normal();
        if (!read_json_file(data_path, referenced_root, result.error)) {
            return result;
        }
        data_array = referenced_root.as_array();
        if (!data_array) {
            result.error = "Difficulty-table data_url must resolve to a standard JSON array.";
            return result;
        }
    }

    std::vector<ParsedTableEntry> parsed_entries;
    if (!parse_data_array(*data_array, parsed_entries, result.warnings, result.error)) {
        return result;
    }

    result.table.name_ = std::move(table_name);
    result.table.symbol_ = std::move(table_symbol);
    result.table.entries_.reserve(parsed_entries.size());
    result.table.by_md5_.reserve(parsed_entries.size());
    result.table.by_sha256_.reserve(parsed_entries.size());
    const auto level_lookup = build_level_order_lookup(level_order);

    for (std::size_t i = 0; i < parsed_entries.size(); ++i) {
        const ParsedTableEntry& parsed_entry = parsed_entries[i];
        bool add_md5 = !parsed_entry.md5.empty();
        bool add_sha256 = !parsed_entry.sha256.empty();
        if (add_md5 && result.table.by_md5_.find(parsed_entry.md5) != result.table.by_md5_.end()) {
            add_md5 = false;
            result.warnings.push_back("Ignored duplicate MD5 in usable difficulty-table entry " +
                                      std::to_string(i + 1u) + ".");
        }
        if (add_sha256 &&
            result.table.by_sha256_.find(parsed_entry.sha256) != result.table.by_sha256_.end()) {
            add_sha256 = false;
            result.warnings.push_back("Ignored duplicate SHA-256 in usable difficulty-table entry " +
                                      std::to_string(i + 1u) + ".");
        }
        if (!add_md5 && !add_sha256) {
            continue;
        }

        int order = -1;
        const auto order_it = level_lookup.find(parsed_entry.level);
        if (order_it != level_lookup.end()) {
            order = order_it->second;
        }
        const std::size_t entry_index = result.table.entries_.size();
        result.table.entries_.push_back(DifficultyTable::Entry{parsed_entry.level, order});
        if (add_md5) {
            result.table.by_md5_.emplace(parsed_entry.md5, entry_index);
        }
        if (add_sha256) {
            result.table.by_sha256_.emplace(parsed_entry.sha256, entry_index);
        }
    }

    if (result.table.entries_.empty()) {
        result.warnings.push_back("Difficulty table contains no usable hashed chart entries.");
    }
    return result;
}

DifficultyTableLoadResult load_difficulty_table_utf8(std::string_view path,
                                                     const DifficultyTableLoadOptions& options) {
    if (path.empty()) {
        DifficultyTableLoadResult result;
        result.error = "Difficulty-table path is empty.";
        return result;
    }
    try {
        return load_difficulty_table(std::filesystem::u8path(path.begin(), path.end()), options);
    } catch (...) {
        DifficultyTableLoadResult result;
        result.error = "Difficulty-table path is not valid UTF-8.";
        return result;
    }
}

}  // namespace tenriff::app
