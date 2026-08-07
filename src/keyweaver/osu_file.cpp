#include "keyweaver/osu_file.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace keyweaver::osu {

namespace {

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool equalsIgnoreCase(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(lhs[index])) !=
            std::tolower(static_cast<unsigned char>(rhs[index]))) {
            return false;
        }
    }
    return true;
}

bool isSectionHeader(const std::string& line, std::string& name) {
    const std::string trimmed = trim(line);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        return false;
    }
    name = trimmed.substr(1, trimmed.size() - 2);
    return true;
}

// Splits "Key: Value" on the first colon. Returns false for comments, blank
// lines and anything without a colon.
bool splitKeyValue(const std::string& line, std::string& key, std::string& value) {
    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed.rfind("//", 0) == 0) {
        return false;
    }
    const std::size_t colon = trimmed.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    key = trim(trimmed.substr(0, colon));
    value = trim(trimmed.substr(colon + 1));
    return true;
}

int toInt(const std::string& value, int fallback) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return fallback;
    }
}

double toDouble(const std::string& value, double fallback) {
    try {
        return std::stod(trim(value));
    } catch (...) {
        return fallback;
    }
}

// Splits on commas, but stops after `limit - 1` splits so the final field keeps
// any commas it contains (custom hit sample filenames may have them).
std::vector<std::string> splitCsv(const std::string& line, std::size_t limit) {
    std::vector<std::string> fields;
    std::size_t cursor = 0;
    while (cursor <= line.size()) {
        if (limit > 0 && fields.size() + 1 == limit) {
            fields.push_back(line.substr(cursor));
            return fields;
        }
        const std::size_t comma = line.find(',', cursor);
        if (comma == std::string::npos) {
            fields.push_back(line.substr(cursor));
            return fields;
        }
        fields.push_back(line.substr(cursor, comma - cursor));
        cursor = comma + 1;
    }
    return fields;
}

std::vector<std::string> splitLines(const std::string& bytes, std::string& newline) {
    newline = bytes.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::vector<std::string> lines;
    std::string current;
    for (const char ch : bytes) {
        if (ch == '\n') {
            if (!current.empty() && current.back() == '\r') {
                current.pop_back();
            }
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        if (current.back() == '\r') {
            current.pop_back();
        }
        lines.push_back(current);
    }
    return lines;
}

std::optional<HitObject> parseHitObject(const std::string& line) {
    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed.rfind("//", 0) == 0) {
        return std::nullopt;
    }
    const auto fields = splitCsv(trimmed, 6);
    if (fields.size() < 4) {
        return std::nullopt;
    }

    HitObject object;
    object.rawLine = trimmed;
    object.x = toInt(fields[0], 0);
    object.y = toInt(fields[1], kManiaY);
    object.time = toInt(fields[2], 0);
    object.type = toInt(fields[3], 1);
    object.hitSound = fields.size() > 4 ? toInt(fields[4], 0) : 0;

    const bool isHold = (object.type & kHoldFlag) != 0;
    if (fields.size() > 5) {
        const std::string tail = fields[5];
        if (isHold) {
            // Spec form is "endTime:hitSample", but O2Jam/BMS converters emit
            // "endTime,hitSample". Cut at whichever separator comes first so the
            // sample keeps all five of its fields either way.
            const std::size_t separator = tail.find_first_of(":,");
            if (separator == std::string::npos) {
                object.endTime = toInt(tail, object.time);
                object.hitSample = "0:0:0:0:";
            } else {
                object.endTime = toInt(tail.substr(0, separator), object.time);
                object.hitSample = tail.substr(separator + 1);
            }
        } else {
            object.hitSample = tail;
        }
    } else if (isHold) {
        object.endTime = object.time;
    }

    // hitSample is "normalSet:additionSet:index:volume:filename". Anything that
    // does not carry all five fields makes osu! reject the whole beatmap, so
    // fall back to the timing point's defaults rather than pass it through.
    if (std::count(object.hitSample.begin(), object.hitSample.end(), ':') != 4) {
        object.hitSample = "0:0:0:0:";
    }
    return object;
}

}  // namespace

int columnToX(int column, int keyCount) {
    if (keyCount <= 0) {
        return 256;
    }
    const int clamped = std::clamp(column, 0, keyCount - 1);
    return (clamped * 512 + 256) / keyCount;
}

int xToColumn(int x, int keyCount) {
    if (keyCount <= 0) {
        return 0;
    }
    const int column = (x * keyCount) / 512;
    return std::clamp(column, 0, keyCount - 1);
}

Section* File::findSection(const std::string& name) {
    for (auto& section : sections) {
        if (equalsIgnoreCase(section.name, name)) {
            return &section;
        }
    }
    return nullptr;
}

const Section* File::findSection(const std::string& name) const {
    return const_cast<File*>(this)->findSection(name);
}

std::optional<std::string> File::getField(const std::string& section, const std::string& key) const {
    const Section* target = findSection(section);
    if (target == nullptr) {
        return std::nullopt;
    }
    for (const auto& line : target->lines) {
        std::string lineKey;
        std::string lineValue;
        if (splitKeyValue(line, lineKey, lineValue) && equalsIgnoreCase(lineKey, key)) {
            return lineValue;
        }
    }
    return std::nullopt;
}

void File::setField(const std::string& section, const std::string& key, const std::string& value) {
    Section* target = findSection(section);
    if (target == nullptr) {
        sections.push_back(Section{section, {key + ":" + value}});
        return;
    }
    for (auto& line : target->lines) {
        std::string lineKey;
        std::string lineValue;
        if (splitKeyValue(line, lineKey, lineValue) && equalsIgnoreCase(lineKey, key)) {
            // Keep the original spacing convention of the file ("Key: v" vs "Key:v").
            const std::size_t colon = line.find(':');
            const bool spaced = colon + 1 < line.size() && line[colon + 1] == ' ';
            line = line.substr(0, colon + 1) + (spaced ? " " : "") + value;
            return;
        }
    }
    // Append before any trailing blank lines so the section stays tidy.
    std::size_t insertAt = target->lines.size();
    while (insertAt > 0 && trim(target->lines[insertAt - 1]).empty()) {
        --insertAt;
    }
    target->lines.insert(target->lines.begin() + static_cast<std::ptrdiff_t>(insertAt), key + ":" + value);
}

ParseResult parseFile(const std::string& rawBytes) {
    ParseResult result;
    std::string bytes = rawBytes;
    if (bytes.rfind("\xEF\xBB\xBF", 0) == 0) {
        result.file.hasBom = true;
        bytes.erase(0, 3);
    }

    const auto lines = splitLines(bytes, result.file.newline);
    if (lines.empty()) {
        result.error = "empty file";
        return result;
    }

    File& file = result.file;
    Section* current = nullptr;
    for (const auto& line : lines) {
        std::string sectionName;
        if (isSectionHeader(line, sectionName)) {
            file.sections.push_back(Section{sectionName, {}});
            current = &file.sections.back();
            continue;
        }
        if (current == nullptr) {
            file.preamble.push_back(line);
            continue;
        }
        current->lines.push_back(line);
    }

    bool sawFormatHeader = false;
    for (const auto& line : file.preamble) {
        if (trim(line).find("osu file format") != std::string::npos) {
            sawFormatHeader = true;
            break;
        }
    }
    if (!sawFormatHeader) {
        result.error = "missing \"osu file format\" header";
        return result;
    }

    file.mode = toInt(file.getField("General", "Mode").value_or("0"), 0);
    file.keyCount = static_cast<int>(toDouble(file.getField("Difficulty", "CircleSize").value_or("0"), 0.0));
    file.version = file.getField("Metadata", "Version").value_or("");
    file.title = file.getField("Metadata", "Title").value_or("");
    file.artist = file.getField("Metadata", "Artist").value_or("");
    file.creator = file.getField("Metadata", "Creator").value_or("");

    if (const Section* hitObjects = file.findSection("HitObjects")) {
        file.hitObjects.reserve(hitObjects->lines.size());
        for (const auto& line : hitObjects->lines) {
            if (auto object = parseHitObject(line)) {
                file.hitObjects.push_back(std::move(*object));
            }
        }
    }

    result.ok = true;
    return result;
}

std::string formatHitObject(const HitObject& object) {
    std::string line;
    line += std::to_string(object.x);
    line += ',';
    line += std::to_string(object.y);
    line += ',';
    line += std::to_string(object.time);
    line += ',';
    line += std::to_string(object.type);
    line += ',';
    line += std::to_string(object.hitSound);
    line += ',';
    if ((object.type & kHoldFlag) != 0) {
        line += std::to_string(object.endTime.value_or(object.time));
        line += ':';
    }
    line += std::count(object.hitSample.begin(), object.hitSample.end(), ':') == 4
                ? object.hitSample
                : "0:0:0:0:";
    return line;
}

std::string serializeFile(const File& file) {
    std::string out;
    if (file.hasBom) {
        out += "\xEF\xBB\xBF";
    }
    const std::string& nl = file.newline;

    for (const auto& line : file.preamble) {
        out += line;
        out += nl;
    }
    for (const auto& section : file.sections) {
        out += '[';
        out += section.name;
        out += ']';
        out += nl;
        for (const auto& line : section.lines) {
            out += line;
            out += nl;
        }
    }
    return out;
}

}  // namespace keyweaver::osu
