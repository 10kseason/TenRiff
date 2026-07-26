#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::render {

inline constexpr std::uint32_t kLunaSrTargetWidth = 1920;
inline constexpr std::uint32_t kLunaSrTargetHeight = 1080;

struct LunaSrBackgroundFrame {
    std::string source_path;
    std::uint64_t request_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;
};

// LunaSR is intentionally isolated from the render and audio threads. A request
// decodes and evaluates on one worker; callers keep drawing the native bitmap
// until a completed FHD frame is available.
class LunaSrBackgroundUpscaler {
public:
    LunaSrBackgroundUpscaler();
    ~LunaSrBackgroundUpscaler();

    LunaSrBackgroundUpscaler(const LunaSrBackgroundUpscaler&) = delete;
    LunaSrBackgroundUpscaler& operator=(const LunaSrBackgroundUpscaler&) = delete;

    void request(std::string path);
    [[nodiscard]] std::shared_ptr<const LunaSrBackgroundFrame> take_ready();
    void clear();

    [[nodiscard]] static bool should_upscale(std::uint32_t width,
                                             std::uint32_t height,
                                             std::string_view mode);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::render
