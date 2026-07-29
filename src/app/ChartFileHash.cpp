#include "app/ChartFileHash.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace tenriff::app {

namespace {

constexpr std::uint32_t rotate_left(std::uint32_t value, std::uint32_t shift) noexcept {
    return (value << shift) | (value >> (32u - shift));
}

constexpr std::uint32_t rotate_right(std::uint32_t value, std::uint32_t shift) noexcept {
    return (value >> shift) | (value << (32u - shift));
}

class Md5Digest {
public:
    void update(const std::uint8_t* data, std::size_t size) {
        total_bytes_ += static_cast<std::uint64_t>(size);
        while (size > 0) {
            const std::size_t copied = std::min(size, buffer_.size() - buffer_size_);
            std::copy_n(data, copied, buffer_.data() + buffer_size_);
            buffer_size_ += copied;
            data += copied;
            size -= copied;
            if (buffer_size_ == buffer_.size()) {
                transform(buffer_.data());
                buffer_size_ = 0;
            }
        }
    }

    [[nodiscard]] std::array<std::uint8_t, 16> finish() const {
        Md5Digest digest = *this;
        const std::uint64_t bit_length = digest.total_bytes_ * 8u;

        digest.buffer_[digest.buffer_size_++] = 0x80u;
        if (digest.buffer_size_ > 56u) {
            std::fill(digest.buffer_.begin() + static_cast<std::ptrdiff_t>(digest.buffer_size_),
                      digest.buffer_.end(), 0u);
            digest.transform(digest.buffer_.data());
            digest.buffer_size_ = 0;
        }
        std::fill(digest.buffer_.begin() + static_cast<std::ptrdiff_t>(digest.buffer_size_),
                  digest.buffer_.begin() + 56, 0u);
        for (std::size_t i = 0; i < 8; ++i) {
            digest.buffer_[56u + i] = static_cast<std::uint8_t>(bit_length >> (i * 8u));
        }
        digest.transform(digest.buffer_.data());

        std::array<std::uint8_t, 16> result{};
        for (std::size_t word = 0; word < digest.state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                result[word * 4u + byte] =
                    static_cast<std::uint8_t>(digest.state_[word] >> (byte * 8u));
            }
        }
        return result;
    }

private:
    void transform(const std::uint8_t* block) {
        static constexpr std::array<std::uint32_t, 64> kShift = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
        };
        static constexpr std::array<std::uint32_t, 64> kConstant = {
            0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
            0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
            0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
            0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
            0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
            0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
            0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
            0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
            0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
            0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
            0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
            0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
            0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
            0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
            0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
            0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
        };

        std::array<std::uint32_t, 16> words{};
        for (std::size_t i = 0; i < words.size(); ++i) {
            const std::size_t offset = i * 4u;
            words[i] = static_cast<std::uint32_t>(block[offset]) |
                       (static_cast<std::uint32_t>(block[offset + 1u]) << 8u) |
                       (static_cast<std::uint32_t>(block[offset + 2u]) << 16u) |
                       (static_cast<std::uint32_t>(block[offset + 3u]) << 24u);
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        for (std::size_t i = 0; i < 64; ++i) {
            std::uint32_t function = 0;
            std::size_t word = 0;
            if (i < 16) {
                function = (b & c) | (~b & d);
                word = i;
            } else if (i < 32) {
                function = (d & b) | (~d & c);
                word = (5u * i + 1u) % 16u;
            } else if (i < 48) {
                function = b ^ c ^ d;
                word = (3u * i + 5u) % 16u;
            } else {
                function = c ^ (b | ~d);
                word = (7u * i) % 16u;
            }

            const std::uint32_t previous_d = d;
            d = c;
            c = b;
            b += rotate_left(a + function + kConstant[i] + words[word], kShift[i]);
            a = previous_d;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
    }

    std::array<std::uint32_t, 4> state_ = {
        0x67452301u,
        0xefcdab89u,
        0x98badcfeu,
        0x10325476u,
    };
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t total_bytes_ = 0;
};

class Sha256Digest {
public:
    void update(const std::uint8_t* data, std::size_t size) {
        total_bytes_ += static_cast<std::uint64_t>(size);
        while (size > 0) {
            const std::size_t copied = std::min(size, buffer_.size() - buffer_size_);
            std::copy_n(data, copied, buffer_.data() + buffer_size_);
            buffer_size_ += copied;
            data += copied;
            size -= copied;
            if (buffer_size_ == buffer_.size()) {
                transform(buffer_.data());
                buffer_size_ = 0;
            }
        }
    }

    [[nodiscard]] std::array<std::uint8_t, 32> finish() const {
        Sha256Digest digest = *this;
        const std::uint64_t bit_length = digest.total_bytes_ * 8u;

        digest.buffer_[digest.buffer_size_++] = 0x80u;
        if (digest.buffer_size_ > 56u) {
            std::fill(digest.buffer_.begin() + static_cast<std::ptrdiff_t>(digest.buffer_size_),
                      digest.buffer_.end(), 0u);
            digest.transform(digest.buffer_.data());
            digest.buffer_size_ = 0;
        }
        std::fill(digest.buffer_.begin() + static_cast<std::ptrdiff_t>(digest.buffer_size_),
                  digest.buffer_.begin() + 56, 0u);
        for (std::size_t i = 0; i < 8; ++i) {
            digest.buffer_[63u - i] = static_cast<std::uint8_t>(bit_length >> (i * 8u));
        }
        digest.transform(digest.buffer_.data());

        std::array<std::uint8_t, 32> result{};
        for (std::size_t word = 0; word < digest.state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                result[word * 4u + byte] =
                    static_cast<std::uint8_t>(digest.state_[word] >> ((3u - byte) * 8u));
            }
        }
        return result;
    }

private:
    void transform(const std::uint8_t* block) {
        static constexpr std::array<std::uint32_t, 64> kConstant = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
        };

        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t offset = i * 4u;
            words[i] = (static_cast<std::uint32_t>(block[offset]) << 24u) |
                       (static_cast<std::uint32_t>(block[offset + 1u]) << 16u) |
                       (static_cast<std::uint32_t>(block[offset + 2u]) << 8u) |
                       static_cast<std::uint32_t>(block[offset + 3u]);
        }
        for (std::size_t i = 16; i < words.size(); ++i) {
            const std::uint32_t sigma0 = rotate_right(words[i - 15u], 7u) ^
                                         rotate_right(words[i - 15u], 18u) ^
                                         (words[i - 15u] >> 3u);
            const std::uint32_t sigma1 = rotate_right(words[i - 2u], 17u) ^
                                         rotate_right(words[i - 2u], 19u) ^
                                         (words[i - 2u] >> 10u);
            words[i] = words[i - 16u] + sigma0 + words[i - 7u] + sigma1;
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];
        for (std::size_t i = 0; i < words.size(); ++i) {
            const std::uint32_t sum1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + sum1 + choose + kConstant[i] + words[i];
            const std::uint32_t sum0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_ = {
        0x6a09e667u,
        0xbb67ae85u,
        0x3c6ef372u,
        0xa54ff53au,
        0x510e527fu,
        0x9b05688cu,
        0x1f83d9abu,
        0x5be0cd19u,
    };
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t total_bytes_ = 0;
};

template <std::size_t Size>
std::string to_hex(const std::array<std::uint8_t, Size>& bytes) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0');
    for (const std::uint8_t byte : bytes) {
        out << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return out.str();
}

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

}  // namespace

ChartFileHashes hash_chart_file(const std::filesystem::path& path, std::string* error) {
    if (error) {
        error->clear();
    }
    if (path.empty()) {
        set_error(error, "Chart path is empty.");
        return {};
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        set_error(error, "Could not open the chart file for hashing.");
        return {};
    }

    Md5Digest md5;
    Sha256Digest sha256;
    std::uint64_t total_size = 0;
    std::array<char, 64u * 1024u> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        if (count <= 0) {
            continue;
        }
        const auto chunk_size = static_cast<std::uint64_t>(count);
        if (total_size > std::numeric_limits<std::uint64_t>::max() - chunk_size) {
            set_error(error, "Chart file is too large to hash.");
            return {};
        }
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(buffer.data());
        md5.update(bytes, static_cast<std::size_t>(count));
        sha256.update(bytes, static_cast<std::size_t>(count));
        total_size += chunk_size;
    }
    if (!stream.eof()) {
        set_error(error, "Could not read the chart file completely.");
        return {};
    }

    ChartFileHashes result;
    result.md5 = to_hex(md5.finish());
    result.sha256 = to_hex(sha256.finish());
    result.size = total_size;
    return result;
}

ChartFileHashes hash_chart_file_utf8(std::string_view path, std::string* error) {
    if (path.empty()) {
        if (error) {
            *error = "Chart path is empty.";
        }
        return {};
    }
    try {
        return hash_chart_file(std::filesystem::u8path(path.begin(), path.end()), error);
    } catch (...) {
        if (error) {
            *error = "Chart path is not valid UTF-8.";
        }
        return {};
    }
}

}  // namespace tenriff::app
