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

#ifdef _WIN32
#include "app/MenuApp.h"
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace tenriff::app {
struct MenuAppRoomChatTestAccess {
    static void check_room_text_routing() {
        using namespace std::chrono_literals;
        const auto wait = [](const std::function<bool()>& predicate) {
            const auto end = std::chrono::steady_clock::now() + 3s;
            do { if (predicate()) return true; std::this_thread::sleep_for(5ms); }
            while (std::chrono::steady_clock::now() < end);
            return predicate();
        };
        auto app = std::make_unique<MenuApp>();
        network::PeerSession other;
        REQUIRE(app->peer_session_.host(0, "Synthetic host"));
        REQUIRE(wait([&] { return app->peer_session_.snapshot().state == network::PeerSessionState::Listening; }));
        REQUIRE(other.join("127.0.0.1", app->peer_session_.snapshot().local_port, "Synthetic guest"));
        REQUIRE(wait([&] { return app->peer_session_.snapshot().state == network::PeerSessionState::Connected &&
                                 other.snapshot().state == network::PeerSessionState::Connected; }));
        CHECK_FALSE(app->global_chat_service_.snapshot().configured);
        app->set_multiplayer_chat_overlay(true);
        // Exercise the same committed UTF-16 -> WM_CHAR queue -> UTF-8 app path.
        for (const wchar_t character : std::wstring(L"Hello 안녕")) app->menu_window_.on_text_input(character);
        while (const auto text = app->menu_window_.poll_text_input()) app->handle_text_input(*text);
        CHECK(app->multiplayer_menu_.chat_input == "Hello 안녕");
        app->handle_text_input("\r\n");
        CHECK(app->multiplayer_menu_.chat_input == "Hello 안녕");
        app->key_enter_ = 0x1c;
        REQUIRE(app->handle_multiplayer_chat_overlay_input(app->key_enter_));
        CHECK(app->multiplayer_menu_.chat_input.empty());
        REQUIRE(wait([&] { return !other.snapshot().chat_messages.empty(); }));
        CHECK(other.snapshot().chat_messages.back().text == "Hello 안녕");
        REQUIRE(other.send_chat("Reply 확인"));
        REQUIRE(wait([&] { return app->peer_session_.snapshot().chat_messages.size() == 2; }));
        render::ChatOverlayData overlay;
        app->populate_multiplayer_chat_overlay(overlay);
        REQUIRE(overlay.messages.size() == 2);
        CHECK(overlay.messages.back().find("Reply 확인") != std::string::npos);
        app->multiplayer_menu_.chat_input = "draft";
        app->set_multiplayer_chat_overlay(false);
        app->handle_text_input("hidden");
        CHECK(app->multiplayer_menu_.chat_input == "draft");
        app->set_multiplayer_chat_overlay(true);
        app->chat_url_warning_visible_ = true;
        app->handle_text_input("blocked by warning");
        CHECK(app->multiplayer_menu_.chat_input == "draft");
        app->chat_url_warning_visible_ = false;
        app->ranked_account_overlay_visible_ = true;
        app->handle_text_input("blocked by account");
        CHECK(app->multiplayer_menu_.chat_input == "draft");
        app->ranked_account_overlay_visible_ = false;
        app->peer_session_.disconnect();
        app->difficulty_table_url_editing_ = true;
        app->difficulty_table_url_input_ = "underlying-field";
        app->handle_text_input("disconnected");
        CHECK(app->multiplayer_menu_.chat_input == "draft");
        CHECK(app->difficulty_table_url_input_ == "underlying-field");
        CHECK_FALSE(app->send_multiplayer_chat_input());
        CHECK(app->multiplayer_menu_.chat_input == "draft");
    }
};
}
TEST_CASE("room chat accepts committed Korean and ASCII without legacy global chat and respects modal disconnect gates") {
    tenriff::app::MenuAppRoomChatTestAccess::check_room_text_routing();
}
#endif
