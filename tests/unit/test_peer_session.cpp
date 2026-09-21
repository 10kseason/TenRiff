#include "doctest/doctest.h"

#include "network/PeerSession.h"
#include "app/MultiplayerPresentation.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace {

using namespace std::chrono_literals;

bool wait_until(const std::function<bool()>& predicate,
                std::chrono::milliseconds timeout = 3000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}

#ifdef _WIN32
// A real TCP participant can deliberately replay stale but well-formed control
// frames. Public PeerSession APIs intentionally cannot generate these packets.
class RawRatePeer {
public:
    ~RawRatePeer() {
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
        if (started_) WSACleanup();
    }

    bool connect_to(uint16_t port) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
        started_ = true;
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET) return false;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) return false;
        u_long nonblocking = 1;
        if (ioctlsocket(socket_, FIONBIO, &nonblocking) != 0) return false;
        tenriff::network::PeerMessage hello;
        hello.type = tenriff::network::PeerMessageType::Hello;
        hello.text = "Raw Rate Peer";
        return send_message(hello);
    }

    bool send_message(const tenriff::network::PeerMessage& message) {
        const auto bytes = tenriff::network::encode_peer_message(message);
        if (bytes.empty()) return false;
        std::size_t offset = 0;
        return wait_until([&] {
            const int sent = send(socket_, reinterpret_cast<const char*>(bytes.data() + offset),
                                  static_cast<int>(bytes.size() - offset), 0);
            if (sent > 0) offset += static_cast<std::size_t>(sent);
            return offset == bytes.size();
        });
    }

    bool fence() {
        using namespace tenriff::network;
        PeerMessage ping;
        ping.type = PeerMessageType::Ping;
        ping.nonce = ++fence_nonce_;
        if (!send_message(ping)) return false;
        return wait_until([&] {
            char buffer[8192];
            for (;;) {
                const int count = recv(socket_, buffer, sizeof(buffer), 0);
                if (count <= 0) break;
                received_.insert(received_.end(), buffer, buffer + count);
            }
            while (!received_.empty()) {
                PeerMessage message;
                std::size_t consumed = 0;
                std::string error;
                const auto status = decode_peer_message(received_, message, consumed, error);
                if (status != PeerDecodeStatus::Complete) break;
                received_.erase(received_.begin(), received_.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (message.type == PeerMessageType::Ping) {
                    message.type = PeerMessageType::Pong;
                    if (!send_message(message)) return false;
                }
                if (message.type == PeerMessageType::Pong && message.nonce == ping.nonce) return true;
            }
            return false;
        });
    }

private:
    SOCKET socket_ = INVALID_SOCKET;
    bool started_ = false;
    uint64_t fence_nonce_ = 1000;
    std::vector<uint8_t> received_;
};
#endif

}  // namespace

TEST_CASE("peer session rejects invalid startup and chart inputs") {
    tenriff::network::PeerSession session;
    CHECK_FALSE(session.host(0, {}));
    CHECK_FALSE(session.join({}, 12345, "Joiner"));
    CHECK_FALSE(session.join("localhost", 0, "Joiner"));
    CHECK_FALSE(session.set_local_chart({}, "No chart"));
    CHECK_FALSE(session.send_chat("Not connected"));
    CHECK_FALSE(session.set_rate(1250));
    CHECK_FALSE(session.set_rate(499));
    CHECK_FALSE(session.set_rate(1025));

    const auto snapshot = session.snapshot();
    CHECK(snapshot.role == tenriff::network::PeerRole::None);
    CHECK(snapshot.state == tenriff::network::PeerSessionState::Idle);
    CHECK_FALSE(snapshot.local_ready);
    CHECK_FALSE(snapshot.remote_ready);
    CHECK_FALSE(snapshot.has_remote_score);
    CHECK(snapshot.rate_milli == 1000);
    CHECK(snapshot.round_rate_milli == 1000);
}

TEST_CASE("peer session can explicitly forget a retained local chart") {
    tenriff::network::PeerSession session;
    tenriff::network::ChartFingerprint chart;
    chart.hash = 0x12345678u;
    chart.size = 4096;

    REQUIRE(session.set_local_chart(chart, "old chart"));
    CHECK(session.snapshot().local_chart.fingerprint.hash == chart.hash);

    session.clear_local_chart();
    const auto cleared = session.snapshot();
    CHECK_FALSE(cleared.local_chart.fingerprint.valid());
    CHECK(cleared.local_chart.name.empty());
    CHECK_FALSE(cleared.local_ready);
}

TEST_CASE("peer session localhost round reaches final score and clean shutdown") {
#ifndef _WIN32
    return;
#else
    using tenriff::network::ChartFingerprint;
    using tenriff::network::PeerScore;
    using tenriff::network::PeerSession;
    using tenriff::network::PeerSessionState;

    PeerSession host;
    PeerSession joiner;
    const ChartFingerprint chart{0x123456789abcdef0ull, 54321};
    const std::string shared_sha =
        "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    const std::string host_only_sha =
        "1111111111111111111111111111111111111111111111111111111111111111";
    host.set_local_library({shared_sha, host_only_sha, shared_sha, "invalid"});
    joiner.set_local_library({shared_sha});

    REQUIRE(host.set_local_chart(chart, "Loopback Chart"));
    REQUIRE(host.host(0, "Host"));
    REQUIRE(wait_until([&]() {
        const auto state = host.snapshot();
        return state.state == PeerSessionState::Listening && state.local_port != 0;
    }));

    const uint16_t port = host.snapshot().local_port;
    REQUIRE(joiner.join("localhost", port, "Joiner"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().state == PeerSessionState::Connected &&
               joiner.snapshot().state == PeerSessionState::Connected;
    }));
    CHECK(host.snapshot().peer_name == "Joiner");
    CHECK(joiner.snapshot().peer_name == "Host");
    REQUIRE(host.send_chat("Welcome to the room"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().chat_messages.size() == 1u &&
               joiner.snapshot().chat_messages.size() == 1u;
    }));
    CHECK(host.snapshot().chat_messages[0].player_id == 1);
    CHECK(joiner.snapshot().chat_messages[0].text == "Welcome to the room");

    REQUIRE(joiner.send_chat("안녕하세요"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().chat_messages.size() == 2u &&
               joiner.snapshot().chat_messages.size() == 2u;
    }));
    CHECK(host.snapshot().chat_messages[1].player_id == 2);
    CHECK(host.snapshot().chat_messages[1].text == "안녕하세요");
    REQUIRE(wait_until([&]() {
        return host.snapshot().remote_library_ready &&
               joiner.snapshot().remote_library_ready;
    }));
    const auto host_library = host.snapshot();
    const auto joiner_library = joiner.snapshot();
    REQUIRE(host_library.remote_library_sha256);
    REQUIRE(joiner_library.remote_library_sha256);
    CHECK(host_library.remote_library_count == 1u);
    CHECK(joiner_library.remote_library_count == 1u);
    CHECK(host_library.remote_library_sha256->count(shared_sha) == 1u);
    CHECK(joiner_library.remote_library_sha256->count(shared_sha) == 1u);
    CHECK(joiner_library.remote_library_sha256->count(host_only_sha) == 0u);
    REQUIRE(wait_until([&]() {
        return host.snapshot().estimated_rtt_ms > 0 &&
               joiner.snapshot().estimated_rtt_ms > 0;
    }));
    CHECK(host.snapshot().estimated_rtt_ms <= 2000);
    CHECK(joiner.snapshot().estimated_rtt_ms <= 2000);

    REQUIRE(wait_until([&]() {
        return joiner.snapshot().remote_chart.fingerprint.hash == chart.hash;
    }));
    CHECK_FALSE(host.snapshot().remote_chart.fingerprint.valid());
    CHECK_FALSE(joiner.set_ready(true));

    // This mirrors the menu resolver: the host announces a chart and the
    // joiner publishes its local chart only after exact HASH+size resolution.
    REQUIRE(joiner.set_local_chart(chart, "Loopback Chart"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().remote_chart.fingerprint.hash == chart.hash &&
               host.snapshot().remote_chart.fingerprint.size == chart.size;
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(joiner.set_ready(true));
    REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));

    // Opening multiplayer Options clears local Ready before changing screens.
    REQUIRE(host.set_ready(false));
    REQUIRE(wait_until([&]() {
        return !joiner.snapshot().remote_ready && !host.snapshot().can_start;
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));

    REQUIRE(host.send_launch());
    std::optional<uint64_t> launch_hash;
    REQUIRE(wait_until([&]() {
        launch_hash = joiner.poll_launch();
        return launch_hash.has_value();
    }));
    CHECK(*launch_hash == chart.hash);
    const uint64_t first_round_nonce = host.snapshot().result_round_nonce;
    CHECK(first_round_nonce != 0);

    REQUIRE(host.mark_loaded());
    REQUIRE(joiner.mark_loaded());
    REQUIRE(host.wait_for_peer_loaded(2000ms));
    REQUIRE(joiner.wait_for_peer_loaded(2000ms));

    REQUIRE(host.send_begin(750));
    uint32_t begin_delay_ms = 0;
    REQUIRE(joiner.wait_for_begin(2000ms, begin_delay_ms));
    CHECK(begin_delay_ms == 750);

    PeerScore live;
    live.score = 7777;
    live.current_sample = 48000;
    live.combo = 120;
    live.max_combo = 120;
    REQUIRE(joiner.publish_score(live));

    PeerScore final = live;
    final.score = 9876;
    final.max_combo = 432;
    final.perfect = 400;
    final.great = 20;
    final.finished = true;
    REQUIRE(joiner.publish_score(final, true));
    REQUIRE(wait_until([&]() {
        const auto state = host.snapshot();
        return state.has_remote_score && state.latest_remote_score.finished;
    }));
    CHECK(host.snapshot().latest_remote_score.score == final.score);
    CHECK(host.snapshot().latest_remote_score.max_combo == final.max_combo);

    PeerScore host_final = final;
    host_final.score = 9999;
    host_final.max_combo = 500;
    REQUIRE(host.publish_score(host_final, true));
    REQUIRE(wait_until([&]() {
        const auto state = joiner.snapshot();
        return state.has_remote_score && state.latest_remote_score.finished;
    }));

    // One player may leave Result while the other comparison screen is still
    // open. A nonce-bound RoundReset preserves the opponent FinalScore and the
    // next Ready/chart change stays gated until both players leave Result.
    host.reset_round();
    CHECK(host.snapshot().round_active);
    CHECK(host.snapshot().local_round_reset);
    CHECK(host.snapshot().has_remote_score);
    CHECK(host.snapshot().latest_remote_score.score == final.score);
    CHECK_FALSE(host.set_ready(true));
    CHECK_FALSE(host.set_local_chart(chart, "Too Early"));
    REQUIRE(wait_until([&]() {
        const auto state = joiner.snapshot();
        return state.state == PeerSessionState::Connected &&
               state.round_active &&
               state.remote_round_reset &&
               state.has_remote_score &&
               state.latest_remote_score.score == host_final.score;
    }));
    CHECK_FALSE(host.snapshot().can_start);

    joiner.reset_round();
    REQUIRE(wait_until([&]() {
        return !host.snapshot().round_active &&
               !joiner.snapshot().round_active &&
               host.snapshot().leader_player_id == 2 &&
               joiner.snapshot().local_is_leader;
    }));
    CHECK(host.snapshot().result_round_nonce == first_round_nonce);
    REQUIRE(host.snapshot().round_participants.size() == 2u);
    CHECK(host.snapshot().round_participants[1].latest_score.score == final.score);
    const ChartFingerprint rematch_chart{0x9988776655443322ull, 8192};
    REQUIRE(joiner.set_local_chart(rematch_chart, "Immediate Rematch Chart"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().state == PeerSessionState::Connected &&
               joiner.snapshot().state == PeerSessionState::Connected &&
               !host.snapshot().round_active && !joiner.snapshot().round_active &&
               !host.snapshot().remote_ready && !joiner.snapshot().remote_ready &&
               host.snapshot().remote_chart.fingerprint.hash == rematch_chart.hash;
    }));
    CHECK_FALSE(host.snapshot().local_ready);
    CHECK_FALSE(host.snapshot().local_loaded);
    CHECK_FALSE(host.snapshot().has_remote_score);
    REQUIRE(host.set_local_chart(rematch_chart, "Immediate Rematch Chart"));
    REQUIRE(wait_until([&]() {
        return joiner.snapshot().remote_chart.fingerprint.hash == rematch_chart.hash;
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(joiner.set_ready(true));
    REQUIRE(wait_until([&]() { return joiner.snapshot().can_start; }));
    CHECK_FALSE(host.snapshot().can_start);

    // START is only actionable after the coordinator validates the latest
    // all-player Ready roster. A crossed follower Ready(false) may make the
    // leader's local request look valid, but it must never launch either side.
    REQUIRE(host.set_ready(false));
    const bool crossed_launch = joiner.send_launch();
    if (crossed_launch) {
        REQUIRE(wait_until([&]() {
            return !host.snapshot().local_ready &&
                   !joiner.snapshot().remote_ready &&
                   !host.snapshot().round_active &&
                   !joiner.snapshot().round_active;
        }));
        CHECK_FALSE(host.poll_launch().has_value());
        CHECK_FALSE(joiner.poll_launch().has_value());
    }

    REQUIRE(host.set_ready(true));
    REQUIRE(wait_until([&]() { return joiner.snapshot().can_start; }));
    REQUIRE(joiner.send_launch());
    std::optional<uint64_t> host_launch;
    std::optional<uint64_t> leader_launch;
    REQUIRE(wait_until([&]() {
        if (!host_launch) host_launch = host.poll_launch();
        if (!leader_launch) leader_launch = joiner.poll_launch();
        return host_launch && leader_launch;
    }));
    CHECK(*host_launch == rematch_chart.hash);
    CHECK(*leader_launch == rematch_chart.hash);
    CHECK(host.snapshot().result_round_nonce != first_round_nonce);
    CHECK(joiner.snapshot().result_round_nonce == host.snapshot().result_round_nonce);
    for (const auto& participant : host.snapshot().round_participants) {
        CHECK_FALSE(participant.has_score);
    }
    host.disconnect("Loopback test complete");
    joiner.disconnect();
    CHECK(host.snapshot().state == PeerSessionState::Disconnected);
    CHECK(joiner.snapshot().state == PeerSessionState::Disconnected);
#endif
}

TEST_CASE("peer result retains two and three player standings after the winner disconnects") {
#ifdef _WIN32
    using namespace tenriff;
    using network::PeerSession;
    using network::PeerSessionState;
    for (const int count : {2, 3}) {
        PeerSession host;
        std::vector<std::unique_ptr<PeerSession>> opponents;
        const network::ChartFingerprint chart{0xabcdef123456ull, 321};
        REQUIRE(host.set_local_chart(chart, "Retained round"));
        REQUIRE(host.host(0, "Local"));
        REQUIRE(wait_until([&]() { return host.snapshot().state == PeerSessionState::Listening; }));
        for (int id = 2; id <= count; ++id) {
            auto opponent = std::make_unique<PeerSession>();
            REQUIRE(opponent->join("127.0.0.1", host.snapshot().local_port,
                                   "Player " + std::to_string(id)));
            REQUIRE(wait_until([&]() {
                return opponent->snapshot().state == PeerSessionState::Connected &&
                       host.snapshot().participants.size() == static_cast<std::size_t>(id) &&
                       opponent->snapshot().selected_chart.fingerprint.hash == chart.hash;
            }));
            REQUIRE(opponent->set_local_chart(chart, "Retained round"));
            opponents.push_back(std::move(opponent));
        }
        REQUIRE(host.set_ready(true));
        for (auto& opponent : opponents) REQUIRE(opponent->set_ready(true));
        REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));
        REQUIRE(host.send_launch());
        REQUIRE(wait_until([&]() {
            return host.snapshot().round_active && std::all_of(opponents.begin(), opponents.end(),
                [](const auto& opponent) { return opponent->snapshot().round_active; });
        }));

        network::PeerScore score;
        score.score = 1000;
        REQUIRE(host.publish_score(score, true));
        score.score = 3000;
        REQUIRE(opponents.front()->publish_score(score, true));
        if (count == 3) {
            score.score = 500;
            score.game_over = true;
            REQUIRE(opponents.back()->publish_score(score));
        }
        REQUIRE(wait_until([&]() {
            const auto room = host.snapshot();
            return room.round_participants.size() == static_cast<std::size_t>(count) &&
                   room.round_participants[1].latest_score.finished &&
                   (count == 2 || room.round_participants[2].has_score);
        }));
        const auto initial = host.snapshot();
        render::MultiplayerPlayerData local;
        local.has_score = true;
        local.finished = true;
        local.score = 1000;
        CHECK(app::multiplayer_standings(initial, local).front().player_id == 2);

        opponents.front()->disconnect("Winner leaves result");
        REQUIRE(wait_until([&]() {
            return host.snapshot().participants.size() == static_cast<std::size_t>(count - 1);
        }));
        if (count == 3) {
            // A live GAME OVER update is not a FinalScore packet. A final result
            // arriving after another player leaves must still replace that score.
            score.score = 2000;
            REQUIRE(opponents.back()->publish_score(score, true));
            REQUIRE(wait_until([&]() {
                return host.snapshot().round_participants[2].latest_score.finished;
            }));
            opponents.back()->disconnect("Last opponent leaves result");
            REQUIRE(wait_until([&]() { return host.snapshot().participants.size() == 1u; }));
        }
        const auto after = host.snapshot();
        const auto standings = app::multiplayer_standings(after, local);
        REQUIRE(standings.size() == static_cast<std::size_t>(count));
        CHECK(standings.front().player_id == 2);
        CHECK(standings.front().score == 3000);
        CHECK(standings.front().finished);
        const auto local_row = std::find_if(standings.begin(), standings.end(),
                                            [](const auto& player) { return player.local; });
        REQUIRE(local_row != standings.end());
        CHECK(local_row->rank == count);
        CHECK(after.result_round_nonce == initial.result_round_nonce);
        CHECK_FALSE(after.round_active);
        host.disconnect("Result regression complete");
    }
#endif
}

TEST_CASE("peer room coordinates four players and rotates leader in join order") {
#ifndef _WIN32
    return;
#else
    using tenriff::network::ChartFingerprint;
    using tenriff::network::PeerScore;
    using tenriff::network::PeerSession;
    using tenriff::network::PeerSessionState;

    PeerSession host;
    PeerSession player_two;
    PeerSession player_three;
    PeerSession player_four;
    const ChartFingerprint chart{0x8877665544332211ull, 77777};
    const std::string common_sha =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const std::string partial_sha =
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    host.set_local_library({common_sha, partial_sha});
    player_two.set_local_library({common_sha, partial_sha});
    player_three.set_local_library({common_sha});
    player_four.set_local_library({common_sha});
    REQUIRE(host.set_local_chart(chart, "Four Player BMS"));
    REQUIRE(host.host(0, "Player One"));
    REQUIRE(wait_until([&]() {
        const auto room = host.snapshot();
        return room.state == PeerSessionState::Listening && room.local_port != 0;
    }));

    const uint16_t port = host.snapshot().local_port;
    REQUIRE(player_two.join("localhost", port, "Player Two"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().participant_count == 2 &&
               player_two.snapshot().local_player_id == 2;
    }, 5000ms));
    REQUIRE(player_three.join("localhost", port, "Player Three"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().participant_count == 3 &&
               player_three.snapshot().local_player_id == 3;
    }, 5000ms));
    REQUIRE(player_four.join("localhost", port, "Player Four"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().participant_count == 4 &&
               player_two.snapshot().participant_count == 4 &&
               player_three.snapshot().participant_count == 4 &&
               player_four.snapshot().participant_count == 4;
    }, 5000ms));

    CHECK(host.snapshot().local_player_id == 1);
    CHECK(player_two.snapshot().local_player_id == 2);
    CHECK(player_three.snapshot().local_player_id == 3);
    CHECK(player_four.snapshot().local_player_id == 4);
    CHECK(host.snapshot().leader_player_id == 1);
    CHECK(host.snapshot().local_is_leader);
    CHECK_FALSE(player_two.snapshot().local_is_leader);

    REQUIRE(wait_until([&]() {
        return host.snapshot().remote_library_ready &&
               player_two.snapshot().remote_library_ready &&
               player_three.snapshot().remote_library_ready &&
               player_four.snapshot().remote_library_ready;
    }, 5000ms));
    CHECK(host.snapshot().remote_library_count == 1u);
    CHECK(player_two.snapshot().remote_library_count == 1u);
    REQUIRE(host.snapshot().remote_library_sha256);
    CHECK(host.snapshot().remote_library_sha256->count(common_sha) == 1u);
    CHECK(host.snapshot().remote_library_sha256->count(partial_sha) == 0u);

    REQUIRE(wait_until([&]() {
        return player_two.snapshot().selected_chart.fingerprint.hash == chart.hash &&
               player_three.snapshot().selected_chart.fingerprint.hash == chart.hash &&
               player_four.snapshot().selected_chart.fingerprint.hash == chart.hash;
    }));
    REQUIRE(player_two.set_local_chart(chart, "Four Player BMS"));
    REQUIRE(player_three.set_local_chart(chart, "Four Player BMS"));
    REQUIRE(player_four.set_local_chart(chart, "Four Player BMS"));
    REQUIRE(wait_until([&]() {
        const auto room = host.snapshot();
        return room.participants.size() == 4 &&
               std::all_of(room.participants.begin(),
                           room.participants.end(),
                           [&](const auto& participant) {
                               return participant.chart.fingerprint.hash == chart.hash;
                           });
    }));

    REQUIRE(host.set_ready(true));
    REQUIRE(player_two.set_ready(true));
    REQUIRE(player_three.set_ready(true));
    REQUIRE(player_four.set_ready(true));
    REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));
    CHECK_FALSE(player_two.snapshot().can_start);

    REQUIRE(host.send_launch());
    std::optional<uint64_t> launch_two;
    std::optional<uint64_t> launch_three;
    std::optional<uint64_t> launch_four;
    REQUIRE(wait_until([&]() {
        if (!launch_two) launch_two = player_two.poll_launch();
        if (!launch_three) launch_three = player_three.poll_launch();
        if (!launch_four) launch_four = player_four.poll_launch();
        return launch_two && launch_three && launch_four;
    }));
    CHECK(*launch_two == chart.hash);
    CHECK(*launch_three == chart.hash);
    CHECK(*launch_four == chart.hash);

    REQUIRE(host.mark_loaded());
    REQUIRE(player_two.mark_loaded());
    REQUIRE(player_three.mark_loaded());
    REQUIRE(player_four.mark_loaded());
    REQUIRE(host.wait_for_peer_loaded(3000ms));
    REQUIRE(player_two.wait_for_peer_loaded(3000ms));
    REQUIRE(player_three.wait_for_peer_loaded(3000ms));
    REQUIRE(player_four.wait_for_peer_loaded(3000ms));

    REQUIRE(host.send_begin(900));
    uint32_t begin_two = 0;
    uint32_t begin_three = 0;
    uint32_t begin_four = 0;
    REQUIRE(player_two.wait_for_begin(3000ms, begin_two));
    REQUIRE(player_three.wait_for_begin(3000ms, begin_three));
    REQUIRE(player_four.wait_for_begin(3000ms, begin_four));
    CHECK(begin_two == 900);
    CHECK(begin_three == 900);
    CHECK(begin_four == 900);

    PeerScore final;
    final.finished = true;
    final.max_combo = 100;
    final.score = 1000;
    REQUIRE(host.publish_score(final, true));
    final.score = 2000;
    REQUIRE(player_two.publish_score(final, true));
    final.score = 3000;
    REQUIRE(player_three.publish_score(final, true));
    final.score = 4000;
    REQUIRE(player_four.publish_score(final, true));
    REQUIRE(wait_until([&]() {
        return host.snapshot().all_remote_finished &&
               player_two.snapshot().all_remote_finished &&
               player_three.snapshot().all_remote_finished &&
               player_four.snapshot().all_remote_finished;
    }, 5000ms));

    host.reset_round();
    player_two.reset_round();
    player_three.reset_round();
    player_four.reset_round();
    REQUIRE(wait_until([&]() {
        return !host.snapshot().round_active &&
               !player_two.snapshot().round_active &&
               !player_three.snapshot().round_active &&
               !player_four.snapshot().round_active &&
               host.snapshot().leader_player_id == 2 &&
               player_two.snapshot().local_is_leader;
    }, 5000ms));

    player_two.disconnect("Leader leaves room");
    REQUIRE(wait_until([&]() {
        return host.snapshot().participant_count == 3 &&
               player_three.snapshot().participant_count == 3 &&
               player_four.snapshot().participant_count == 3 &&
               host.snapshot().leader_player_id == 3 &&
               player_three.snapshot().local_is_leader;
    }, 5000ms));

    player_three.disconnect();
    player_four.disconnect();
    host.disconnect("Four-player room test complete");
#endif
}
TEST_CASE("peer room accepts eight players and rejects a ninth") {
#ifndef _WIN32
    return;
#else
    using tenriff::network::PeerSession;
    using tenriff::network::PeerSessionState;

    PeerSession host;
    host.set_local_library({});
    REQUIRE(host.host(0, "Capacity Host"));
    REQUIRE(wait_until([&]() {
        const auto room = host.snapshot();
        return room.state == PeerSessionState::Listening && room.local_port != 0;
    }));

    const uint16_t port = host.snapshot().local_port;
    std::vector<std::unique_ptr<PeerSession>> joiners;
    joiners.reserve(7);
    for (std::size_t index = 0; index < 7; ++index) {
        auto joiner = std::make_unique<PeerSession>();
        joiner->set_local_library({});
        REQUIRE(joiner->join("localhost",
                             port,
                             "Capacity Player " + std::to_string(index + 2)));
        const auto expected_count = static_cast<uint8_t>(index + 2);
        REQUIRE(wait_until([&]() {
            return host.snapshot().participant_count == expected_count &&
                   joiner->snapshot().local_player_id == expected_count;
        }, 5000ms));
        joiners.push_back(std::move(joiner));
    }

    REQUIRE(wait_until([&]() {
        return host.snapshot().participant_count ==
                   tenriff::network::kPeerMaxPlayers &&
               std::all_of(joiners.begin(), joiners.end(), [](const auto& joiner) {
                   return joiner->snapshot().participant_count ==
                          tenriff::network::kPeerMaxPlayers;
               });
    }, 5000ms));

    PeerSession overflow;
    overflow.set_local_library({});
    REQUIRE(overflow.join("localhost", port, "Capacity Overflow"));
    REQUIRE(wait_until([&]() {
        const auto state = overflow.snapshot().state;
        return state == PeerSessionState::Disconnected ||
               state == PeerSessionState::Failed;
    }, 5000ms));
    CHECK(host.snapshot().participant_count == tenriff::network::kPeerMaxPlayers);

    overflow.disconnect();
    for (auto& joiner : joiners) joiner->disconnect();
    host.disconnect("Capacity test complete");
#endif
}
TEST_CASE("peer session discards a queued launch when the connection closes") {
#ifndef _WIN32
    return;
#else
    using tenriff::network::ChartFingerprint;
    using tenriff::network::PeerSession;
    using tenriff::network::PeerSessionState;

    PeerSession host;
    PeerSession joiner;
    const ChartFingerprint chart{0xaabbccddeeff0011ull, 12345};

    REQUIRE(host.set_local_chart(chart, "Disconnect Race Chart"));
    REQUIRE(host.host(0, "Host"));
    REQUIRE(wait_until([&]() {
        const auto state = host.snapshot();
        return state.state == PeerSessionState::Listening && state.local_port != 0;
    }));
    REQUIRE(joiner.join("localhost", host.snapshot().local_port, "Joiner"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().state == PeerSessionState::Connected &&
               joiner.snapshot().state == PeerSessionState::Connected &&
               joiner.snapshot().remote_chart.fingerprint.hash == chart.hash;
    }));
    REQUIRE(joiner.set_local_chart(chart, "Disconnect Race Chart"));
    REQUIRE(wait_until([&]() {
        return host.snapshot().remote_chart.fingerprint.hash == chart.hash;
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(joiner.set_ready(true));
    REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));
    REQUIRE(host.send_launch());
    REQUIRE(wait_until([&]() { return joiner.snapshot().round_active; }));

    // Leave the received Launch queued, then close the socket. poll_launch()
    // must not expose it after disconnect or the menu could start a ghost game.
    joiner.disconnect("Disconnect before consuming Launch");
    CHECK(joiner.snapshot().state == PeerSessionState::Disconnected);
    CHECK_FALSE(joiner.poll_launch().has_value());

    host.disconnect("Disconnect-race test complete");
#endif
}

TEST_CASE("peer room rate follows the leader through readiness launch rotation and rejoin") {
#ifdef _WIN32
    using namespace tenriff::network;
    PeerSession host;
    PeerSession second;
    PeerSession third;
    const ChartFingerprint chart{0x1122334455667788ull, 9876};
    REQUIRE(host.set_local_chart(chart, "Rate Chart"));
    REQUIRE(host.host(0, "Rate Host"));
    REQUIRE(wait_until([&] { return host.snapshot().state == PeerSessionState::Listening; }));
    REQUIRE(host.set_rate(1500));
    REQUIRE(wait_until([&] {
        return host.snapshot().rate_milli == 1500 && !host.snapshot().rate_change_pending;
    }));
    const auto port = host.snapshot().local_port;
    REQUIRE(second.join("127.0.0.1", port, "Rate Second"));
    REQUIRE(wait_until([&] { return second.snapshot().state == PeerSessionState::Connected; }));
    CHECK(second.snapshot().rate_milli == 1500);
    CHECK_FALSE(second.set_rate(750));
    CHECK_FALSE(host.set_rate(2001));
    CHECK_FALSE(host.set_rate(1025));
    REQUIRE(second.set_local_chart(chart, "Rate Chart"));
    REQUIRE(wait_until([&] { return host.snapshot().remote_chart.fingerprint.hash == chart.hash; }));
    REQUIRE(host.set_ready(true));
    REQUIRE(second.set_ready(true));
    REQUIRE(wait_until([&] { return host.snapshot().can_start; }));
    const auto old_revision = host.snapshot().rate_revision;
    REQUIRE(host.set_rate(500));
    REQUIRE(wait_until([&] {
        const auto a = host.snapshot();
        const auto b = second.snapshot();
        return a.rate_milli == 500 && b.rate_milli == 500 &&
               !a.local_ready && !a.remote_ready && !b.local_ready && !b.remote_ready &&
               !a.rate_change_pending && a.rate_revision > old_revision;
    }));
    CHECK_FALSE(host.send_launch());

    REQUIRE(third.join("127.0.0.1", port, "Rate Third"));
    REQUIRE(wait_until([&] {
        return host.snapshot().participant_count == 3 &&
               second.snapshot().participant_count == 3 && third.snapshot().participant_count == 3;
    }));
    CHECK(third.snapshot().rate_milli == 500);
    REQUIRE(third.set_local_chart(chart, "Rate Chart"));
    REQUIRE(host.set_rate(2000));
    REQUIRE(wait_until([&] {
        return host.snapshot().rate_milli == 2000 && second.snapshot().rate_milli == 2000 &&
               third.snapshot().rate_milli == 2000 && !host.snapshot().rate_change_pending;
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(second.set_ready(true));
    REQUIRE(third.set_ready(true));
    REQUIRE(wait_until([&] { return host.snapshot().can_start; }));
    REQUIRE(host.send_launch());
    REQUIRE(wait_until([&] {
        return host.snapshot().round_active && second.snapshot().round_active && third.snapshot().round_active;
    }));
    CHECK(host.snapshot().round_rate_milli == 2000);
    CHECK(second.snapshot().round_rate_milli == 2000);
    CHECK(third.snapshot().round_rate_milli == 2000);
    CHECK_FALSE(host.set_rate(1000));
    CHECK_FALSE(second.set_rate(1000));
    REQUIRE(host.mark_loaded());
    REQUIRE(second.mark_loaded());
    REQUIRE(third.mark_loaded());
    REQUIRE(wait_until([&] { return host.snapshot().remote_loaded; }));
    REQUIRE(host.send_begin(0));
    uint32_t delay_ms = 0;
    REQUIRE(second.wait_for_begin(3000ms, delay_ms));
    REQUIRE(third.wait_for_begin(3000ms, delay_ms));
    CHECK_FALSE(host.set_rate(1000));
    PeerScore final;
    final.score = 5000;
    final.finished = true;
    REQUIRE(host.publish_score(final, true));
    REQUIRE(second.publish_score(final, true));
    REQUIRE(third.publish_score(final, true));
    REQUIRE(wait_until([&] {
        return host.snapshot().all_remote_finished && second.snapshot().all_remote_finished &&
               third.snapshot().all_remote_finished;
    }));
    CHECK_FALSE(host.set_rate(1000));
    host.reset_round();
    CHECK_FALSE(host.set_rate(1000));
    second.reset_round();
    third.reset_round();
    REQUIRE(wait_until([&] {
        return !host.snapshot().round_active && !second.snapshot().round_active &&
               !third.snapshot().round_active && second.snapshot().local_is_leader;
    }));
    CHECK(host.snapshot().rate_milli == 2000);
    CHECK(second.snapshot().round_rate_milli == 2000);
    CHECK_FALSE(host.set_rate(1250));
    CHECK_FALSE(third.set_rate(1250));
    REQUIRE(second.set_rate(1250));
    REQUIRE(wait_until([&] {
        return host.snapshot().rate_milli == 1250 && third.snapshot().rate_milli == 1250 &&
               !second.snapshot().rate_change_pending;
    }));
    // Retained result metadata belongs to the previous launch, even while the
    // new lobby leader changes settings for the following song.
    CHECK(host.snapshot().round_rate_milli == 2000);
    CHECK(second.snapshot().round_rate_milli == 2000);
    CHECK(third.snapshot().round_rate_milli == 2000);

    third.disconnect("Rejoin rate room");
    REQUIRE(wait_until([&] { return host.snapshot().participant_count == 2; }));
    REQUIRE(third.join("127.0.0.1", port, "Rate Third Rejoined"));
    REQUIRE(wait_until([&] {
        return third.snapshot().state == PeerSessionState::Connected &&
               host.snapshot().participant_count == 3 && third.snapshot().rate_milli == 1250;
    }));
    REQUIRE(second.set_local_chart(chart, "Second Rate Round"));
    REQUIRE(wait_until([&] {
        return host.snapshot().selected_chart.fingerprint.hash == chart.hash &&
               third.snapshot().selected_chart.fingerprint.hash == chart.hash;
    }));
    REQUIRE(host.set_local_chart(chart, "Second Rate Round"));
    REQUIRE(third.set_local_chart(chart, "Second Rate Round"));
    REQUIRE(wait_until([&] {
        const auto room = second.snapshot();
        return std::all_of(room.participants.begin(), room.participants.end(), [&](const auto& player) {
            return player.chart.fingerprint.hash == chart.hash;
        });
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(second.set_ready(true));
    REQUIRE(third.set_ready(true));
    REQUIRE(wait_until([&] { return second.snapshot().can_start; }));
    REQUIRE(second.send_launch());
    REQUIRE(wait_until([&] {
        return host.snapshot().round_active && third.snapshot().round_active &&
               host.snapshot().round_rate_milli == 1250 && third.snapshot().round_rate_milli == 1250;
    }));
    CHECK(second.snapshot().round_rate_milli == 1250);
    CHECK_FALSE(second.set_rate(1000));
    third.disconnect();
    second.disconnect();
    host.disconnect();
    REQUIRE(host.host(0, "Fresh Rate Room"));
    REQUIRE(wait_until([&] { return host.snapshot().state == PeerSessionState::Listening; }));
    CHECK(host.snapshot().rate_milli == 1000);
    CHECK(host.snapshot().round_rate_milli == 1000);
    host.disconnect();
#endif
}

TEST_CASE("peer coordinator rejects stale ready launch and rate requests after returning to the same rate") {
#ifdef _WIN32
    using namespace tenriff::network;
    PeerSession host;
    RawRatePeer raw;
    const ChartFingerprint chart{0x8877665544332211ull, 4321};
    REQUIRE(host.set_local_chart(chart, "Stale Rate Chart"));
    REQUIRE(host.host(0, "Generation Host"));
    REQUIRE(wait_until([&] { return host.snapshot().state == PeerSessionState::Listening; }));
    REQUIRE(raw.connect_to(host.snapshot().local_port));
    REQUIRE(wait_until([&] { return host.snapshot().participant_count == 2; }));
    REQUIRE(raw.fence());
    PeerMessage message;
    message.type = PeerMessageType::RoomRate;
    message.rate_milli = 1500;
    message.rate_revision = host.snapshot().rate_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().rate_milli == 1000); // Socket host coordinates; only room leader decides.
    CHECK(host.snapshot().participant_count == 2);

    message.type = PeerMessageType::Chart;
    message.chart_hash = chart.hash;
    message.chart_size = chart.size;
    message.text = "Stale Rate Chart";
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(host.set_ready(true));
    message.type = PeerMessageType::Ready;
    message.ready = true;
    message.rate_milli = 1000;
    message.rate_revision = host.snapshot().rate_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(wait_until([&] { return host.snapshot().can_start; }));
    REQUIRE(host.send_launch());
    REQUIRE(wait_until([&] { return host.snapshot().round_active; }));
    const uint64_t first_round = host.snapshot().result_round_nonce;
    REQUIRE(host.mark_loaded());
    message.type = PeerMessageType::Loaded;
    message.nonce = first_round;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(wait_until([&] { return host.snapshot().remote_loaded; }));
    REQUIRE(host.send_begin(0));
    PeerScore final;
    final.finished = true;
    REQUIRE(host.publish_score(final, true));
    message.type = PeerMessageType::FinalScore;
    message.score = final;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(wait_until([&] { return host.snapshot().all_remote_finished; }));
    host.reset_round();
    message.type = PeerMessageType::RoundReset;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(wait_until([&] {
        return !host.snapshot().round_active && host.snapshot().leader_player_id == 2;
    }));
    const uint64_t stale_revision = host.snapshot().rate_revision;
    message.type = PeerMessageType::RoomRate;
    message.rate_milli = 1500;
    message.rate_revision = stale_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().rate_milli == 1500);
    message.rate_milli = 1000;
    message.rate_revision = host.snapshot().rate_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().rate_milli == 1000);
    CHECK(host.snapshot().rate_revision > stale_revision);

    message.type = PeerMessageType::Chart;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(host.set_local_chart(chart, "Stale Rate Chart"));
    REQUIRE(wait_until([&] { return host.snapshot().local_chart.fingerprint.hash == chart.hash; }));
    REQUIRE(host.set_ready(true));
    message.type = PeerMessageType::Ready;
    message.ready = true;
    message.rate_revision = stale_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK_FALSE(host.snapshot().remote_ready);
    CHECK_FALSE(host.snapshot().round_active);

    message.rate_revision = host.snapshot().rate_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    REQUIRE(wait_until([&] { return host.snapshot().remote_ready && host.snapshot().local_ready; }));
    message.type = PeerMessageType::Launch;
    message.nonce = first_round + 100;
    message.rate_revision = stale_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK_FALSE(host.snapshot().round_active);

    message.type = PeerMessageType::RoomRate;
    message.rate_milli = 1500;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().rate_milli == 1000);
    CHECK(host.snapshot().local_ready);
    CHECK(host.snapshot().remote_ready);

    message.type = PeerMessageType::Launch;
    message.rate_milli = 1000;
    message.rate_revision = host.snapshot().rate_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().round_active);
    CHECK(host.snapshot().round_rate_milli == 1000);
    // An obsolete Ready(false) must not cancel the new loading barrier.
    message.type = PeerMessageType::Ready;
    message.ready = false;
    message.rate_revision = stale_revision;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().round_active);
    message.type = PeerMessageType::RoomRate;
    message.rate_revision = host.snapshot().rate_revision;
    message.rate_milli = 2000;
    REQUIRE(raw.send_message(message));
    REQUIRE(raw.fence());
    CHECK(host.snapshot().rate_milli == 1000);
    CHECK(host.snapshot().round_rate_milli == 1000);
    host.disconnect("Stale rate regression complete");
#endif
}
