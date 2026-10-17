#pragma once
#include <atomic>
#include <cstdint>

namespace tenriff::input {
// Win32 message timestamps are 32-bit uptime milliseconds. Compare their
// original enqueue time, not the later time our render thread dispatches them.
class ModalInputFence {
public:
    void close(uint64_t tick) { closed_tick_.store(tick, std::memory_order_release); }
    bool rejects(uint32_t message_tick, uint64_t now_tick) const {
        const uint64_t closed = closed_tick_.load(std::memory_order_acquire);
        if (!closed) return false;
        const uint32_t age = static_cast<uint32_t>(now_tick) - message_tick;
        return age < 0x80000000u && now_tick >= age && now_tick - age <= closed;
    }
private:
    std::atomic<uint64_t> closed_tick_{0};
};
inline ModalInputFence native_modal_input_fence;
} // namespace tenriff::input
