#include "doctest/doctest.h"

#include "app/ChatInteraction.h"
#include "app/AccountInput.h"

TEST_CASE("now playing chat command ignores case and surrounding whitespace") {
    CHECK(tenriff::app::is_now_playing_chat_command("/np"));
    CHECK(tenriff::app::is_now_playing_chat_command("  /NP\r\n"));
    CHECK_FALSE(tenriff::app::is_now_playing_chat_command("/np extra"));
    CHECK_FALSE(tenriff::app::is_now_playing_chat_command("np"));
}

TEST_CASE("chat URL extraction accepts web links and strips sentence punctuation") {
    const auto url = tenriff::app::first_chat_web_url(
        "listen here: HTTPS://example.test/song?id=10). next");
    REQUIRE(url.has_value());
    CHECK(*url == "HTTPS://example.test/song?id=10");
    CHECK_FALSE(tenriff::app::first_chat_web_url("javascript:alert(1)").has_value());
    CHECK_FALSE(tenriff::app::first_chat_web_url("https://").has_value());
}

TEST_CASE("account password paste keeps spaces and complete UTF-8 characters") {
    using tenriff::app::sanitize_pasted_account_password;

    CHECK(sanitize_pasted_account_password("  copied pass  ") == "  copied pass  ");
    CHECK(sanitize_pasted_account_password("line\nbreak\tremoved") ==
          "linebreakremoved");

    const std::string long_password = std::string(127, 'a') + "안";
    CHECK(sanitize_pasted_account_password(long_password) == std::string(127, 'a'));
}
