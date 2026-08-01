#include "network/PeerSession.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>
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

namespace tenriff::network {

namespace {

using SteadyClock = std::chrono::steady_clock;

constexpr std::size_t kMaxControlMessages = 128;
constexpr std::size_t kMaxWireBytes = 128 * 1024;
constexpr std::size_t kMaxReceiveBytes = 2 * kPeerMaxPayloadSize + kPeerFrameHeaderSize;
constexpr auto kIoPollInterval = std::chrono::milliseconds(20);
constexpr auto kConnectTimeout = std::chrono::seconds(5);

bool normalize_library_sha256(std::string& value) {
    if (value.size() != 64u) return false;
    for (char& ch : value) {
        if (ch >= '0' && ch <= '9') continue;
        if (ch >= 'a' && ch <= 'f') continue;
        if (ch >= 'A' && ch <= 'F') {
            ch = static_cast<char>(ch - 'A' + 'a');
            continue;
        }
        return false;
    }
    return true;
}

constexpr auto kHandshakeTimeout = std::chrono::seconds(5);
constexpr auto kHeartbeatInterval = std::chrono::seconds(2);
constexpr auto kPeerTimeout = std::chrono::seconds(10);
constexpr auto kGracefulCloseTimeout = std::chrono::milliseconds(250);
constexpr auto kScoreInterval = std::chrono::milliseconds(100);

bool chart_matches(const PeerChartInfo& lhs, const PeerChartInfo& rhs) {
    return lhs.fingerprint.valid() && rhs.fingerprint.valid() &&
           lhs.fingerprint.hash == rhs.fingerprint.hash &&
           lhs.fingerprint.size == rhs.fingerprint.size;
}

#ifdef _WIN32

std::string winsock_error(std::string_view operation, int error) {
    std::string result(operation);
    result += " failed (Winsock ";
    result += std::to_string(error);
    result += ").";
    return result;
}

class WinsockRuntime {
public:
    bool start(std::string& error) {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            error = winsock_error("WSAStartup", result);
            return false;
        }
        started_ = true;
        if (LOBYTE(data.wVersion) != 2 || HIBYTE(data.wVersion) != 2) {
            error = "Winsock 2.2 is not available.";
            return false;
        }
        return true;
    }

    ~WinsockRuntime() {
        if (started_) {
            WSACleanup();
        }
    }

private:
    bool started_ = false;
};

class SocketHandle {
public:
    SocketHandle() = default;
    explicit SocketHandle(SOCKET value) : value_(value) {}
    ~SocketHandle() { reset(); }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : value_(other.release()) {}
    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] SOCKET get() const { return value_; }
    [[nodiscard]] bool valid() const { return value_ != INVALID_SOCKET; }

    SOCKET release() {
        const SOCKET result = value_;
        value_ = INVALID_SOCKET;
        return result;
    }

    void reset(SOCKET value = INVALID_SOCKET) {
        if (value_ != INVALID_SOCKET) {
            closesocket(value_);
        }
        value_ = value;
    }

private:
    SOCKET value_ = INVALID_SOCKET;
};

bool set_nonblocking(SOCKET socket, std::string& error) {
    u_long enabled = 1;
    if (ioctlsocket(socket, FIONBIO, &enabled) == SOCKET_ERROR) {
        error = winsock_error("ioctlsocket(FIONBIO)", WSAGetLastError());
        return false;
    }
    return true;
}

bool configure_peer_socket(SOCKET socket, std::string& error) {
    if (!set_nonblocking(socket, error)) {
        return false;
    }
    const BOOL enabled = TRUE;
    if (setsockopt(socket,
                   IPPROTO_TCP,
                   TCP_NODELAY,
                   reinterpret_cast<const char*>(&enabled),
                   sizeof(enabled)) == SOCKET_ERROR) {
        error = winsock_error("setsockopt(TCP_NODELAY)", WSAGetLastError());
        return false;
    }
    return true;
}

struct WireQueue {
    std::deque<std::vector<uint8_t>> frames;
    std::size_t front_offset = 0;
    std::size_t bytes = 0;

    [[nodiscard]] bool empty() const { return frames.empty(); }

    bool push(const PeerMessage& message, std::string& error) {
        std::vector<uint8_t> frame = encode_peer_message(message, &error);
        if (frame.empty()) {
            if (error.empty()) error = "Peer protocol encoded an empty frame.";
            return false;
        }
        if (frame.size() > kMaxWireBytes || bytes > kMaxWireBytes - frame.size()) {
            error = "Peer outgoing queue exceeded its byte limit.";
            return false;
        }
        bytes += frame.size();
        frames.push_back(std::move(frame));
        return true;
    }

    bool flush(SOCKET socket, bool& sent_any, std::string& error) {
        sent_any = false;
        while (!frames.empty()) {
            const std::vector<uint8_t>& frame = frames.front();
            const std::size_t remaining = frame.size() - front_offset;
            const int request = static_cast<int>(std::min<std::size_t>(
                remaining, static_cast<std::size_t>((std::numeric_limits<int>::max)())));
            const int sent = send(socket,
                                  reinterpret_cast<const char*>(frame.data() + front_offset),
                                  request,
                                  0);
            if (sent > 0) {
                const std::size_t sent_size = static_cast<std::size_t>(sent);
                front_offset += sent_size;
                bytes -= sent_size;
                sent_any = true;
                if (front_offset == frame.size()) {
                    frames.pop_front();
                    front_offset = 0;
                }
                continue;
            }
            if (sent == 0) {
                error = "Peer socket stopped accepting outgoing data.";
                return false;
            }
            const int socket_error = WSAGetLastError();
            if (socket_error == WSAEWOULDBLOCK) {
                return true;
            }
            error = winsock_error("send", socket_error);
            return false;
        }
        return true;
    }
};

#endif  // _WIN32

}  // namespace

struct PeerSession::Impl {
    mutable std::mutex mutex;
    std::condition_variable state_cv;
    std::thread worker;
    std::atomic<bool> hard_stop{false};
    std::atomic<bool> graceful_stop{false};

    PeerSessionSnapshot current;
    std::string local_name;
    bool worker_running = false;

    std::deque<PeerMessage> controls;
    bool chart_announcement_pending = false;
    std::vector<std::string> local_library_sha256;
    std::size_t local_library_cursor = 0;
    bool library_begin_pending = false;
    bool library_end_pending = false;
    bool remote_library_receiving = false;
    uint32_t remote_library_expected = 0;
    std::vector<std::string> remote_library_builder;
    std::optional<PeerScore> pending_score;
    bool pending_score_dirty = false;

    std::optional<uint64_t> pending_launch;
    std::optional<uint32_t> pending_begin;
    bool round_launch_active = false;
    uint64_t active_round_nonce = 0;
    uint64_t next_round_nonce = 0;
    uint64_t last_closed_round_nonce = 0;
    bool local_round_reset = false;
    bool remote_round_reset = false;
    bool local_final_score_sent = false;
    bool begin_sent = false;
    uint64_t pending_cancel_ack_nonce = 0;
    uint64_t pending_ping_nonce = 0;
    SteadyClock::time_point pending_ping_sent_at{};

    void recompute_can_start_locked() {
        current.can_start = current.role == PeerRole::Host &&
                            current.state == PeerSessionState::Connected &&
                            !round_launch_active &&
                            pending_cancel_ack_nonce == 0 &&
                            current.local_ready && current.remote_ready &&
                            chart_matches(current.local_chart, current.remote_chart);
    }

    void touch_locked() {
        current.round_active = round_launch_active;
        current.round_transition_pending = pending_cancel_ack_nonce != 0;
        current.local_round_reset = local_round_reset;
        current.remote_round_reset = remote_round_reset;
        recompute_can_start_locked();
        ++current.revision;
        state_cv.notify_all();
    }

    void set_state_locked(PeerSessionState state, std::string detail) {
        current.state = state;
        current.status_detail = std::move(detail);
        touch_locked();
    }

    void set_state(PeerSessionState state, std::string detail) {
        std::lock_guard<std::mutex> lock(mutex);
        set_state_locked(state, std::move(detail));
    }

    void fail(std::string detail) {
        std::lock_guard<std::mutex> lock(mutex);
        set_state_locked(PeerSessionState::Failed, std::move(detail));
    }

    bool queue_control_locked(PeerMessage message) {
        if (controls.size() >= kMaxControlMessages) {
            current.status_detail = "Peer command queue is full.";
            touch_locked();
            return false;
        }
        controls.push_back(std::move(message));
        return true;
    }

    void invalidate_round_locked(bool clear_remote_ready) {
        if (active_round_nonce != 0) {
            last_closed_round_nonce = active_round_nonce;
        }
        current.local_ready = false;
        if (clear_remote_ready) current.remote_ready = false;
        current.local_loaded = false;
        current.remote_loaded = false;
        current.has_remote_score = false;
        current.latest_remote_score = {};
        pending_score.reset();
        pending_score_dirty = false;
        pending_launch.reset();
        pending_begin.reset();
        round_launch_active = false;
        active_round_nonce = 0;
        local_round_reset = false;
        remote_round_reset = false;
        local_final_score_sent = false;
        begin_sent = false;
    }

    bool start(PeerRole role, std::string address, uint16_t port, std::string name) {
        std::thread completed_worker;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (worker_running) {
                return false;
            }
            if (worker.joinable()) {
                completed_worker = std::move(worker);
            }
        }
        if (completed_worker.joinable()) {
            completed_worker.join();
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            const PeerChartInfo retained_chart = current.local_chart;
            const uint64_t next_revision = current.revision + 1;
            current = {};
            current.local_chart = retained_chart;
            current.role = role;
            current.state = PeerSessionState::Starting;
            current.status_detail = role == PeerRole::Host ? "Starting peer host." : "Starting peer connection.";
            current.revision = next_revision;
            local_name = std::move(name);
            controls.clear();
            chart_announcement_pending = retained_chart.fingerprint.valid();
            local_library_cursor = 0;
            library_begin_pending = true;
            library_end_pending = true;
            remote_library_receiving = false;
            remote_library_expected = 0;
            remote_library_builder.clear();
            pending_score.reset();
            pending_score_dirty = false;
            pending_launch.reset();
            pending_begin.reset();
            round_launch_active = false;
            active_round_nonce = 0;
            next_round_nonce = 0;
            last_closed_round_nonce = 0;
            local_round_reset = false;
            remote_round_reset = false;
            local_final_score_sent = false;
            begin_sent = false;
            pending_cancel_ack_nonce = 0;
            pending_ping_nonce = 0;
            pending_ping_sent_at = {};
            hard_stop.store(false, std::memory_order_release);
            graceful_stop.store(false, std::memory_order_release);
            worker_running = true;
            state_cv.notify_all();
        }

        try {
            worker = std::thread(&Impl::worker_main, this, role, std::move(address), port);
        } catch (const std::exception& error) {
            std::lock_guard<std::mutex> lock(mutex);
            worker_running = false;
            set_state_locked(PeerSessionState::Failed,
                             std::string("Could not start peer worker: ") + error.what());
            return false;
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            worker_running = false;
            set_state_locked(PeerSessionState::Failed, "Could not start peer worker.");
            return false;
        }
        return true;
    }

    void finish_worker() {
        std::lock_guard<std::mutex> lock(mutex);
        worker_running = false;
        // A launch/begin command is only actionable while this exact socket is
        // alive.  Never let a command received just before disconnect leak into
        // the menu loop (or a later connection) as a phantom game start.
        pending_launch.reset();
        pending_begin.reset();
        if (current.state == PeerSessionState::Closing ||
            current.state == PeerSessionState::Starting ||
            current.state == PeerSessionState::Listening ||
            current.state == PeerSessionState::Resolving ||
            current.state == PeerSessionState::Connecting ||
            current.state == PeerSessionState::Handshaking ||
            current.state == PeerSessionState::Connected) {
            set_state_locked(PeerSessionState::Disconnected, "Disconnected.");
        } else {
            state_cv.notify_all();
        }
    }

    enum class IncomingResult {
        Continue,
        RemoteDisconnect,
        ProtocolError,
    };

    IncomingResult handle_incoming(const PeerMessage& message,
                                   bool& handshake_complete,
                                   std::optional<PeerMessage>& response,
                                   std::string& error) {
        if (!handshake_complete) {
            if (message.type != PeerMessageType::Hello) {
                error = "Peer sent data before completing the handshake.";
                return IncomingResult::ProtocolError;
            }
            if (message.text.empty() || message.text.size() > 64) {
                error = "Peer handshake name is invalid.";
                return IncomingResult::ProtocolError;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                current.peer_name = message.text;
                set_state_locked(PeerSessionState::Connected, "Connected to " + message.text + ".");
            }
            handshake_complete = true;
            return IncomingResult::Continue;
        }

        if (message.type == PeerMessageType::Hello) {
            error = "Peer sent a duplicate handshake.";
            return IncomingResult::ProtocolError;
        }

        std::lock_guard<std::mutex> lock(mutex);
        switch (message.type) {
            case PeerMessageType::Chart: {
                if (round_launch_active) {
                    error = "Peer changed the chart before both players left the result.";
                    return IncomingResult::ProtocolError;
                }
                const bool must_clear_local_ready = current.local_ready;
                current.remote_chart.fingerprint.hash = message.chart_hash;
                current.remote_chart.fingerprint.size = message.chart_size;
                current.remote_chart.name = message.text;
                invalidate_round_locked(true);
                if (must_clear_local_ready) {
                    PeerMessage ready;
                    ready.type = PeerMessageType::Ready;
                    ready.ready = false;
                    response = std::move(ready);
                }
                touch_locked();
                return IncomingResult::Continue;
            }
            case PeerMessageType::Ready:
                if (pending_cancel_ack_nonce != 0) {
                    // Ready frames queued before the peer saw RoundCancel are
                    // stale by definition. Discard them until the ordered ACK
                    // proves both sides returned to the lobby baseline.
                    current.remote_ready = false;
                    current.remote_loaded = false;
                    touch_locked();
                    return IncomingResult::Continue;
                }
                if (round_launch_active) {
                    // A joiner can leave Ready just before the host's Launch
                    // reaches it. Cancel that pre-start race without treating a
                    // lobby readiness frame as a completed-result signal.
                    if (!message.ready && current.role == PeerRole::Host &&
                        !begin_sent && !current.remote_loaded) {
                        const uint64_t canceled_nonce = active_round_nonce;
                        invalidate_round_locked(true);
                        pending_cancel_ack_nonce = canceled_nonce;
                        current.status_detail = "Match launch canceled because peer readiness changed.";
                        PeerMessage cancel;
                        cancel.type = PeerMessageType::RoundCancel;
                        cancel.nonce = canceled_nonce;
                        if (!queue_control_locked(std::move(cancel))) {
                            error = "Could not queue multiplayer launch cancellation.";
                            return IncomingResult::ProtocolError;
                        }
                        touch_locked();
                        return IncomingResult::Continue;
                    }
                    error = "Peer changed readiness after launch.";
                    return IncomingResult::ProtocolError;
                }
                current.remote_ready = message.ready;
                if (!message.ready) current.remote_loaded = false;
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::RoundReset:
                if (!round_launch_active || message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) {
                        return IncomingResult::Continue;
                    }
                    error = "Peer reset an unknown multiplayer round.";
                    return IncomingResult::ProtocolError;
                }
                if (!current.has_remote_score || !current.latest_remote_score.finished) {
                    error = "Peer reset the round before sending a final score.";
                    return IncomingResult::ProtocolError;
                }
                remote_round_reset = true;
                current.remote_ready = false;
                current.remote_loaded = false;
                if (local_round_reset) {
                    invalidate_round_locked(true);
                }
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::RoundCancel:
                if ((!round_launch_active || message.nonce != active_round_nonce) &&
                    message.nonce != last_closed_round_nonce) {
                    error = "Peer canceled an unknown multiplayer round.";
                    return IncomingResult::ProtocolError;
                }
                // Even if Launch was already declined, synchronize both lobby
                // Ready flags to false before acknowledging the cancellation.
                invalidate_round_locked(true);
                current.status_detail = "Match launch canceled because readiness changed.";
                {
                    PeerMessage ack;
                    ack.type = PeerMessageType::RoundCancelAck;
                    ack.nonce = message.nonce;
                    if (!queue_control_locked(std::move(ack))) {
                        error = "Could not queue multiplayer cancellation acknowledgment.";
                        return IncomingResult::ProtocolError;
                    }
                }
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::RoundCancelAck:
                if (pending_cancel_ack_nonce == message.nonce && message.nonce != 0) {
                    pending_cancel_ack_nonce = 0;
                    current.remote_ready = false;
                    current.remote_loaded = false;
                    current.status_detail = "Match launch cancellation synchronized.";
                    touch_locked();
                    return IncomingResult::Continue;
                }
                if (pending_cancel_ack_nonce == 0 &&
                    message.nonce == last_closed_round_nonce) {
                    return IncomingResult::Continue;
                }
                error = "Peer acknowledged an unknown multiplayer cancellation.";
                return IncomingResult::ProtocolError;
            case PeerMessageType::Launch:
                if (current.role != PeerRole::Joiner) {
                    error = "Only the host may send a launch command.";
                    return IncomingResult::ProtocolError;
                }
                if (!current.local_ready || !current.remote_ready) {
                    // The local Ready(false) may have crossed the host's Launch.
                    // Decline the launch and repeat Ready(false) so the host can
                    // cancel its pre-start barrier without disconnecting.
                    last_closed_round_nonce = message.nonce;
                    current.local_ready = false;
                    current.local_loaded = false;
                    PeerMessage ready;
                    ready.type = PeerMessageType::Ready;
                    ready.ready = false;
                    response = std::move(ready);
                    current.status_detail = "Match launch ignored because readiness changed.";
                    touch_locked();
                    return IncomingResult::Continue;
                }
                if (!current.local_chart.fingerprint.valid() ||
                    current.local_chart.fingerprint.hash != message.chart_hash ||
                    !chart_matches(current.local_chart, current.remote_chart)) {
                    error = "Peer launch chart does not match the local chart.";
                    return IncomingResult::ProtocolError;
                }
                pending_launch = message.chart_hash;
                pending_begin.reset();
                round_launch_active = true;
                active_round_nonce = message.nonce;
                local_round_reset = false;
                remote_round_reset = false;
                local_final_score_sent = false;
                begin_sent = false;
                current.local_loaded = false;
                current.remote_loaded = false;
                current.has_remote_score = false;
                current.latest_remote_score = {};
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::Loaded:
                if (!round_launch_active || message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) {
                        return IncomingResult::Continue;
                    }
                    error = "Peer reported Loaded before launch.";
                    return IncomingResult::ProtocolError;
                }
                current.remote_loaded = true;
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::Begin:
                if (current.role != PeerRole::Joiner || !round_launch_active ||
                    message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) {
                        return IncomingResult::Continue;
                    }
                    error = "Unexpected Begin command.";
                    return IncomingResult::ProtocolError;
                }
                pending_begin = message.delay_ms;
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::Score:
            case PeerMessageType::FinalScore:
                if (!round_launch_active || message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) {
                        return IncomingResult::Continue;
                    }
                    error = "Peer sent score data before launch.";
                    return IncomingResult::ProtocolError;
                }
                current.latest_remote_score = message.score;
                current.has_remote_score = true;
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::Ping: {
                PeerMessage pong;
                pong.type = PeerMessageType::Pong;
                pong.nonce = message.nonce;
                response = std::move(pong);
                return IncomingResult::Continue;
            }
            case PeerMessageType::Pong:
                if (message.nonce == pending_ping_nonce && pending_ping_nonce != 0) {
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        SteadyClock::now() - pending_ping_sent_at);
                    current.estimated_rtt_ms = static_cast<uint32_t>(
                        std::clamp<int64_t>(elapsed.count(), 1, 2000));
                    pending_ping_nonce = 0;
                    touch_locked();
                }
                return IncomingResult::Continue;
            case PeerMessageType::LibraryBegin:
                remote_library_receiving = true;
                remote_library_expected = message.library_count;
                remote_library_builder.clear();
                remote_library_builder.reserve(message.library_count);
                current.remote_library_ready = false;
                current.remote_library_count = 0;
                current.remote_library_sha256.reset();
                touch_locked();
                return IncomingResult::Continue;
            case PeerMessageType::LibraryChunk:
                if (!remote_library_receiving || message.chart_sha256.empty() ||
                    remote_library_builder.size() + message.chart_sha256.size() > remote_library_expected) {
                    error = "Peer sent an unexpected library chunk.";
                    return IncomingResult::ProtocolError;
                }
                remote_library_builder.insert(remote_library_builder.end(),
                                              message.chart_sha256.begin(),
                                              message.chart_sha256.end());
                return IncomingResult::Continue;
            case PeerMessageType::LibraryEnd: {
                if (!remote_library_receiving ||
                    remote_library_builder.size() != remote_library_expected) {
                    error = "Peer library transfer ended with the wrong chart count.";
                    return IncomingResult::ProtocolError;
                }
                auto hashes = std::make_shared<std::unordered_set<std::string>>();
                hashes->reserve(remote_library_builder.size());
                for (const auto& sha256 : remote_library_builder) {
                    if (!hashes->insert(sha256).second) {
                        error = "Peer library transfer contains duplicate chart hashes.";
                        return IncomingResult::ProtocolError;
                    }
                }
                current.remote_library_sha256 = std::move(hashes);
                current.remote_library_count = remote_library_builder.size();
                current.remote_library_ready = true;
                ++current.remote_library_revision;
                remote_library_receiving = false;
                remote_library_expected = 0;
                remote_library_builder.clear();
                touch_locked();
                return IncomingResult::Continue;
            }
            case PeerMessageType::Disconnect:
                set_state_locked(PeerSessionState::Disconnected,
                                 message.text.empty() ? "Peer disconnected."
                                                      : "Peer disconnected: " + message.text);
                return IncomingResult::RemoteDisconnect;
            default:
                error = "Peer sent an unsupported session message.";
                return IncomingResult::ProtocolError;
        }
    }

#ifdef _WIN32
    bool collect_outgoing(WireQueue& wire,
                          bool handshake_complete,
                          SteadyClock::time_point now,
                          SteadyClock::time_point& last_score_sent,
                          std::string& error) {
        std::deque<PeerMessage> messages;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!handshake_complete && !graceful_stop.load(std::memory_order_acquire)) {
                return true;
            }
            messages.insert(messages.end(),
                            std::make_move_iterator(controls.begin()),
                            std::make_move_iterator(controls.end()));
            controls.clear();

            // RoundReset/Ready controls must stay ahead of a newly selected
            // chart. Otherwise one peer can still be inside the previous
            // Result while the next Chart frame reaches it first.
            if (handshake_complete && chart_announcement_pending &&
                current.local_chart.fingerprint.valid() &&
                current.state != PeerSessionState::Closing) {
                PeerMessage chart;
                chart.type = PeerMessageType::Chart;
                chart.chart_hash = current.local_chart.fingerprint.hash;
                chart.chart_size = current.local_chart.fingerprint.size;
                chart.text = current.local_chart.name;
                messages.push_back(std::move(chart));
                chart_announcement_pending = false;
            }

            if (handshake_complete && current.state != PeerSessionState::Closing &&
                !graceful_stop.load(std::memory_order_acquire)) {
                if (library_begin_pending) {
                    PeerMessage begin;
                    begin.type = PeerMessageType::LibraryBegin;
                    begin.library_count = static_cast<uint32_t>(local_library_sha256.size());
                    messages.push_back(std::move(begin));
                    library_begin_pending = false;
                } else if (local_library_cursor < local_library_sha256.size()) {
                    const std::size_t end = std::min(
                        local_library_sha256.size(),
                        local_library_cursor + kPeerLibraryHashesPerChunk);
                    PeerMessage chunk;
                    chunk.type = PeerMessageType::LibraryChunk;
                    chunk.chart_sha256.assign(local_library_sha256.begin() + local_library_cursor,
                                               local_library_sha256.begin() + end);
                    messages.push_back(std::move(chunk));
                    local_library_cursor = end;
                } else if (library_end_pending) {
                    PeerMessage end;
                    end.type = PeerMessageType::LibraryEnd;
                    messages.push_back(std::move(end));
                    library_end_pending = false;
                }
            }

            if (handshake_complete && !graceful_stop.load(std::memory_order_acquire) &&
                round_launch_active && active_round_nonce != 0 &&
                pending_score_dirty && pending_score.has_value() &&
                now - last_score_sent >= kScoreInterval) {
                PeerMessage score;
                score.type = PeerMessageType::Score;
                score.nonce = active_round_nonce;
                score.score = *pending_score;
                messages.push_back(std::move(score));
                pending_score_dirty = false;
                last_score_sent = now;
            }
        }

        for (const PeerMessage& message : messages) {
            if (!wire.push(message, error)) {
                return false;
            }
        }
        return true;
    }

    SocketHandle create_listener(uint16_t port, std::string& error) {
        SocketHandle listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!listener.valid()) {
            error = winsock_error("socket", WSAGetLastError());
            return {};
        }

        const BOOL exclusive = TRUE;
        if (setsockopt(listener.get(),
                       SOL_SOCKET,
                       SO_EXCLUSIVEADDRUSE,
                       reinterpret_cast<const char*>(&exclusive),
                       sizeof(exclusive)) == SOCKET_ERROR) {
            error = winsock_error("setsockopt(SO_EXCLUSIVEADDRUSE)", WSAGetLastError());
            return {};
        }
        if (!set_nonblocking(listener.get(), error)) {
            return {};
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        if (bind(listener.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            error = winsock_error("bind", WSAGetLastError());
            return {};
        }
        if (listen(listener.get(), 1) == SOCKET_ERROR) {
            error = winsock_error("listen", WSAGetLastError());
            return {};
        }

        sockaddr_in bound{};
        int bound_size = sizeof(bound);
        if (getsockname(listener.get(), reinterpret_cast<sockaddr*>(&bound), &bound_size) == SOCKET_ERROR) {
            error = winsock_error("getsockname", WSAGetLastError());
            return {};
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            current.local_port = ntohs(bound.sin_port);
            set_state_locked(PeerSessionState::Listening,
                             "Listening on IPv4 port " + std::to_string(current.local_port) + ".");
        }
        return listener;
    }

    SocketHandle accept_peer(SocketHandle& listener, std::string& error) {
        while (!hard_stop.load(std::memory_order_acquire)) {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(listener.get(), &read_set);
            timeval timeout{};
            timeout.tv_usec = static_cast<long>(
                std::chrono::duration_cast<std::chrono::microseconds>(kIoPollInterval).count());
            const int selected = select(0, &read_set, nullptr, nullptr, &timeout);
            if (selected == SOCKET_ERROR) {
                error = winsock_error("select(accept)", WSAGetLastError());
                return {};
            }
            if (selected == 0) continue;

            SocketHandle peer(accept(listener.get(), nullptr, nullptr));
            if (!peer.valid()) {
                const int accept_error = WSAGetLastError();
                if (accept_error == WSAEWOULDBLOCK) continue;
                error = winsock_error("accept", accept_error);
                return {};
            }
            if (!configure_peer_socket(peer.get(), error)) {
                return {};
            }
            return peer;
        }
        return {};
    }

    SocketHandle connect_peer(const std::string& address, uint16_t port, std::string& error) {
        set_state(PeerSessionState::Resolving, "Resolving " + address + ".");

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        addrinfo* results = nullptr;
        const std::string service = std::to_string(port);
        const int resolve_result = getaddrinfo(address.c_str(), service.c_str(), &hints, &results);
        if (resolve_result != 0) {
            error = "Could not resolve peer address (Winsock " + std::to_string(resolve_result) + ").";
            return {};
        }

        struct AddrInfoGuard {
            addrinfo* value = nullptr;
            ~AddrInfoGuard() { if (value) freeaddrinfo(value); }
        } results_guard{results};

        if (hard_stop.load(std::memory_order_acquire)) {
            return {};
        }
        set_state(PeerSessionState::Connecting,
                  "Connecting to " + address + ":" + std::to_string(port) + ".");

        const auto deadline = SteadyClock::now() + kConnectTimeout;
        int last_error = WSAETIMEDOUT;
        for (addrinfo* candidate = results; candidate && SteadyClock::now() < deadline;
             candidate = candidate->ai_next) {
            if (hard_stop.load(std::memory_order_acquire)) return {};
            SocketHandle peer(socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol));
            if (!peer.valid()) {
                last_error = WSAGetLastError();
                continue;
            }
            if (!set_nonblocking(peer.get(), error)) {
                last_error = WSAGetLastError();
                continue;
            }

            const int connect_result = connect(peer.get(), candidate->ai_addr,
                                               static_cast<int>(candidate->ai_addrlen));
            if (connect_result == 0) {
                if (!configure_peer_socket(peer.get(), error)) return {};
                return peer;
            }
            last_error = WSAGetLastError();
            if (last_error != WSAEWOULDBLOCK && last_error != WSAEINPROGRESS) {
                continue;
            }

            while (!hard_stop.load(std::memory_order_acquire) && SteadyClock::now() < deadline) {
                fd_set write_set;
                fd_set except_set;
                FD_ZERO(&write_set);
                FD_ZERO(&except_set);
                FD_SET(peer.get(), &write_set);
                FD_SET(peer.get(), &except_set);
                timeval timeout{};
                timeout.tv_usec = static_cast<long>(
                    std::chrono::duration_cast<std::chrono::microseconds>(kIoPollInterval).count());
                const int selected = select(0, nullptr, &write_set, &except_set, &timeout);
                if (selected == SOCKET_ERROR) {
                    last_error = WSAGetLastError();
                    break;
                }
                if (selected == 0) continue;

                int socket_error = 0;
                int socket_error_size = sizeof(socket_error);
                if (getsockopt(peer.get(), SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char*>(&socket_error),
                               &socket_error_size) == SOCKET_ERROR) {
                    last_error = WSAGetLastError();
                    break;
                }
                if (socket_error == 0 && FD_ISSET(peer.get(), &write_set)) {
                    if (!configure_peer_socket(peer.get(), error)) return {};
                    return peer;
                }
                last_error = socket_error == 0 ? WSAECONNREFUSED : socket_error;
                break;
            }
        }

        if (!hard_stop.load(std::memory_order_acquire)) {
            error = winsock_error("connect", last_error);
        }
        return {};
    }

    bool run_connected(SocketHandle& peer, std::string& error) {
        set_state(PeerSessionState::Handshaking, "Exchanging peer handshake.");

        WireQueue wire;
        PeerMessage hello;
        hello.type = PeerMessageType::Hello;
        {
            std::lock_guard<std::mutex> lock(mutex);
            hello.text = local_name;
        }
        if (!wire.push(hello, error)) return false;

        std::vector<uint8_t> received;
        received.reserve(16 * 1024);
        bool handshake_complete = false;
        bool remote_disconnected = false;
        bool shutdown_sent = false;
        auto now = SteadyClock::now();
        auto last_received = now;
        auto last_ping_sent = now - kHeartbeatInterval;
        auto last_score_sent = now - kScoreInterval;
        const auto handshake_deadline = now + kHandshakeTimeout;
        std::optional<SteadyClock::time_point> close_deadline;
        uint64_t heartbeat_nonce = 0;

        while (!hard_stop.load(std::memory_order_acquire)) {
            now = SteadyClock::now();
            const bool closing = graceful_stop.load(std::memory_order_acquire);
            if (closing && !close_deadline.has_value()) {
                close_deadline = now + kGracefulCloseTimeout;
            }

            if (!collect_outgoing(wire, handshake_complete, now, last_score_sent, error)) {
                return false;
            }
            if (!closing && handshake_complete && now - last_ping_sent >= kHeartbeatInterval) {
                PeerMessage ping;
                ping.type = PeerMessageType::Ping;
                ping.nonce = ++heartbeat_nonce;
                if (!wire.push(ping, error)) return false;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    pending_ping_nonce = ping.nonce;
                    pending_ping_sent_at = now;
                }
                last_ping_sent = now;
            }
            if (!handshake_complete && now >= handshake_deadline) {
                error = "Peer handshake timed out.";
                return false;
            }
            if (handshake_complete && !closing && now - last_received >= kPeerTimeout) {
                error = "Peer connection timed out.";
                return false;
            }

            if (closing && !shutdown_sent && wire.empty()) {
                shutdown(peer.get(), SD_SEND);
                shutdown_sent = true;
            }
            if (closing && close_deadline.has_value() && now >= *close_deadline) {
                break;
            }

            fd_set read_set;
            fd_set write_set;
            fd_set except_set;
            FD_ZERO(&read_set);
            FD_ZERO(&write_set);
            FD_ZERO(&except_set);
            FD_SET(peer.get(), &read_set);
            FD_SET(peer.get(), &except_set);
            if (!wire.empty()) FD_SET(peer.get(), &write_set);
            timeval timeout{};
            timeout.tv_usec = static_cast<long>(
                std::chrono::duration_cast<std::chrono::microseconds>(kIoPollInterval).count());
            const int selected = select(0,
                                        &read_set,
                                        wire.empty() ? nullptr : &write_set,
                                        &except_set,
                                        &timeout);
            if (selected == SOCKET_ERROR) {
                error = winsock_error("select(peer)", WSAGetLastError());
                return false;
            }
            if (selected == 0) continue;
            if (FD_ISSET(peer.get(), &except_set)) {
                int socket_error = 0;
                int socket_error_size = sizeof(socket_error);
                getsockopt(peer.get(), SOL_SOCKET, SO_ERROR,
                           reinterpret_cast<char*>(&socket_error), &socket_error_size);
                error = winsock_error("peer socket", socket_error == 0 ? WSAECONNRESET : socket_error);
                return false;
            }

            if (!wire.empty() && FD_ISSET(peer.get(), &write_set)) {
                bool sent_any = false;
                if (!wire.flush(peer.get(), sent_any, error)) return false;
            }

            if (FD_ISSET(peer.get(), &read_set)) {
                std::array<uint8_t, 8192> chunk{};
                for (;;) {
                    const int read = recv(peer.get(), reinterpret_cast<char*>(chunk.data()),
                                          static_cast<int>(chunk.size()), 0);
                    if (read > 0) {
                        const std::size_t read_size = static_cast<std::size_t>(read);
                        if (received.size() > kMaxReceiveBytes - read_size) {
                            error = "Peer receive buffer exceeded its byte limit.";
                            return false;
                        }
                        received.insert(received.end(), chunk.begin(), chunk.begin() + read);
                        last_received = SteadyClock::now();
                        continue;
                    }
                    if (read == 0) {
                        remote_disconnected = true;
                        break;
                    }
                    const int read_error = WSAGetLastError();
                    if (read_error == WSAEWOULDBLOCK) break;
                    error = winsock_error("recv", read_error);
                    return false;
                }

                int processed = 0;
                while (!received.empty() && processed < 64) {
                    PeerMessage message;
                    std::size_t consumed = 0;
                    std::string decode_error;
                    const PeerDecodeStatus status =
                        decode_peer_message(received, message, consumed, decode_error);
                    if (status == PeerDecodeStatus::Incomplete) break;
                    if (status == PeerDecodeStatus::Error) {
                        error = std::move(decode_error);
                        return false;
                    }
                    received.erase(received.begin(), received.begin() + static_cast<std::ptrdiff_t>(consumed));
                    ++processed;

                    std::optional<PeerMessage> response;
                    const IncomingResult incoming =
                        handle_incoming(message, handshake_complete, response, error);
                    if (incoming == IncomingResult::ProtocolError) return false;
                    if (response.has_value() && !wire.push(*response, error)) return false;
                    if (incoming == IncomingResult::RemoteDisconnect) {
                        remote_disconnected = true;
                        break;
                    }
                }
            }

            if (remote_disconnected) {
                std::lock_guard<std::mutex> lock(mutex);
                if (current.state != PeerSessionState::Disconnected) {
                    set_state_locked(PeerSessionState::Disconnected, "Peer closed the connection.");
                }
                break;
            }
        }
        return true;
    }
#endif  // _WIN32

    void worker_main(PeerRole role, std::string address, uint16_t port) {
#ifdef _WIN32
        std::string error;
        WinsockRuntime runtime;
        if (!runtime.start(error)) {
            fail(std::move(error));
            finish_worker();
            return;
        }

        SocketHandle peer;
        if (role == PeerRole::Host) {
            SocketHandle listener = create_listener(port, error);
            if (listener.valid()) {
                peer = accept_peer(listener, error);
            }
        } else {
            peer = connect_peer(address, port, error);
        }

        if (hard_stop.load(std::memory_order_acquire)) {
            finish_worker();
            return;
        }
        if (!peer.valid()) {
            fail(error.empty() ? "Peer connection could not be established." : std::move(error));
            finish_worker();
            return;
        }

        error.clear();
        if (!run_connected(peer, error) && !hard_stop.load(std::memory_order_acquire) &&
            !graceful_stop.load(std::memory_order_acquire)) {
            fail(error.empty() ? "Peer connection failed." : std::move(error));
        }
        if (peer.valid()) {
            shutdown(peer.get(), SD_BOTH);
        }
#else
        static_cast<void>(role);
        static_cast<void>(address);
        static_cast<void>(port);
        fail("Peer networking is only available on Windows.");
#endif
        finish_worker();
    }
};

PeerSession::PeerSession() : impl_(std::make_unique<Impl>()) {}

PeerSession::~PeerSession() {
    disconnect("Session shutdown");
}

bool PeerSession::host(uint16_t port, std::string name) {
    if (name.empty() || name.size() > 64) return false;
    return impl_->start(PeerRole::Host, {}, port, std::move(name));
}

bool PeerSession::join(std::string address, uint16_t port, std::string name) {
    if (address.empty() || address.size() > 255 || port == 0 || name.empty() || name.size() > 64) {
        return false;
    }
    return impl_->start(PeerRole::Joiner, std::move(address), port, std::move(name));
}

void PeerSession::disconnect(std::string reason) {
    bool join_worker = false;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        join_worker = impl_->worker.joinable();
        if (impl_->worker_running) {
            if (impl_->current.state == PeerSessionState::Connected) {
                if (reason.size() > 1024) reason.resize(1024);
                PeerMessage message;
                message.type = PeerMessageType::Disconnect;
                message.text = std::move(reason);
                if (impl_->controls.size() >= kMaxControlMessages) {
                    impl_->controls.clear();
                }
                impl_->controls.push_back(std::move(message));
                impl_->pending_score.reset();
                impl_->pending_score_dirty = false;
                impl_->set_state_locked(PeerSessionState::Closing, "Closing peer connection.");
                impl_->graceful_stop.store(true, std::memory_order_release);
            } else {
                impl_->hard_stop.store(true, std::memory_order_release);
            }
        }
    }
    impl_->state_cv.notify_all();
    if (join_worker && impl_->worker.joinable()) {
        impl_->worker.join();
    }
}

PeerSessionSnapshot PeerSession::snapshot() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->current;
}

void PeerSession::set_local_library(std::vector<std::string> chart_sha256) {
    std::vector<std::string> normalized;
    normalized.reserve(std::min(chart_sha256.size(), kPeerLibraryMaxCharts));
    for (auto& sha256 : chart_sha256) {
        if (normalize_library_sha256(sha256)) {
            normalized.push_back(std::move(sha256));
        }
    }
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
    if (normalized.size() > kPeerLibraryMaxCharts) {
        normalized.resize(kPeerLibraryMaxCharts);
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->local_library_sha256 = std::move(normalized);
    impl_->local_library_cursor = 0;
    impl_->library_begin_pending = true;
    impl_->library_end_pending = true;
    impl_->state_cv.notify_all();
}

bool PeerSession::set_local_chart(const ChartFingerprint& fingerprint, std::string display_name) {
    if (!fingerprint.valid() || display_name.size() > 1024) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->round_launch_active || impl_->pending_cancel_ack_nonce != 0 ||
        impl_->current.state == PeerSessionState::Closing) {
        return false;
    }

    const bool announce = impl_->worker_running;
    impl_->current.local_chart.fingerprint = fingerprint;
    impl_->current.local_chart.name = std::move(display_name);
    impl_->invalidate_round_locked(true);
    impl_->chart_announcement_pending = announce;
    if (impl_->current.state == PeerSessionState::Connected) {
        PeerMessage ready;
        ready.type = PeerMessageType::Ready;
        ready.ready = false;
        impl_->queue_control_locked(std::move(ready));
    }
    impl_->touch_locked();
    return true;
}

void PeerSession::clear_local_chart() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->current.local_chart = {};
    impl_->chart_announcement_pending = false;
    impl_->invalidate_round_locked(true);
    if (impl_->current.state == PeerSessionState::Connected) {
        // The Chart frame intentionally requires a non-zero hash. Ready(false)
        // still tells the peer that this side can no longer launch the round;
        // a later resolved chart announcement replaces its retained view.
        PeerMessage ready;
        ready.type = PeerMessageType::Ready;
        ready.ready = false;
        impl_->queue_control_locked(std::move(ready));
    }
    impl_->touch_locked();
}

bool PeerSession::set_ready(bool ready) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state != PeerSessionState::Connected || impl_->round_launch_active ||
        impl_->pending_cancel_ack_nonce != 0) {
        return false;
    }
    if (ready && !chart_matches(impl_->current.local_chart, impl_->current.remote_chart)) return false;

    PeerMessage message;
    message.type = PeerMessageType::Ready;
    message.ready = ready;
    if (!impl_->queue_control_locked(std::move(message))) return false;
    impl_->current.local_ready = ready;
    if (!ready) impl_->current.local_loaded = false;
    impl_->touch_locked();
    return true;
}

bool PeerSession::send_launch() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->current.can_start || impl_->round_launch_active) return false;

    uint64_t round_nonce = ++impl_->next_round_nonce;
    if (round_nonce == 0) round_nonce = ++impl_->next_round_nonce;
    PeerMessage message;
    message.type = PeerMessageType::Launch;
    message.chart_hash = impl_->current.local_chart.fingerprint.hash;
    message.nonce = round_nonce;
    if (!impl_->queue_control_locked(std::move(message))) return false;
    impl_->round_launch_active = true;
    impl_->active_round_nonce = round_nonce;
    impl_->local_round_reset = false;
    impl_->remote_round_reset = false;
    impl_->local_final_score_sent = false;
    impl_->begin_sent = false;
    impl_->current.local_loaded = false;
    impl_->current.remote_loaded = false;
    impl_->current.has_remote_score = false;
    impl_->current.latest_remote_score = {};
    impl_->touch_locked();
    return true;
}

std::optional<uint64_t> PeerSession::poll_launch() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state != PeerSessionState::Connected ||
        impl_->current.role != PeerRole::Joiner ||
        !impl_->round_launch_active) {
        impl_->pending_launch.reset();
        return std::nullopt;
    }
    std::optional<uint64_t> result = impl_->pending_launch;
    impl_->pending_launch.reset();
    return result;
}

bool PeerSession::mark_loaded() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state != PeerSessionState::Connected || !impl_->round_launch_active) return false;
    PeerMessage message;
    message.type = PeerMessageType::Loaded;
    message.nonce = impl_->active_round_nonce;
    if (!impl_->queue_control_locked(std::move(message))) return false;
    impl_->current.local_loaded = true;
    impl_->touch_locked();
    return true;
}

bool PeerSession::wait_for_peer_loaded(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->state_cv.wait_for(lock, timeout, [this]() {
        return impl_->current.remote_loaded ||
               !impl_->round_launch_active ||
               impl_->current.state == PeerSessionState::Disconnected ||
               impl_->current.state == PeerSessionState::Failed;
    });
    return impl_->current.remote_loaded;
}

bool PeerSession::send_begin(uint32_t delay_ms) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.role != PeerRole::Host ||
        impl_->current.state != PeerSessionState::Connected ||
        !impl_->round_launch_active || impl_->begin_sent ||
        !impl_->current.local_loaded || !impl_->current.remote_loaded) {
        return false;
    }
    PeerMessage message;
    message.type = PeerMessageType::Begin;
    message.nonce = impl_->active_round_nonce;
    message.delay_ms = delay_ms;
    if (!impl_->queue_control_locked(std::move(message))) return false;
    impl_->begin_sent = true;
    impl_->touch_locked();
    return true;
}

bool PeerSession::wait_for_begin(std::chrono::milliseconds timeout, uint32_t& out_delay_ms) {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->state_cv.wait_for(lock, timeout, [this]() {
        return impl_->pending_begin.has_value() ||
               !impl_->round_launch_active ||
               impl_->current.state == PeerSessionState::Disconnected ||
               impl_->current.state == PeerSessionState::Failed;
    });
    if (!impl_->pending_begin.has_value()) return false;
    out_delay_ms = *impl_->pending_begin;
    impl_->pending_begin.reset();
    return true;
}

bool PeerSession::publish_score(const PeerScore& score, bool final) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state != PeerSessionState::Connected || !impl_->round_launch_active) return false;
    const bool send_final = final || score.finished;
    if (send_final) {
        PeerMessage message;
        message.type = PeerMessageType::FinalScore;
        message.nonce = impl_->active_round_nonce;
        message.score = score;
        message.score.finished = true;
        if (!impl_->queue_control_locked(std::move(message))) return false;
        impl_->pending_score.reset();
        impl_->pending_score_dirty = false;
        impl_->local_final_score_sent = true;
        impl_->touch_locked();
        return true;
    }
    impl_->pending_score = score;
    impl_->pending_score_dirty = true;
    return true;
}

void PeerSession::reset_round() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->round_launch_active &&
        impl_->current.state == PeerSessionState::Connected) {
        if (!impl_->local_round_reset) {
            if (!impl_->local_final_score_sent || impl_->active_round_nonce == 0) {
                impl_->current.status_detail = "Cannot leave the round before the local final score is queued.";
                impl_->touch_locked();
                return;
            }

            PeerMessage reset;
            reset.type = PeerMessageType::RoundReset;
            reset.nonce = impl_->active_round_nonce;
            if (!impl_->queue_control_locked(std::move(reset))) {
                impl_->set_state_locked(PeerSessionState::Failed,
                                        "Could not queue multiplayer round reset.");
                return;
            }
            impl_->local_round_reset = true;
            impl_->current.local_ready = false;
            impl_->current.local_loaded = false;
            impl_->pending_score.reset();
            impl_->pending_score_dirty = false;
            impl_->pending_launch.reset();
            impl_->pending_begin.reset();
        }
        if (impl_->remote_round_reset) {
            impl_->invalidate_round_locked(true);
        }
        impl_->touch_locked();
        return;
    }

    impl_->invalidate_round_locked(true);
    if (impl_->current.state == PeerSessionState::Connected) {
        PeerMessage ready;
        ready.type = PeerMessageType::Ready;
        ready.ready = false;
        impl_->queue_control_locked(std::move(ready));
    }
    impl_->touch_locked();
}

}  // namespace tenriff::network
