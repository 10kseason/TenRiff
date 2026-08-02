#include "network/PeerProtocol.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace tenriff::network {

namespace {

constexpr uint32_t kPeerFrameMagic = 0x54525031u;  // "TRP1"
constexpr std::size_t kMaxPeerTextBytes = 1024;

void set_error(std::string* target, std::string value) {
    if (target) {
        *target = std::move(value);
    }
}

void append_u8(std::vector<uint8_t>& out, uint8_t value) {
    out.push_back(value);
}

void append_u16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    out.push_back(static_cast<uint8_t>(value & 0xffu));
}

void append_u32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
    out.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    out.push_back(static_cast<uint8_t>(value & 0xffu));
}

void append_u64(std::vector<uint8_t>& out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((value >> static_cast<unsigned>(shift)) & 0xffu));
    }
}

void append_i32(std::vector<uint8_t>& out, int value) {
    append_u32(out, static_cast<uint32_t>(static_cast<int32_t>(value)));
}

void append_i64(std::vector<uint8_t>& out, int64_t value) {
    append_u64(out, static_cast<uint64_t>(value));
}

bool append_string(std::vector<uint8_t>& out, const std::string& value, std::string* error) {
    if (value.size() > kMaxPeerTextBytes || value.size() > std::numeric_limits<uint16_t>::max()) {
        set_error(error, "Peer protocol text field is too long.");
        return false;
    }
    append_u16(out, static_cast<uint16_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    return true;
}

int hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool append_sha256(std::vector<uint8_t>& out, const std::string& value, std::string* error) {
    if (value.size() != 64u) {
        set_error(error, "Library SHA-256 values must contain 64 hex characters.");
        return false;
    }
    for (std::size_t index = 0; index < value.size(); index += 2) {
        const int high = hex_nibble(value[index]);
        const int low = hex_nibble(value[index + 1]);
        if (high < 0 || low < 0) {
            set_error(error, "Library SHA-256 values must be hexadecimal.");
            return false;
        }
        out.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

class Reader {
public:
    Reader(const std::vector<uint8_t>& bytes, std::size_t offset, std::size_t limit)
        : bytes_(bytes), cursor_(offset), limit_(limit) {}

    bool read_u8(uint8_t& value) {
        if (cursor_ + 1 > limit_) return false;
        value = bytes_[cursor_++];
        return true;
    }

    bool read_u16(uint16_t& value) {
        if (cursor_ + 2 > limit_) return false;
        value = static_cast<uint16_t>((static_cast<uint16_t>(bytes_[cursor_]) << 8u) |
                                      static_cast<uint16_t>(bytes_[cursor_ + 1]));
        cursor_ += 2;
        return true;
    }

    bool read_u32(uint32_t& value) {
        if (cursor_ + 4 > limit_) return false;
        value = (static_cast<uint32_t>(bytes_[cursor_]) << 24u) |
                (static_cast<uint32_t>(bytes_[cursor_ + 1]) << 16u) |
                (static_cast<uint32_t>(bytes_[cursor_ + 2]) << 8u) |
                static_cast<uint32_t>(bytes_[cursor_ + 3]);
        cursor_ += 4;
        return true;
    }

    bool read_u64(uint64_t& value) {
        if (cursor_ + 8 > limit_) return false;
        value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8u) | static_cast<uint64_t>(bytes_[cursor_ + static_cast<std::size_t>(i)]);
        }
        cursor_ += 8;
        return true;
    }

    bool read_i32(int& value) {
        uint32_t encoded = 0;
        if (!read_u32(encoded)) return false;
        value = static_cast<int32_t>(encoded);
        return true;
    }

    bool read_i64(int64_t& value) {
        uint64_t encoded = 0;
        if (!read_u64(encoded)) return false;
        value = static_cast<int64_t>(encoded);
        return true;
    }

    bool read_string(std::string& value) {
        uint16_t size = 0;
        if (!read_u16(size) || size > kMaxPeerTextBytes || cursor_ + size > limit_) return false;
        value.assign(reinterpret_cast<const char*>(bytes_.data() + cursor_), size);
        cursor_ += size;
        return true;
    }

    bool read_sha256(std::string& value) {
        constexpr char kHex[] = "0123456789abcdef";
        constexpr std::size_t kHashBytes = 32;
        if (cursor_ + kHashBytes > limit_) return false;
        value.resize(kHashBytes * 2);
        for (std::size_t index = 0; index < kHashBytes; ++index) {
            const uint8_t byte = bytes_[cursor_++];
            value[index * 2] = kHex[(byte >> 4u) & 0x0fu];
            value[index * 2 + 1] = kHex[byte & 0x0fu];
        }
        return true;
    }

    [[nodiscard]] bool at_end() const { return cursor_ == limit_; }

private:
    const std::vector<uint8_t>& bytes_;
    std::size_t cursor_ = 0;
    std::size_t limit_ = 0;
};

bool is_known_type(uint16_t raw) {
    return raw >= static_cast<uint16_t>(PeerMessageType::Hello) &&
           raw <= static_cast<uint16_t>(PeerMessageType::Chat);
}

bool decode_score(Reader& reader, PeerScore& score) {
    uint8_t flags = 0;
    if (!reader.read_i64(score.score) ||
        !reader.read_i64(score.current_sample) ||
        !reader.read_i32(score.combo) ||
        !reader.read_i32(score.max_combo) ||
        !reader.read_i32(score.perfect) ||
        !reader.read_i32(score.great) ||
        !reader.read_i32(score.good) ||
        !reader.read_i32(score.bad) ||
        !reader.read_i32(score.poor) ||
        !reader.read_i32(score.gauge_milli) ||
        !reader.read_u8(flags)) {
        return false;
    }
    score.finished = (flags & 0x01u) != 0u;
    score.game_over = (flags & 0x02u) != 0u;
    score.aborted = (flags & 0x04u) != 0u;
    return true;
}

}  // namespace

std::vector<uint8_t> encode_peer_message(const PeerMessage& message, std::string* error) {
    if (error) error->clear();
    std::vector<uint8_t> payload;

    switch (message.type) {
        case PeerMessageType::Hello:
        case PeerMessageType::Disconnect:
            if (!append_string(payload, message.text, error)) return {};
            break;
        case PeerMessageType::Chat:
            if (message.text.empty() || message.text.size() > kPeerChatMaxBytes) {
                set_error(error, "Peer chat message is empty or too long.");
                return {};
            }
            append_u8(payload, message.player_id);
            if (!append_string(payload, message.text, error)) return {};
            break;
        case PeerMessageType::Chart:
            if (message.chart_hash == 0) {
                set_error(error, "Chart messages require a non-zero fingerprint.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u64(payload, message.chart_hash);
            append_u64(payload, message.chart_size);
            if (!append_string(payload, message.text, error)) return {};
            break;
        case PeerMessageType::Ready:
            append_u8(payload, message.player_id);
            append_u8(payload, message.ready ? 1u : 0u);
            break;
        case PeerMessageType::Launch:
            if (message.chart_hash == 0 || message.nonce == 0) {
                set_error(error, "Launch messages require a non-zero fingerprint and round nonce.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u64(payload, message.chart_hash);
            append_u64(payload, message.nonce);
            break;
        case PeerMessageType::Loaded:
            if (message.nonce == 0) {
                set_error(error, "Loaded messages require a non-zero round nonce.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u64(payload, message.nonce);
            break;
        case PeerMessageType::Begin:
            if (message.nonce == 0) {
                set_error(error, "Begin messages require a non-zero round nonce.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u64(payload, message.nonce);
            append_u32(payload, message.delay_ms);
            break;
        case PeerMessageType::Score:
        case PeerMessageType::FinalScore: {
            if (message.nonce == 0) {
                set_error(error, "Score messages require a non-zero round nonce.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u64(payload, message.nonce);
            append_i64(payload, message.score.score);
            append_i64(payload, message.score.current_sample);
            append_i32(payload, message.score.combo);
            append_i32(payload, message.score.max_combo);
            append_i32(payload, message.score.perfect);
            append_i32(payload, message.score.great);
            append_i32(payload, message.score.good);
            append_i32(payload, message.score.bad);
            append_i32(payload, message.score.poor);
            append_i32(payload, message.score.gauge_milli);
            const uint8_t flags = (message.score.finished ? 0x01u : 0u) |
                                  (message.score.game_over ? 0x02u : 0u) |
                                  (message.score.aborted ? 0x04u : 0u);
            append_u8(payload, flags);
            break;
        }
        case PeerMessageType::Ping:
        case PeerMessageType::Pong:
            append_u64(payload, message.nonce);
            break;
        case PeerMessageType::RoundReset:
        case PeerMessageType::RoundCancel:
        case PeerMessageType::RoundCancelAck:
            if (message.nonce == 0) {
                set_error(error, "Round control messages require a non-zero round nonce.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u64(payload, message.nonce);
            break;
        case PeerMessageType::LibraryBegin:
        case PeerMessageType::CommonLibraryBegin:
            if (message.library_count > kPeerLibraryMaxCharts) {
                set_error(error, "Peer library exceeds the chart limit.");
                return {};
            }
            append_u32(payload, message.library_count);
            break;
        case PeerMessageType::LibraryChunk:
        case PeerMessageType::CommonLibraryChunk:
            if (message.chart_sha256.empty() ||
                message.chart_sha256.size() > kPeerLibraryHashesPerChunk) {
                set_error(error, "Peer library chunk has an invalid hash count.");
                return {};
            }
            append_u16(payload, static_cast<uint16_t>(message.chart_sha256.size()));
            for (const auto& sha256 : message.chart_sha256) {
                if (!append_sha256(payload, sha256, error)) return {};
            }
            break;
        case PeerMessageType::LibraryEnd:
        case PeerMessageType::CommonLibraryEnd:
            break;
        case PeerMessageType::RoomWelcome:
            if (message.player_id == 0 || message.player_id > kPeerMaxPlayers ||
                message.leader_id == 0 || message.leader_id > kPeerMaxPlayers) {
                set_error(error, "Room welcome contains an invalid player id.");
                return {};
            }
            append_u8(payload, message.player_id);
            append_u8(payload, message.leader_id);
            break;
        case PeerMessageType::RoomRoster:
            if (message.leader_id == 0 || message.leader_id > kPeerMaxPlayers ||
                message.participants.empty() || message.participants.size() > kPeerMaxPlayers) {
                set_error(error, "Room roster has invalid player metadata.");
                return {};
            }
            append_u8(payload, message.leader_id);
            append_u8(payload, message.round_active ? 1u : 0u);
            append_u64(payload, message.nonce);
            append_u8(payload, static_cast<uint8_t>(message.participants.size()));
            for (const auto& participant : message.participants) {
                if (participant.player_id == 0 || participant.player_id > kPeerMaxPlayers) {
                    set_error(error, "Room roster contains an invalid player id.");
                    return {};
                }
                append_u8(payload, participant.player_id);
                const uint8_t flags = (participant.ready ? 0x01u : 0u) |
                                      (participant.loaded ? 0x02u : 0u) |
                                      (participant.round_reset ? 0x04u : 0u) |
                                      (participant.chart_hash != 0 ? 0x08u : 0u);
                append_u8(payload, flags);
                if (!append_string(payload, participant.name, error)) return {};
                if (participant.chart_hash != 0) {
                    append_u64(payload, participant.chart_hash);
                    append_u64(payload, participant.chart_size);
                    if (!append_string(payload, participant.chart_name, error)) return {};
                }
            }
            break;
        default:
            set_error(error, "Unknown peer protocol message type.");
            return {};
    }
    if (payload.size() > kPeerMaxPayloadSize) {
        set_error(error, "Peer protocol payload exceeds the size limit.");
        return {};
    }

    std::vector<uint8_t> frame;
    frame.reserve(kPeerFrameHeaderSize + payload.size());
    append_u32(frame, kPeerFrameMagic);
    append_u16(frame, kPeerProtocolVersion);
    append_u16(frame, static_cast<uint16_t>(message.type));
    append_u32(frame, static_cast<uint32_t>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

PeerDecodeStatus decode_peer_message(const std::vector<uint8_t>& bytes,
                                     PeerMessage& message,
                                     std::size_t& consumed,
                                     std::string& error) {
    consumed = 0;
    error.clear();
    if (bytes.size() < kPeerFrameHeaderSize) {
        return PeerDecodeStatus::Incomplete;
    }

    Reader header(bytes, 0, kPeerFrameHeaderSize);
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t raw_type = 0;
    uint32_t payload_size = 0;
    if (!header.read_u32(magic) || !header.read_u16(version) ||
        !header.read_u16(raw_type) || !header.read_u32(payload_size)) {
        error = "Malformed peer protocol header.";
        return PeerDecodeStatus::Error;
    }
    if (magic != kPeerFrameMagic) {
        error = "Peer protocol magic does not match.";
        return PeerDecodeStatus::Error;
    }
    if (version != kPeerProtocolVersion) {
        error = "Peer protocol version does not match.";
        return PeerDecodeStatus::Error;
    }
    if (!is_known_type(raw_type)) {
        error = "Peer protocol message type is unknown.";
        return PeerDecodeStatus::Error;
    }
    if (payload_size > kPeerMaxPayloadSize) {
        error = "Peer protocol payload exceeds the size limit.";
        return PeerDecodeStatus::Error;
    }

    const std::size_t frame_size = kPeerFrameHeaderSize + static_cast<std::size_t>(payload_size);
    if (bytes.size() < frame_size) {
        return PeerDecodeStatus::Incomplete;
    }

    PeerMessage decoded;
    decoded.type = static_cast<PeerMessageType>(raw_type);
    Reader payload(bytes, kPeerFrameHeaderSize, frame_size);
    bool valid = true;
    switch (decoded.type) {
        case PeerMessageType::Hello:
        case PeerMessageType::Disconnect:
            valid = payload.read_string(decoded.text);
            break;
        case PeerMessageType::Chat:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_string(decoded.text) &&
                    !decoded.text.empty() &&
                    decoded.text.size() <= kPeerChatMaxBytes;
            break;
        case PeerMessageType::Chart:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u64(decoded.chart_hash) && decoded.chart_hash != 0 &&
                    payload.read_u64(decoded.chart_size) &&
                    payload.read_string(decoded.text);
            break;
        case PeerMessageType::Ready: {
            uint8_t value = 0;
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u8(value) && value <= 1;
            decoded.ready = value != 0;
            break;
        }
        case PeerMessageType::Launch:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u64(decoded.chart_hash) && decoded.chart_hash != 0 &&
                    payload.read_u64(decoded.nonce) && decoded.nonce != 0;
            break;
        case PeerMessageType::Loaded:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u64(decoded.nonce) && decoded.nonce != 0;
            break;
        case PeerMessageType::Begin:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u64(decoded.nonce) && decoded.nonce != 0 &&
                    payload.read_u32(decoded.delay_ms);
            break;
        case PeerMessageType::Score:
        case PeerMessageType::FinalScore:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u64(decoded.nonce) && decoded.nonce != 0 &&
                    decode_score(payload, decoded.score);
            if (decoded.type == PeerMessageType::FinalScore) decoded.score.finished = true;
            break;
        case PeerMessageType::Ping:
        case PeerMessageType::Pong:
            valid = payload.read_u64(decoded.nonce);
            break;
        case PeerMessageType::RoundReset:
        case PeerMessageType::RoundCancel:
        case PeerMessageType::RoundCancelAck:
            valid = payload.read_u8(decoded.player_id) &&
                    payload.read_u64(decoded.nonce) && decoded.nonce != 0;
            break;
        case PeerMessageType::LibraryBegin:
        case PeerMessageType::CommonLibraryBegin:
            valid = payload.read_u32(decoded.library_count) &&
                    decoded.library_count <= kPeerLibraryMaxCharts;
            break;
        case PeerMessageType::LibraryChunk:
        case PeerMessageType::CommonLibraryChunk: {
            uint16_t count = 0;
            valid = payload.read_u16(count) && count > 0 &&
                    count <= kPeerLibraryHashesPerChunk;
            decoded.chart_sha256.clear();
            if (valid) decoded.chart_sha256.reserve(count);
            for (uint16_t index = 0; valid && index < count; ++index) {
                std::string sha256;
                valid = payload.read_sha256(sha256);
                if (valid) decoded.chart_sha256.push_back(std::move(sha256));
            }
            break;
        }
        case PeerMessageType::LibraryEnd:
        case PeerMessageType::CommonLibraryEnd:
            break;
        case PeerMessageType::RoomWelcome:
            valid = payload.read_u8(decoded.player_id) &&
                    decoded.player_id > 0 && decoded.player_id <= kPeerMaxPlayers &&
                    payload.read_u8(decoded.leader_id) &&
                    decoded.leader_id > 0 && decoded.leader_id <= kPeerMaxPlayers;
            break;
        case PeerMessageType::RoomRoster: {
            uint8_t active = 0;
            uint8_t count = 0;
            valid = payload.read_u8(decoded.leader_id) &&
                    decoded.leader_id > 0 && decoded.leader_id <= kPeerMaxPlayers &&
                    payload.read_u8(active) && active <= 1 &&
                    payload.read_u64(decoded.nonce) &&
                    payload.read_u8(count) && count > 0 && count <= kPeerMaxPlayers;
            decoded.round_active = active != 0;
            decoded.participants.clear();
            if (valid) decoded.participants.reserve(count);
            for (uint8_t index = 0; valid && index < count; ++index) {
                PeerParticipantWire participant;
                uint8_t flags = 0;
                valid = payload.read_u8(participant.player_id) &&
                        participant.player_id > 0 && participant.player_id <= kPeerMaxPlayers &&
                        payload.read_u8(flags) &&
                        payload.read_string(participant.name);
                participant.ready = (flags & 0x01u) != 0u;
                participant.loaded = (flags & 0x02u) != 0u;
                participant.round_reset = (flags & 0x04u) != 0u;
                if (valid && (flags & 0x08u) != 0u) {
                    valid = payload.read_u64(participant.chart_hash) &&
                            participant.chart_hash != 0 &&
                            payload.read_u64(participant.chart_size) &&
                            payload.read_string(participant.chart_name);
                }
                if (valid) decoded.participants.push_back(std::move(participant));
            }
            break;
        }
        default:
            valid = false;
            break;
    }
    if (!valid || !payload.at_end()) {
        error = "Malformed peer protocol payload.";
        return PeerDecodeStatus::Error;
    }

    message = std::move(decoded);
    consumed = frame_size;
    return PeerDecodeStatus::Complete;
}

ChartFingerprint fingerprint_chart_file(std::string_view path, std::string* error) {
    if (error) error->clear();
    ChartFingerprint result;
    if (path.empty()) {
        set_error(error, "Chart path is empty.");
        return result;
    }

    namespace fs = std::filesystem;
    fs::path chart_path;
    try {
        chart_path = fs::u8path(path.begin(), path.end());
    } catch (const std::exception&) {
        set_error(error, "Chart path is not valid UTF-8.");
        return result;
    }

    std::ifstream stream(chart_path, std::ios::binary);
    if (!stream) {
        set_error(error, "Could not open the selected chart for fingerprinting.");
        return result;
    }

    constexpr uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr uint64_t kFnvPrime = 1099511628211ull;
    uint64_t hash = kFnvOffset;
    uint64_t size = 0;
    std::array<char, 64 * 1024> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<uint8_t>(buffer[static_cast<std::size_t>(i)]);
            hash *= kFnvPrime;
        }
        size += static_cast<uint64_t>(std::max<std::streamsize>(0, count));
    }
    if (!stream.eof()) {
        set_error(error, "Could not read the selected chart completely.");
        return {};
    }

    // Zero is reserved as "no chart" in the wire protocol.
    result.hash = hash == 0 ? 1 : hash;
    result.size = size;
    return result;
}

}  // namespace tenriff::network
