#include "app/Lr2Course.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

std::string lower_ascii(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string decode_xml_entities(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size();) {
        if (value[i] != '&') {
            decoded.push_back(value[i++]);
            continue;
        }
        const std::size_t end = value.find(';', i + 1);
        if (end == std::string_view::npos) {
            decoded.push_back(value[i++]);
            continue;
        }
        const std::string_view entity = value.substr(i, end - i + 1);
        if (entity == "&amp;") decoded.push_back('&');
        else if (entity == "&lt;") decoded.push_back('<');
        else if (entity == "&gt;") decoded.push_back('>');
        else if (entity == "&quot;") decoded.push_back('"');
        else if (entity == "&apos;") decoded.push_back('\'');
        else {
            decoded.append(entity);
            i = end + 1;
            continue;
        }
        i = end + 1;
    }
    return decoded;
}

std::string encode_xml_entities(std::string_view value) {
    std::string encoded;
    encoded.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&': encoded += "&amp;"; break;
        case '<': encoded += "&lt;"; break;
        case '>': encoded += "&gt;"; break;
        case '"': encoded += "&quot;"; break;
        case '\'': encoded += "&apos;"; break;
        default: encoded.push_back(ch); break;
        }
    }
    return encoded;
}

std::string tag_text(std::string_view block, std::string_view tag) {
    const std::string lowered = lower_ascii(block);
    const std::string open = "<" + std::string(tag) + ">";
    const std::string close = "</" + std::string(tag) + ">";
    const std::size_t begin = lowered.find(open);
    if (begin == std::string::npos) return {};
    const std::size_t value_begin = begin + open.size();
    const std::size_t end = lowered.find(close, value_begin);
    if (end == std::string::npos) return {};
    return trim_copy(block.substr(value_begin, end - value_begin));
}

int parse_int(std::string_view value) {
    int parsed = 0;
    const std::string trimmed = trim_copy(value);
    const auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed);
    return result.ec == std::errc{} && result.ptr == trimmed.data() + trimmed.size() ? parsed : 0;
}

bool normalize_course_hash(std::string_view value,
                           std::vector<std::string>& chart_md5) {
    std::string compact;
    compact.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isspace(ch)) continue;
        if (!std::isxdigit(ch)) return false;
        compact.push_back(static_cast<char>(std::tolower(ch)));
    }

    // LR2 prefixes every course hash with one 32-character rules block. The
    // remaining 32-character chunks are chart MD5 values in play order.
    if (compact.size() < 64u || compact.size() % 32u != 0u) return false;
    for (std::size_t offset = 32u; offset < compact.size(); offset += 32u) {
        chart_md5.push_back(compact.substr(offset, 32u));
    }
    return !chart_md5.empty();
}

}  // namespace

Lr2CourseLoadResult parse_lr2_course_text(std::string_view content) {
    Lr2CourseLoadResult result;
    const std::string utf8 = util::ensure_utf8_text(content);
    const std::string lowered = lower_ascii(utf8);

    std::size_t cursor = 0;
    int course_number = 0;
    while (true) {
        const std::size_t open = lowered.find("<course", cursor);
        if (open == std::string::npos) break;
        const std::size_t name_end = open + 7u;
        if (name_end >= lowered.size() ||
            (lowered[name_end] != '>' &&
             !std::isspace(static_cast<unsigned char>(lowered[name_end])))) {
            cursor = name_end;
            continue;
        }
        const std::size_t open_end = lowered.find('>', open + 7u);
        if (open_end == std::string::npos) break;
        const std::size_t close = lowered.find("</course>", open_end + 1u);
        if (close == std::string::npos) break;
        ++course_number;

        const std::string_view block(utf8.data() + open_end + 1u, close - open_end - 1u);
        Lr2CourseDefinition course;
        course.title = decode_xml_entities(tag_text(block, "title"));
        course.key_count = parse_int(tag_text(block, "line"));
        course.type = parse_int(tag_text(block, "type"));
        if (course.title.empty()) {
            course.title = "LR2 Course " + std::to_string(course_number);
        }
        if (!normalize_course_hash(tag_text(block, "hash"), course.chart_md5)) {
            result.warnings.push_back("Ignored LR2 course with an invalid hash list: " + course.title);
        } else {
            result.courses.push_back(std::move(course));
        }
        cursor = close + 9u;
    }

    if (result.courses.empty()) {
        result.error = "No playable LR2 course definitions were found.";
    }
    return result;
}

Lr2CourseLoadResult load_lr2_course_file(std::string_view path) {
    const std::filesystem::path file_path = util::path_from_utf8_lossy(path);
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        Lr2CourseLoadResult result;
        result.error = "Failed to open LR2 course file.";
        return result;
    }
    const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parse_lr2_course_text(bytes);
}

std::string serialize_lr2_course_text(const Lr2CourseDefinition& course) {
    if (course.chart_md5.empty()) {
        return {};
    }
    std::string hashes = "00000000000000000000000000005190";
    for (const auto& md5 : course.chart_md5) {
        const std::string normalized = lower_ascii(trim_copy(md5));
        if (normalized.size() != 32u ||
            !std::all_of(normalized.begin(), normalized.end(), [](unsigned char ch) {
                return std::isxdigit(ch) != 0;
            })) {
            return {};
        }
        hashes += normalized;
    }

    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
           "<courselist>\n"
           "  <course>\n"
           "    <title>" + encode_xml_entities(course.title.empty() ? "TenRiff Custom Course" : course.title) +
           "</title>\n"
           "    <line>" + std::to_string(std::max(0, course.key_count)) + "</line>\n"
           "    <hash>" + hashes + "</hash>\n"
           "    <type>" + std::to_string(course.type) + "</type>\n"
           "  </course>\n"
           "</courselist>\n";
}

Lr2CourseSaveResult save_lr2_course_file(std::string_view path,
                                         const Lr2CourseDefinition& course) {
    Lr2CourseSaveResult result;
    const std::string content = serialize_lr2_course_text(course);
    if (content.empty()) {
        result.error = "Course contains an invalid or empty MD5 list.";
        return result;
    }
    const std::filesystem::path file_path = util::path_from_utf8_lossy(path);
    std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        result.error = "Failed to create LR2 course file.";
        return result;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        result.error = "Failed while writing LR2 course file.";
    }
    return result;
}

Lr2CourseMatch match_lr2_course(const Lr2CourseDefinition& course,
                                const std::vector<SongEntry>& songs) {
    std::unordered_map<std::string, std::size_t> by_md5;
    by_md5.reserve(songs.size());
    for (std::size_t i = 0; i < songs.size(); ++i) {
        if (songs[i].md5.size() == 32u) {
            by_md5.try_emplace(lower_ascii(songs[i].md5), i);
        }
    }

    Lr2CourseMatch match;
    match.song_indices.reserve(course.chart_md5.size());
    for (const auto& md5 : course.chart_md5) {
        const auto found = by_md5.find(lower_ascii(md5));
        if (found == by_md5.end()) {
            match.missing_md5.push_back(md5);
        } else {
            match.song_indices.push_back(found->second);
        }
    }
    return match;
}

}  // namespace tenriff::app
