#include "doctest/doctest.h"

#include "network/PeerSession.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

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

}  // namespace

TEST_CASE("peer session rejects invalid startup and chart inputs") {
    tenriff::network::PeerSession session;
    CHECK_FALSE(session.host(0, {}));
    CHECK_FALSE(session.join({}, 12345, "Joiner"));
    CHECK_FALSE(session.join("localhost", 0, "Joiner"));
    CHECK_FALSE(session.set_local_chart({}, "No chart"));

    const auto snapshot = session.snapshot();
    CHECK(snapshot.role == tenriff::network::PeerRole::None);
    CHECK(snapshot.state == tenriff::network::PeerSessionState::Idle);
    CHECK_FALSE(snapshot.local_ready);
    CHECK_FALSE(snapshot.remote_ready);
    CHECK_FALSE(snapshot.has_remote_score);
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

    REQUIRE(host.mark_loaded());
    REQUIRE(joiner.mark_loaded());
    REQUIRE(host.wait_for_peer_loaded(2000ms));
    REQUIRE(joiner.wait_for_peer_loaded(2000ms));

    REQUIRE(host.send_begin(750));
    uint32_t begin_delay_ms = 0;
    REQUIRE(joiner.wait_for_begin(2000ms, begin_delay_ms));
    CHECK(begin_delay_ms == 750);

    PeerScore live;
    live.score = 111111;
    live.current_sample = 48000;
    live.combo = 120;
    REQUIRE(joiner.publish_score(live));

    PeerScore final = live;
    final.score = 987654;
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
    host_final.score = 1'012'345;
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
    host.disconnect("Loopback test complete");
    joiner.disconnect();
    CHECK(host.snapshot().state == PeerSessionState::Disconnected);
    CHECK(joiner.snapshot().state == PeerSessionState::Disconnected);
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
