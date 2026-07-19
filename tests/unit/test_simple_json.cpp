#include "doctest/doctest.h"

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
