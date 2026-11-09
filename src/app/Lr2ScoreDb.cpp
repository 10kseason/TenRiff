#include "app/Lr2ScoreDb.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <unordered_map>

namespace tenriff::app {

namespace {

constexpr std::size_t kMaxDatabaseBytes = 512u * 1024u * 1024u;
constexpr std::size_t kMaxPayloadBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaxRows = 1'000'000;
constexpr int kMaxTreeDepth = 40;

struct Value {
    enum class Kind { Null, Integer, Real, Text, Blob };
    Kind kind = Kind::Null;
    int64_t integer = 0;
    std::string text;
};

class Reader {
public:
    explicit Reader(std::string_view bytes) : bytes_(bytes) {}

    bool open(std::string& error) {
        static constexpr char kMagic[] = "SQLite format 3";
        if (bytes_.size() < 100 || bytes_.compare(0, 16, std::string_view(kMagic, 16)) != 0) {
            error = "Not an SQLite database.";
            return false;
        }
        page_size_ = be16(16);
        if (page_size_ == 1) page_size_ = 65536;
        if (page_size_ < 512 || (page_size_ & (page_size_ - 1)) != 0) {
            error = "Unsupported SQLite page size.";
            return false;
        }
        const std::size_t reserved = byte(20);
        if (reserved + 480 > page_size_) {
            error = "Unsupported SQLite reserved space.";
            return false;
        }
        usable_ = page_size_ - reserved;
        encoding_ = be32(56);
        if (encoding_ < 1 || encoding_ > 3) {
            error = "Unsupported SQLite text encoding.";
            return false;
        }
        page_count_ = bytes_.size() / page_size_;
        if (page_count_ == 0) {
            error = "Truncated SQLite database.";
            return false;
        }
        return true;
    }

    // Visits every record of the table b-tree rooted at `root`.
    template <typename Visitor>
    bool walk_table(std::size_t root, Visitor&& visit, std::string& error) {
        visited_pages_ = 0;
        return walk(root, 0, visit, error);
    }

private:
    [[nodiscard]] std::size_t byte(std::size_t offset) const {
        return static_cast<unsigned char>(bytes_[offset]);
    }
    [[nodiscard]] std::size_t be16(std::size_t offset) const {
        return (byte(offset) << 8) | byte(offset + 1);
    }
    [[nodiscard]] std::size_t be32(std::size_t offset) const {
        return (byte(offset) << 24) | (byte(offset + 1) << 16) | (byte(offset + 2) << 8) | byte(offset + 3);
    }

    // Reads a varint inside [offset, limit). Returns false on overrun.
    bool varint(std::size_t& offset, std::size_t limit, uint64_t& out) const {
        out = 0;
        for (int i = 0; i < 9; ++i) {
            if (offset >= limit) return false;
            const uint64_t b = byte(offset++);
            if (i == 8) {
                out = (out << 8) | b;
                return true;
            }
            out = (out << 7) | (b & 0x7f);
            if ((b & 0x80) == 0) return true;
        }
        return true;
    }

    template <typename Visitor>
    bool walk(std::size_t page, int depth, Visitor& visit, std::string& error) {
        if (page == 0 || page > page_count_ || depth > kMaxTreeDepth || ++visited_pages_ > page_count_) {
            error = "Corrupt SQLite table tree.";
            return false;
        }
        const std::size_t base = (page - 1) * page_size_;
        const std::size_t end = base + page_size_;
        const std::size_t header = base + (page == 1 ? 100 : 0);
        const std::size_t type = byte(header);
        if (type != 0x0d && type != 0x05) {
            error = "Unexpected SQLite page type.";
            return false;
        }
        const std::size_t cells = be16(header + 3);
        const std::size_t pointers = header + (type == 0x05 ? 12 : 8);
        if (pointers + cells * 2 > end) {
            error = "Corrupt SQLite cell index.";
            return false;
        }
        for (std::size_t i = 0; i < cells; ++i) {
            std::size_t cell = base + be16(pointers + i * 2);
            if (cell < pointers + cells * 2 || cell + (type == 0x05 ? 4 : 1) > end) {
                error = "Corrupt SQLite cell offset.";
                return false;
            }
            if (type == 0x05) {
                if (!walk(be32(cell), depth + 1, visit, error)) return false;
                continue;
            }
            uint64_t payload_size = 0;
            uint64_t rowid = 0;
            if (!varint(cell, end, payload_size) || !varint(cell, end, rowid) ||
                payload_size > kMaxPayloadBytes) {
                error = "Corrupt SQLite record header.";
                return false;
            }
            std::string payload;
            if (!read_payload(cell, end, static_cast<std::size_t>(payload_size), payload)) {
                error = "Corrupt SQLite record payload.";
                return false;
            }
            std::vector<Value> values;
            if (!decode_record(payload, values)) {
                error = "Corrupt SQLite record.";
                return false;
            }
            if (!visit(values)) {
                error = "Too many rows.";
                return false;
            }
        }
        if (type == 0x05) return walk(be32(header + 8), depth + 1, visit, error);
        return true;
    }

    bool read_payload(std::size_t cell, std::size_t page_end, std::size_t size, std::string& out) const {
        const std::size_t max_local = usable_ - 35;
        std::size_t local = size;
        if (size > max_local) {
            const std::size_t min_local = ((usable_ - 12) * 32 / 255) - 23;
            const std::size_t candidate = min_local + ((size - min_local) % (usable_ - 4));
            local = candidate <= max_local ? candidate : min_local;
        }
        if (cell + local > page_end) return false;
        out.assign(bytes_.substr(cell, local));
        if (local == size) return true;
        if (cell + local + 4 > page_end) return false;
        std::size_t overflow = be32(cell + local);
        std::size_t guard = 0;
        while (out.size() < size) {
            if (overflow == 0 || overflow > page_count_ || ++guard > page_count_) return false;
            const std::size_t base = (overflow - 1) * page_size_;
            const std::size_t take = (std::min)(size - out.size(), usable_ - 4);
            out.append(bytes_.substr(base + 4, take));
            overflow = be32(base);
        }
        return true;
    }

    bool decode_record(const std::string& payload, std::vector<Value>& values) const {
        const Reader record(payload);
        std::size_t offset = 0;
        uint64_t header_size = 0;
        if (!record.varint(offset, payload.size(), header_size) || header_size > payload.size() ||
            header_size < offset) {
            return false;
        }
        std::size_t body = static_cast<std::size_t>(header_size);
        while (offset < header_size) {
            uint64_t serial = 0;
            if (!record.varint(offset, static_cast<std::size_t>(header_size), serial)) return false;
            Value value;
            std::size_t length = 0;
            if (serial == 0) {
                // NULL
            } else if (serial <= 6) {
                static constexpr std::size_t kSizes[] = {0, 1, 2, 3, 4, 6, 8};
                length = kSizes[serial];
                if (body + length > payload.size()) return false;
                int64_t number = (static_cast<unsigned char>(payload[body]) & 0x80) ? -1 : 0;
                for (std::size_t i = 0; i < length; ++i) {
                    number = static_cast<int64_t>((static_cast<uint64_t>(number) << 8) |
                                                  static_cast<unsigned char>(payload[body + i]));
                }
                value.kind = Value::Kind::Integer;
                value.integer = number;
            } else if (serial == 7) {
                length = 8;
                if (body + length > payload.size()) return false;
                value.kind = Value::Kind::Real;
            } else if (serial == 8 || serial == 9) {
                value.kind = Value::Kind::Integer;
                value.integer = serial == 9 ? 1 : 0;
            } else if (serial >= 12) {
                if ((serial - 12) / 2 > payload.size()) return false;
                length = static_cast<std::size_t>((serial - 12) / 2);
                if (body + length > payload.size()) return false;
                const bool text = (serial & 1) != 0;
                value.kind = text ? Value::Kind::Text : Value::Kind::Blob;
                value.text = text ? decode_text(std::string_view(payload).substr(body, length))
                                  : payload.substr(body, length);
            } else {
                return false;  // serial types 10 and 11 are reserved
            }
            body += length;
            values.push_back(std::move(value));
        }
        return true;
    }

    // The importer only needs ASCII columns. UTF-16 databases keep ASCII code
    // units; anything else becomes '?' instead of being misread.
    [[nodiscard]] std::string decode_text(std::string_view raw) const {
        if (encoding_ == 1) return std::string(raw);
        std::string out;
        out.reserve(raw.size() / 2);
        for (std::size_t i = 0; i + 1 < raw.size(); i += 2) {
            const unsigned lo = static_cast<unsigned char>(raw[encoding_ == 2 ? i : i + 1]);
            const unsigned hi = static_cast<unsigned char>(raw[encoding_ == 2 ? i + 1 : i]);
            out.push_back(hi == 0 && lo < 0x80 ? static_cast<char>(lo) : '?');
        }
        return out;
    }

    std::string_view bytes_;
    std::size_t page_size_ = 0;
    std::size_t usable_ = 0;
    std::size_t encoding_ = 1;
    std::size_t page_count_ = 0;
    std::size_t visited_pages_ = 0;
};

std::string lower_ascii(std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

// Column names of a simple `CREATE TABLE name(col type, ...)` statement.
std::vector<std::string> column_names(const std::string& sql) {
    std::vector<std::string> names;
    const auto open = sql.find('(');
    const auto close = sql.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) return names;
    std::size_t start = open + 1;
    int depth = 0;
    for (std::size_t i = open + 1; i <= close; ++i) {
        const char ch = sql[i];
        if (ch == '(') ++depth;
        if (ch == ')' && i != close) --depth;
        if ((ch == ',' && depth == 0) || i == close) {
            std::size_t a = start;
            while (a < i && std::isspace(static_cast<unsigned char>(sql[a]))) ++a;
            std::size_t b = a;
            while (b < i && !std::isspace(static_cast<unsigned char>(sql[b]))) ++b;
            std::string name = lower_ascii(sql.substr(a, b - a));
            name.erase(std::remove_if(name.begin(), name.end(),
                                      [](char c) { return c == '"' || c == '`' || c == '[' || c == ']'; }),
                       name.end());
            names.push_back(std::move(name));
            start = i + 1;
        }
    }
    return names;
}

int clamp_int(const Value& value) {
    if (value.kind != Value::Kind::Integer) return 0;
    return static_cast<int>(std::clamp<int64_t>(value.integer, (std::numeric_limits<int>::min)(),
                                                (std::numeric_limits<int>::max)()));
}

}  // namespace

Lr2ScoreDbResult read_lr2_score_db_bytes(std::string_view bytes) {
    Lr2ScoreDbResult result;
    Reader reader(bytes);
    if (!reader.open(result.error)) return result;

    std::size_t root = 0;
    std::string sql;
    const auto find_table = [&](const std::vector<Value>& row) {
        if (row.size() >= 5 && row[0].text == "table" && lower_ascii(row[1].text) == "score" &&
            row[3].kind == Value::Kind::Integer && row[3].integer > 0) {
            root = static_cast<std::size_t>(row[3].integer);
            sql = row[4].text;
        }
        return true;
    };
    if (!reader.walk_table(1, find_table, result.error)) return result;
    if (root == 0) {
        result.error = "No LR2 score table in this database.";
        return result;
    }

    const auto names = column_names(sql);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < names.size(); ++i) index.emplace(names[i], i);
    for (const char* required : {"hash", "clear", "perfect", "great", "good", "bad", "poor", "totalnotes"}) {
        if (index.find(required) == index.end()) {
            result.error = std::string("LR2 score table has no '") + required + "' column.";
            return result;
        }
    }
    const auto column = [&](const std::vector<Value>& row, const char* name) -> const Value* {
        const auto it = index.find(name);
        return it != index.end() && it->second < row.size() ? &row[it->second] : nullptr;
    };
    const auto number = [&](const std::vector<Value>& row, const char* name) {
        const Value* value = column(row, name);
        return value ? clamp_int(*value) : 0;
    };
    const auto text = [&](const std::vector<Value>& row, const char* name) {
        const Value* value = column(row, name);
        return value && value->kind == Value::Kind::Text ? value->text : std::string{};
    };
    const auto read_row = [&](const std::vector<Value>& row) {
        if (result.rows.size() >= kMaxRows) return false;
        Lr2ScoreRow score;
        score.hash = lower_ascii(text(row, "hash"));
        score.clear = number(row, "clear");
        score.perfect = number(row, "perfect");
        score.great = number(row, "great");
        score.good = number(row, "good");
        score.bad = number(row, "bad");
        score.poor = number(row, "poor");
        score.totalnotes = number(row, "totalnotes");
        score.maxcombo = number(row, "maxcombo");
        score.minbp = number(row, "minbp");
        score.ghost = text(row, "ghost");
        result.rows.push_back(std::move(score));
        return true;
    };
    if (!reader.walk_table(root, read_row, result.error)) result.rows.clear();
    return result;
}

Lr2ScoreDbResult read_lr2_score_db(const std::filesystem::path& path) {
    Lr2ScoreDbResult result;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        result.error = "Cannot read the LR2 score database.";
        return result;
    }
    if (size > kMaxDatabaseBytes) {
        result.error = "The LR2 score database is too large.";
        return result;
    }
    std::ifstream stream(path, std::ios::binary);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!stream || !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        result.error = "Cannot read the LR2 score database.";
        return result;
    }
    return read_lr2_score_db_bytes(bytes);
}

}  // namespace tenriff::app
