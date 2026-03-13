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
