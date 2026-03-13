#include "doctest/doctest.h"

#include <string>

#include "util/Utf8Compat.h"

TEST_CASE("utf8 compat preserves valid UTF-8 text") {
    const std::string text = u8"漢字 테스트";

    CHECK(tenriff::util::ensure_utf8_text(text) == text);
    CHECK_FALSE(tenriff::util::wide_from_utf8_lossy(text).empty());
    CHECK(tenriff::util::path_from_utf8_lossy(text).u8string() == text);
}

TEST_CASE("utf8 compat tolerates malformed byte strings") {
    std::string malformed = "bad_";
    malformed.push_back(static_cast<char>(0x81));
    malformed.push_back(static_cast<char>(0xFF));
    malformed += "_title";

    CHECK_FALSE(tenriff::util::wide_from_utf8_lossy(malformed).empty());
    CHECK_FALSE(tenriff::util::ensure_utf8_text(malformed).empty());
    bool threw = false;
    try {
        (void)tenriff::util::path_from_utf8_lossy(malformed);
    } catch (...) {
        threw = true;
    }
    CHECK_FALSE(threw);
}

TEST_CASE("utf8 compat sanitizes UI text by stripping controls and normalizing whitespace") {
    std::string raw = "  title";
    raw.push_back('\0');
    raw += "\r\n";
    raw += u8"漢字";
    raw += "\tartist";
    raw.push_back(static_cast<char>(0x1F));

    CHECK(tenriff::util::sanitize_ui_text(raw) == std::string("title 漢字 artist"));
}

TEST_CASE("utf8 compat sanitizes malformed UI bytes into stable non-empty text") {
    std::string malformed = "bad";
    malformed.push_back(static_cast<char>(0x81));
    malformed.push_back(static_cast<char>(0xFF));
    malformed += "\nname";

    const std::string sanitized = tenriff::util::sanitize_ui_text(malformed);
    CHECK_FALSE(sanitized.empty());
    CHECK(sanitized.find('\n') == std::string::npos);
    CHECK(sanitized.find('\0') == std::string::npos);
}
