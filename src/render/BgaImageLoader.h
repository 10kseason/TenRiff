#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tenriff::render {

struct BgaImageFrame {
    std::string source_path;
    std::uint64_t request_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;
};

// Static BGA images are decoded away from the render thread. Requests are
// latest-only so dense BMP animations skip stale work instead of stalling play.
class BgaImageLoader {
public:
    BgaImageLoader();
    ~BgaImageLoader();

    BgaImageLoader(const BgaImageLoader&) = delete;
    BgaImageLoader& operator=(const BgaImageLoader&) = delete;

    void request(std::string path);
    [[nodiscard]] std::shared_ptr<const BgaImageFrame> take_ready();
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::render
