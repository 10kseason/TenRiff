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

    [[nodiscard]] bool at_end() const { return cursor_ == limit_; }

private:
    const std::vector<uint8_t>& bytes_;
    std::size_t cursor_ = 0;
    std::size_t limit_ = 0;
};

bool is_known_type(uint16_t raw) {
    return raw >= static_cast<uint16_t>(PeerMessageType::Hello) &&
           raw <= static_cast<uint16_t>(PeerMessageType::RoundCancelAck);
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
        case PeerMessageType::Chart:
            if (message.chart_hash == 0) {
                set_error(error, "Chart messages require a non-zero fingerprint.");
                return {};
            }
            append_u64(payload, message.chart_hash);
            append_u64(payload, message.chart_size);
            if (!append_string(payload, message.text, error)) return {};
            break;
        case PeerMessageType::Ready:
            append_u8(payload, message.ready ? 1u : 0u);
            break;
        case PeerMessageType::Launch:
            if (message.chart_hash == 0 || message.nonce == 0) {
                set_error(error, "Launch messages require a non-zero fingerprint and round nonce.");
                return {};
            }
            append_u64(payload, message.chart_hash);
            append_u64(payload, message.nonce);
            break;
        case PeerMessageType::Loaded:
            if (message.nonce == 0) {
                set_error(error, "Loaded messages require a non-zero round nonce.");
                return {};
            }
            append_u64(payload, message.nonce);
            break;
        case PeerMessageType::Begin:
            if (message.nonce == 0) {
                set_error(error, "Begin messages require a non-zero round nonce.");
                return {};
            }
            append_u64(payload, message.nonce);
            append_u32(payload, message.delay_ms);
            break;
        case PeerMessageType::Score:
        case PeerMessageType::FinalScore: {
            if (message.nonce == 0) {
                set_error(error, "Score messages require a non-zero round nonce.");
                return {};
            }
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
            append_u64(payload, message.nonce);
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
        case PeerMessageType::Chart:
            valid = payload.read_u64(decoded.chart_hash) &&
                    payload.read_u64(decoded.chart_size) &&
                    decoded.chart_hash != 0 &&
                    payload.read_string(decoded.text);
            break;
        case PeerMessageType::Ready: {
            uint8_t value = 0;
            valid = payload.read_u8(value) && value <= 1;
            decoded.ready = value != 0;
            break;
        }
        case PeerMessageType::Launch:
            valid = payload.read_u64(decoded.chart_hash) && decoded.chart_hash != 0 &&
                    payload.read_u64(decoded.nonce) && decoded.nonce != 0;
            break;
        case PeerMessageType::Loaded:
            valid = payload.read_u64(decoded.nonce) && decoded.nonce != 0;
            break;
        case PeerMessageType::Begin:
            valid = payload.read_u64(decoded.nonce) && decoded.nonce != 0 &&
                    payload.read_u32(decoded.delay_ms);
            break;
        case PeerMessageType::Score:
        case PeerMessageType::FinalScore:
            valid = payload.read_u64(decoded.nonce) && decoded.nonce != 0 &&
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
            valid = payload.read_u64(decoded.nonce) && decoded.nonce != 0;
            break;
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
