#include "config/SimpleJson.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <sstream>

namespace tenriff::config {

bool JsonValue::as_bool(bool fallback) const {
    if (auto* data = std::get_if<bool>(&value)) {
        return *data;
    }
    return fallback;
}

double JsonValue::as_number(double fallback) const {
    if (auto* data = std::get_if<double>(&value)) {
        return *data;
    }
    return fallback;
}

std::string JsonValue::as_string(std::string fallback) const {
    if (auto* data = std::get_if<std::string>(&value)) {
        return *data;
    }
    return fallback;
}

const JsonObject* JsonValue::as_object() const {
    return std::get_if<JsonObject>(&value);
}

const JsonArray* JsonValue::as_array() const {
    return std::get_if<JsonArray>(&value);
}

namespace {

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    JsonParseResult parse() {
        JsonParseResult result;
        auto value = parse_value();
        if (!value.has_value()) {
            result.error = error_;
            result.error_offset = error_offset_;
            return result;
        }
        skip_ws();
        if (pos_ != input_.size()) {
            set_error("Trailing characters after JSON value.");
            result.error = error_;
            result.error_offset = error_offset_;
            return result;
        }
        result.root = std::move(value);
        return result;
    }

private:
    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    void set_error(const char* message) {
        if (!error_.empty()) {
            return;
        }
        error_ = message;
        error_offset_ = pos_;
    }

    bool consume(char expected) {
        if (pos_ < input_.size() && input_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::optional<JsonValue> parse_value() {
        skip_ws();
        if (pos_ >= input_.size()) {
            set_error("Unexpected end of input while parsing JSON value.");
            return std::nullopt;
        }
        char ch = input_[pos_];
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == '"') {
            auto str = parse_string();
            if (!str.has_value()) {
                return std::nullopt;
            }
            return JsonValue{std::move(str.value())};
        }
        if (ch == 't' && match_literal("true")) {
            return JsonValue{true};
        }
        if (ch == 'f' && match_literal("false")) {
            return JsonValue{false};
        }
        if (ch == 'n' && match_literal("null")) {
            return JsonValue{};
        }
        auto number = parse_number();
        if (number.has_value()) {
            return JsonValue{number.value()};
        }
        set_error("Invalid JSON value.");
        return std::nullopt;
    }

    bool match_literal(std::string_view literal) {
        if (input_.substr(pos_, literal.size()) != literal) {
            return false;
        }
        pos_ += literal.size();
        return true;
    }

    std::optional<JsonValue> parse_object() {
        if (!consume('{')) {
            set_error("Expected '{' to start JSON object.");
            return std::nullopt;
        }
        JsonObject object;
        skip_ws();
        if (consume('}')) {
            return JsonValue{std::move(object)};
        }
        while (pos_ < input_.size()) {
            skip_ws();
            auto key = parse_string();
            if (!key.has_value()) {
                set_error("Expected string key in JSON object.");
                return std::nullopt;
            }
            skip_ws();
            if (!consume(':')) {
                set_error("Expected ':' after JSON object key.");
                return std::nullopt;
            }
            auto value = parse_value();
            if (!value.has_value()) {
                return std::nullopt;
            }
            object.emplace(std::move(key.value()), std::move(value.value()));
            skip_ws();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) {
                set_error("Expected ',' between JSON object members.");
                return std::nullopt;
            }
        }
        return JsonValue{std::move(object)};
    }

    std::optional<JsonValue> parse_array() {
        if (!consume('[')) {
            set_error("Expected '[' to start JSON array.");
            return std::nullopt;
        }
        JsonArray array;
        skip_ws();
        if (consume(']')) {
            return JsonValue{std::move(array)};
        }
        while (pos_ < input_.size()) {
            auto value = parse_value();
            if (!value.has_value()) {
                return std::nullopt;
            }
            array.push_back(std::move(value.value()));
            skip_ws();
            if (consume(']')) {
                break;
            }
            if (!consume(',')) {
                set_error("Expected ',' between JSON array elements.");
                return std::nullopt;
            }
        }
        return JsonValue{std::move(array)};
    }

    std::optional<std::string> parse_string() {
        if (!consume('"')) {
            set_error("Expected '\"' to start JSON string.");
            return std::nullopt;
        }
        std::string out;
        while (pos_ < input_.size()) {
            char ch = input_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (ch == '\\') {
                if (pos_ >= input_.size()) {
                    set_error("Unexpected end of input in JSON escape.");
                    return std::nullopt;
                }
                char esc = input_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > input_.size()) {
                            set_error("Invalid unicode escape in JSON string.");
                            return std::nullopt;
                        }
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char hex = input_[pos_++];
                            code <<= 4;
                            if (hex >= '0' && hex <= '9') {
                                code += static_cast<unsigned int>(hex - '0');
                            } else if (hex >= 'a' && hex <= 'f') {
                                code += static_cast<unsigned int>(hex - 'a' + 10);
                            } else if (hex >= 'A' && hex <= 'F') {
                                code += static_cast<unsigned int>(hex - 'A' + 10);
                            } else {
                                set_error("Invalid hex digit in unicode escape.");
                                return std::nullopt;
                            }
                        }
                        if (code <= 0x7F) {
                            out.push_back(static_cast<char>(code));
                        } else {
                            out.push_back('?');
                        }
                        break;
                    }
                    default:
                        set_error("Unsupported escape sequence in JSON string.");
                        return std::nullopt;
                }
                continue;
            }
            out.push_back(ch);
        }
        set_error("Unterminated JSON string.");
        return std::nullopt;
    }

    std::optional<double> parse_number() {
        std::size_t start = pos_;
        if (pos_ < input_.size() && (input_[pos_] == '-' || input_[pos_] == '+')) {
            ++pos_;
        }
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '-' || input_[pos_] == '+')) {
                ++pos_;
            }
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                ++pos_;
            }
        }
        if (start == pos_) {
            return std::nullopt;
        }
        std::string_view token = input_.substr(start, pos_ - start);
        double value = 0.0;
        auto* first = token.data();
        auto* last = token.data() + token.size();
        std::string temp(first, last);
        try {
            size_t consumed = 0;
            value = std::stod(temp, &consumed);
            if (consumed != temp.size() || !std::isfinite(value)) {
                set_error("Invalid JSON number.");
                return std::nullopt;
            }
        } catch (...) {
            set_error("Invalid JSON number.");
            return std::nullopt;
        }
        return value;
    }

    std::string_view input_;
    std::size_t pos_ = 0;
    std::string error_;
    std::size_t error_offset_ = 0;
};

void write_indent(std::ostringstream& out, int indent, int depth) {
    for (int i = 0; i < indent * depth; ++i) {
        out.put(' ');
    }
}

void write_string(std::ostringstream& out, std::string_view value) {
    out.put('"');
    for (char ch : value) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "?";
                } else {
                    out.put(ch);
                }
                break;
        }
    }
    out.put('"');
}

void write_value(std::ostringstream& out, const JsonValue& value, int indent, int depth) {
    if (value.is_null()) {
        out << "null";
    } else if (value.is_bool()) {
        out << (value.as_bool() ? "true" : "false");
    } else if (value.is_number()) {
        out << value.as_number();
    } else if (value.is_string()) {
        write_string(out, value.as_string());
    } else if (value.is_array()) {
        const auto* array = value.as_array();
        out << '[';
        if (array && !array->empty()) {
            if (indent > 0) {
                out << '\n';
            }
            for (std::size_t i = 0; i < array->size(); ++i) {
                if (indent > 0) {
                    write_indent(out, indent, depth + 1);
                }
                write_value(out, (*array)[i], indent, depth + 1);
                if (i + 1 < array->size()) {
                    out << ',';
                }
                if (indent > 0) {
                    out << '\n';
                }
            }
            if (indent > 0) {
                write_indent(out, indent, depth);
            }
        }
        out << ']';
    } else if (value.is_object()) {
        const auto* object = value.as_object();
        out << '{';
        if (object && !object->empty()) {
            if (indent > 0) {
                out << '\n';
            }
            std::size_t count = 0;
            for (const auto& [key, entry] : *object) {
                if (indent > 0) {
                    write_indent(out, indent, depth + 1);
                }
                write_string(out, key);
                out << ':';
                if (indent > 0) {
                    out << ' ';
                }
                write_value(out, entry, indent, depth + 1);
                ++count;
                if (count < object->size()) {
                    out << ',';
                }
                if (indent > 0) {
                    out << '\n';
                }
            }
            if (indent > 0) {
                write_indent(out, indent, depth);
            }
        }
        out << '}';
    }
}

}  // namespace

JsonParseResult parse_json(std::string_view text) {
    Parser parser(text);
    return parser.parse();
}

std::string json_stringify(const JsonValue& value, int indent) {
    std::ostringstream out;
    write_value(out, value, indent, 0);
    return out.str();
}

}  // namespace tenriff::config
