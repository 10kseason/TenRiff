#include "render/RenderThread.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

#include "timing/HighResClock.h"

namespace tenriff::render {

namespace {

constexpr int64_t kSleepThresholdNs = 2'000'000LL;
constexpr int64_t kSleepGuardNs = 1'000'000LL;
constexpr int64_t kYieldThresholdNs = 100'000LL;
constexpr std::size_t kMaxPerformanceHistory = 24000;
constexpr int64_t kPerformanceWindowNs = 10'000'000'000LL;
constexpr int64_t kGraphRefreshNs = 50'000'000LL;
constexpr int64_t kMetricsRefreshNs = 250'000'000LL;
constexpr std::size_t kMinTailSamples0p1 = 8;
constexpr std::size_t kMinTailSamples0p01 = 4;

}  // namespace

RenderThread::RenderThread() = default;

RenderThread::~RenderThread() {
    shutdown();
}

bool RenderThread::initialize(const RenderConfig& config, RenderCallback callback,
                              RenderShutdownCallback shutdown_callback) {
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
    }
    callback_ = std::move(callback);
    shutdown_callback_ = std::move(shutdown_callback);
    reset_performance_tracking();
    return static_cast<bool>(callback_);
}

bool RenderThread::start() {
    if (is_running_.load(std::memory_order_acquire)) {
        return true;
    }
    if (!callback_) {
        return false;
    }

    should_stop_.store(false, std::memory_order_release);
    is_running_.store(true, std::memory_order_release);
    reset_performance_tracking();
    thread_ = std::thread(&RenderThread::thread_main, this);
    return true;
}

void RenderThread::stop() {
    if (!is_running_.load(std::memory_order_acquire)) {
        return;
    }

    should_stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    is_running_.store(false, std::memory_order_release);
}

void RenderThread::shutdown() {
    stop();
    callback_ = nullptr;
    shutdown_callback_ = nullptr;
}

void RenderThread::update_config(const RenderConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

RenderPerformanceSnapshot RenderThread::performance_snapshot() const {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    return performance_snapshot_cache_;
}

void RenderThread::reset_performance_tracking() {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    frame_history_.clear();
    graph_history_ms_.fill(0.0f);
    graph_history_start_ = 0;
    graph_history_count_ = 0;
    performance_snapshot_cache_ = {};
    next_graph_revision_ = 0;
    next_metrics_revision_ = 0;
    last_graph_refresh_ns_ = 0;
    last_metrics_refresh_ns_ = 0;
}

void RenderThread::record_frame_interval(int64_t frame_interval_ns, int64_t frame_start_ns) {
    if (frame_interval_ns <= 0) {
        return;
    }

    const double frame_ms = static_cast<double>(frame_interval_ns) / 1'000'000.0;

    std::lock_guard<std::mutex> lock(performance_mutex_);
    frame_history_.push_back(FrameSample{frame_ms, frame_start_ns});
    while (!frame_history_.empty() &&
           (frame_history_.size() > kMaxPerformanceHistory ||
            frame_history_.front().frame_start_ns < frame_start_ns - kPerformanceWindowNs)) {
        frame_history_.pop_front();
    }

    const std::size_t graph_index = (graph_history_start_ + graph_history_count_) % kPerformanceGraphSamples;
    if (graph_history_count_ < kPerformanceGraphSamples) {
        graph_history_ms_[graph_index] = static_cast<float>(frame_ms);
        ++graph_history_count_;
    } else {
        graph_history_ms_[graph_history_start_] = static_cast<float>(frame_ms);
        graph_history_start_ = (graph_history_start_ + 1) % kPerformanceGraphSamples;
    }

    if (frame_history_.empty()) {
        performance_snapshot_cache_ = {};
        return;
    }

    performance_snapshot_cache_.valid = true;
    performance_snapshot_cache_.sample_count = frame_history_.size();
    performance_snapshot_cache_.graph_sample_count = graph_history_count_;

    if (last_graph_refresh_ns_ == 0 || frame_start_ns - last_graph_refresh_ns_ >= kGraphRefreshNs) {
        refresh_graph_snapshot_locked();
        last_graph_refresh_ns_ = frame_start_ns;
    }

    if (last_metrics_refresh_ns_ == 0 || frame_start_ns - last_metrics_refresh_ns_ >= kMetricsRefreshNs) {
        recompute_performance_snapshot_locked();
        last_metrics_refresh_ns_ = frame_start_ns;
    }
}

void RenderThread::refresh_graph_snapshot_locked() {
    performance_snapshot_cache_.graph_sample_count = graph_history_count_;
    performance_snapshot_cache_.frame_times_ms.fill(0.0f);
    for (std::size_t i = 0; i < graph_history_count_; ++i) {
        const std::size_t source_index = (graph_history_start_ + i) % kPerformanceGraphSamples;
        performance_snapshot_cache_.frame_times_ms[i] = graph_history_ms_[source_index];
    }
    performance_snapshot_cache_.graph_revision = ++next_graph_revision_;
}

void RenderThread::recompute_performance_snapshot_locked() {
    RenderPerformanceSnapshot snapshot = performance_snapshot_cache_;
    snapshot.sample_count = frame_history_.size();
    snapshot.valid = snapshot.sample_count > 0;
    if (!snapshot.valid) {
        performance_snapshot_cache_ = {};
        return;
    }

    double sum_ms = 0.0;
    std::vector<double> sorted_samples;
    sorted_samples.reserve(frame_history_.size());
    for (const FrameSample& sample : frame_history_) {
        sum_ms += sample.frame_ms;
        sorted_samples.push_back(sample.frame_ms);
    }

    snapshot.average_frame_ms = sum_ms / static_cast<double>(snapshot.sample_count);
    if (snapshot.average_frame_ms > 0.0) {
        snapshot.average_fps = 1000.0 / snapshot.average_frame_ms;
    }

    const auto min_it = std::min_element(sorted_samples.begin(), sorted_samples.end());
    if (min_it != sorted_samples.end() && *min_it > 0.0) {
        snapshot.max_fps = 1000.0 / *min_it;
    }

    std::sort(sorted_samples.begin(), sorted_samples.end());

    const auto worst_tail_average_fps = [&](double portion, std::size_t min_tail_samples) {
        if (sorted_samples.empty()) {
            return 0.0;
        }
        const double count_d = std::ceil(static_cast<double>(sorted_samples.size()) * portion);
        const std::size_t tail_count = std::min<std::size_t>(
            sorted_samples.size(),
            std::max<std::size_t>(min_tail_samples, std::max<std::size_t>(1, static_cast<std::size_t>(count_d))));
        const auto begin = sorted_samples.end() - static_cast<std::ptrdiff_t>(tail_count);
        const double tail_sum = std::accumulate(begin, sorted_samples.end(), 0.0);
        const double tail_avg_ms = tail_sum / static_cast<double>(tail_count);
        return (tail_avg_ms > 0.0) ? (1000.0 / tail_avg_ms) : 0.0;
    };

    snapshot.fps_0_1_low = worst_tail_average_fps(0.001, kMinTailSamples0p1);
    snapshot.fps_0_01_low = worst_tail_average_fps(0.0001, kMinTailSamples0p01);
    snapshot.metrics_revision = ++next_metrics_revision_;

    performance_snapshot_cache_ = std::move(snapshot);
}

RenderConfig RenderThread::current_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

void RenderThread::thread_main() {
    int64_t next_tick_ns = timing::HighResClock::now_ns();
    int64_t last_frame_start_ns = 0;

    while (!should_stop_.load(std::memory_order_acquire)) {
        const RenderConfig config = current_config();
        int target_fps = config.fps_limit;
        if (config.vsync && target_fps <= 0) {
            target_fps = 60;
        }
        if (target_fps <= 0) {
            target_fps = 60;
        }
        const int64_t frame_interval_ns = 1'000'000'000LL / target_fps;

        const int64_t now_ns = timing::HighResClock::now_ns();
        if (now_ns < next_tick_ns) {
            const int64_t remaining_ns = next_tick_ns - now_ns;
            if (remaining_ns > kSleepThresholdNs) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(remaining_ns - kSleepGuardNs));
            } else if (remaining_ns > kYieldThresholdNs) {
                std::this_thread::yield();
            }
            continue;
        }

        const int64_t frame_start_ns = now_ns;
        if (last_frame_start_ns > 0) {
            record_frame_interval(frame_start_ns - last_frame_start_ns, frame_start_ns);
        }
        last_frame_start_ns = frame_start_ns;

        callback_();

        const int64_t after_callback_ns = timing::HighResClock::now_ns();
        next_tick_ns += frame_interval_ns;
        if (next_tick_ns <= after_callback_ns) {
            next_tick_ns = after_callback_ns + frame_interval_ns;
        }
    }

    if (shutdown_callback_) {
        shutdown_callback_();
    }
}

}  // namespace tenriff::render
