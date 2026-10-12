#include "doctest/doctest.h"

#include <limits>

#include "config/SimpleJson.h"

using tenriff::config::parse_json;

TEST_CASE("simple json parses nested objects") {
    auto result = parse_json("{\"audio\":{\"rate\":48000,\"exclusive\":true}} ");
    CHECK(result.success());
    REQUIRE(result.root.has_value());
    const auto* root = result.root->as_object();
    REQUIRE(root != nullptr);
    auto audio_it = root->find("audio");
    REQUIRE(audio_it != root->end());
    const auto* audio = audio_it->second.as_object();
    REQUIRE(audio != nullptr);
    auto rate_it = audio->find("rate");
    REQUIRE(rate_it != audio->end());
    CHECK(rate_it->second.as_number() == doctest::Approx(48000.0));
}

TEST_CASE("simple json rejects malformed input") {
    auto result = parse_json("{\"audio\": [1,}");
    CHECK_FALSE(result.success());
}

TEST_CASE("simple json rejects unterminated containers") {
    for (const char* input : {
             "{\"audio\":true",
             "[1,2",
             "{\"audio\":[1,2]",
             "[1,{\"nested\":true}",
         }) {
        CHECK_FALSE(parse_json(input).success());
    }
}

TEST_CASE("simple json enforces the JSON number grammar") {
    for (const char* input : {
             "+1",
             "01",
             "-01",
             "1.",
             "1e",
             "1e+",
         }) {
        CHECK_FALSE(parse_json(input).success());
    }

    for (const char* input : {"0", "-0", "1.25", "1e2", "-2.5E-3"}) {
        CHECK(parse_json(input).success());
    }
}

TEST_CASE("simple json rejects unescaped control characters in strings") {
    CHECK_FALSE(parse_json("\"line\nbreak\"").success());
    CHECK(parse_json("\"line\\nbreak\"").success());
}

TEST_CASE("simple json optional precision retains exact evidence numbers and default formatting") {
    const tenriff::config::JsonValue integer{1'234'567.0};
    CHECK(tenriff::config::json_stringify(integer) == "1.23457e+06");
    const auto exact = tenriff::config::json_stringify(integer, 0, std::numeric_limits<double>::max_digits10);
    CHECK(exact == "1234567");
    const auto parsed = parse_json(exact);
    REQUIRE(parsed.success());
    CHECK(parsed.root->as_number() == 1'234'567.0);
    const tenriff::config::JsonValue moment{0.12345678901234567};
    const auto moment_parsed = parse_json(tenriff::config::json_stringify(
        moment, 0, std::numeric_limits<double>::max_digits10));
    REQUIRE(moment_parsed.success());
    CHECK(moment_parsed.root->as_number() == moment.as_number());
}
