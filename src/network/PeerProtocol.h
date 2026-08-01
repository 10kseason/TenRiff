#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::network {

constexpr uint16_t kPeerProtocolVersion = 3;
constexpr std::size_t kPeerLibraryHashesPerChunk = 512;
constexpr std::size_t kPeerLibraryMaxCharts = 250'000;
constexpr std::size_t kPeerFrameHeaderSize = 12;
constexpr std::size_t kPeerMaxPayloadSize = 64 * 1024;

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

struct PeerMessage {
    PeerMessageType type = PeerMessageType::Hello;
    std::string text;
    uint64_t chart_hash = 0;
    uint64_t chart_size = 0;
    uint64_t nonce = 0;
    uint32_t delay_ms = 0;
    bool ready = false;
    PeerScore score;
    uint32_t library_count = 0;
    std::vector<std::string> chart_sha256;
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
