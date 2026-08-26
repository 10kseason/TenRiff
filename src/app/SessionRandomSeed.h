#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>

namespace tenriff::app {

inline bool random_mode_uses_fresh_session_seed(std::string_view token) {
    std::string normalized(token);
    for (char& byte : normalized) {
        if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte + ('a' - 'A'));
    }
    return normalized == "fr" || normalized == "fullrandom" ||
           normalized == "full_random" || normalized == "frns" ||
           normalized == "fr_no_scratch" || normalized == "random_no_scratch";
}

inline uint32_t next_random_session_seed() {
    static std::atomic<uint64_t> sequence{0};
    static std::atomic<uint32_t> previous{0};
    uint64_t material = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    material ^= ++sequence * 0x9e3779b97f4a7c15ULL;
    try {
        std::random_device source;
        material ^= (static_cast<uint64_t>(source()) << 32U) ^ source();
    } catch (...) {
        // Time plus the monotonic process sequence still prevents the immediate
        // retry from reusing the same layout when OS entropy is unavailable.
    }
    material += 0x9e3779b97f4a7c15ULL;
    material = (material ^ (material >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    material = (material ^ (material >> 27U)) * 0x94d049bb133111ebULL;
    material ^= material >> 31U;
    uint32_t seed = static_cast<uint32_t>(material ^ (material >> 32U));
    const uint32_t last = previous.load(std::memory_order_relaxed);
    if (seed == last) seed ^= 0xa511e9b3U;
    previous.store(seed, std::memory_order_relaxed);
    return seed;
}

}  // namespace tenriff::app
