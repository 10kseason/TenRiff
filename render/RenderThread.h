#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace tenriff::render {

inline constexpr std::size_t kPerformanceGraphSamples = 180;

struct RenderConfig {
    bool vsync = false;
    int fps_limit = 60;
};

struct RenderPerformanceSnapshot {
    bool valid = false;
    std::size_t sample_count = 0;
    std::size_t graph_sample_count = 0;
    uint64_t graph_revision = 0;
    uint64_t metrics_revision = 0;
    double average_frame_ms = 0.0;
    double average_fps = 0.0;
    double max_fps = 0.0;
    double fps_0_1_low = 0.0;
    double fps_0_01_low = 0.0;
    std::array<float, kPerformanceGraphSamples> frame_times_ms{};
};

class PerformanceTracker {
public:
    void reset();
    void record_frame_start_ns(int64_t frame_start_ns);
    [[nodiscard]] const RenderPerformanceSnapshot& snapshot() const { return snapshot_cache_; }

private:
    struct FrameSample {
        double frame_ms = 0.0;
        int64_t frame_start_ns = 0;
    };

    void refresh_graph_snapshot();
    void recompute_snapshot();

    std::deque<FrameSample> frame_history_{};
    std::array<float, kPerformanceGraphSamples> graph_history_ms_{};
    std::size_t graph_history_start_ = 0;
    std::size_t graph_history_count_ = 0;
    RenderPerformanceSnapshot snapshot_cache_{};
    uint64_t next_graph_revision_ = 0;
    uint64_t next_metrics_revision_ = 0;
    int64_t last_graph_refresh_ns_ = 0;
    int64_t last_metrics_refresh_ns_ = 0;
    int64_t last_frame_start_ns_ = 0;
    bool has_last_frame_start_ = false;
};

class RenderThread {
public:
    using RenderCallback = std::function<void()>;
    using RenderShutdownCallback = std::function<void()>;

    RenderThread();
    ~RenderThread();

    RenderThread(const RenderThread&) = delete;
    RenderThread& operator=(const RenderThread&) = delete;

    [[nodiscard]] bool initialize(const RenderConfig& config, RenderCallback callback,
                                  RenderShutdownCallback shutdown_callback = {});
    [[nodiscard]] bool start();
    void stop();
    void shutdown();
    void update_config(const RenderConfig& config);
    [[nodiscard]] RenderPerformanceSnapshot performance_snapshot() const;

    [[nodiscard]] bool is_running() const { return is_running_.load(std::memory_order_acquire); }

private:
    void reset_performance_tracking();
    [[nodiscard]] RenderConfig current_config() const;
    void thread_main();

    RenderConfig config_{};
    RenderCallback callback_;
    RenderShutdownCallback shutdown_callback_;

    std::thread thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    mutable std::mutex config_mutex_{};
    mutable std::mutex performance_mutex_{};
    PerformanceTracker performance_tracker_{};
};

}  // namespace tenriff::render
