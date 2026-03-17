#include "chart/BmsParser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tenriff::chart {

namespace {

std::string trim(std::string_view view) {
    std::size_t begin = 0;
    std::size_t end = view.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(view[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(view[end - 1]))) {
        --end;
    }
    return std::string(view.substr(begin, end - begin));
}

void to_upper_ascii(std::string& value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
}

double parse_double(std::string_view token, bool& ok) {
    std::string trimmed = trim(token);  // Store trimmed result in stable string
    if (trimmed.empty()) {
        ok = false;
        return 0.0;
    }
    double result = 0.0;
    std::istringstream ss(trimmed);
    ss.imbue(std::locale::classic());  // Locale-independent parsing
    ss >> result;
    // Check for complete consumption: no failure AND no remaining characters.
    ok = !ss.fail() && (ss.peek() == std::char_traits<char>::eof());
    return result;
}

bool parse_int(std::string_view token, int& out_value) {
    std::string trimmed = trim(token);  // Store in stable string
    if (trimmed.empty()) {
        return false;
    }
    int value = 0;
    auto first = trimmed.data();
    auto last = trimmed.data() + trimmed.size();
    auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    out_value = value;
    return true;
}

std::string strip_comment(std::string_view view) {
    auto pos = view.find(';');
    if (pos != std::string_view::npos) {
        view = view.substr(0, pos);
    }
    return trim(view);
}

void add_message(std::vector<BmsParseMessage>& messages, BmsParseSeverity severity, std::size_t line, std::string text) {
    messages.push_back(BmsParseMessage{severity, line, std::move(text)});
}

std::string remove_bom(std::string_view content) {
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.remove_prefix(3);
    }
    return std::string(content);
}

bool is_valid_utf8(std::string_view content) {
    std::size_t index = 0;
    while (index < content.size()) {
        const auto lead = static_cast<unsigned char>(content[index]);
        if (lead <= 0x7F) {
            ++index;
            continue;
        }

        std::size_t width = 0;
        uint32_t code_point = 0;
        if ((lead & 0xE0) == 0xC0) {
            width = 2;
            code_point = static_cast<uint32_t>(lead & 0x1F);
        } else if ((lead & 0xF0) == 0xE0) {
            width = 3;
            code_point = static_cast<uint32_t>(lead & 0x0F);
        } else if ((lead & 0xF8) == 0xF0) {
            width = 4;
            code_point = static_cast<uint32_t>(lead & 0x07);
        } else {
            return false;
        }

        if (index + width > content.size()) {
            return false;
        }

        for (std::size_t offset = 1; offset < width; ++offset) {
            const auto next = static_cast<unsigned char>(content[index + offset]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | static_cast<uint32_t>(next & 0x3F);
        }

        if ((width == 2 && code_point < 0x80) ||
            (width == 3 && code_point < 0x800) ||
            (width == 4 && (code_point < 0x10000 || code_point > 0x10FFFF)) ||
            (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            return false;
        }

        index += width;
    }
    return true;
}

#ifdef _WIN32
std::string wide_to_utf8(std::wstring_view content) {
    if (content.empty()) {
        return {};
    }
    if (content.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }

    const int wide_size = static_cast<int>(content.size());
    const int utf8_size =
        WideCharToMultiByte(CP_UTF8, 0, content.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(utf8_size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, content.data(), wide_size, utf8.data(), utf8_size, nullptr, nullptr) <= 0) {
        return {};
    }
    return utf8;
}

std::optional<std::wstring> multibyte_to_wide(UINT code_page, std::string_view content) {
    if (content.empty()) {
        return std::wstring{};
    }
    if (content.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }

    const int narrow_size = static_cast<int>(content.size());
    const int wide_size = MultiByteToWideChar(code_page, 0, content.data(), narrow_size, nullptr, 0);
    if (wide_size <= 0) {
        return std::nullopt;
    }

    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(code_page, 0, content.data(), narrow_size, wide.data(), wide_size) <= 0) {
        return std::nullopt;
    }
    return wide;
}

std::optional<std::string> decode_utf16_bom(std::string_view content) {
    if (content.size() < 2) {
        return std::nullopt;
    }

    const auto first = static_cast<unsigned char>(content[0]);
    const auto second = static_cast<unsigned char>(content[1]);
    const bool little_endian = (first == 0xFF && second == 0xFE);
    const bool big_endian = (first == 0xFE && second == 0xFF);
    if (!little_endian && !big_endian) {
        return std::nullopt;
    }

    std::wstring wide;
    wide.reserve((content.size() - 2) / 2);
    for (std::size_t index = 2; index + 1 < content.size(); index += 2) {
        const auto high = static_cast<unsigned char>(content[index]);
        const auto low = static_cast<unsigned char>(content[index + 1]);
        const uint16_t code_unit = little_endian
                                       ? static_cast<uint16_t>(high | (low << 8))
                                       : static_cast<uint16_t>((high << 8) | low);
        wide.push_back(static_cast<wchar_t>(code_unit));
    }

    return wide_to_utf8(wide);
}
#endif

std::string normalize_bms_text(std::string_view content) {
    if (content.empty()) {
        return {};
    }

#ifdef _WIN32
    if (auto utf16 = decode_utf16_bom(content); utf16.has_value()) {
        return *utf16;
    }
#endif

    if (is_valid_utf8(content)) {
        return std::string(content);
    }

#ifdef _WIN32
    if (auto wide = multibyte_to_wide(932, content); wide.has_value()) {
        std::string utf8 = wide_to_utf8(*wide);
        if (!utf8.empty()) {
            return utf8;
        }
    }
#endif

    return std::string(content);
}

bool validate_measure_token_length(const std::string& data) {
    return (data.size() % 2) == 0;
}

bool is_measure_command_key(std::string_view key) {
    if (key.size() != 5) {
        return false;
    }
    return std::all_of(key.begin(), key.begin() + 3, [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

std::string parse_header_value(std::string_view tail) {
    std::string value = trim(tail);
    if (!value.empty() && value.front() == ':') {
        value = trim(std::string_view(value).substr(1));
    }
    return value;
}

std::string normalize_mode_token(std::string_view token) {
    std::string normalized;
    normalized.reserve(token.size());
    for (unsigned char ch : token) {
        if (std::isspace(ch) != 0 || ch == '_' || ch == '-') {
            continue;
        }
        if (ch >= 'a' && ch <= 'z') {
            normalized.push_back(static_cast<char>(ch - ('a' - 'A')));
        } else {
            normalized.push_back(static_cast<char>(ch));
        }
    }
    return normalized;
}

int key_count_from_mode_token(std::string_view token) {
    const std::string normalized = normalize_mode_token(token);
    if (normalized == "4K" || normalized == "4KEY" || normalized == "4KEYS" || normalized == "KEYS4") {
        return 4;
    }
    if (normalized == "5K" || normalized == "5KEY" || normalized == "5KEYS" || normalized == "KEYS5") {
        return 5;
    }
    if (normalized == "6K" || normalized == "6KEY" || normalized == "6KEYS" || normalized == "KEYS6") {
        return 6;
    }
    if (normalized == "7K" || normalized == "7KEY" || normalized == "7KEYS" || normalized == "KEYS7") {
        return 7;
    }
    if (normalized == "8K" || normalized == "8KEY" || normalized == "8KEYS" || normalized == "KEYS8") {
        return 8;
    }
    if (normalized == "9K" || normalized == "9KEY" || normalized == "9KEYS" || normalized == "KEYS9") {
        return 9;
    }
    if (normalized == "10K" || normalized == "10KEY" || normalized == "10KEYS" || normalized == "KEYS10") {
        return 10;
    }
    if (normalized == "16K" || normalized == "16KEY" || normalized == "16KEYS" || normalized == "KEYS16") {
        return 16;
    }
    return 0;
}

bool should_store_index_header(std::string_view key) {
    if (key == "BPM" || key == "PLAYER" || key == "GENRE" || key == "TITLE" || key == "ARTIST" ||
        key == "SUBTITLE" || key == "DIFFICULTY" || key == "PLAYLEVEL" || key == "RANK" ||
        key == "TOTAL" || key == "VOLWAV" || key == "LNOBJ") {
        return true;
    }
    if (key_count_from_mode_token(key) > 0) {
        return true;
    }
    const std::string normalized_key = normalize_mode_token(key);
    return normalized_key == "PLAYMODE" || normalized_key == "KEYMODE";
}

bool should_retain_command_for_index(std::string_view channel) {
    if (channel == "02" || channel == "03" || channel == "08" || channel == "09") {
        return true;
    }
    return channel.size() == 2 && (channel[0] == '1' || channel[0] == '2' || channel[0] == '5' || channel[0] == '6');
}

bool is_note_lane_channel(std::string_view channel);
std::string canonical_lane_channel(std::string_view channel);

int detect_explicit_key_count(const BmsChart& chart) {
    for (const auto& [key, value] : chart.headers) {
        if (const int from_key = key_count_from_mode_token(key); from_key > 0) {
            return from_key;
        }
        const std::string normalized_key = normalize_mode_token(key);
        if (normalized_key == "PLAYMODE" || normalized_key == "KEYMODE") {
            if (const int from_value = key_count_from_mode_token(value); from_value > 0) {
                return from_value;
            }
        }
    }
    return 0;
}

std::vector<std::string> collect_lane_channels(const BmsChart& chart) {
    std::vector<std::string> lane_channels;
    std::unordered_set<std::string> seen;
    lane_channels.reserve(chart.commands.size());

    for (const auto& command : chart.commands) {
        if (!is_note_lane_channel(command.channel)) {
            continue;
        }
        const std::string canonical = canonical_lane_channel(command.channel);
        if (canonical.empty()) {
            continue;
        }
        if (seen.emplace(canonical).second) {
            lane_channels.push_back(canonical);
        }
    }

    std::sort(lane_channels.begin(), lane_channels.end());
    return lane_channels;
}

bool contains_channel(const std::vector<std::string>& lane_channels, std::string_view channel) {
    return std::find(lane_channels.begin(), lane_channels.end(), channel) != lane_channels.end();
}

bool matches_only_allowed_channels(const std::vector<std::string>& lane_channels,
                                   std::initializer_list<std::string_view> allowed) {
    for (const auto& channel : lane_channels) {
        bool matched = false;
        for (std::string_view allowed_channel : allowed) {
            if (channel == allowed_channel) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return !lane_channels.empty();
}

bool is_player_one_header(const BmsChart& chart) {
    auto it = chart.headers.find("PLAYER");
    if (it == chart.headers.end()) {
        return false;
    }
    return trim(it->second) == "1";
}

struct DeclaredLayout {
    int key_count = 0;
    std::string layout_label;
    std::vector<std::string> canonical_channels;
};

template <std::size_t N>
int template_position(std::string_view channel, const std::array<std::string_view, N>& channels) {
    int index = 1;
    for (const std::string_view candidate : channels) {
        if (candidate == channel) {
            return index;
        }
        ++index;
    }
    return 0;
}

template <std::size_t N>
int highest_template_position(const std::vector<std::string>& lane_channels,
                              const std::array<std::string_view, N>& channels) {
    int highest = 0;
    for (const auto& channel : lane_channels) {
        highest = (std::max)(highest, template_position(channel, channels));
    }
    return highest;
}

template <std::size_t N>
bool matches_only_allowed_channels(const std::vector<std::string>& lane_channels,
                                   const std::array<std::string_view, N>& allowed) {
    for (const auto& channel : lane_channels) {
        bool matched = false;
        for (const std::string_view allowed_channel : allowed) {
            if (channel == allowed_channel) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return !lane_channels.empty();
}

bool contains_any_channel(const std::vector<std::string>& lane_channels,
                          std::initializer_list<std::string_view> allowed) {
    for (std::string_view allowed_channel : allowed) {
        if (contains_channel(lane_channels, allowed_channel)) {
            return true;
        }
    }
    return false;
}

DeclaredLayout detect_declared_layout(const BmsChart& chart, std::string_view source_extension = {}) {
    static constexpr std::array<std::string_view, 6> kSpFiveTemplate = {"11", "12", "13", "14", "15", "16"};
    static constexpr std::array<std::string_view, 8> kSpSevenTemplate = {"11", "12", "13", "14", "15", "16", "18", "19"};
    static constexpr std::array<std::string_view, 10> kDpTenTemplate = {"11", "12", "13", "14", "15",
                                                                        "21", "22", "23", "24", "25"};
    static constexpr std::array<std::string_view, 16> kDpFourteenPlusTwoTemplate = {"11", "12", "13", "14", "15", "16", "18", "19",
                                                                                    "21", "22", "23", "24", "25", "26", "28", "29"};
    static constexpr std::array<std::string_view, 9> kPmsNativeTemplate = {"11", "12", "13", "14", "15", "22", "23", "24", "25"};
    static constexpr std::array<std::string_view, 9> kPmsBmeTemplate = {"11", "12", "13", "14", "15", "16", "17", "18", "19"};

    const int explicit_key_count = detect_explicit_key_count(chart);
    const auto lane_channels = collect_lane_channels(chart);
    const bool has_two_player_channels = std::any_of(lane_channels.begin(), lane_channels.end(),
                                                     [](const std::string& channel) {
                                                         return !channel.empty() && channel[0] == '2';
                                                     });
    const bool is_pms_source = source_extension == ".pms";
    const bool uses_sp5_channels =
        matches_only_allowed_channels(lane_channels, kSpFiveTemplate) &&
        contains_channel(lane_channels, "16");
    const bool uses_sp7_channels =
        matches_only_allowed_channels(lane_channels, kSpSevenTemplate) &&
        (contains_channel(lane_channels, "18") || contains_channel(lane_channels, "19"));
    const bool uses_pms_native_channels =
        matches_only_allowed_channels(lane_channels, kPmsNativeTemplate) &&
        (contains_any_channel(lane_channels, {"22", "23", "24", "25"}) || is_pms_source || explicit_key_count == 9);
    const bool uses_pms_bme_channels =
        matches_only_allowed_channels(lane_channels, kPmsBmeTemplate) &&
        (contains_any_channel(lane_channels, {"16", "17", "18", "19"}) || is_pms_source || explicit_key_count == 9);
    const bool uses_dp14_plus_two_channels =
        has_two_player_channels &&
        matches_only_allowed_channels(lane_channels, kDpFourteenPlusTwoTemplate) &&
        contains_any_channel(lane_channels, {"16", "18", "19"}) &&
        contains_any_channel(lane_channels, {"26", "28", "29"});
    const bool fits_standard_sp_five = matches_only_allowed_channels(lane_channels, kSpFiveTemplate);
    const bool fits_standard_sp_seven = matches_only_allowed_channels(lane_channels, kSpSevenTemplate);
    const bool fits_standard_dp_ten = has_two_player_channels &&
                                      matches_only_allowed_channels(lane_channels, kDpTenTemplate);
    const bool fits_standard_dp_fourteen_plus_two = has_two_player_channels &&
                                                    matches_only_allowed_channels(lane_channels, kDpFourteenPlusTwoTemplate);

    if (!has_two_player_channels) {
        if (explicit_key_count == 5 && uses_sp5_channels) {
            return DeclaredLayout{6, "5+1 SP", {"11", "12", "13", "14", "15", "16"}};
        }
        if (explicit_key_count == 7 && uses_sp7_channels) {
            return DeclaredLayout{8, "7+1 SP", {"11", "12", "13", "14", "15", "16", "18", "19"}};
        }
    }

    if ((explicit_key_count == 9 || is_pms_source) && uses_pms_native_channels) {
        return DeclaredLayout{9, "PMS 9K", {"11", "12", "13", "14", "15", "22", "23", "24", "25"}};
    }
    if ((explicit_key_count == 9 || is_pms_source) && uses_pms_bme_channels) {
        return DeclaredLayout{9, "PMS 9K", {"11", "12", "13", "14", "15", "16", "17", "18", "19"}};
    }
    if ((explicit_key_count == 16 || uses_dp14_plus_two_channels) && uses_dp14_plus_two_channels) {
        return DeclaredLayout{16, "14+2 DP", {"11", "12", "13", "14", "15", "16", "18", "19",
                                              "21", "22", "23", "24", "25", "26", "28", "29"}};
    }

    if (explicit_key_count > 0) {
        return DeclaredLayout{explicit_key_count, {}, {}};
    }

    if ((is_player_one_header(chart) || !has_two_player_channels) && !has_two_player_channels) {
        if (uses_sp7_channels) {
            return DeclaredLayout{8, "7+1 SP", {"11", "12", "13", "14", "15", "16", "18", "19"}};
        }
        if (uses_sp5_channels) {
            return DeclaredLayout{6, "5+1 SP", {"11", "12", "13", "14", "15", "16"}};
        }
    }

    if (!has_two_player_channels && fits_standard_sp_seven &&
        contains_any_channel(lane_channels, {"18", "19"})) {
        return DeclaredLayout{8, "7+1 SP", std::vector<std::string>(kSpSevenTemplate.begin(), kSpSevenTemplate.end())};
    }
    if (!has_two_player_channels && fits_standard_sp_five && contains_channel(lane_channels, "16")) {
        return DeclaredLayout{6, "5+1 SP", std::vector<std::string>(kSpFiveTemplate.begin(), kSpFiveTemplate.end())};
    }
    if (fits_standard_dp_fourteen_plus_two &&
        contains_any_channel(lane_channels, {"16", "18", "19", "26", "28", "29"})) {
        return DeclaredLayout{
            16,
            "14+2 DP",
            std::vector<std::string>(kDpFourteenPlusTwoTemplate.begin(), kDpFourteenPlusTwoTemplate.end()),
        };
    }
    if (fits_standard_dp_ten) {
        return DeclaredLayout{10, {}, std::vector<std::string>(kDpTenTemplate.begin(), kDpTenTemplate.end())};
    }
    static constexpr std::array<std::string_view, 5> kSingleFiveTemplate = {"11", "12", "13", "14", "15"};
    if (!has_two_player_channels && matches_only_allowed_channels(lane_channels, kSingleFiveTemplate)) {
        const int inferred_key_count = highest_template_position(lane_channels, kSingleFiveTemplate);
        if (inferred_key_count >= 4) {
            return DeclaredLayout{inferred_key_count, {}, {}};
        }
    }

    if (!lane_channels.empty()) {
        const int inferred_key_count = static_cast<int>(lane_channels.size());
        if (inferred_key_count >= 4 && inferred_key_count <= 16) {
            return DeclaredLayout{inferred_key_count, {}, {}};
        }
    }

    return {};
}

bool is_note_lane_channel(std::string_view channel) {
    if (channel.size() != 2) {
        return false;
    }
    return channel[0] == '1' || channel[0] == '2' || channel[0] == '5' || channel[0] == '6';
}

std::string canonical_lane_channel(std::string_view channel) {
    std::string normalized(channel);
    to_upper_ascii(normalized);
    if (normalized.size() != 2) {
        return {};
    }
    if (normalized[0] == '5') {
        normalized[0] = '1';
    } else if (normalized[0] == '6') {
        normalized[0] = '2';
    }
    return normalized;
}

NoteLaneMapping build_ordered_lane_mapping(const std::vector<std::string>& lane_channels) {
    std::unordered_map<std::string, std::size_t> mapping;
    mapping.reserve(lane_channels.size() * 2u);
    for (std::size_t index = 0; index < lane_channels.size(); ++index) {
        const std::size_t lane = index + 1;
        const std::string& canonical = lane_channels[index];
        mapping[canonical] = lane;

        std::string long_note_channel = canonical;
        if (long_note_channel[0] == '1') {
            long_note_channel[0] = '5';
            mapping[long_note_channel] = lane;
        } else if (long_note_channel[0] == '2') {
            long_note_channel[0] = '6';
            mapping[long_note_channel] = lane;
        }
    }

    return mapping.empty() ? NoteLaneMapping::TenKeyDualPlayerDefault() : NoteLaneMapping(std::move(mapping));
}

NoteLaneMapping build_compact_lane_mapping(const BmsChart& chart) {
    return build_ordered_lane_mapping(collect_lane_channels(chart));
}

NoteLaneMapping build_lane_mapping(const BmsChart& chart, const DeclaredLayout& declared_layout) {
    if (!declared_layout.canonical_channels.empty()) {
        return build_ordered_lane_mapping(declared_layout.canonical_channels);
    }
    return build_compact_lane_mapping(chart);
}

std::string lower_extension(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

bool BmsParseResult::success() const {
    return std::none_of(messages.begin(), messages.end(), [](const BmsParseMessage& msg) {
        return msg.severity == BmsParseSeverity::Error;
    });
}

BmsParseResult BmsParser::parse(std::string_view content, const BmsParserOptions& options) const {
    BmsParseResult result;
    std::string cleaned = remove_bom(normalize_bms_text(content));

    std::istringstream stream(cleaned);
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(stream, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = strip_comment(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.front() == '*') {
            continue;
        }
        if (trimmed.front() != '#') {
            add_message(result.messages, BmsParseSeverity::Warning, line_number,
                        "Line ignored because it does not start with '#'.");
            continue;
        }

        std::string command = trim(std::string_view(trimmed).substr(1));
        auto separator_pos = command.find_first_of(" \t:");
        std::string key = separator_pos == std::string::npos ? command : command.substr(0, separator_pos);
        if (key.empty()) {
            add_message(result.messages, BmsParseSeverity::Error, line_number,
                        "Empty command key.");
            continue;
        }

        const std::string_view tail = separator_pos == std::string::npos
                                          ? std::string_view{}
                                          : std::string_view(command).substr(separator_pos);
        std::string_view measure_tail = tail;
        while (!measure_tail.empty() &&
               std::isspace(static_cast<unsigned char>(measure_tail.front())) != 0) {
            measure_tail.remove_prefix(1);
        }

        if (is_measure_command_key(key) && !measure_tail.empty() && measure_tail.front() == ':') {
            std::string measure_token = key.substr(0, 3);
            int measure_index = 0;
            if (!parse_int(measure_token, measure_index)) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Failed to parse measure index.");
                continue;
            }

            std::string channel_token = key.substr(3);
            channel_token = trim(channel_token);
            to_upper_ascii(channel_token);
            if (channel_token.size() != 2) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Channel token must have length 2.");
                continue;
            }

            measure_tail.remove_prefix(1);
            std::string data = trim(measure_tail);
            // Remove all spaces and tabs from data to ensure even-length token parsing.
            data.erase(std::remove_if(data.begin(), data.end(), [](unsigned char ch) {
                          return ch == ' ' || ch == '\t';
                      }),
                      data.end());

            if (channel_token != "02" && !validate_measure_token_length(data)) {
                auto severity = options.tolerant ? BmsParseSeverity::Warning : BmsParseSeverity::Error;
                add_message(result.messages, severity, line_number,
                            "Measure data must consist of pairs of characters.");
                if (!options.tolerant) {
                    continue;
                }
            }

            if (options.retain_nonessential_commands || should_retain_command_for_index(channel_token)) {
                result.chart.commands.push_back(BmsMeasureCommand{measure_index, channel_token, data});
            }
            continue;
        }

        std::string value = parse_header_value(tail);
        to_upper_ascii(key);

        if (key == "BPM" || key == "PLAYER" || key == "GENRE" || key == "TITLE" ||
            key == "ARTIST" || key == "SUBTITLE" || key == "DIFFICULTY" || key == "PLAYLEVEL" ||
            key == "RANK" || key == "TOTAL" || key == "VOLWAV") {
            result.chart.headers[key] = value;
            if (key == "BPM") {
                bool ok = false;
                double bpm_value = parse_double(value, ok);
                if (!ok) {
                    add_message(result.messages, BmsParseSeverity::Error, line_number,
                                "Failed to parse #BPM value as floating point.");
                } else {
                    result.chart.base_bpm = bpm_value;
                }
            }
            continue;
        }

        if (key.rfind("WAV", 0) == 0) {
            std::string slot = key.substr(3);
            if (slot.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "#WAV entry missing identifier.");
                continue;
            }
            if (options.retain_wav_bmp) {
                result.chart.wav[slot] = value;
            }
            continue;
        }

        if (key.rfind("BMP", 0) == 0) {
            std::string slot = key.substr(3);
            if (slot.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "#BMP entry missing identifier.");
                continue;
            }
            if (options.retain_wav_bmp) {
                result.chart.bmp[slot] = value;
            }
            continue;
        }

        if (key.rfind("BPM", 0) == 0 && key.size() > 3) {
            std::string slot = key.substr(3);
            bool ok = false;
            double bpm_value = parse_double(value, ok);
            if (!ok) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Failed to parse #BPMxx value as floating point.");
                continue;
            }
            result.chart.bpm[slot] = bpm_value;
            continue;
        }

        if (key.rfind("STOP", 0) == 0) {
            std::string slot = key.substr(4);
            if (slot.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "#STOP entry missing identifier.");
                continue;
            }
            bool ok = false;
            double value_ms = parse_double(value, ok);
            if (!ok) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Failed to parse #STOPxx value as floating point.");
                continue;
            }
            result.chart.stop[slot] = value_ms;
            continue;
        }

        if (options.retain_unknown_headers || should_store_index_header(key)) {
            result.chart.headers[key] = value;
        }
    }

    const DeclaredLayout declared_layout = detect_declared_layout(result.chart);
    result.chart.declared_key_count = declared_layout.key_count;
    result.chart.layout_label = declared_layout.layout_label;
    if (result.chart.declared_key_count > 0) {
        result.chart.lane_mapping = build_lane_mapping(result.chart, declared_layout);
    }

    return result;
}

BmsParseResult BmsParser::parseFile(const std::string& path, const BmsParserOptions& options) const {
    std::ifstream file;
#ifdef _WIN32
    try {
        file.open(std::filesystem::u8path(path), std::ios::binary);
    } catch (...) {
        file.open(path, std::ios::binary);
    }
#else
    file.open(path, std::ios::binary);
#endif
    if (!file) {
        BmsParseResult result;
        add_message(result.messages, BmsParseSeverity::Error, 0,
                    "Failed to open BMS file: " + path);
        return result;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    BmsParseResult result = parse(buffer.str(), options);
    const std::string extension = lower_extension(std::filesystem::path(std::filesystem::u8path(path)).extension().u8string());
    const DeclaredLayout declared_layout = detect_declared_layout(result.chart, extension);
    result.chart.declared_key_count = declared_layout.key_count;
    result.chart.layout_label = declared_layout.layout_label;
    if (result.chart.declared_key_count > 0) {
        result.chart.lane_mapping = build_lane_mapping(result.chart, declared_layout);
    }
    return result;
}

}  // namespace tenriff::chart
