#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace tenriff::render {

struct BgaVideoFrame;

// Persistent FFmpeg image-pipe fallback for video containers/codecs that the
// local Media Foundation installation cannot decode (notably MPEG-1 MPG).
class BgaVideoFfmpegReader {
public:
    BgaVideoFfmpegReader();
    ~BgaVideoFfmpegReader();

    BgaVideoFfmpegReader(const BgaVideoFfmpegReader&) = delete;
    BgaVideoFfmpegReader& operator=(const BgaVideoFfmpegReader&) = delete;

    bool open(std::string_view path);
    std::shared_ptr<BgaVideoFrame> read_at(std::int64_t target_100ns);
    void close();

    [[nodiscard]] const std::string& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::render
