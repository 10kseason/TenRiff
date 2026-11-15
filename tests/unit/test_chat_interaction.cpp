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
#include "config/SimpleJson.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace tenriff::app {
namespace {
using ChatJson = config::JsonValue;
using ChatObject = config::JsonObject;
using ChatArray = config::JsonArray;

const ChatJson& chat_field(const ChatJson& value, const char* key) {
    static const ChatJson empty;
    const auto* fields = value.as_object();
    if (!fields) return empty;
    const auto found = fields->find(key);
    return found == fields->end() ? empty : found->second;
}

// No sockets, real account, profile or room is needed to exercise UI routing.
// The fixture asserts that only account-scoped global-chat endpoints are used.
struct GlobalChatUiFixture {
    const std::string upload_key = "tr_" + std::string(64, 'a');
    const std::string session_key = "mp_" + std::string(64, 'b');
    std::mutex mutex;
    ChatArray history;
    std::atomic<bool> unexpected_request{false};
    std::atomic<int> sent{0};

    network::HostedHttpTransport transport() {
        return [this](std::string_view method, std::string_view path,
                      std::string_view bearer, std::string_view body,
                      network::HostedHttpResponse& response) {
            const auto parsed = config::parse_json(body);
            if (method != "POST" || !parsed.success() ||
                chat_field(*parsed.root, "protocol").as_number() != 2 ||
                chat_field(*parsed.root, "score_version").as_string() != "ex-v1") {
                unexpected_request = true; return false;
            }
            const auto now = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            ChatObject envelope{{"protocol", ChatJson(2.0)}, {"score_version", ChatJson("ex-v1")},
                {"server_received_ms", ChatJson(now)}, {"server_time_ms", ChatJson(now)}};
            if (path == "/api/chat/session" && bearer == upload_key &&
                chat_field(*parsed.root, "purpose").as_string() == "chat") {
                envelope["session_token"] = ChatJson(session_key);
                envelope["expires_at_ms"] = ChatJson(now + 30000);
            } else if (path == "/api/chat" && bearer == session_key) {
                std::lock_guard lock(mutex);
                const auto action = chat_field(*parsed.root, "action").as_string();
                if (action == "send") {
                    history.emplace_back(ChatObject{{"id", ChatJson(static_cast<double>(history.size() + 1))},
                        {"name", ChatJson("Online player")}, {"text", chat_field(*parsed.root, "text")},
                        {"created_at_ms", ChatJson(now)}});
                    ++sent;
                } else if (action != "poll") {
                    unexpected_request = true; return false;
                }
                ChatArray incoming;
                const auto after = chat_field(*parsed.root, "after_id").as_number();
                for (const auto& message : history)
                    if (chat_field(message, "id").as_number() > after) incoming.push_back(message);
                envelope["messages"] = ChatJson(std::move(incoming));
            } else {
                unexpected_request = true; return false;
            }
            response.status = 200;
            response.body = config::json_stringify(ChatJson(std::move(envelope)), 0, 17);
            return true;
        };
    }
};

bool wait_chat_ui(const std::function<bool()>& predicate) {
    using namespace std::chrono_literals;
    const auto end = std::chrono::steady_clock::now() + 3s;
    do { if (predicate()) return true; std::this_thread::sleep_for(5ms); }
    while (std::chrono::steady_clock::now() < end);
    return predicate();
}
}  // namespace

struct MenuAppRoomChatTestAccess {
    static void check_global_text_routing() {
        GlobalChatUiFixture server;
        auto app = std::make_unique<MenuApp>(server.transport());
        app->config_.ui.language = "en";
        app->online_play_mode_ = true;
        app->key_enter_ = 0x1c;
        app->global_chat_service_.configure(server.upload_key);
        REQUIRE(wait_chat_ui([&] { return app->global_chat_available(); }));
        CHECK(app->peer_session_.snapshot().state == network::PeerSessionState::Idle);

        int expected = 0;
        for (const auto screen : {MenuApp::Screen::Title, MenuApp::Screen::SongSelect,
                MenuApp::Screen::OptionsHub, MenuApp::Screen::Gameplay,
                MenuApp::Screen::Result, MenuApp::Screen::Multiplayer}) {
            app->reset_screen(screen);
            app->set_multiplayer_chat_overlay(true);
            // Same committed UTF-16 -> WM_CHAR queue -> UTF-8 path on every screen.
            for (const wchar_t character : std::wstring(L"Hello 안녕"))
                app->menu_window_.on_text_input(character);
            while (const auto text = app->menu_window_.poll_text_input()) app->handle_text_input(*text);
            CHECK(app->global_chat_input_ == "Hello 안녕");
            app->handle_text_input("\r\n");
            CHECK(app->global_chat_input_ == "Hello 안녕");
            REQUIRE(app->handle_multiplayer_chat_overlay_input(app->key_enter_));
            CHECK(app->global_chat_input_.empty());
            ++expected;
            REQUIRE(wait_chat_ui([&] {
                return app->global_chat_service_.snapshot().messages.size() == static_cast<std::size_t>(expected);
            }));
            render::ChatOverlayData overlay;
            app->populate_multiplayer_chat_overlay(overlay);
            CHECK(overlay.connected);
            CHECK(overlay.title == "Global Chat");
            REQUIRE_FALSE(overlay.messages.empty());
            CHECK(overlay.messages.back() == "Online player: Hello 안녕");
            CHECK(app->peer_session_.snapshot().chat_messages.empty());
        }
        CHECK(server.sent == expected);
        CHECK_FALSE(server.unexpected_request);

        // Room teardown must not reset an account-global conversation or draft.
        app->global_chat_input_ = "draft";
        app->reset_multiplayer_for_single_player();
        CHECK(app->global_chat_available());
        CHECK(app->global_chat_input_ == "draft");
        app->handle_text_input(" kept");
        CHECK(app->global_chat_input_ == "draft kept");
        CHECK(app->global_chat_service_.snapshot().messages.size() == static_cast<std::size_t>(expected));

        app->set_multiplayer_chat_overlay(false);
        app->handle_text_input("hidden");
        CHECK(app->global_chat_input_ == "draft kept");
        app->set_multiplayer_chat_overlay(true);
        app->chat_url_warning_visible_ = true;
        app->handle_text_input("blocked by warning");
        CHECK(app->global_chat_input_ == "draft kept");
        app->chat_url_warning_visible_ = false;
        app->ranked_account_overlay_visible_ = true;
        app->handle_text_input("blocked by account");
        CHECK(app->global_chat_input_ == "draft kept");
        app->ranked_account_overlay_visible_ = false;

        // Send errors remain visible even outside the multiplayer menu.
        app->global_chat_input_.clear();
        CHECK_FALSE(app->send_multiplayer_chat_input());
        render::ChatOverlayData failed_send;
        app->populate_multiplayer_chat_overlay(failed_send);
        CHECK(failed_send.status == app->global_chat_status_);
        CHECK_FALSE(failed_send.status.empty());

        // Guest mode revokes both network access and display immediately.
        app->choose_play_mode(false);
        CHECK_FALSE(app->global_chat_available());
        CHECK_FALSE(app->global_chat_service_.snapshot().configured);
        CHECK(app->global_chat_input_.empty());
        app->set_multiplayer_chat_overlay(true);
        app->difficulty_table_url_editing_ = true;
        app->difficulty_table_url_input_ = "underlying-field";
        app->handle_text_input("disconnected");
        CHECK(app->global_chat_input_.empty());
        CHECK(app->difficulty_table_url_input_ == "underlying-field");
        app->global_chat_input_ = "must not send";
        CHECK_FALSE(app->send_multiplayer_chat_input());
        render::ChatOverlayData offline;
        offline.messages.push_back("stale visible message");
        app->populate_multiplayer_chat_overlay(offline);
        CHECK_FALSE(offline.connected);
        CHECK(offline.messages.empty());
        CHECK_FALSE(server.unexpected_request);
    }
};
}
TEST_CASE("global chat accepts committed Korean and ASCII on every screen and respects account and modal gates") {
    tenriff::app::MenuAppRoomChatTestAccess::check_global_text_routing();
}
#endif
