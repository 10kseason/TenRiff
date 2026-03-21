#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tenriff::config {

struct JsonValue;
using JsonObject = std::unordered_map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Storage = std::variant<std::monostate, bool, double, std::string, JsonObject, JsonArray>;
    Storage value;

    JsonValue() = default;
    explicit JsonValue(bool data) : value(data) {}
    explicit JsonValue(double data) : value(data) {}
    explicit JsonValue(std::string data) : value(std::move(data)) {}
    explicit JsonValue(const char* data) : value(std::string(data)) {}
    explicit JsonValue(JsonObject data) : value(std::move(data)) {}
    explicit JsonValue(JsonArray data) : value(std::move(data)) {}

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::monostate>(value); }
    [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(value); }
    [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(value); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(value); }
    [[nodiscard]] bool is_object() const { return std::holds_alternative<JsonObject>(value); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<JsonArray>(value); }

    [[nodiscard]] bool as_bool(bool fallback = false) const;
    [[nodiscard]] double as_number(double fallback = 0.0) const;
    [[nodiscard]] std::string as_string(std::string fallback = {}) const;
    [[nodiscard]] const JsonObject* as_object() const;
    [[nodiscard]] const JsonArray* as_array() const;
};

struct JsonParseResult {
    std::optional<JsonValue> root;
    std::string error;
    std::size_t error_offset = 0;

    [[nodiscard]] bool success() const { return root.has_value(); }
};

JsonParseResult parse_json(std::string_view text);

std::string json_stringify(const JsonValue& value, int indent = 0);

}  // namespace tenriff::config
