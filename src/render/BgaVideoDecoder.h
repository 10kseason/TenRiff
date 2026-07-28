#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::render {

struct BgaVideoFrame {
    std::string source_path;
    std::uint64_t request_id = 0;
    std::int64_t timestamp_100ns = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;
};

// Media Foundation decoding stays off the render thread. Requests are latest-only,
// so a slow decoder drops obsolete positions instead of building a gameplay backlog.
class BgaVideoDecoder {
public:
    BgaVideoDecoder();
    ~BgaVideoDecoder();

    BgaVideoDecoder(const BgaVideoDecoder&) = delete;
    BgaVideoDecoder& operator=(const BgaVideoDecoder&) = delete;

    void request(std::string path, double position_seconds);
    [[nodiscard]] std::shared_ptr<const BgaVideoFrame> take_ready();
    void clear();

    [[nodiscard]] static bool is_supported_video_path(std::string_view path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::render
