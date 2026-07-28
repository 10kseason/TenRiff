#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::render {

inline constexpr std::uint32_t kOnnxUpscaleTargetWidth = 1920;
inline constexpr std::uint32_t kOnnxUpscaleTargetHeight = 1080;
inline constexpr double kOnnxUpscaleMinimumBenchmarkFps = 35.0;

struct OnnxUpscaleFrame {
    std::string source_path;
    std::uint64_t request_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;
};

enum class OnnxUpscaleBenchmarkState {
    Pending,
    Running,
    Passed,
    Failed,
};

struct OnnxUpscaleBenchmarkStatus {
    OnnxUpscaleBenchmarkState state = OnnxUpscaleBenchmarkState::Pending;
    double fps = 0.0;
    std::string detail;
};

// External ONNX inference is intentionally isolated from the render and audio threads. A request
// decodes and evaluates on one worker; callers keep drawing the native bitmap
// until a completed FHD frame is available.
class OnnxBackgroundUpscaler {
public:
    explicit OnnxBackgroundUpscaler(std::string model_path = {});
    ~OnnxBackgroundUpscaler();

    OnnxBackgroundUpscaler(const OnnxBackgroundUpscaler&) = delete;
    OnnxBackgroundUpscaler& operator=(const OnnxBackgroundUpscaler&) = delete;

    void request(std::string path);
    void request_bgra(std::string source_key,
                      std::uint32_t width,
                      std::uint32_t height,
                      const std::vector<std::uint8_t>& bgra);
    [[nodiscard]] std::shared_ptr<const OnnxUpscaleFrame> take_ready();
    void clear();

    [[nodiscard]] static bool should_upscale(std::uint32_t width,
                                             std::uint32_t height,
                                             std::string_view mode);
    [[nodiscard]] static bool meets_performance_gate(double fps);
    [[nodiscard]] OnnxUpscaleBenchmarkStatus benchmark_status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tenriff::render
