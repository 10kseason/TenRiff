#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::network {

constexpr uint16_t kPeerProtocolVersion = 5;
constexpr uint8_t kPeerMaxPlayers = 8;
constexpr std::size_t kPeerChatMaxBytes = 256;
constexpr std::size_t kPeerChatHistoryLimit = 32;
constexpr std::size_t kPeerLibraryHashesPerChunk = 512;
constexpr std::size_t kPeerLibraryMaxCharts = 250'000;
constexpr std::size_t kPeerFrameHeaderSize = 12;
constexpr std::size_t kPeerMaxPayloadSize = 64 * 1024;
constexpr int64_t kPeerMaximumClaimedScore = 10'000;
constexpr int kPeerMaximumJudgementCount = 10'000'000;

enum class PeerMessageType : uint16_t {
    Hello = 1,
    Chart = 2,
    Ready = 3,
    Launch = 4,
    Loaded = 5,
    Begin = 6,
    Score = 7,
    FinalScore = 8,
    Ping = 9,
    Pong = 10,
    Disconnect = 11,
    RoundReset = 12,
    RoundCancel = 13,
    RoundCancelAck = 14,
    LibraryBegin = 15,
    LibraryChunk = 16,
    LibraryEnd = 17,
    RoomWelcome = 18,
    RoomRoster = 19,
    CommonLibraryBegin = 20,
    CommonLibraryChunk = 21,
    CommonLibraryEnd = 22,
    Chat = 23,
};

struct PeerScore {
    int64_t score = 0;
    int64_t current_sample = 0;
    int combo = 0;
    int max_combo = 0;
    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;
    int gauge_milli = 0;
    bool finished = false;
    bool game_over = false;
    bool aborted = false;
};

// This is a wire-safety/consistency bound, not proof that the peer earned the
// score. Direct-P2P results remain untrusted claims until replay proof exists.
[[nodiscard]] bool peer_score_claim_is_sane(const PeerScore& score);

struct PeerParticipantWire {
    uint8_t player_id = 0;
    std::string name;
    bool ready = false;
    bool loaded = false;
    bool round_reset = false;
    uint64_t chart_hash = 0;
    uint64_t chart_size = 0;
    std::string chart_name;
};

struct PeerMessage {
    PeerMessageType type = PeerMessageType::Hello;
    std::string text;
    uint64_t chart_hash = 0;
    uint64_t chart_size = 0;
    uint64_t nonce = 0;
    uint32_t delay_ms = 0;
    bool ready = false;
    uint8_t player_id = 0;
    uint8_t leader_id = 0;
    bool round_active = false;
    PeerScore score;
    uint32_t library_count = 0;
    std::vector<std::string> chart_sha256;
    std::vector<PeerParticipantWire> participants;
};

enum class PeerDecodeStatus {
    Complete,
    Incomplete,
    Error,
};

// Frames are deliberately small and versioned so malformed or incompatible peers
// are rejected before their payload is interpreted.
[[nodiscard]] std::vector<uint8_t> encode_peer_message(const PeerMessage& message,
                                                       std::string* error = nullptr);

[[nodiscard]] PeerDecodeStatus decode_peer_message(const std::vector<uint8_t>& bytes,
                                                   PeerMessage& message,
                                                   std::size_t& consumed,
                                                   std::string& error);

struct ChartFingerprint {
    uint64_t hash = 0;
    uint64_t size = 0;

    [[nodiscard]] bool valid() const { return hash != 0; }
};

// This is an identity check, not a security primitive. Exact file bytes must match
// on both peers; charts themselves are never transferred by the protocol.
[[nodiscard]] ChartFingerprint fingerprint_chart_file(std::string_view path,
                                                      std::string* error = nullptr);

}  // namespace tenriff::network
