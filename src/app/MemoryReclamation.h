#pragma once

#include <cstddef>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <malloc.h>
#endif

namespace tenriff::app {

// Serialize a forced trim with session startup, not with audio/input callbacks.
// Requests coalesce while a session (including pause/loading) owns a deferral.
// The callback must not reenter the gate. Releasing a deferral never collects;
// a menu or a later indexing request supplies the safe collection point.
class MemoryReclamationGate {
public:
    using Collector = void (*)();
    explicit MemoryReclamationGate(Collector collector) : collector_(collector) {}

    void defer() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++deferrals_;
    }
    void allow() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (deferrals_ != 0) --deferrals_;
    }
    void request() {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = true;
        collect_locked();
    }
    void collect_if_allowed() {
        std::lock_guard<std::mutex> lock(mutex_);
        collect_locked();
    }
private:
    void collect_locked() {
        if (pending_ && deferrals_ == 0) {
            pending_ = false;
            collector_();
        }
    }
    std::mutex mutex_;
    Collector collector_;
    std::size_t deferrals_ = 0;
    bool pending_ = false;
};

inline MemoryReclamationGate& process_memory_reclamation() {
    static MemoryReclamationGate gate([] {
#ifdef _WIN32
        _heapmin();
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
    });
    return gate;
}

class GameplayMemoryDeferral {
public:
    explicit GameplayMemoryDeferral(MemoryReclamationGate& gate = process_memory_reclamation()) : gate_(gate) {}
    ~GameplayMemoryDeferral() { end(); }
    GameplayMemoryDeferral(const GameplayMemoryDeferral&) = delete;
    GameplayMemoryDeferral& operator=(const GameplayMemoryDeferral&) = delete;
    void begin() {
        if (!active_) { gate_.defer(); active_ = true; }
    }
    void end() {
        if (active_) { active_ = false; gate_.allow(); }
    }
private:
    MemoryReclamationGate& gate_;
    bool active_ = false;
};

}  // namespace tenriff::app
