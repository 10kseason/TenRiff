#include "doctest/doctest.h"

#include "network/PeerSession.h"

#include <chrono>
#include <functional>
#include <optional>
#include <thread>

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
    CHECK(joiner_library.remote_library_count == 2u);
    CHECK(host_library.remote_library_sha256->count(shared_sha) == 1u);
    CHECK(joiner_library.remote_library_sha256->count(shared_sha) == 1u);
    CHECK(joiner_library.remote_library_sha256->count(host_only_sha) == 1u);
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
    REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));

    // Ready(false) and Launch travel in opposite TCP directions. If they cross,
    // the lobby unready must cancel the pending launch, never masquerade as a
    // completed-result RoundReset or disconnect either peer.
    REQUIRE(joiner.set_ready(false));
    const bool crossed_launch = host.send_launch();
    if (crossed_launch) {
        // If this queues before Ready(false) reaches the host, the resulting
        // stale Loaded frame must be ignored by the joiner's closed nonce.
        (void)host.mark_loaded();
        REQUIRE(wait_until([&]() {
            const auto host_state = host.snapshot();
            const auto joiner_state = joiner.snapshot();
            return host_state.state == PeerSessionState::Connected &&
                   joiner_state.state == PeerSessionState::Connected &&
                   !host_state.round_active && !joiner_state.round_active &&
                   !host_state.round_transition_pending &&
                   !host_state.local_ready && !host_state.remote_ready &&
                   !joiner_state.local_ready && !joiner_state.remote_ready &&
                   joiner_state.status_detail.find("canceled because readiness changed") !=
                       std::string::npos;
        }));
        CHECK_FALSE(joiner.poll_launch().has_value());
        CHECK_FALSE(host.snapshot().can_start);
    } else {
        REQUIRE(wait_until([&]() { return !host.snapshot().remote_ready; }));
    }

    // A rapid false -> true flip may straddle Launch and RoundCancel. After the
    // ordered cancellation ACK settles, the peers must be either consistently
    // canceled, consistently ready in the lobby, or consistently in the same
    // active round -- never a stale host-only can_start state.
    REQUIRE(host.set_ready(false));
    REQUIRE(joiner.set_ready(false));
    REQUIRE(wait_until([&]() {
        return !host.snapshot().remote_ready && !joiner.snapshot().remote_ready;
    }));
    REQUIRE(host.set_ready(true));
    REQUIRE(joiner.set_ready(true));
    REQUIRE(wait_until([&]() { return host.snapshot().can_start; }));
    REQUIRE(joiner.set_ready(false));
    REQUIRE(joiner.set_ready(true));
    const bool rapid_flip_launch = host.send_launch();
    if (rapid_flip_launch) {
        (void)host.mark_loaded();
    }
    std::this_thread::sleep_for(300ms);
    const auto rapid_host = host.snapshot();
    const auto rapid_joiner = joiner.snapshot();
    CHECK(rapid_host.state == PeerSessionState::Connected);
    CHECK(rapid_joiner.state == PeerSessionState::Connected);
    CHECK_FALSE(rapid_host.round_transition_pending);
    const bool canceled_baseline =
        !rapid_host.round_active && !rapid_joiner.round_active &&
        !rapid_host.local_ready && !rapid_host.remote_ready &&
        !rapid_joiner.local_ready && !rapid_joiner.remote_ready;
    const bool ready_lobby =
        !rapid_host.round_active && !rapid_joiner.round_active &&
        rapid_host.local_ready && rapid_host.remote_ready &&
        rapid_joiner.local_ready && rapid_joiner.remote_ready &&
        rapid_host.can_start;
    const bool active_round =
        rapid_host.round_active && rapid_joiner.round_active &&
        rapid_host.local_ready && rapid_host.remote_ready &&
        rapid_joiner.local_ready && rapid_joiner.remote_ready &&
        !rapid_host.can_start;
    CHECK(canceled_baseline || ready_lobby || active_round);

    host.disconnect("Loopback test complete");
    joiner.disconnect();
    CHECK(host.snapshot().state == PeerSessionState::Disconnected);
    CHECK(joiner.snapshot().state == PeerSessionState::Disconnected);
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
