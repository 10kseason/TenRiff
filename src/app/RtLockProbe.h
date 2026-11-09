#pragma once

#include "timing/HighResClock.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <ostream>

namespace tenriff::app {

// Opt-in diagnostics for the audio callback / HUD thread lock. Disabled unless
// the TENRIFF_RT_PROBE environment variable is set, in which case each
// measured site costs two QPC reads. Never changes gameplay behaviour.
class RtLockProbe {
public:
    struct Series {
        // Upper bounds in ns: 10us, 50us, 100us, 250us, 500us, 1ms, 2ms, 5ms, +inf.
        static constexpr std::array<int64_t, 8> kBounds{
            10'000, 50'000, 100'000, 250'000, 500'000, 1'000'000, 2'000'000, 5'000'000};
        std::array<std::atomic<uint64_t>, 9> buckets{};
        std::atomic<uint64_t> count{0};
        std::atomic<int64_t> total_ns{0};
        std::atomic<int64_t> max_ns{0};

        void add(int64_t ns) {
            if (ns < 0) ns = 0;
            std::size_t index = 0;
            while (index < kBounds.size() && ns >= kBounds[index]) ++index;
            buckets[index].fetch_add(1, std::memory_order_relaxed);
            count.fetch_add(1, std::memory_order_relaxed);
            total_ns.fetch_add(ns, std::memory_order_relaxed);
            int64_t peak = max_ns.load(std::memory_order_relaxed);
            while (ns > peak && !max_ns.compare_exchange_weak(peak, ns, std::memory_order_relaxed)) {}
        }

        void write(std::ostream& out, const char* name) const {
            const uint64_t n = count.load(std::memory_order_relaxed);
            out << "\"" << name << "\":{\"count\":" << n
                << ",\"mean_us\":" << (n ? static_cast<double>(total_ns.load()) / 1000.0 / static_cast<double>(n) : 0.0)
                << ",\"max_us\":" << static_cast<double>(max_ns.load()) / 1000.0
                << ",\"buckets_lt_10us_50us_100us_250us_500us_1ms_2ms_5ms_inf\":[";
            for (std::size_t i = 0; i < buckets.size(); ++i) {
                out << (i ? "," : "") << buckets[i].load(std::memory_order_relaxed);
            }
            out << "]}";
        }
    };

    [[nodiscard]] static bool enabled() {
        static const bool value = [] {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
            const char* raw = std::getenv("TENRIFF_RT_PROBE");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
            return raw && *raw && *raw != '0';
        }();
        return value;
    }

    [[nodiscard]] static int64_t begin() {
        return enabled() ? timing::HighResClock::now_ns() : 0;
    }

    static void end(Series& series, int64_t started_ns) {
        if (started_ns > 0) series.add(timing::HighResClock::now_ns() - started_ns);
    }

    class Scope {
    public:
        explicit Scope(Series& series) : series_(series), started_ns_(begin()) {}
        ~Scope() { end(series_, started_ns_); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        Series& series_;
        int64_t started_ns_;
    };

    void write(std::ostream& out) const {
        out << "{";
        audio_lock_wait.write(out, "audio_lock_wait");
        out << ",";
        audio_callback.write(out, "audio_callback");
        out << ",";
        hud_lock_wait.write(out, "hud_lock_wait");
        out << ",";
        hud_lock_hold.write(out, "hud_lock_hold");
        out << ",";
        foreground_query.write(out, "foreground_query");
        out << "}";
    }

    Series audio_lock_wait;   // audio callback blocked on engine_mutex_
    Series audio_callback;    // whole audio callback
    Series hud_lock_wait;     // HUD thread blocked on engine_mutex_
    Series hud_lock_hold;     // HUD thread holding engine_mutex_
    Series foreground_query;  // GetForegroundWindow-based ownership query
};

}  // namespace tenriff::app
