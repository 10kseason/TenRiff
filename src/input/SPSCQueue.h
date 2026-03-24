#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace tenriff::input {

// Lock-free single-producer single-consumer ring buffer with a fixed capacity.
template <typename T, std::size_t Capacity>
class SPSCQueue {
public:
    static_assert(Capacity > 1, "Queue capacity must exceed 1 to store elements.");

    bool push(const T& value) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;  // empty
        }
        auto value = buffer_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return value;
    }

    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t capacity() const { return Capacity - 1; }

    void reset() {
        tail_.store(0, std::memory_order_release);
        head_.store(0, std::memory_order_release);
    }

private:
    [[nodiscard]] std::size_t increment(std::size_t index) const { return (index + 1) % Capacity; }

    std::array<T, Capacity> buffer_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}  // namespace tenriff::input
