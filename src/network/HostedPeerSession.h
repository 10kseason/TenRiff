#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "network/PeerSession.h"

namespace tenriff::network {

// Production requests can only reach this origin. Tests inject a transport, not
// a configurable origin that could receive the user's leaderboard credential.
inline constexpr char kHostedMultiplayerOrigin[] =
    "https://tenriff-multiplayer.lastestarcorp.chatgpt.site";

struct HostedHttpResponse {
    unsigned status = 0;
    std::string body;
};
using HostedHttpTransport = std::function<bool(std::string_view method,
    std::string_view path, std::string_view bearer, std::string_view body,
    HostedHttpResponse& response)>;

// A separate HTTP protocol; it does not reinterpret legacy peer wire packets.
class HostedPeerSession {
public:
    explicit HostedPeerSession(HostedHttpTransport transport = {});
    ~HostedPeerSession();
    HostedPeerSession(const HostedPeerSession&) = delete;
    HostedPeerSession& operator=(const HostedPeerSession&) = delete;

    [[nodiscard]] bool configure(std::string upload_token);
    [[nodiscard]] bool browse();
    [[nodiscard]] bool host();
    [[nodiscard]] bool join(std::string room_id);
    // Menu/account revocation never waits for in-flight synchronous HTTP.
    void request_stop();
    [[nodiscard]] bool running() const;
    // Destruction/tests join after requesting stop; no asynchronous handle close.
    void disconnect();
    [[nodiscard]] PeerSessionSnapshot snapshot() const;
    [[nodiscard]] std::vector<HostedRoomInfo> rooms() const;
    [[nodiscard]] bool set_local_chart(const ChartFingerprint&, std::string name);
    void clear_local_chart();
    [[nodiscard]] bool set_ready(bool);
    [[nodiscard]] bool set_rate(uint32_t);
    [[nodiscard]] bool send_chat(std::string);
    [[nodiscard]] bool send_launch();
    [[nodiscard]] std::optional<uint64_t> poll_launch();
    [[nodiscard]] bool mark_loaded();
    [[nodiscard]] bool wait_for_peer_loaded(std::chrono::milliseconds);
    [[nodiscard]] bool send_begin();
    [[nodiscard]] bool wait_for_begin(std::chrono::milliseconds, uint32_t&);
    [[nodiscard]] bool publish_score(const PeerScore&, bool final);
    void reset_round();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::network
