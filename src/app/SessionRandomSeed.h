#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>

namespace tenriff::app {

inline uint32_t next_random_session_seed() {
    static std::atomic<uint32_t> sequence{[] {
        const auto now = static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        uint32_t start = static_cast<uint32_t>(now ^ (now >> 32U));
        try {
            std::random_device source;
            start ^= source();
        } catch (...) {
            // The time seed and process sequence also work without OS entropy.
        }
        return start;
    }()};
    // Odd multiplication and xor-shifts form a permutation of all 32-bit values.
    // Even interleaved BMS branch generation cannot repeat a play seed before
    // the 2^32-call wrap. Replays never call this to replace their stored seed.
    uint32_t seed = sequence.fetch_add(1, std::memory_order_relaxed);
    seed = (seed ^ (seed >> 16U)) * 0x7feb352dU;
    seed = (seed ^ (seed >> 15U)) * 0x846ca68bU;
    seed ^= seed >> 16U;
    return seed;
}

}  // namespace tenriff::app
