#include "doctest/doctest.h"
#include "app/GlobalChatService.h"
#include "config/SimpleJson.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace {
using namespace std::chrono_literals;
using tenriff::app::GlobalChatService;
using tenriff::config::JsonValue;
using tenriff::config::JsonObject;
using tenriff::config::JsonArray;
using tenriff::network::HostedHttpResponse;
using tenriff::network::HostedHttpTransport;
using Clock = std::chrono::steady_clock;
const std::string account_a = "tr_" + std::string(64, 'a');
const std::string account_b = "tr_" + std::string(64, 'b');
bool wait_chat(const std::function<bool()>& predicate) {
    const auto end = Clock::now() + 4s;
    do { if (predicate()) return true; std::this_thread::sleep_for(5ms); } while (Clock::now() < end);
    return predicate();
}
const JsonValue& field(const JsonValue& value, const char* key) {
    static const JsonValue empty;
    const auto* object = value.as_object();
    if (!object) return empty;
    const auto found = object->find(key);
    return found == object->end() ? empty : found->second;
}
struct ChatFixture {
    std::mutex mutex;
    std::atomic<int> sessions{0}, polls{0}, sends{0};
    std::atomic<bool> bad_scope{false}, blocked{false}, release{false};
    bool block_poll = false, lose_first_send = false, malformed = false, reject_auth = false;
    bool lost_send = false;
    unsigned chat_status = 200;
    std::vector<std::string> send_ids;
    std::vector<Clock::time_point> send_times;
    JsonArray messages;
    HostedHttpTransport transport() {
        return [this](std::string_view method, std::string_view path, std::string_view token,
                      std::string_view body, HostedHttpResponse& response) {
            const auto parsed = tenriff::config::parse_json(body);
            if (method != "POST" || !parsed.success()) { bad_scope = true; return false; }
            const auto& input = *parsed.root;
            response.status = 200;
            if (path == "/api/chat/session") {
                ++sessions;
                if (token.substr(0, 3) != "tr_" || field(input, "purpose").as_string() != "chat") bad_scope = true;
                if (reject_auth) { response.status = 401; response.body = "{}"; return true; }
                response.body = tenriff::config::json_stringify(JsonValue(JsonObject{
                    {"protocol", JsonValue(2.0)}, {"score_version", JsonValue("ex-v1")},
                    {"server_time_ms", JsonValue(100000.0)}, {"expires_at_ms", JsonValue(130000.0)},
                    {"session_token", JsonValue("mp_" + std::string(64, token.back()))}}));
                return true;
            }
            if (path != "/api/chat" || token.substr(0, 3) != "mp_") { bad_scope = true; return false; }
            const bool sending = field(input, "action").as_string() == "send";
            if (!sending) {
                ++polls;
                if (block_poll && !blocked.exchange(true)) {
                    while (!release) std::this_thread::sleep_for(1ms);
                }
            }
            std::lock_guard lock(mutex);
            if (sending) {
                ++sends;
                const auto id = field(input, "request_id").as_string();
                send_ids.push_back(id); send_times.push_back(Clock::now());
                bool duplicate = false;
                for (std::size_t i = 0; i + 1 < send_ids.size(); ++i) duplicate |= send_ids[i] == id;
                if (!duplicate) messages.emplace_back(JsonObject{
                    {"id", JsonValue(static_cast<double>(messages.size() + 1))},
                    {"name", JsonValue(token.back() == 'a' ? "Alice" : "Bob")},
                    {"text", field(input, "text")}, {"created_at_ms", JsonValue(100000.0)}});
                if (lose_first_send && !lost_send) { lost_send = true; return false; }
            }
            response.status = chat_status;
            if (malformed) { response.body = "{\"protocol\":2,\"score_version\":\"ex-v1\",\"messages\":[{}]}"; return true; }
            JsonArray incoming;
            const auto after = field(input, "after_id").as_number();
            for (const auto& message : messages) if (field(message, "id").as_number() > after) incoming.push_back(message);
            response.body = tenriff::config::json_stringify(JsonValue(JsonObject{
                {"protocol", JsonValue(2.0)}, {"score_version", JsonValue("ex-v1")},
                {"server_received_ms", JsonValue(100000.0)}, {"server_time_ms", JsonValue(100000.0)},
                {"messages", JsonValue(std::move(incoming))}}));
            return true;
        };
    }
};
}

TEST_CASE("Global chat authenticates independently and accepts bounded UTF8 without a room") {
    ChatFixture fixture;
    GlobalChatService chat(fixture.transport());
    CHECK_FALSE(chat.send("offline"));
    chat.configure("invalid");
    CHECK_FALSE(chat.snapshot().configured);
    CHECK(fixture.sessions == 0);
    chat.configure(account_a);
    REQUIRE(wait_chat([&] { return chat.snapshot().online; }));
    CHECK_FALSE(chat.send(""));
    CHECK_FALSE(chat.send("   "));
    CHECK_FALSE(chat.send("\xE3\x80\x80"));
    CHECK_FALSE(chat.send(std::string("x\0y", 3)));
    CHECK_FALSE(chat.send("\xC0\xAF"));
    CHECK_FALSE(chat.send("\xED\xA0\x80"));
    CHECK_FALSE(chat.send(std::string(257, 'x')));
    REQUIRE(chat.send("안녕하세요 hello"));
    REQUIRE(wait_chat([&] { return chat.snapshot().messages.size() == 1; }));
    const auto before = chat.snapshot();
    chat.configure(account_a);
    CHECK(chat.snapshot().messages.size() == 1);
    CHECK(chat.snapshot().revision == before.revision);
    CHECK(chat.snapshot().messages[0].username == "Alice");
    CHECK(chat.snapshot().messages[0].role.empty());
    CHECK_FALSE(fixture.bad_scope);
    CHECK(fixture.sessions == 1);
    chat.clear();
    CHECK_FALSE(chat.snapshot().online);
    CHECK(chat.snapshot().messages.empty());
    CHECK_FALSE(chat.send("after logout"));
}

TEST_CASE("Global chat uncertain sends keep one request id and queued sends respect the rate") {
    ChatFixture fixture;
    fixture.lose_first_send = true;
    GlobalChatService chat(fixture.transport());
    chat.configure(account_a);
    REQUIRE(wait_chat([&] { return chat.snapshot().online; }));
    REQUIRE(chat.send("first"));
    REQUIRE(chat.send("second"));
    REQUIRE(wait_chat([&] { return chat.snapshot().messages.size() == 2; }));
    std::lock_guard lock(fixture.mutex);
    REQUIRE(fixture.send_ids.size() == 3);
    CHECK(fixture.send_ids[0] == fixture.send_ids[1]);
    CHECK(fixture.send_ids[1] != fixture.send_ids[2]);
    CHECK(fixture.send_times[2] - fixture.send_times[1] >= 1s);
}

TEST_CASE("Global chat account changes fence in flight replies and clear is nonblocking") {
    ChatFixture fixture;
    fixture.block_poll = true;
    fixture.messages.emplace_back(JsonObject{{"id", JsonValue(1.0)}, {"name", JsonValue("Old")},
        {"text", JsonValue("old account reply")}, {"created_at_ms", JsonValue(100000.0)}});
    GlobalChatService chat(fixture.transport());
    struct ReleaseGate { ChatFixture& fixture; ~ReleaseGate() { fixture.release = true; } } gate{fixture};
    chat.configure(account_a);
    REQUIRE(wait_chat([&] { return fixture.blocked.load(); }));
    const auto started = Clock::now();
    chat.clear();
    CHECK(Clock::now() - started < 100ms);
    CHECK_FALSE(chat.snapshot().configured);
    // Replace the fixture history before admitting a new account. The old
    // blocked request is still authenticated as A and must not publish anything.
    fixture.release = true;
    REQUIRE(wait_chat([&] { return fixture.polls.load() == 1; }));
    std::this_thread::sleep_for(25ms);
    CHECK_FALSE(chat.snapshot().online);
    CHECK(chat.snapshot().messages.empty());
    { std::lock_guard lock(fixture.mutex); fixture.messages.clear(); }
    chat.configure(account_b);
    REQUIRE(wait_chat([&] { return chat.snapshot().online && fixture.sessions == 2; }));
    REQUIRE(chat.send("new account"));
    REQUIRE(wait_chat([&] { return chat.snapshot().messages.size() == 1; }));
    CHECK(chat.snapshot().messages[0].username == "Bob");
}

TEST_CASE("Global chat invalid replies and authorization failures close the send gate") {
    for (int mode = 0; mode < 3; ++mode) {
        ChatFixture fixture;
        fixture.malformed = mode == 0;
        fixture.reject_auth = mode == 1;
        fixture.chat_status = mode == 2 ? 401 : 200;
        GlobalChatService chat(fixture.transport());
        chat.configure(account_a);
        REQUIRE(wait_chat([&] { return !chat.snapshot().error.empty(); }));
        CHECK_FALSE(chat.snapshot().online);
        CHECK_FALSE(chat.send("must not send"));
        CHECK(chat.snapshot().messages.empty());
        CHECK(chat.snapshot().error.find(account_a) == std::string::npos);
        CHECK(fixture.sessions == 1);
    }
}


TEST_CASE("Global chat bounds retained history and rejects malformed message identities") {
    {
        ChatFixture fixture;
        for (int i = 1; i <= 100; ++i) fixture.messages.emplace_back(JsonObject{
            {"id", JsonValue(static_cast<double>(i))}, {"name", JsonValue("Alice")},
            {"text", JsonValue("history")}, {"created_at_ms", JsonValue(100000.0)}});
        GlobalChatService chat(fixture.transport());
        chat.configure(account_a);
        REQUIRE(wait_chat([&] { return chat.snapshot().messages.size() == 100; }));
        REQUIRE(chat.send("newest"));
        REQUIRE(wait_chat([&] { return chat.snapshot().messages.back().id == 101; }));
        CHECK(chat.snapshot().messages.size() == 100);
        CHECK(chat.snapshot().messages.front().id == 2);
    }
    for (int invalid = 0; invalid < 4; ++invalid) {
        ChatFixture fixture;
        JsonObject message{{"id", JsonValue(1.0)}, {"name", JsonValue("Alice")},
            {"text", JsonValue("valid")}, {"created_at_ms", JsonValue(100000.0)}};
        if (invalid == 0) message["name"] = JsonValue(std::string("\xC0\xAF", 2));
        if (invalid == 1) message["text"] = JsonValue(std::string(257, 'x'));
        if (invalid == 2) message["created_at_ms"] = JsonValue(100001.0);
        fixture.messages.emplace_back(message);
        if (invalid == 3) fixture.messages.emplace_back(message); // duplicate IDs
        GlobalChatService chat(fixture.transport());
        chat.configure(account_a);
        REQUIRE(wait_chat([&] { return !chat.snapshot().error.empty(); }));
        CHECK_FALSE(chat.snapshot().online);
        CHECK(chat.snapshot().messages.empty());
    }
}
