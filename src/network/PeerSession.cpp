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

#include "util/Utf8Compat.h"
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
    std::deque<PeerMessage> broadcasts;
    std::vector<std::string> local_library_sha256;
    uint64_t local_library_generation = 1;
    std::optional<PeerScore> pending_score;
    bool pending_score_dirty = false;
    std::optional<uint64_t> pending_launch;
    std::optional<uint32_t> pending_begin;
    uint64_t active_round_nonce = 0;
    uint64_t last_closed_round_nonce = 0;
    bool begin_sent = false;
    bool local_final_score_sent = false;

    struct LibraryState {
        bool receiving = false;
        bool ready = false;
        uint32_t expected = 0;
        std::vector<std::string> builder;
        std::shared_ptr<const std::unordered_set<std::string>> hashes;
    };
    std::array<LibraryState, kPeerMaxPlayers + 1> libraries{};
    std::vector<uint8_t> join_order;
    std::vector<std::string> common_library_sorted;
    uint64_t common_library_generation = 0;

#ifdef _WIN32
    struct RoomLink {
        SocketHandle socket;
        WireQueue wire;
        std::vector<uint8_t> received;
        uint8_t player_id = 0;
        bool handshake_complete = false;
        bool close_after_flush = false;
        std::string name;
        SteadyClock::time_point accepted_at = SteadyClock::now();
        SteadyClock::time_point last_received = SteadyClock::now();
        SteadyClock::time_point last_ping_sent = SteadyClock::now() - kHeartbeatInterval;
        uint64_t ping_nonce = 0;
        SteadyClock::time_point ping_sent_at{};
        uint64_t common_generation = 0;
        std::size_t common_cursor = 0;
        int common_stage = 0;
    };
#endif

    PeerParticipantSnapshot* participant_locked(uint8_t id) {
        for (auto& participant : current.participants) {
            if (participant.player_id == id) return &participant;
        }
        return nullptr;
    }

    const PeerParticipantSnapshot* participant_locked(uint8_t id) const {
        for (const auto& participant : current.participants) {
            if (participant.player_id == id) return &participant;
        }
        return nullptr;
    }

    static std::string normalize_chat_text(std::string_view value) {
        const std::string cleaned = util::sanitize_ui_text(value);
        std::size_t cursor = 0;
        std::size_t accepted = 0;
        while (cursor < cleaned.size() && cursor < kPeerChatMaxBytes) {
            const unsigned char lead = static_cast<unsigned char>(cleaned[cursor]);
            std::size_t length = 1;
            if ((lead & 0x80u) == 0u) {
                length = 1;
            } else if ((lead & 0xE0u) == 0xC0u) {
                length = 2;
            } else if ((lead & 0xF0u) == 0xE0u) {
                length = 3;
            } else if ((lead & 0xF8u) == 0xF0u) {
                length = 4;
            }
            if (cursor + length > cleaned.size() ||
                cursor + length > kPeerChatMaxBytes) {
                break;
            }
            accepted = cursor + length;
            cursor += length;
        }
        return cleaned.substr(0, accepted);
    }

    bool append_chat_locked(uint8_t player_id, std::string_view text) {
        if (!participant_locked(player_id)) return false;
        std::string normalized = normalize_chat_text(text);
        if (normalized.empty()) return false;
        current.chat_messages.push_back(PeerChatEntry{player_id, std::move(normalized)});
        if (current.chat_messages.size() > kPeerChatHistoryLimit) {
            current.chat_messages.erase(
                current.chat_messages.begin(),
                current.chat_messages.begin() +
                    static_cast<std::ptrdiff_t>(current.chat_messages.size() -
                                                kPeerChatHistoryLimit));
        }
        return true;
    }

    uint8_t next_available_id_locked() const {
        for (uint8_t id = 2; id <= kPeerMaxPlayers; ++id) {
            if (!participant_locked(id)) return id;
        }
        return 0;
    }

    uint8_t next_leader_locked(uint8_t id) const {
        if (join_order.empty()) return 0;
        const auto found = std::find(join_order.begin(), join_order.end(), id);
        const std::size_t start =
            found == join_order.end() ? 0 : static_cast<std::size_t>(found - join_order.begin() + 1);
        for (std::size_t offset = 0; offset < join_order.size(); ++offset) {
            const uint8_t candidate = join_order[(start + offset) % join_order.size()];
            if (participant_locked(candidate)) return candidate;
        }
        return join_order.front();
    }

    void refresh_compat_locked() {
        current.participant_count = current.participants.size();
        current.local_is_leader =
            current.local_player_id != 0 && current.local_player_id == current.leader_player_id;
        current.selected_chart = {};
        if (const auto* leader = participant_locked(current.leader_player_id)) {
            current.selected_chart = leader->chart;
        }

        if (auto* local = participant_locked(current.local_player_id)) {
            local->local = true;
            current.local_chart = local->chart;
            current.local_ready = local->ready;
            current.local_loaded = local->loaded;
            current.local_round_reset = local->round_reset;
        } else {
            current.local_ready = false;
            current.local_loaded = false;
            current.local_round_reset = false;
        }

        const PeerParticipantSnapshot* primary = nullptr;
        bool any_remote = false;
        bool all_ready = true;
        bool all_loaded = true;
        bool all_reset = true;
        bool all_finished = true;
        uint32_t maximum_rtt = 0;
        for (auto& participant : current.participants) {
            participant.local = participant.player_id == current.local_player_id;
            participant.leader = participant.player_id == current.leader_player_id;
            if (participant.local) continue;
            any_remote = true;
            all_ready = all_ready && participant.ready;
            all_loaded = all_loaded && participant.loaded;
            all_reset = all_reset && participant.round_reset;
            all_finished =
                all_finished && participant.has_score && participant.latest_score.finished;
            maximum_rtt = std::max(maximum_rtt, participant.estimated_rtt_ms);
            if (!primary ||
                (participant.has_score &&
                 (!primary->has_score ||
                  participant.latest_score.score > primary->latest_score.score)) ||
                (participant.player_id == current.leader_player_id &&
                 primary->player_id != current.leader_player_id)) {
                primary = &participant;
            }
        }
        current.remote_ready = any_remote && all_ready;
        current.remote_loaded = any_remote && all_loaded;
        current.remote_round_reset = any_remote && all_reset;
        current.all_remote_finished = any_remote && all_finished;
        current.estimated_rtt_ms = maximum_rtt;
        current.peer_name = primary ? primary->name : std::string{};
        current.remote_chart =
            current.local_is_leader && primary ? primary->chart : current.selected_chart;
        current.has_remote_score = primary && primary->has_score;
        current.latest_remote_score =
            current.has_remote_score ? primary->latest_score : PeerScore{};

        bool room_ready = current.participants.size() >= 2;
        bool charts_match = current.selected_chart.fingerprint.valid();
        for (const auto& participant : current.participants) {
            room_ready = room_ready && participant.ready;
            charts_match =
                charts_match && chart_matches(participant.chart, current.selected_chart);
        }
        current.round_active = active_round_nonce != 0;
        current.round_transition_pending =
            current.round_active && std::any_of(
                current.participants.begin(), current.participants.end(),
                [](const PeerParticipantSnapshot& participant) {
                    return participant.round_reset;
                });
        current.can_start =
            current.state == PeerSessionState::Connected &&
            current.local_is_leader && !current.round_active &&
            room_ready && charts_match;
    }

    void touch_locked() {
        refresh_compat_locked();
        ++current.revision;
        state_cv.notify_all();
    }

    void set_state_locked(PeerSessionState state, std::string detail) {
        current.state = state;
        current.status_detail = std::move(detail);
        touch_locked();
    }

    void fail(std::string detail) {
        std::lock_guard<std::mutex> lock(mutex);
        set_state_locked(PeerSessionState::Failed, std::move(detail));
    }

    bool queue_control_locked(PeerMessage message) {
        if (controls.size() >= kMaxControlMessages) {
            current.status_detail = "Multiplayer command queue is full.";
            touch_locked();
            return false;
        }
        controls.push_back(std::move(message));
        state_cv.notify_all();
        return true;
    }

    PeerMessage make_roster_locked() const {
        PeerMessage roster;
        roster.type = PeerMessageType::RoomRoster;
        roster.leader_id = current.leader_player_id;
        roster.round_active = active_round_nonce != 0;
        roster.nonce = active_round_nonce;
        roster.participants.reserve(current.participants.size());
        for (const auto& participant : current.participants) {
            PeerParticipantWire wire;
            wire.player_id = participant.player_id;
            wire.name = participant.name;
            wire.ready = participant.ready;
            wire.loaded = participant.loaded;
            wire.round_reset = participant.round_reset;
            wire.chart_hash = participant.chart.fingerprint.hash;
            wire.chart_size = participant.chart.fingerprint.size;
            wire.chart_name = participant.chart.name;
            roster.participants.push_back(std::move(wire));
        }
        return roster;
    }

    void queue_roster_locked() {
        if (!current.participants.empty()) broadcasts.push_back(make_roster_locked());
    }

    void clear_round_locked(bool rotate_leader) {
        if (active_round_nonce != 0) last_closed_round_nonce = active_round_nonce;
        active_round_nonce = 0;
        begin_sent = false;
        local_final_score_sent = false;
        pending_launch.reset();
        pending_begin.reset();
        pending_score.reset();
        pending_score_dirty = false;
        if (rotate_leader && current.participants.size() > 1) {
            current.leader_player_id = next_leader_locked(current.leader_player_id);
        }
        for (auto& participant : current.participants) {
            participant.ready = false;
            participant.loaded = false;
            participant.round_reset = false;
            participant.chart = {};
            participant.has_score = false;
            participant.latest_score = {};
        }
        current.selected_chart = {};
    }

    void cancel_launch_locked() {
        if (active_round_nonce != 0) {
            last_closed_round_nonce = active_round_nonce;
        }
        active_round_nonce = 0;
        begin_sent = false;
        local_final_score_sent = false;
        pending_launch.reset();
        pending_begin.reset();
        pending_score.reset();
        pending_score_dirty = false;
        for (auto& participant : current.participants) {
            participant.ready = false;
            participant.loaded = false;
            participant.round_reset = false;
            participant.has_score = false;
            participant.latest_score = {};
        }
        current.status_detail =
            "Launch canceled because a player changed Ready.";
    }
    bool all_players_loaded_locked() const {
        return current.participants.size() >= 2 &&
               std::all_of(current.participants.begin(), current.participants.end(),
                           [](const PeerParticipantSnapshot& participant) {
                               return participant.loaded;
                           });
    }

    bool all_players_reset_locked() const {
        return !current.participants.empty() &&
               std::all_of(current.participants.begin(), current.participants.end(),
                           [](const PeerParticipantSnapshot& participant) {
                               return participant.round_reset;
                           });
    }

    bool all_players_ready_locked() const {
        if (current.participants.size() < 2 ||
            !current.selected_chart.fingerprint.valid()) return false;
        return std::all_of(
            current.participants.begin(), current.participants.end(),
            [this](const PeerParticipantSnapshot& participant) {
                return participant.ready &&
                       chart_matches(participant.chart, current.selected_chart);
            });
    }

    void recompute_common_library_locked() {
        bool ready = current.participants.size() >= 2;
        for (const auto& participant : current.participants) {
            ready = ready && libraries[participant.player_id].ready &&
                    static_cast<bool>(libraries[participant.player_id].hashes);
        }
        if (!ready) {
            if (current.remote_library_ready || current.remote_library_sha256) {
                current.remote_library_ready = false;
                current.remote_library_count = 0;
                current.remote_library_sha256.reset();
                common_library_sorted.clear();
                ++current.remote_library_revision;
                ++common_library_generation;
                touch_locked();
            }
            return;
        }
        const uint8_t first_id = current.participants.front().player_id;
        std::unordered_set<std::string> intersection = *libraries[first_id].hashes;
        for (std::size_t index = 1;
             index < current.participants.size() && !intersection.empty(); ++index) {
            const auto& hashes =
                *libraries[current.participants[index].player_id].hashes;
            for (auto it = intersection.begin(); it != intersection.end();) {
                if (hashes.find(*it) == hashes.end()) {
                    it = intersection.erase(it);
                } else {
                    ++it;
                }
            }
        }
        common_library_sorted.assign(intersection.begin(), intersection.end());
        std::sort(common_library_sorted.begin(), common_library_sorted.end());
        current.remote_library_sha256 =
            std::make_shared<const std::unordered_set<std::string>>(std::move(intersection));
        current.remote_library_count = common_library_sorted.size();
        current.remote_library_ready = true;
        ++current.remote_library_revision;
        ++common_library_generation;
        touch_locked();
    }

    void set_local_library_state_locked() {
        auto hashes = std::make_shared<std::unordered_set<std::string>>();
        hashes->reserve(local_library_sha256.size());
        for (const auto& value : local_library_sha256) hashes->insert(value);
        const uint8_t id = current.local_player_id == 0 ? 1 : current.local_player_id;
        libraries[id].hashes =
            std::const_pointer_cast<const std::unordered_set<std::string>>(hashes);
        libraries[id].ready = true;
    }

    bool start(PeerRole role, std::string address, uint16_t port, std::string name) {
        std::thread completed;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (worker_running) return false;
            if (worker.joinable()) completed = std::move(worker);
        }
        if (completed.joinable()) completed.join();
        {
            std::lock_guard<std::mutex> lock(mutex);
            const PeerChartInfo retained_chart = current.local_chart;
            const uint64_t revision = current.revision + 1;
            current = {};
            current.role = role;
            current.state = PeerSessionState::Starting;
            current.revision = revision;
            current.status_detail =
                role == PeerRole::Host ? "Starting 8-player room."
                                       : "Connecting to multiplayer room.";
            local_name = std::move(name);
            controls.clear();
            broadcasts.clear();
            pending_score.reset();
            pending_score_dirty = false;
            pending_launch.reset();
            pending_begin.reset();
            active_round_nonce = 0;
            last_closed_round_nonce = 0;
            begin_sent = false;
            local_final_score_sent = false;
            libraries = {};
            join_order.clear();
            common_library_sorted.clear();
            common_library_generation = 0;
            if (role == PeerRole::Host) {
                current.local_player_id = 1;
                current.leader_player_id = 1;
                PeerParticipantSnapshot host;
                host.player_id = 1;
                host.name = local_name;
                host.local = true;
                host.leader = true;
                host.chart = retained_chart;
                current.participants.push_back(std::move(host));
                join_order.push_back(1);
                set_local_library_state_locked();
            } else {
                current.local_chart = retained_chart;
            }
            hard_stop.store(false, std::memory_order_release);
            graceful_stop.store(false, std::memory_order_release);
            worker_running = true;
            touch_locked();
        }
        try {
            worker = std::thread(
                &Impl::worker_main, this, role, std::move(address), port);
        } catch (const std::exception& error) {
            std::lock_guard<std::mutex> lock(mutex);
            worker_running = false;
            set_state_locked(PeerSessionState::Failed,
                             std::string("Could not start multiplayer worker: ") +
                                 error.what());
            return false;
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            worker_running = false;
            set_state_locked(PeerSessionState::Failed,
                             "Could not start multiplayer worker.");
            return false;
        }
        return true;
    }

    void finish_worker() {
        std::lock_guard<std::mutex> lock(mutex);
        worker_running = false;
        pending_launch.reset();
        pending_begin.reset();
        if (current.state != PeerSessionState::Failed) {
            set_state_locked(PeerSessionState::Disconnected, "Disconnected.");
        } else {
            state_cv.notify_all();
        }
    }
    void apply_client_roster_locked(const PeerMessage& message) {
        const std::size_t previous_count = current.participants.size();
        std::vector<PeerParticipantSnapshot> rebuilt;
        rebuilt.reserve(message.participants.size());
        for (const auto& wire : message.participants) {
            PeerParticipantSnapshot participant;
            participant.player_id = wire.player_id;
            participant.name = wire.name;
            participant.ready = wire.ready;
            participant.loaded = wire.loaded;
            participant.round_reset = wire.round_reset;
            participant.chart.fingerprint = {wire.chart_hash, wire.chart_size};
            participant.chart.name = wire.chart_name;
            if (const auto* previous = participant_locked(wire.player_id)) {
                participant.has_score = previous->has_score;
                participant.latest_score = previous->latest_score;
                participant.estimated_rtt_ms = previous->estimated_rtt_ms;
            }
            rebuilt.push_back(std::move(participant));
        }
        current.participants = std::move(rebuilt);
        current.leader_player_id = message.leader_id;
        if (!message.round_active) {
            if (active_round_nonce != 0) last_closed_round_nonce = active_round_nonce;
            active_round_nonce = 0;
            pending_launch.reset();
            pending_begin.reset();
            pending_score.reset();
            pending_score_dirty = false;
            begin_sent = false;
            local_final_score_sent = false;
        } else {
            active_round_nonce = message.nonce;
        }
        if (previous_count != current.participants.size()) {
            current.remote_library_ready = false;
            current.remote_library_count = 0;
            current.remote_library_sha256.reset();
        }
        touch_locked();
    }

    bool apply_client_message_locked(const PeerMessage& message,
                                     PeerMessage& response,
                                     bool& has_response,
                                     std::string& error) {
        has_response = false;
        switch (message.type) {
            case PeerMessageType::RoomWelcome:
                current.local_player_id = message.player_id;
                current.leader_player_id = message.leader_id;
                return true;
            case PeerMessageType::RoomRoster:
                apply_client_roster_locked(message);
                if (participant_locked(current.local_player_id)) {
                    set_state_locked(
                        PeerSessionState::Connected,
                        "Connected to room with " +
                            std::to_string(current.participants.size()) + " players.");
                }
                return true;
            case PeerMessageType::Launch:
                if (message.player_id != current.leader_player_id ||
                    message.nonce == 0 || message.chart_hash == 0) {
                    error = "Room coordinator sent an invalid launch.";
                    return false;
                }
                active_round_nonce = message.nonce;
                begin_sent = false;
                local_final_score_sent = false;
                pending_begin.reset();
                for (auto& participant : current.participants) {
                    participant.loaded = false;
                    participant.round_reset = false;
                    participant.has_score = false;
                    participant.latest_score = {};
                }
                pending_launch = message.chart_hash;
                touch_locked();
                return true;
            case PeerMessageType::Begin:
                if (message.player_id != current.leader_player_id ||
                    message.nonce != active_round_nonce) {
                    error = "Room coordinator sent an invalid begin command.";
                    return false;
                }
                if (message.player_id != current.local_player_id) {
                    pending_begin = message.delay_ms;
                }
                begin_sent = true;
                touch_locked();
                return true;
            case PeerMessageType::Chat:
                if (message.player_id == 0 ||
                    !append_chat_locked(message.player_id, message.text)) {
                    error = "Room chat message is invalid.";
                    return false;
                }
                touch_locked();
                return true;
            case PeerMessageType::Score:
            case PeerMessageType::FinalScore:
                if (message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) return true;
                    error = "Room score belongs to an unknown round.";
                    return false;
                }
                if (auto* participant = participant_locked(message.player_id)) {
                    participant->latest_score = message.score;
                    participant->has_score = true;
                    touch_locked();
                }
                return true;
            case PeerMessageType::CommonLibraryBegin:
                libraries[0].receiving = true;
                libraries[0].ready = false;
                libraries[0].expected = message.library_count;
                libraries[0].builder.clear();
                libraries[0].builder.reserve(message.library_count);
                current.remote_library_ready = false;
                current.remote_library_count = 0;
                current.remote_library_sha256.reset();
                touch_locked();
                return true;
            case PeerMessageType::CommonLibraryChunk:
                if (!libraries[0].receiving ||
                    libraries[0].builder.size() + message.chart_sha256.size() >
                        libraries[0].expected) {
                    error = "Room common-library chunk is out of sequence.";
                    return false;
                }
                libraries[0].builder.insert(libraries[0].builder.end(),
                                            message.chart_sha256.begin(),
                                            message.chart_sha256.end());
                return true;
            case PeerMessageType::CommonLibraryEnd: {
                if (!libraries[0].receiving ||
                    libraries[0].builder.size() != libraries[0].expected) {
                    error = "Room common-library transfer is incomplete.";
                    return false;
                }
                auto hashes =
                    std::make_shared<std::unordered_set<std::string>>();
                hashes->reserve(libraries[0].builder.size());
                for (const auto& value : libraries[0].builder) {
                    hashes->insert(value);
                }
                current.remote_library_sha256 =
                    std::const_pointer_cast<
                        const std::unordered_set<std::string>>(hashes);
                current.remote_library_count = hashes->size();
                current.remote_library_ready = true;
                ++current.remote_library_revision;
                libraries[0].receiving = false;
                libraries[0].ready = true;
                libraries[0].builder.clear();
                touch_locked();
                return true;
            }
            case PeerMessageType::Ping:
                response.type = PeerMessageType::Pong;
                response.nonce = message.nonce;
                has_response = true;
                return true;
            case PeerMessageType::Pong:
                return true;
            case PeerMessageType::Disconnect:
                set_state_locked(
                    PeerSessionState::Disconnected,
                    message.text.empty() ? "Room host disconnected."
                                         : "Room closed: " + message.text);
                return false;
            default:
                error = "Room host sent an unsupported message.";
                return false;
        }
    }

    bool apply_host_action_locked(uint8_t source_id,
                                  PeerMessage message,
                                  std::string& error) {
        auto* source = participant_locked(source_id);
        if (!source) {
            error = "Multiplayer action came from an unknown player.";
            return false;
        }
        message.player_id = source_id;
        switch (message.type) {
            case PeerMessageType::Chat:
                if (!append_chat_locked(source_id, message.text)) {
                    error = "Chat message is empty or invalid.";
                    return false;
                }
                message.text = current.chat_messages.back().text;
                broadcasts.push_back(std::move(message));
                touch_locked();
                return true;
            case PeerMessageType::Chart:
                if (active_round_nonce != 0) {
                    // A pre-launch chart confirmation can cross the canonical
                    // Launch on the opposite TCP direction. The active room
                    // roster remains authoritative, so ignore that stale frame.
                    return true;
                }
                if (source_id != current.leader_player_id &&
                    (!current.selected_chart.fingerprint.valid() ||
                     current.selected_chart.fingerprint.hash != message.chart_hash ||
                     current.selected_chart.fingerprint.size != message.chart_size)) {
                    error =
                        "Non-leader chart does not match the room selection.";
                    return false;
                }
                source->chart.fingerprint =
                    {message.chart_hash, message.chart_size};
                source->chart.name = message.text;
                if (source_id == current.leader_player_id) {
                    for (auto& participant : current.participants) {
                        participant.ready = false;
                        participant.loaded = false;
                        participant.round_reset = false;
                        participant.has_score = false;
                        participant.latest_score = {};
                        if (participant.player_id != source_id) {
                            participant.chart = {};
                        }
                    }
                    current.selected_chart = source->chart;
                } else {
                    source->ready = false;
                }
                queue_roster_locked();
                touch_locked();
                return true;
            case PeerMessageType::Ready:
                if (active_round_nonce != 0) {
                    if (!begin_sent && !message.ready) {
                        cancel_launch_locked();
                        queue_roster_locked();
                        touch_locked();
                    }
                    // Ready frames have no round nonce and may have been queued
                    // immediately before Launch. Ignore all other active-round
                    // readiness frames instead of disconnecting a healthy peer.
                    return true;
                }
                if (message.ready &&
                    !chart_matches(source->chart, current.selected_chart)) {
                    error =
                        "Ready requires the selected chart and an idle room.";
                    return false;
                }
                source->ready = message.ready;
                if (!message.ready) source->loaded = false;
                queue_roster_locked();
                touch_locked();
                return true;
            case PeerMessageType::Launch: {
                if (source_id != current.leader_player_id ||
                    active_round_nonce != 0) {
                    error = "Only the current leader can launch an idle room.";
                    return false;
                }
                if (!all_players_ready_locked()) {
                    current.status_detail =
                        "Launch ignored because a player changed Ready.";
                    queue_roster_locked();
                    touch_locked();
                    return true;
                }
                active_round_nonce = message.nonce;
                for (auto& participant : current.participants) {
                    participant.loaded = false;
                    participant.round_reset = false;
                    participant.has_score = false;
                    participant.latest_score = {};
                }
                begin_sent = false;
                local_final_score_sent = false;
                message.chart_hash = current.selected_chart.fingerprint.hash;
                broadcasts.push_back(message);
                queue_roster_locked();
                pending_launch = message.chart_hash;
                touch_locked();
                return true;
            }
            case PeerMessageType::Loaded:
                if (active_round_nonce == 0 ||
                    message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) return true;
                    error = "Loaded belongs to an unknown round.";
                    return false;
                }
                source->loaded = true;
                queue_roster_locked();
                touch_locked();
                return true;
            case PeerMessageType::Begin: {
                const bool local_optimistic =
                    source_id == current.local_player_id && begin_sent;
                if (source_id != current.leader_player_id ||
                    message.nonce != active_round_nonce ||
                    (begin_sent && !local_optimistic) ||
                    !all_players_loaded_locked()) {
                    error =
                        "Begin requires the leader and all loaded players.";
                    return false;
                }
                begin_sent = true;
                broadcasts.push_back(message);
                if (source_id != current.local_player_id) {
                    pending_begin = message.delay_ms;
                }
                touch_locked();
                return true;
            }
            case PeerMessageType::Score:
            case PeerMessageType::FinalScore:
                if (message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) return true;
                    error = "Score belongs to an unknown round.";
                    return false;
                }
                source->latest_score = message.score;
                source->has_score = true;
                broadcasts.push_back(message);
                touch_locked();
                return true;
            case PeerMessageType::RoundReset:
                if (message.nonce != active_round_nonce) {
                    if (message.nonce == last_closed_round_nonce) return true;
                    error = "Round reset belongs to an unknown round.";
                    return false;
                }
                if (!source->has_score ||
                    !source->latest_score.finished) {
                    error =
                        "Player left the result before sending a final score.";
                    return false;
                }
                source->round_reset = true;
                source->ready = false;
                source->loaded = false;
                if (all_players_reset_locked()) {
                    clear_round_locked(true);
                }
                queue_roster_locked();
                touch_locked();
                return true;
            default:
                error = "Unsupported player action.";
                return false;
        }
    }

    void remove_host_player_locked(uint8_t player_id, std::string detail) {
        const bool was_leader = current.leader_player_id == player_id;
        const auto order_it =
            std::find(join_order.begin(), join_order.end(), player_id);
        const std::size_t removed_index =
            order_it == join_order.end()
                ? 0
                : static_cast<std::size_t>(order_it - join_order.begin());
        if (order_it != join_order.end()) join_order.erase(order_it);
        current.participants.erase(
            std::remove_if(
                current.participants.begin(), current.participants.end(),
                [player_id](const PeerParticipantSnapshot& participant) {
                    return participant.player_id == player_id;
                }),
            current.participants.end());
        libraries[player_id] = {};
        if (was_leader && !join_order.empty()) {
            current.leader_player_id =
                join_order[removed_index % join_order.size()];
        }
        if (current.participants.size() < 2) {
            clear_round_locked(false);
            current.leader_player_id = 1;
            current.state = PeerSessionState::Listening;
            current.status_detail =
                "Listening for up to seven players.";
        } else {
            if (active_round_nonce != 0 && all_players_reset_locked()) {
                clear_round_locked(true);
            }
            current.status_detail = std::move(detail);
        }
        recompute_common_library_locked();
        queue_roster_locked();
        touch_locked();
    }

#ifdef _WIN32
    SocketHandle create_listener(uint16_t port, std::string& error) {
        SocketHandle listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!listener.valid()) {
            error = winsock_error("socket", WSAGetLastError());
            return {};
        }
        const BOOL exclusive = TRUE;
        if (setsockopt(listener.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       reinterpret_cast<const char*>(&exclusive),
                       sizeof(exclusive)) == SOCKET_ERROR) {
            error =
                winsock_error("setsockopt(SO_EXCLUSIVEADDRUSE)", WSAGetLastError());
            return {};
        }
        if (!set_nonblocking(listener.get(), error)) return {};
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        if (bind(listener.get(), reinterpret_cast<const sockaddr*>(&address),
                 sizeof(address)) == SOCKET_ERROR) {
            error = winsock_error("bind", WSAGetLastError());
            return {};
        }
        if (listen(listener.get(),
                   static_cast<int>(kPeerMaxPlayers - 1)) == SOCKET_ERROR) {
            error = winsock_error("listen", WSAGetLastError());
            return {};
        }
        sockaddr_in bound{};
        int bound_size = sizeof(bound);
        if (getsockname(listener.get(), reinterpret_cast<sockaddr*>(&bound),
                        &bound_size) == SOCKET_ERROR) {
            error = winsock_error("getsockname", WSAGetLastError());
            return {};
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            current.local_port = ntohs(bound.sin_port);
            set_state_locked(
                PeerSessionState::Listening,
                "Listening for up to seven players on IPv4 port " +
                    std::to_string(current.local_port) + ".");
        }
        return listener;
    }

    SocketHandle connect_peer(const std::string& address,
                              uint16_t port,
                              std::string& error) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            set_state_locked(PeerSessionState::Resolving,
                             "Resolving " + address + ".");
        }
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        addrinfo* results = nullptr;
        const std::string service = std::to_string(port);
        const int resolved =
            getaddrinfo(address.c_str(), service.c_str(), &hints, &results);
        if (resolved != 0) {
            error =
                "Could not resolve room address (Winsock " +
                std::to_string(resolved) + ").";
            return {};
        }
        struct Guard {
            addrinfo* value = nullptr;
            ~Guard() {
                if (value) freeaddrinfo(value);
            }
        } guard{results};

        {
            std::lock_guard<std::mutex> lock(mutex);
            set_state_locked(
                PeerSessionState::Connecting,
                "Connecting to " + address + ":" + std::to_string(port) + ".");
        }
        const auto deadline = SteadyClock::now() + kConnectTimeout;
        int last_error = WSAETIMEDOUT;
        for (addrinfo* candidate = results;
             candidate && SteadyClock::now() < deadline;
             candidate = candidate->ai_next) {
            SocketHandle peer(socket(candidate->ai_family,
                                     candidate->ai_socktype,
                                     candidate->ai_protocol));
            if (!peer.valid()) {
                last_error = WSAGetLastError();
                continue;
            }
            if (!set_nonblocking(peer.get(), error)) continue;
            const int connected =
                connect(peer.get(), candidate->ai_addr,
                        static_cast<int>(candidate->ai_addrlen));
            if (connected == 0) {
                if (!configure_peer_socket(peer.get(), error)) return {};
                return peer;
            }
            last_error = WSAGetLastError();
            if (last_error != WSAEWOULDBLOCK &&
                last_error != WSAEINPROGRESS) {
                continue;
            }
            while (!hard_stop.load(std::memory_order_acquire) &&
                   SteadyClock::now() < deadline) {
                fd_set write_set;
                fd_set except_set;
                FD_ZERO(&write_set);
                FD_ZERO(&except_set);
                FD_SET(peer.get(), &write_set);
                FD_SET(peer.get(), &except_set);
                timeval timeout{};
                timeout.tv_usec = static_cast<long>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        kIoPollInterval).count());
                const int selected =
                    select(0, nullptr, &write_set, &except_set, &timeout);
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
                if (socket_error == 0 &&
                    FD_ISSET(peer.get(), &write_set)) {
                    if (!configure_peer_socket(peer.get(), error)) return {};
                    return peer;
                }
                last_error =
                    socket_error == 0 ? WSAECONNREFUSED : socket_error;
                break;
            }
        }
        if (!hard_stop.load(std::memory_order_acquire)) {
            error = winsock_error("connect", last_error);
        }
        return {};
    }
    bool receive_frames(RoomLink& link,
                        std::vector<PeerMessage>& messages,
                        bool& closed,
                        std::string& error) {
        closed = false;
        std::array<uint8_t, 8192> chunk{};
        for (;;) {
            const int read =
                recv(link.socket.get(),
                     reinterpret_cast<char*>(chunk.data()),
                     static_cast<int>(chunk.size()), 0);
            if (read > 0) {
                const std::size_t size = static_cast<std::size_t>(read);
                if (link.received.size() > kMaxReceiveBytes - size) {
                    error = "Peer receive buffer exceeded its byte limit.";
                    return false;
                }
                link.received.insert(link.received.end(),
                                     chunk.begin(), chunk.begin() + read);
                link.last_received = SteadyClock::now();
                continue;
            }
            if (read == 0) {
                closed = true;
                break;
            }
            const int read_error = WSAGetLastError();
            if (read_error == WSAEWOULDBLOCK) break;
            error = winsock_error("recv", read_error);
            return false;
        }
        int processed = 0;
        while (!link.received.empty() && processed < 64) {
            PeerMessage message;
            std::size_t consumed = 0;
            std::string decode_error;
            const PeerDecodeStatus status =
                decode_peer_message(link.received, message,
                                    consumed, decode_error);
            if (status == PeerDecodeStatus::Incomplete) break;
            if (status == PeerDecodeStatus::Error) {
                error = std::move(decode_error);
                return false;
            }
            link.received.erase(
                link.received.begin(),
                link.received.begin() +
                    static_cast<std::ptrdiff_t>(consumed));
            messages.push_back(std::move(message));
            ++processed;
        }
        return true;
    }

    void feed_common_library(RoomLink& link, std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!link.handshake_complete ||
            !current.remote_library_ready) return;
        if (link.common_generation != common_library_generation) {
            link.common_generation = common_library_generation;
            link.common_cursor = 0;
            link.common_stage = 0;
        }
        if (link.wire.bytes >= kMaxWireBytes / 2) return;
        PeerMessage message;
        if (link.common_stage == 0) {
            message.type = PeerMessageType::CommonLibraryBegin;
            message.library_count =
                static_cast<uint32_t>(common_library_sorted.size());
            if (link.wire.push(message, error)) link.common_stage = 1;
        } else if (link.common_stage == 1 &&
                   link.common_cursor < common_library_sorted.size()) {
            const std::size_t end =
                std::min(common_library_sorted.size(),
                         link.common_cursor + kPeerLibraryHashesPerChunk);
            message.type = PeerMessageType::CommonLibraryChunk;
            message.chart_sha256.assign(
                common_library_sorted.begin() + link.common_cursor,
                common_library_sorted.begin() + end);
            if (link.wire.push(message, error)) {
                link.common_cursor = end;
            }
        } else if (link.common_stage == 1) {
            message.type = PeerMessageType::CommonLibraryEnd;
            if (link.wire.push(message, error)) link.common_stage = 2;
        }
    }

    bool process_host_link_message(RoomLink& link,
                                   const PeerMessage& message,
                                   std::string& error) {
        if (!link.handshake_complete) {
            if (message.type != PeerMessageType::Hello ||
                message.text.empty() || message.text.size() > 64) {
                error = "Player handshake is invalid.";
                return false;
            }
            std::lock_guard<std::mutex> lock(mutex);
            if (active_round_nonce != 0 ||
                current.participants.size() >= kPeerMaxPlayers) {
                PeerMessage rejected;
                rejected.type = PeerMessageType::Disconnect;
                rejected.text =
                    active_round_nonce != 0
                        ? "Room is currently playing."
                        : "Room is full (8/8).";
                if (!link.wire.push(rejected, error)) return false;
                link.close_after_flush = true;
                return true;
            }
            const uint8_t id = next_available_id_locked();
            if (id == 0) {
                error = "Room has no available player slot.";
                return false;
            }
            link.player_id = id;
            link.name = message.text;
            link.handshake_complete = true;
            PeerParticipantSnapshot participant;
            participant.player_id = id;
            participant.name = message.text;
            current.participants.push_back(std::move(participant));
            join_order.push_back(id);
            libraries[id] = {};
            current.state = PeerSessionState::Connected;
            current.status_detail =
                message.text + " joined (" +
                std::to_string(current.participants.size()) + "/8).";
            PeerMessage welcome;
            welcome.type = PeerMessageType::RoomWelcome;
            welcome.player_id = id;
            welcome.leader_id = current.leader_player_id;
            if (!link.wire.push(welcome, error)) return false;
            recompute_common_library_locked();
            queue_roster_locked();
            touch_locked();
            return true;
        }

        if (message.type == PeerMessageType::Disconnect) return false;
        if (message.type == PeerMessageType::Ping) {
            PeerMessage pong;
            pong.type = PeerMessageType::Pong;
            pong.nonce = message.nonce;
            return link.wire.push(pong, error);
        }
        if (message.type == PeerMessageType::Pong) {
            if (message.nonce == link.ping_nonce &&
                link.ping_nonce != 0) {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        SteadyClock::now() - link.ping_sent_at);
                std::lock_guard<std::mutex> lock(mutex);
                if (auto* participant =
                        participant_locked(link.player_id)) {
                    participant->estimated_rtt_ms =
                        static_cast<uint32_t>(
                            std::clamp<int64_t>(
                                elapsed.count(), 1, 2000));
                    touch_locked();
                }
                link.ping_nonce = 0;
            }
            return true;
        }

        if (message.type == PeerMessageType::LibraryBegin) {
            std::lock_guard<std::mutex> lock(mutex);
            auto& library = libraries[link.player_id];
            library.receiving = true;
            library.ready = false;
            library.expected = message.library_count;
            library.builder.clear();
            library.builder.reserve(message.library_count);
            recompute_common_library_locked();
            return true;
        }
        if (message.type == PeerMessageType::LibraryChunk) {
            std::lock_guard<std::mutex> lock(mutex);
            auto& library = libraries[link.player_id];
            if (!library.receiving ||
                library.builder.size() +
                        message.chart_sha256.size() >
                    library.expected) {
                error = "Player library chunk is out of sequence.";
                return false;
            }
            library.builder.insert(
                library.builder.end(),
                message.chart_sha256.begin(),
                message.chart_sha256.end());
            return true;
        }
        if (message.type == PeerMessageType::LibraryEnd) {
            std::lock_guard<std::mutex> lock(mutex);
            auto& library = libraries[link.player_id];
            if (!library.receiving ||
                library.builder.size() != library.expected) {
                error = "Player library transfer is incomplete.";
                return false;
            }
            auto hashes =
                std::make_shared<std::unordered_set<std::string>>();
            hashes->reserve(library.builder.size());
            for (const auto& value : library.builder) {
                hashes->insert(value);
            }
            library.hashes =
                std::const_pointer_cast<
                    const std::unordered_set<std::string>>(hashes);
            library.ready = true;
            library.receiving = false;
            library.builder.clear();
            recompute_common_library_locked();
            return true;
        }

        std::lock_guard<std::mutex> lock(mutex);
        return apply_host_action_locked(
            link.player_id, message, error);
    }

    void distribute_broadcasts(std::vector<RoomLink>& links) {
        std::deque<PeerMessage> pending;
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending.swap(broadcasts);
        }
        for (const auto& message : pending) {
            for (auto& link : links) {
                if (!link.handshake_complete ||
                    link.close_after_flush) continue;
                std::string error;
                if (!link.wire.push(message, error)) {
                    link.close_after_flush = true;
                }
            }
        }
    }

    void drain_host_local_actions() {
        std::deque<PeerMessage> pending;
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending.swap(controls);
        }
        for (auto& message : pending) {
            if (message.type == PeerMessageType::Disconnect) continue;
            std::string error;
            std::lock_guard<std::mutex> lock(mutex);
            if (!apply_host_action_locked(
                    1, std::move(message), error)) {
                current.status_detail = error;
                touch_locked();
            }
        }
    }

    bool run_host(uint16_t port, std::string& error) {
        SocketHandle listener = create_listener(port, error);
        if (!listener.valid()) return false;
        std::vector<RoomLink> links;
        links.reserve(kPeerMaxPlayers - 1);
        auto close_deadline = SteadyClock::time_point::max();
        bool shutdown_queued = false;
        uint64_t seen_library_generation = 0;
        auto last_score_sent =
            SteadyClock::now() - kScoreInterval;

        while (!hard_stop.load(std::memory_order_acquire)) {
            const auto now = SteadyClock::now();
            const bool closing =
                graceful_stop.load(std::memory_order_acquire);
            if (closing && !shutdown_queued) {
                std::string reason = "Room host closed the room.";
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    for (const auto& message : controls) {
                        if (message.type ==
                            PeerMessageType::Disconnect) {
                            reason = message.text;
                            break;
                        }
                    }
                    controls.clear();
                }
                PeerMessage disconnected;
                disconnected.type = PeerMessageType::Disconnect;
                disconnected.text = reason;
                for (auto& link : links) {
                    if (link.handshake_complete) {
                        std::string ignored;
                        (void)link.wire.push(disconnected, ignored);
                    }
                    link.close_after_flush = true;
                }
                shutdown_queued = true;
                close_deadline = now + kGracefulCloseTimeout;
            }
            if (closing && now >= close_deadline) break;

            {
                std::lock_guard<std::mutex> lock(mutex);
                if (seen_library_generation !=
                    local_library_generation) {
                    seen_library_generation =
                        local_library_generation;
                    set_local_library_state_locked();
                    recompute_common_library_locked();
                }
                if (pending_score_dirty &&
                    pending_score.has_value() &&
                    active_round_nonce != 0 &&
                    now - last_score_sent >= kScoreInterval) {
                    PeerMessage score;
                    score.type = PeerMessageType::Score;
                    score.nonce = active_round_nonce;
                    score.score = *pending_score;
                    controls.push_back(std::move(score));
                    pending_score_dirty = false;
                    last_score_sent = now;
                }
            }
            if (!closing) drain_host_local_actions();
            distribute_broadcasts(links);
            for (auto& link : links) {
                if (!link.close_after_flush) {
                    feed_common_library(link, error);
                }
                if (!error.empty()) {
                    link.close_after_flush = true;
                    error.clear();
                }
                if (link.handshake_complete &&
                    !link.close_after_flush &&
                    now - link.last_ping_sent >=
                        kHeartbeatInterval) {
                    PeerMessage ping;
                    ping.type = PeerMessageType::Ping;
                    ping.nonce = ++link.ping_nonce;
                    link.ping_sent_at = now;
                    if (!link.wire.push(ping, error)) {
                        link.close_after_flush = true;
                    }
                    link.last_ping_sent = now;
                }
            }

            fd_set read_set;
            fd_set write_set;
            fd_set except_set;
            FD_ZERO(&read_set);
            FD_ZERO(&write_set);
            FD_ZERO(&except_set);
            if (!closing) {
                FD_SET(listener.get(), &read_set);
                FD_SET(listener.get(), &except_set);
            }
            for (auto& link : links) {
                FD_SET(link.socket.get(), &read_set);
                FD_SET(link.socket.get(), &except_set);
                if (!link.wire.empty()) {
                    FD_SET(link.socket.get(), &write_set);
                }
            }
            timeval timeout{};
            timeout.tv_usec = static_cast<long>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    kIoPollInterval).count());
            const int selected =
                select(0, &read_set, &write_set,
                       &except_set, &timeout);
            if (selected == SOCKET_ERROR) {
                error =
                    winsock_error("select(room host)",
                                  WSAGetLastError());
                return false;
            }

            if (!closing &&
                FD_ISSET(listener.get(), &read_set)) {
                for (;;) {
                    SocketHandle accepted(
                        accept(listener.get(), nullptr, nullptr));
                    if (!accepted.valid()) {
                        const int accept_error = WSAGetLastError();
                        if (accept_error == WSAEWOULDBLOCK) break;
                        error =
                            winsock_error("accept", accept_error);
                        return false;
                    }
                    if (!configure_peer_socket(
                            accepted.get(), error)) return false;
                    RoomLink link;
                    link.socket = std::move(accepted);
                    link.received.reserve(16 * 1024);
                    PeerMessage hello;
                    hello.type = PeerMessageType::Hello;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        hello.text = local_name;
                    }
                    if (!link.wire.push(hello, error)) return false;
                    links.push_back(std::move(link));
                }
            }

            std::vector<std::size_t> remove;
            for (std::size_t index = 0;
                 index < links.size(); ++index) {
                auto& link = links[index];
                bool drop =
                    FD_ISSET(link.socket.get(), &except_set) != 0;
                std::string link_error;
                if (!drop && !link.wire.empty() &&
                    FD_ISSET(link.socket.get(), &write_set)) {
                    bool sent_any = false;
                    if (!link.wire.flush(link.socket.get(),
                                         sent_any, link_error)) {
                        drop = true;
                    }
                }
                if (!drop &&
                    FD_ISSET(link.socket.get(), &read_set)) {
                    std::vector<PeerMessage> messages;
                    bool closed = false;
                    if (!receive_frames(link, messages,
                                        closed, link_error)) {
                        drop = true;
                    } else {
                        drop = closed;
                        for (const auto& message : messages) {
                            if (!process_host_link_message(
                                    link, message, link_error)) {
                                drop = true;
                                break;
                            }
                        }
                    }
                }
                if (!drop && !link.handshake_complete &&
                    now - link.accepted_at >= kHandshakeTimeout) {
                    drop = true;
                }
                if (!drop && link.handshake_complete &&
                    now - link.last_received >= kPeerTimeout) {
                    drop = true;
                }
                if (!drop && link.close_after_flush &&
                    link.wire.empty()) {
                    drop = true;
                }
                if (drop) remove.push_back(index);
            }
            for (auto it = remove.rbegin();
                 it != remove.rend(); ++it) {
                const uint8_t id = links[*it].player_id;
                const std::string name = links[*it].name;
                shutdown(links[*it].socket.get(), SD_BOTH);
                links.erase(
                    links.begin() +
                    static_cast<std::ptrdiff_t>(*it));
                if (id != 0) {
                    std::lock_guard<std::mutex> lock(mutex);
                    remove_host_player_locked(
                        id,
                        (name.empty() ? "Player" : name) +
                            " disconnected.");
                }
            }
        }

        for (auto& link : links) {
            shutdown(link.socket.get(), SD_BOTH);
        }
        return true;
    }
    bool collect_joiner_outgoing(
        RoomLink& link,
        bool room_ready,
        uint64_t& sent_library_generation,
        std::size_t& library_cursor,
        int& library_stage,
        SteadyClock::time_point now,
        SteadyClock::time_point& last_score_sent,
        std::string& error) {
        std::deque<PeerMessage> pending;
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending.swap(controls);
            if (room_ready &&
                sent_library_generation !=
                    local_library_generation) {
                sent_library_generation =
                    local_library_generation;
                library_cursor = 0;
                library_stage = 0;
            }
            if (room_ready && pending_score_dirty &&
                pending_score.has_value() &&
                active_round_nonce != 0 &&
                now - last_score_sent >= kScoreInterval) {
                PeerMessage score;
                score.type = PeerMessageType::Score;
                score.nonce = active_round_nonce;
                score.score = *pending_score;
                pending.push_back(std::move(score));
                pending_score_dirty = false;
                last_score_sent = now;
            }
            if (room_ready &&
                link.wire.bytes < kMaxWireBytes / 2) {
                PeerMessage library;
                if (library_stage == 0) {
                    library.type =
                        PeerMessageType::LibraryBegin;
                    library.library_count =
                        static_cast<uint32_t>(
                            local_library_sha256.size());
                    pending.push_back(std::move(library));
                    library_stage = 1;
                } else if (
                    library_stage == 1 &&
                    library_cursor <
                        local_library_sha256.size()) {
                    const std::size_t end =
                        std::min(
                            local_library_sha256.size(),
                            library_cursor +
                                kPeerLibraryHashesPerChunk);
                    library.type =
                        PeerMessageType::LibraryChunk;
                    library.chart_sha256.assign(
                        local_library_sha256.begin() +
                            library_cursor,
                        local_library_sha256.begin() + end);
                    pending.push_back(std::move(library));
                    library_cursor = end;
                } else if (library_stage == 1) {
                    library.type =
                        PeerMessageType::LibraryEnd;
                    pending.push_back(std::move(library));
                    library_stage = 2;
                }
            }
        }
        for (const auto& message : pending) {
            if (!link.wire.push(message, error)) return false;
        }
        return true;
    }

    bool run_joiner(SocketHandle socket,
                    std::string& error) {
        RoomLink link;
        link.socket = std::move(socket);
        link.received.reserve(16 * 1024);
        PeerMessage hello;
        hello.type = PeerMessageType::Hello;
        {
            std::lock_guard<std::mutex> lock(mutex);
            hello.text = local_name;
            set_state_locked(PeerSessionState::Handshaking,
                             "Exchanging room handshake.");
        }
        if (!link.wire.push(hello, error)) return false;
        bool saw_host_hello = false;
        bool room_ready = false;
        bool shutdown_sent = false;
        auto close_deadline =
            SteadyClock::time_point::max();
        uint64_t sent_library_generation = 0;
        std::size_t library_cursor = 0;
        int library_stage = 0;
        auto last_score_sent =
            SteadyClock::now() - kScoreInterval;
        const auto handshake_deadline =
            SteadyClock::now() + kHandshakeTimeout;

        while (!hard_stop.load(std::memory_order_acquire)) {
            const auto now = SteadyClock::now();
            const bool closing =
                graceful_stop.load(std::memory_order_acquire);
            if (closing &&
                close_deadline ==
                    SteadyClock::time_point::max()) {
                close_deadline =
                    now + kGracefulCloseTimeout;
            }
            if (!collect_joiner_outgoing(
                    link, room_ready,
                    sent_library_generation,
                    library_cursor, library_stage,
                    now, last_score_sent, error)) {
                return false;
            }
            if (!closing && room_ready &&
                now - link.last_ping_sent >=
                    kHeartbeatInterval) {
                PeerMessage ping;
                ping.type = PeerMessageType::Ping;
                ping.nonce = ++link.ping_nonce;
                link.ping_sent_at = now;
                if (!link.wire.push(ping, error)) {
                    return false;
                }
                link.last_ping_sent = now;
            }
            if (!room_ready && now >= handshake_deadline) {
                error = "Room handshake timed out.";
                return false;
            }
            if (room_ready && !closing &&
                now - link.last_received >= kPeerTimeout) {
                error = "Room host connection timed out.";
                return false;
            }
            if (closing && link.wire.empty() &&
                !shutdown_sent) {
                shutdown(link.socket.get(), SD_SEND);
                shutdown_sent = true;
            }
            if (closing && now >= close_deadline) break;

            fd_set read_set;
            fd_set write_set;
            fd_set except_set;
            FD_ZERO(&read_set);
            FD_ZERO(&write_set);
            FD_ZERO(&except_set);
            FD_SET(link.socket.get(), &read_set);
            FD_SET(link.socket.get(), &except_set);
            if (!link.wire.empty()) {
                FD_SET(link.socket.get(), &write_set);
            }
            timeval timeout{};
            timeout.tv_usec = static_cast<long>(
                std::chrono::duration_cast<
                    std::chrono::microseconds>(
                    kIoPollInterval).count());
            const int selected =
                select(0, &read_set, &write_set,
                       &except_set, &timeout);
            if (selected == SOCKET_ERROR) {
                error =
                    winsock_error("select(room player)",
                                  WSAGetLastError());
                return false;
            }
            if (FD_ISSET(link.socket.get(), &except_set)) {
                error = "Room host socket failed.";
                return false;
            }
            if (!link.wire.empty() &&
                FD_ISSET(link.socket.get(), &write_set)) {
                bool sent_any = false;
                if (!link.wire.flush(link.socket.get(),
                                     sent_any, error)) {
                    return false;
                }
            }
            if (!FD_ISSET(link.socket.get(), &read_set)) {
                continue;
            }

            std::vector<PeerMessage> messages;
            bool closed = false;
            if (!receive_frames(link, messages,
                                closed, error)) {
                return false;
            }
            for (const auto& message : messages) {
                if (!saw_host_hello) {
                    if (message.type !=
                            PeerMessageType::Hello ||
                        message.text.empty() ||
                        message.text.size() > 64) {
                        error =
                            "Room host handshake is invalid.";
                        return false;
                    }
                    saw_host_hello = true;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        current.peer_name = message.text;
                    }
                    continue;
                }
                PeerMessage response;
                bool has_response = false;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (!apply_client_message_locked(
                            message, response,
                            has_response, error)) {
                        if (message.type ==
                            PeerMessageType::Disconnect) {
                            return true;
                        }
                        return false;
                    }
                    room_ready =
                        current.local_player_id != 0 &&
                        current.state ==
                            PeerSessionState::Connected;
                }
                if (has_response &&
                    !link.wire.push(response, error)) {
                    return false;
                }
                if (message.type == PeerMessageType::Pong &&
                    message.nonce == link.ping_nonce &&
                    link.ping_nonce != 0) {
                    const auto elapsed =
                        std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            SteadyClock::now() -
                            link.ping_sent_at);
                    std::lock_guard<std::mutex> lock(mutex);
                    if (auto* coordinator =
                            participant_locked(1)) {
                        coordinator->estimated_rtt_ms =
                            static_cast<uint32_t>(
                                std::clamp<int64_t>(
                                    elapsed.count(), 1, 2000));
                    }
                    link.ping_nonce = 0;
                    touch_locked();
                }
            }
            // recv() can deliver the final Disconnect frame and FIN in one
            // pass. Process every buffered frame first so a graceful room
            // close is not misreported as a socket failure.
            if (closed) {
                error = "Room host closed the connection.";
                return false;
            }
        }
        shutdown(link.socket.get(), SD_BOTH);
        return true;
    }
#endif

    void worker_main(PeerRole role,
                     std::string address,
                     uint16_t port) {
#ifdef _WIN32
        std::string error;
        WinsockRuntime runtime;
        if (!runtime.start(error)) {
            fail(std::move(error));
            finish_worker();
            return;
        }
        bool success = false;
        if (role == PeerRole::Host) {
            success = run_host(port, error);
        } else {
            SocketHandle socket =
                connect_peer(address, port, error);
            if (socket.valid()) {
                success =
                    run_joiner(std::move(socket), error);
            }
        }
        if (!success &&
            !hard_stop.load(std::memory_order_acquire) &&
            !graceful_stop.load(std::memory_order_acquire)) {
            fail(error.empty()
                     ? "Multiplayer connection failed."
                     : std::move(error));
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
PeerSession::PeerSession()
    : impl_(std::make_unique<Impl>()) {}

PeerSession::~PeerSession() {
    disconnect("Session shutdown");
}

bool PeerSession::host(uint16_t port, std::string name) {
    if (name.empty() || name.size() > 64) return false;
    return impl_->start(
        PeerRole::Host, {}, port, std::move(name));
}

bool PeerSession::join(std::string address,
                       uint16_t port,
                       std::string name) {
    if (address.empty() || address.size() > 255 ||
        port == 0 || name.empty() || name.size() > 64) {
        return false;
    }
    return impl_->start(
        PeerRole::Joiner, std::move(address),
        port, std::move(name));
}

void PeerSession::disconnect(std::string reason) {
    bool join_worker = false;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        join_worker = impl_->worker.joinable();
        if (impl_->worker_running) {
            if (reason.size() > 1024) reason.resize(1024);
            PeerMessage message;
            message.type = PeerMessageType::Disconnect;
            message.text = std::move(reason);
            if (impl_->controls.size() >=
                kMaxControlMessages) {
                impl_->controls.clear();
            }
            impl_->controls.push_front(std::move(message));
            impl_->set_state_locked(
                PeerSessionState::Closing,
                "Closing multiplayer room.");
            impl_->graceful_stop.store(
                true, std::memory_order_release);
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

void PeerSession::set_local_library(
    std::vector<std::string> chart_sha256) {
    std::vector<std::string> normalized;
    normalized.reserve(
        std::min(chart_sha256.size(),
                 kPeerLibraryMaxCharts));
    for (auto& value : chart_sha256) {
        if (normalize_library_sha256(value)) {
            normalized.push_back(std::move(value));
        }
    }
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(
        std::unique(normalized.begin(), normalized.end()),
        normalized.end());
    if (normalized.size() > kPeerLibraryMaxCharts) {
        normalized.resize(kPeerLibraryMaxCharts);
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->local_library_sha256 =
        std::move(normalized);
    ++impl_->local_library_generation;
    impl_->state_cv.notify_all();
}

bool PeerSession::set_local_chart(
    const ChartFingerprint& fingerprint,
    std::string display_name) {
    if (!fingerprint.valid() ||
        display_name.size() > 1024) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->active_round_nonce != 0 ||
        impl_->current.state ==
            PeerSessionState::Closing) {
        return false;
    }
    if (impl_->current.local_player_id != 0 &&
        impl_->current.local_player_id !=
            impl_->current.leader_player_id &&
        impl_->current.selected_chart.fingerprint.valid() &&
        (fingerprint.hash !=
             impl_->current.selected_chart.fingerprint.hash ||
         fingerprint.size !=
             impl_->current.selected_chart.fingerprint.size)) {
        return false;
    }

    impl_->current.local_chart.fingerprint = fingerprint;
    impl_->current.local_chart.name = display_name;
    if (auto* local = impl_->participant_locked(
            impl_->current.local_player_id)) {
        local->chart = impl_->current.local_chart;
        local->ready = false;
        if (local->leader) {
            impl_->current.selected_chart = local->chart;
            for (auto& participant :
                 impl_->current.participants) {
                participant.ready = false;
                if (!participant.local) {
                    participant.chart = {};
                }
            }
        }
    }
    if (impl_->worker_running) {
        PeerMessage chart;
        chart.type = PeerMessageType::Chart;
        chart.chart_hash = fingerprint.hash;
        chart.chart_size = fingerprint.size;
        chart.text = std::move(display_name);
        if (!impl_->queue_control_locked(
                std::move(chart))) {
            return false;
        }
    }
    impl_->touch_locked();
    return true;
}

void PeerSession::clear_local_chart() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->current.local_chart = {};
    if (auto* local = impl_->participant_locked(
            impl_->current.local_player_id)) {
        local->chart = {};
        local->ready = false;
    }
    if (impl_->current.state ==
        PeerSessionState::Connected) {
        PeerMessage ready;
        ready.type = PeerMessageType::Ready;
        ready.ready = false;
        (void)impl_->queue_control_locked(
            std::move(ready));
    }
    impl_->touch_locked();
}

bool PeerSession::set_ready(bool ready) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state !=
            PeerSessionState::Connected ||
        impl_->active_round_nonce != 0) {
        return false;
    }
    auto* local = impl_->participant_locked(
        impl_->current.local_player_id);
    if (!local ||
        (ready &&
         !chart_matches(
             local->chart,
             impl_->current.selected_chart))) {
        return false;
    }
    PeerMessage message;
    message.type = PeerMessageType::Ready;
    message.ready = ready;
    if (!impl_->queue_control_locked(
            std::move(message))) {
        return false;
    }
    local->ready = ready;
    if (!ready) local->loaded = false;
    impl_->touch_locked();
    return true;
}

bool PeerSession::send_chat(std::string text) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state != PeerSessionState::Connected) {
        return false;
    }
    text = Impl::normalize_chat_text(text);
    if (text.empty()) {
        return false;
    }
    PeerMessage message;
    message.type = PeerMessageType::Chat;
    message.text = std::move(text);
    return impl_->queue_control_locked(std::move(message));
}

bool PeerSession::send_launch() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->current.can_start ||
        impl_->active_round_nonce != 0) {
        return false;
    }
    uint64_t nonce = static_cast<uint64_t>(
        SteadyClock::now().time_since_epoch().count());
    nonce ^=
        static_cast<uint64_t>(
            impl_->current.local_player_id) << 56u;
    if (nonce == 0) nonce = 1;
    PeerMessage launch;
    launch.type = PeerMessageType::Launch;
    launch.chart_hash =
        impl_->current.selected_chart.fingerprint.hash;
    launch.nonce = nonce;
    if (!impl_->queue_control_locked(
            std::move(launch))) {
        return false;
    }
    impl_->current.status_detail = "Start requested; waiting for room coordinator.";
    impl_->touch_locked();
    return true;
}

std::optional<uint64_t> PeerSession::poll_launch() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state !=
            PeerSessionState::Connected ||
        impl_->active_round_nonce == 0) {
        impl_->pending_launch.reset();
        return std::nullopt;
    }
    auto result = impl_->pending_launch;
    impl_->pending_launch.reset();
    return result;
}

bool PeerSession::mark_loaded() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state !=
            PeerSessionState::Connected ||
        impl_->active_round_nonce == 0) {
        return false;
    }
    PeerMessage loaded;
    loaded.type = PeerMessageType::Loaded;
    loaded.nonce = impl_->active_round_nonce;
    if (!impl_->queue_control_locked(
            std::move(loaded))) {
        return false;
    }
    if (auto* local = impl_->participant_locked(
            impl_->current.local_player_id)) {
        local->loaded = true;
    }
    impl_->touch_locked();
    return true;
}

bool PeerSession::wait_for_peer_loaded(
    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->state_cv.wait_for(
        lock, timeout, [this]() {
            return impl_->current.remote_loaded ||
                   impl_->active_round_nonce == 0 ||
                   impl_->current.state ==
                       PeerSessionState::Disconnected ||
                   impl_->current.state ==
                       PeerSessionState::Failed;
        });
    return impl_->current.remote_loaded;
}

bool PeerSession::send_begin(uint32_t delay_ms) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state !=
            PeerSessionState::Connected ||
        !impl_->current.local_is_leader ||
        impl_->active_round_nonce == 0 ||
        impl_->begin_sent ||
        !impl_->all_players_loaded_locked()) {
        return false;
    }
    PeerMessage begin;
    begin.type = PeerMessageType::Begin;
    begin.nonce = impl_->active_round_nonce;
    begin.delay_ms = delay_ms;
    if (!impl_->queue_control_locked(
            std::move(begin))) {
        return false;
    }
    impl_->begin_sent = true;
    impl_->touch_locked();
    return true;
}

bool PeerSession::wait_for_begin(
    std::chrono::milliseconds timeout,
    uint32_t& out_delay_ms) {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->state_cv.wait_for(
        lock, timeout, [this]() {
            return impl_->pending_begin.has_value() ||
                   impl_->active_round_nonce == 0 ||
                   impl_->current.state ==
                       PeerSessionState::Disconnected ||
                   impl_->current.state ==
                       PeerSessionState::Failed;
        });
    if (!impl_->pending_begin.has_value()) {
        return false;
    }
    out_delay_ms = *impl_->pending_begin;
    impl_->pending_begin.reset();
    return true;
}

bool PeerSession::publish_score(
    const PeerScore& score, bool final) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current.state !=
            PeerSessionState::Connected ||
        impl_->active_round_nonce == 0) {
        return false;
    }
    const bool send_final = final || score.finished;
    if (auto* local = impl_->participant_locked(
            impl_->current.local_player_id)) {
        local->latest_score = score;
        local->latest_score.finished = send_final;
        local->has_score = true;
    }
    if (send_final) {
        PeerMessage message;
        message.type = PeerMessageType::FinalScore;
        message.nonce = impl_->active_round_nonce;
        message.score = score;
        message.score.finished = true;
        if (!impl_->queue_control_locked(
                std::move(message))) {
            return false;
        }
        impl_->pending_score.reset();
        impl_->pending_score_dirty = false;
        impl_->local_final_score_sent = true;
        impl_->touch_locked();
        return true;
    }
    impl_->pending_score = score;
    impl_->pending_score_dirty = true;
    impl_->touch_locked();
    return true;
}

void PeerSession::reset_round() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->active_round_nonce == 0 ||
        impl_->current.state !=
            PeerSessionState::Connected) {
        if (auto* local = impl_->participant_locked(
                impl_->current.local_player_id)) {
            local->ready = false;
            local->loaded = false;
        }
        impl_->touch_locked();
        return;
    }
    if (!impl_->local_final_score_sent) {
        impl_->current.status_detail =
            "Cannot leave the round before the local final score is queued.";
        impl_->touch_locked();
        return;
    }
    auto* local = impl_->participant_locked(
        impl_->current.local_player_id);
    if (!local || local->round_reset) return;
    PeerMessage reset;
    reset.type = PeerMessageType::RoundReset;
    reset.nonce = impl_->active_round_nonce;
    if (!impl_->queue_control_locked(
            std::move(reset))) {
        impl_->set_state_locked(
            PeerSessionState::Failed,
            "Could not queue multiplayer round reset.");
        return;
    }
    local->round_reset = true;
    local->ready = false;
    local->loaded = false;
    impl_->pending_score.reset();
    impl_->pending_score_dirty = false;
    impl_->pending_launch.reset();
    impl_->pending_begin.reset();
    impl_->touch_locked();
}

}  // namespace tenriff::network