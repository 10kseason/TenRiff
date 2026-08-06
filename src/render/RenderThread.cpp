#include "render/RenderThread.h"
#include "render/RenderPacing.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#include <immintrin.h>
#endif

#include "timing/HighResClock.h"

namespace tenriff::render {

namespace {

constexpr int64_t kMaxOversleepEstimateNs = 2'000'000LL;
constexpr std::size_t kMaxPerformanceHistory = 24000;
constexpr int64_t kPerformanceWindowNs = 10'000'000'000LL;
constexpr int64_t kGraphRefreshNs = 50'000'000LL;
constexpr int64_t kMetricsRefreshNs = 250'000'000LL;
constexpr std::size_t kMinTailSamples0p1 = 8;
constexpr std::size_t kMinTailSamples0p01 = 4;

#ifdef _WIN32
class HighResolutionWaitableTimer {
public:
    HighResolutionWaitableTimer() {
        timer_ = CreateWaitableTimerExW(nullptr,
                                        nullptr,
                                        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_MODIFY_STATE | SYNCHRONIZE);
        if (!timer_) {
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
        }
    }

    ~HighResolutionWaitableTimer() {
        if (timer_) {
            CloseHandle(timer_);
            timer_ = nullptr;
        }
    }

    HighResolutionWaitableTimer(const HighResolutionWaitableTimer&) = delete;
    HighResolutionWaitableTimer& operator=(const HighResolutionWaitableTimer&) = delete;

    bool wait_for_ns(int64_t duration_ns) const {
        if (!timer_ || duration_ns <= 0) {
            return false;
        }

        LARGE_INTEGER due_time{};
        due_time.QuadPart = -static_cast<LONGLONG>((duration_ns + 99LL) / 100LL);
        if (!SetWaitableTimerEx(timer_, &due_time, 0, nullptr, nullptr, nullptr, 0)) {
            return false;
        }
        return WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0;
    }

private:
    HANDLE timer_ = nullptr;
};
#endif

void cpu_relax() {
#ifdef _WIN32
    _mm_pause();
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

void update_oversleep_estimate(int64_t target_wake_ns,
                               int64_t actual_wake_ns,
                               int64_t* oversleep_estimate_ns) {
    if (!oversleep_estimate_ns) {
        return;
    }

    const int64_t overshoot_ns = (std::max)(int64_t{0}, actual_wake_ns - target_wake_ns);
    const int64_t clamped = std::clamp(overshoot_ns, int64_t{0}, kMaxOversleepEstimateNs);
    *oversleep_estimate_ns = std::clamp(((*oversleep_estimate_ns) * 7 + clamped) / 8,
                                        int64_t{0},
                                        kMaxOversleepEstimateNs);
}

template <typename Waiter>
void precise_wait_until_ns(int64_t deadline_ns,
                           int64_t* oversleep_estimate_ns,
                           const RenderWaitPolicy& wait_policy,
                           const Waiter& waiter) {
    for (;;) {
        const int64_t now_ns = timing::HighResClock::now_ns();
        if (now_ns >= deadline_ns) {
            return;
        }

        const int64_t remaining_ns = deadline_ns - now_ns;
        const int64_t oversleep_ns = oversleep_estimate_ns ? *oversleep_estimate_ns : 0;
        if (remaining_ns > wait_policy.coarse_sleep_min_ns + wait_policy.spin_guard_ns + oversleep_ns) {
            const int64_t sleep_ns = remaining_ns - wait_policy.spin_guard_ns - oversleep_ns;
            const int64_t target_wake_ns = now_ns + sleep_ns;
            if (waiter(sleep_ns)) {
                update_oversleep_estimate(target_wake_ns, timing::HighResClock::now_ns(), oversleep_estimate_ns);
                continue;
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
            update_oversleep_estimate(target_wake_ns, timing::HighResClock::now_ns(), oversleep_estimate_ns);
            continue;
        }

        if (remaining_ns > wait_policy.yield_threshold_ns) {
            std::this_thread::yield();
            continue;
        }

        while (timing::HighResClock::now_ns() < deadline_ns) {
            cpu_relax();
        }
        return;
    }
}

float median3(float a, float b, float c) {
    if (a > b) {
        std::swap(a, b);
    }
    if (b > c) {
        std::swap(b, c);
    }
    if (a > b) {
        std::swap(a, b);
    }
    return b;
}

std::vector<float> smooth_graph_samples(const std::array<float, kPerformanceGraphSamples>& history,
                                        std::size_t start,
                                        std::size_t count) {
    std::vector<float> ordered;
    ordered.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t source_index = (start + i) % kPerformanceGraphSamples;
        ordered.push_back(history[source_index]);
    }
    if (ordered.size() < 3u) {
        return ordered;
    }

    std::vector<float> median_filtered = ordered;
    for (std::size_t i = 1; i + 1 < ordered.size(); ++i) {
        median_filtered[i] = median3(ordered[i - 1], ordered[i], ordered[i + 1]);
    }

    std::vector<float> smoothed = median_filtered;
    for (std::size_t i = 1; i + 1 < median_filtered.size(); ++i) {
        smoothed[i] = (median_filtered[i - 1] + median_filtered[i] * 2.0f + median_filtered[i + 1]) * 0.25f;
    }
    return smoothed;
}

}  // namespace

void PerformanceTracker::reset() {
    frame_history_.clear();
    graph_history_ms_.fill(0.0f);
    graph_history_start_ = 0;
    graph_history_count_ = 0;
    snapshot_cache_ = {};
    next_graph_revision_ = 0;
    next_metrics_revision_ = 0;
    last_graph_refresh_ns_ = 0;
    last_metrics_refresh_ns_ = 0;
    last_frame_start_ns_ = 0;
    has_last_frame_start_ = false;
}

void PerformanceTracker::record_frame_start_ns(int64_t frame_start_ns) {
    if (frame_start_ns < 0) {
        return;
    }

    if (!has_last_frame_start_) {
        last_frame_start_ns_ = frame_start_ns;
        has_last_frame_start_ = true;
        return;
    }

    const int64_t frame_interval_ns = frame_start_ns - last_frame_start_ns_;
    last_frame_start_ns_ = frame_start_ns;
    if (frame_interval_ns <= 0) {
        return;
    }

    const double frame_ms = static_cast<double>(frame_interval_ns) / 1'000'000.0;

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
        snapshot_cache_ = {};
        return;
    }

    snapshot_cache_.valid = true;
    snapshot_cache_.sample_count = frame_history_.size();
    snapshot_cache_.graph_sample_count = graph_history_count_;

    if (last_graph_refresh_ns_ == 0 || frame_start_ns - last_graph_refresh_ns_ >= kGraphRefreshNs) {
        refresh_graph_snapshot();
        last_graph_refresh_ns_ = frame_start_ns;
    }

    if (last_metrics_refresh_ns_ == 0 || frame_start_ns - last_metrics_refresh_ns_ >= kMetricsRefreshNs) {
        recompute_snapshot();
        last_metrics_refresh_ns_ = frame_start_ns;
    }
}

void PerformanceTracker::refresh_graph_snapshot() {
    snapshot_cache_.graph_sample_count = graph_history_count_;
    snapshot_cache_.frame_times_ms.fill(0.0f);
    const auto smoothed = smooth_graph_samples(graph_history_ms_, graph_history_start_, graph_history_count_);
    for (std::size_t i = 0; i < smoothed.size(); ++i) {
        snapshot_cache_.frame_times_ms[i] = smoothed[i];
    }
    snapshot_cache_.graph_revision = ++next_graph_revision_;
}

void PerformanceTracker::recompute_snapshot() {
    RenderPerformanceSnapshot snapshot = snapshot_cache_;
    snapshot.sample_count = frame_history_.size();
    snapshot.valid = snapshot.sample_count > 0;
    if (!snapshot.valid) {
        snapshot_cache_ = {};
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

    snapshot_cache_ = std::move(snapshot);
}

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
    return performance_tracker_.snapshot();
}

void RenderThread::reset_performance_tracking() {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    performance_tracker_.reset();
}

RenderConfig RenderThread::current_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

void RenderThread::thread_main() {
    int64_t next_tick_ns = timing::HighResClock::now_ns();
    int64_t oversleep_estimate_ns = 0;
#ifdef _WIN32
    HighResolutionWaitableTimer waitable_timer;
#endif

    while (!should_stop_.load(std::memory_order_acquire)) {
        const RenderConfig config = current_config();
        const bool unlimited = should_use_unlimited_render_pacing(config.vsync, config.fps_limit);
        int target_fps = config.fps_limit;
        if (config.vsync && target_fps <= 0) {
            target_fps = 60;
        }
        if (!unlimited && target_fps <= 0) {
            target_fps = 60;
        }
        const RenderWaitPolicy wait_policy =
            unlimited ? RenderWaitPolicy{} : render_wait_policy(config.vsync, target_fps);
        const int64_t frame_interval_ns =
            unlimited ? 0 : 1'000'000'000LL / target_fps;

        const int64_t now_ns = timing::HighResClock::now_ns();
        if (!unlimited && now_ns < next_tick_ns) {
            precise_wait_until_ns(
                next_tick_ns,
                &oversleep_estimate_ns,
                wait_policy,
#ifdef _WIN32
                [&waitable_timer](int64_t duration_ns) { return waitable_timer.wait_for_ns(duration_ns); }
#else
                [](int64_t) { return false; }
#endif
            );
            continue;
        }

        const int64_t frame_start_ns = now_ns;
        {
            std::lock_guard<std::mutex> lock(performance_mutex_);
            performance_tracker_.record_frame_start_ns(frame_start_ns);
        }

        callback_();

        const int64_t after_callback_ns = timing::HighResClock::now_ns();
        if (unlimited) {
            next_tick_ns = after_callback_ns;
            oversleep_estimate_ns = 0;
        } else {
            next_tick_ns = advance_frame_deadline_ns(next_tick_ns, frame_interval_ns, after_callback_ns);
        }
    }

    if (shutdown_callback_) {
        shutdown_callback_();
    }
}

}  // namespace tenriff::render
