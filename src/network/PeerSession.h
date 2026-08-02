#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "network/PeerProtocol.h"

namespace tenriff::network {

enum class PeerRole : uint8_t {
    None = 0,
    Host,
    Joiner,
};

enum class PeerSessionState : uint8_t {
    Idle = 0,
    Starting,
    Listening,
    Resolving,
    Connecting,
    Handshaking,
    Connected,
    Closing,
    Disconnected,
    Failed,
};

struct PeerChartInfo {
    ChartFingerprint fingerprint;
    std::string name;
};

struct PeerParticipantSnapshot {
    uint8_t player_id = 0;
    std::string name;
    bool local = false;
    bool leader = false;
    bool ready = false;
    bool loaded = false;
    bool round_reset = false;
    PeerChartInfo chart;
    bool has_score = false;
    PeerScore latest_score;
    uint32_t estimated_rtt_ms = 0;
};

struct PeerChatEntry {
    uint8_t player_id = 0;
    std::string text;
};

struct PeerSessionSnapshot {
    PeerRole role = PeerRole::None;
    PeerSessionState state = PeerSessionState::Idle;
    uint64_t revision = 0;
    uint16_t local_port = 0;

    std::string peer_name;
    std::string status_detail;

    uint8_t local_player_id = 0;
    uint8_t leader_player_id = 0;
    std::size_t participant_count = 0;
    bool local_is_leader = false;
    std::vector<PeerParticipantSnapshot> participants;
    std::vector<PeerChatEntry> chat_messages;

    bool remote_library_ready = false;
    std::size_t remote_library_count = 0;
    uint64_t remote_library_revision = 0;
    std::shared_ptr<const std::unordered_set<std::string>> remote_library_sha256;

    PeerChartInfo local_chart;
    PeerChartInfo remote_chart;
    PeerChartInfo selected_chart;
    bool local_ready = false;
    bool remote_ready = false;
    bool local_loaded = false;
    bool remote_loaded = false;
    bool can_start = false;
    uint32_t estimated_rtt_ms = 0;
    bool round_active = false;
    bool round_transition_pending = false;
    bool local_round_reset = false;
    bool remote_round_reset = false;

    bool has_remote_score = false;
    bool all_remote_finished = false;
    PeerScore latest_remote_score;
};

/// Direct-IP TCP room for up to eight players. The network host remains the
/// coordinator while chart/start leadership rotates after completed rounds.
/// Socket work stays on one background thread; callers use snapshots and commands.
class PeerSession {
public:
    PeerSession();
    ~PeerSession();

    PeerSession(const PeerSession&) = delete;
    PeerSession& operator=(const PeerSession&) = delete;

    /// Starts listening on all IPv4 interfaces. Port zero requests an ephemeral
    /// port, exposed through snapshot().local_port once Listening is reached.
    [[nodiscard]] bool host(uint16_t port, std::string name);

    /// Resolves an IPv4 literal or hostname and connects asynchronously.
    [[nodiscard]] bool join(std::string address, uint16_t port, std::string name);

    /// Sends a best-effort Disconnect frame, closes sockets, and joins the worker.
    void disconnect(std::string reason = "Local disconnect");

    [[nodiscard]] PeerSessionSnapshot snapshot() const;

    /// Replaces the locally indexed chart inventory. Valid SHA-256 values are
    /// deduplicated and announced in bounded chunks after the handshake.
    void set_local_library(std::vector<std::string> chart_sha256);

    /// Publishes the locally selected chart and invalidates round readiness.
    [[nodiscard]] bool set_local_chart(const ChartFingerprint& fingerprint,
                                       std::string display_name = {});
    /// Clears retained local chart state. Used when leaving multiplayer so a
    /// later join cannot announce a stale chart chosen in an earlier session.
    void clear_local_chart();
    [[nodiscard]] bool set_ready(bool ready);

    /// Sends a bounded room-chat line. The coordinator attributes the sender
    /// from the authenticated room link before broadcasting it.
    [[nodiscard]] bool send_chat(std::string text);

    /// Leader-only launch barrier. The current chart must match and every participant must
    /// be ready. The joiner consumes the chart hash through poll_launch().
    [[nodiscard]] bool send_launch();
    [[nodiscard]] std::optional<uint64_t> poll_launch();

    [[nodiscard]] bool mark_loaded();
    [[nodiscard]] bool wait_for_peer_loaded(std::chrono::milliseconds timeout);

    /// Leader-only begin command, normally sent after every participant reports Loaded.
    [[nodiscard]] bool send_begin(uint32_t delay_ms);
    [[nodiscard]] bool wait_for_begin(std::chrono::milliseconds timeout,
                                      uint32_t& out_delay_ms);

    /// Non-final scores are coalesced and sent at most ten times per second.
    /// Final scores bypass that throttle and are queued immediately.
    [[nodiscard]] bool publish_score(const PeerScore& score, bool final = false);

    /// Leaves the current result/round while keeping the connection and chart.
    /// A launched round finishes resetting only after every connected player calls this.
    void reset_round();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::network
