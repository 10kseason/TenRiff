#include "doctest/doctest.h"
#include "network/HostedPeerSession.h"
#include "config/SimpleJson.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>

namespace {
using namespace tenriff::network;
using namespace std::chrono_literals;
using Json = tenriff::config::JsonValue;
using Object = tenriff::config::JsonObject;
using Array = tenriff::config::JsonArray;
constexpr const char* room_id = "10000000-0000-4000-8000-000000000001";
const std::string upload_key = "tr_" + std::string(64, 'a');
const std::string session_key = "mp_" + std::string(64, 'b');

bool wait_hosted(const std::function<bool()>& test) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    do { if (test()) return true; std::this_thread::sleep_for(5ms); }
    while (std::chrono::steady_clock::now() < deadline);
    return test();
}
const Json& value(const Json& object, const char* key) {
    static const Json empty;
    const auto* fields = object.as_object();
    if (!fields) return empty;
    const auto found = fields->find(key);
    return found == fields->end() ? empty : found->second;
}

// Synthetic protocol server: records only synthetic credentials and returns a
// controlled state. Real server state/race tests belong to the Worker suite.
struct HostedFixture {
    std::mutex mutex;
    std::atomic<int> calls{0}, sessions{0}, room_calls{0}, chart_calls{0};
    std::atomic<bool> unsafe_credential{false};
    std::atomic<bool> reject_auth{false}, malformed_room{false}, drop_create_once{false};
    bool legacy_protocol = false, missing_score_version = false;
    std::string phase = "lobby";
    std::string nonce = "0";
    uint64_t hash = 18446744073709551614ULL;
    unsigned local = 1;
    bool ready = true, loaded = false;
    double begin = 0, received = 0;
    std::atomic<int> processing_delay_ms{0};
    Json claimed_score;
    Array chat_history;
    std::string first_create_id, retried_create_id;
    std::string last_chart_hash, last_nonce;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();

    double now() const {
        return 1800000000000.0 + static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count());
    }
    Json chart() const {
        return Json(Object{{"hash", Json(std::to_string(hash))}, {"size", Json("8000")}, {"name", Json("Synthetic chart")}});
    }
    Json participant(unsigned id) const {
        return Json(Object{{"player_id", Json(static_cast<double>(id))}, {"name", Json(id == 1 ? "Alice" : "Bob")},
            {"ready", Json(ready)}, {"loaded", Json(loaded)}, {"round_reset", Json(false)},
            {"chart", chart()}, {"score", id == 1 ? claimed_score : Json()}});
    }
    Json snapshot() const {
        return Json(Object{{"protocol", Json(2.0)}, {"score_version", Json("ex-v1")}, {"server_time_ms", Json(now())},
            {"server_received_ms", Json(received)}, {"local_player_id", Json(static_cast<double>(local))},
            {"room", Json(Object{{"id", Json(room_id)}, {"revision", Json(1.0)}, {"leader_id", Json(1.0)},
                {"phase", Json(phase)}, {"nonce", Json(nonce)}, {"rate_milli", Json(1000.0)},
                {"rate_revision", Json(1.0)}, {"begin_at_ms", Json(begin)}, {"selected_chart", chart()},
                {"participants", Json(Array{participant(1), participant(2)})},
                {"round_participants", Json(phase == "lobby" ? Array{} : Array{participant(1), participant(2)})},
                {"chat", Json(chat_history)}})}});
    }
    HostedHttpTransport transport() {
        return [this](std::string_view method, std::string_view path, std::string_view bearer,
                      std::string_view body, HostedHttpResponse& response) {
            ++calls;
            std::lock_guard lock(mutex);
            response.status = 200;
            received = now();
            if (method == "POST") {
                const auto request = tenriff::config::parse_json(body);
                if (!request.success() || value(*request.root, "protocol").as_number() != 2.0 ||
                    value(*request.root, "score_version").as_string() != "ex-v1") {
                    response.status = 426; response.body = "{\"error\":\"update_required\"}"; return true;
                }
            }
            Json reply;
            if (path == "/api/session") {
                ++sessions;
                if (method != "POST" || bearer != upload_key) unsafe_credential = true;
                if (reject_auth) { response.status = 401; response.body = "{\"error\":\"expired\"}"; return true; }
                reply = Json(Object{{"protocol", Json(legacy_protocol ? 1.0 : 2.0)},
                    {"score_version", Json(missing_score_version ? "" : "ex-v1")}, {"session_token", Json(session_key)},
                    {"expires_at_ms", Json(now() + 30000)}, {"server_time_ms", Json(now())},
                    {"player_id", Json("synthetic-account-with-no-scores")}, {"name", Json("Alice")}});
            } else {
                if (bearer != session_key || body.find(upload_key) != std::string_view::npos) unsafe_credential = true;
                if (path == "/api/rooms" && method == "GET") {
                    reply = Json(Object{{"protocol", Json(2.0)}, {"score_version", Json("ex-v1")}, {"server_time_ms", Json(now())},
                        {"rooms", Json(Array{Json(Object{{"id", Json(room_id)}, {"name", Json("Alice")},
                            {"player_count", Json(1.0)}, {"max_players", Json(8.0)}, {"accepting_players", Json(true)}})})}});
                } else {
                    ++room_calls;
                    const auto parsed = tenriff::config::parse_json(body);
                    if (!parsed.success()) return false;
                    const auto action = path == "/api/rooms" ? "create" : value(*parsed.root, "action").as_string();
                    const auto id = value(*parsed.root, "request_id").as_string();
                    if (action == "create") {
                        if (first_create_id.empty()) first_create_id = id;
                        else retried_create_id = id;
                        if (drop_create_once.exchange(false)) return false;
                    }
                    if (action == "launch") { phase = "loading"; nonce = "17"; }
                    if (action == "loaded") loaded = true;
                    if (action == "begin") { phase = "playing"; begin = now() + 5000; }
                    if (action == "chat") chat_history.emplace_back(Object{
                        {"player_id", Json(static_cast<double>(local))}, {"text", value(*parsed.root, "text")}});
                    if (action == "score") claimed_score = value(*parsed.root, "score");
                    if (action == "chart") { ++chart_calls; last_chart_hash = value(value(*parsed.root, "chart"), "hash").as_string(); }
                    last_nonce = value(*parsed.root, "nonce").as_string();
                    if (processing_delay_ms) std::this_thread::sleep_for(std::chrono::milliseconds(processing_delay_ms.load()));
                    reply = snapshot();
                    if (malformed_room) {
                        response.body = "{\"protocol\":1,\"room\":{}}";
                        return true;
                    }
                }
            }
            response.body = tenriff::config::json_stringify(reply, 0, 17);
            return true;
        };
    }
};
}

TEST_CASE("Hosted multiplayer rejects invalid credentials and path injection before HTTP") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    CHECK_FALSE(client.configure(""));
    CHECK_FALSE(client.configure("tr_" + std::string(63, 'a') + "\n"));
    CHECK_FALSE(client.browse());
    CHECK_FALSE(client.host());
    CHECK(server.calls == 0);
    REQUIRE(client.configure(upload_key));
    CHECK_FALSE(client.join("../api/session"));
    CHECK_FALSE(client.join("https://example.invalid"));
    CHECK(server.calls == 0);
}

TEST_CASE("Hosted account connection admits no-score identity and isolates credential scopes") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.browse());
    REQUIRE(wait_hosted([&] { return client.rooms().size() == 1; }));
    CHECK(client.rooms()[0].id == room_id);
    REQUIRE(client.join(room_id));
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Connected; }));
    CHECK(client.snapshot().participant_count == 2);
    CHECK(client.snapshot().local_player_id == 1);
    CHECK(client.snapshot().local_chart.fingerprint.hash == server.hash);
    CHECK(client.snapshot().selected_chart.fingerprint.hash == server.hash);
    CHECK(client.snapshot().remote_library_sha256 == nullptr);
    CHECK_FALSE(server.unsafe_credential);
    client.disconnect();
    CHECK(client.snapshot().state == PeerSessionState::Disconnected);
}

TEST_CASE("Hosted transport retries uncertain creation with exactly the same idempotency key") {
    HostedFixture server;
    server.drop_create_once = true;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Connected; }));
    std::lock_guard lock(server.mutex);
    CHECK(server.first_create_id.size() == 36);
    CHECK(server.retried_create_id == server.first_create_id);
}

TEST_CASE("Hosted chart fingerprints stay exact above JSON double integer precision") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Connected; }));
    REQUIRE(client.set_local_chart({server.hash, 8000}, "Synthetic chart"));
    REQUIRE(wait_hosted([&] { std::lock_guard lock(server.mutex); return !server.last_chart_hash.empty(); }));
    std::lock_guard lock(server.mutex);
    CHECK(server.last_chart_hash == "18446744073709551614");
}

TEST_CASE("Hosted leader consumes server start and poll launch returns chart fingerprint") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().can_start; }));
    REQUIRE(client.send_launch());
    REQUIRE(wait_hosted([&] { return client.snapshot().round_active; }));
    const auto launch = client.poll_launch();
    REQUIRE(launch.has_value());
    CHECK(*launch == server.hash);
    CHECK_FALSE(client.poll_launch().has_value());
    REQUIRE(client.mark_loaded());
    REQUIRE(wait_hosted([&] { return client.wait_for_peer_loaded(1ms); }));
    REQUIRE(client.send_begin());
    uint32_t delay = 0;
    REQUIRE(wait_hosted([&] { return client.wait_for_begin(1ms, delay); }));
    CHECK(delay > 4000);
    CHECK(delay <= 5000);
    CHECK_FALSE(client.wait_for_begin(1ms, delay));
    CHECK(client.snapshot().result_round_nonce == 17);
    PeerScore score;
    score.perfect = 8000; score.great = 123; score.good = 99;
    score.score = 9000;
    CHECK_FALSE(client.publish_score(score, false)); // Normalized claims must not enter EX rooms.
    score.score = 16123; // EX is not capped at the old 10,000-point ceiling.
    score.current_sample = -48000; // Intro/countdown samples are valid signed positions.
    REQUIRE(client.publish_score(score, false));
    score.finished = true;
    REQUIRE(client.publish_score(score, true));
    CHECK_FALSE(client.publish_score(score, true));
    REQUIRE(wait_hosted([&] { return client.snapshot().participants[0].has_score; }));
    CHECK(client.snapshot().participants[0].latest_score.current_sample == -48000);
    CHECK(client.snapshot().participants[0].latest_score.score == 16123);
}

TEST_CASE("Hosted EX client rejects legacy and unmarked score protocols before room access") {
    for (bool missing : {false, true}) {
        HostedFixture server;
        server.legacy_protocol = !missing;
        server.missing_score_version = missing;
        HostedPeerSession client(server.transport());
        REQUIRE(client.configure(upload_key));
        REQUIRE(client.host());
        REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Failed; }));
        CHECK(server.room_calls == 0);
    }
}

TEST_CASE("Hosted EX client rejects inconsistent server score counts") {
    HostedFixture server;
    server.claimed_score = Json(Object{{"score", Json(9000.0)}, {"current_sample", Json(0.0)},
        {"combo", Json(1.0)}, {"max_combo", Json(1.0)}, {"perfect", Json(1.0)},
        {"great", Json(0.0)}, {"good", Json(0.0)}, {"bad", Json(0.0)}, {"poor", Json(0.0)},
        {"gauge_milli", Json(100000.0)}, {"finished", Json(false)}, {"game_over", Json(false)}, {"aborted", Json(false)}});
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Failed; }));
    CHECK_FALSE(client.snapshot().has_remote_score);
}

TEST_CASE("Hosted expired identity fails closed before room access") {
        HostedFixture server;
        server.reject_auth = true;
        HostedPeerSession client(server.transport());
        REQUIRE(client.configure(upload_key));
        REQUIRE(client.host());
        REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Failed; }));
        CHECK(server.room_calls == 0);
        CHECK(client.snapshot().status_detail.find(upload_key) == std::string::npos);
        CHECK_FALSE(client.send_launch());
}
TEST_CASE("Hosted malformed room response fails closed") {
        HostedFixture server;
        server.malformed_room = true;
        HostedPeerSession client(server.transport());
        REQUIRE(client.configure(upload_key));
        REQUIRE(client.host());
        REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Failed; }));
        CHECK_FALSE(client.snapshot().can_start);
        CHECK_FALSE(client.snapshot().round_active);
}

TEST_CASE("Explicit hosted mode cannot silently fall back to legacy unauthenticated sockets") {
    PeerSession client;
    REQUIRE(client.configure_hosted(upload_key));
    CHECK(client.is_hosted());
    CHECK_FALSE(client.host(0, "Guest"));
    CHECK_FALSE(client.join("127.0.0.1", 27301, "Guest"));
    client.disable_hosted();
    CHECK_FALSE(client.is_hosted());
}

TEST_CASE("Hosted local chart invalidation survives stale server snapshots") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().can_start; }));
    client.clear_local_chart();
    const auto revision = client.snapshot().revision;
    REQUIRE(wait_hosted([&] { return client.snapshot().revision > revision; }));
    CHECK_FALSE(client.snapshot().local_chart.fingerprint.valid());
    CHECK_FALSE(client.snapshot().local_ready);
    CHECK_FALSE(client.set_ready(true));
    CHECK_FALSE(client.snapshot().can_start);
}

TEST_CASE("Hosted revocation retains per-call lifetime during gameplay snapshots") {
    PeerSession client;
    REQUIRE(client.configure_hosted(upload_key));
    std::atomic<bool> done{false};
    std::thread gameplay([&] {
        while (!done) {
            (void)client.snapshot();
            (void)client.publish_score(PeerScore{}, false);
            (void)client.poll_launch();
        }
    });
    for (int i = 0; i < 100; ++i) {
        client.disable_hosted();
        (void)client.configure_hosted(upload_key);
    }
    done = true;
    gameplay.join();
    client.disable_hosted();
    CHECK_FALSE(client.is_hosted());
}

TEST_CASE("Hosted start estimate compensates server processing time") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().can_start; }));
    REQUIRE(client.send_launch());
    REQUIRE(wait_hosted([&] { return client.snapshot().round_active; }));
    REQUIRE(client.mark_loaded());
    REQUIRE(wait_hosted([&] { return client.wait_for_peer_loaded(1ms); }));
    server.processing_delay_ms = 400;
    REQUIRE(client.send_begin());
    uint32_t delay = 0;
    REQUIRE(wait_hosted([&] { return client.wait_for_begin(1ms, delay); }));
    const auto estimated = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
    std::chrono::steady_clock::time_point expected;
    {
        std::lock_guard lock(server.mutex);
        expected = server.started + std::chrono::milliseconds(static_cast<int64_t>(server.begin - 1800000000000.0));
    }
    const auto error = std::chrono::duration_cast<std::chrono::milliseconds>(estimated - expected).count();
    CHECK(std::abs(error) < 100);
    server.processing_delay_ms = 0;
}

TEST_CASE("Hosted stop request does not wait for synchronous HTTP") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Connected; }));
    server.processing_delay_ms = 400;
    const auto count = server.room_calls.load();
    REQUIRE(wait_hosted([&] { return server.room_calls > count; }));
    const auto before = std::chrono::steady_clock::now();
    client.request_stop();
    const auto elapsed = std::chrono::steady_clock::now() - before;
    CHECK(elapsed < 100ms);
    CHECK(client.snapshot().state == PeerSessionState::Closing);
    CHECK_FALSE(client.snapshot().round_active);
    CHECK_FALSE(client.publish_score(PeerScore{}, false));
    server.processing_delay_ms = 0;
    REQUIRE(wait_hosted([&] { return !client.running(); }));
    CHECK(client.snapshot().state == PeerSessionState::Disconnected);
    CHECK_FALSE(client.poll_launch().has_value());
    client.disconnect();
}

TEST_CASE("Hosted chart acknowledgment coalesces repeated per-frame matching") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key));
    REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Connected; }));
    server.processing_delay_ms = 400;
    for (int i = 0; i < 30; ++i)
        REQUIRE(client.set_local_chart({server.hash - 1, 9000}, "Repeated local match"));
    REQUIRE(wait_hosted([&] { return server.chart_calls > 0; }));
    server.processing_delay_ms = 0;
    client.disconnect();
    CHECK(server.chart_calls == 1);
}

TEST_CASE("Hosted room chat sends bounded UTF8 through session authentication and receives server history") {
    HostedFixture server;
    HostedPeerSession client(server.transport());
    REQUIRE(client.configure(upload_key)); REQUIRE(client.host());
    REQUIRE(wait_hosted([&] { return client.snapshot().state == PeerSessionState::Connected; }));
    CHECK_FALSE(client.send_chat(""));
    CHECK_FALSE(client.send_chat(std::string(257, 'x')));
    CHECK_FALSE(client.send_chat(std::string("x\0y", 3)));
    REQUIRE(client.send_chat("Hosted 안녕하세요 ASCII"));
    REQUIRE(wait_hosted([&] { return client.snapshot().chat_messages.size() == 1; }));
    CHECK(client.snapshot().chat_messages.front().text == "Hosted 안녕하세요 ASCII");
    CHECK(client.snapshot().chat_messages.front().player_id == 1);
    {
        std::lock_guard lock(server.mutex);
        server.chat_history.emplace_back(Object{{"player_id", Json(2.0)}, {"text", Json("Remote 확인")}});
    }
    REQUIRE(wait_hosted([&] { return client.snapshot().chat_messages.size() == 2; }));
    CHECK(client.snapshot().chat_messages.back().text == "Remote 확인");
    CHECK_FALSE(server.unsafe_credential);
    client.request_stop(); CHECK_FALSE(client.send_chat("after disconnect"));
}
