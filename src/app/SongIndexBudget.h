#pragma once

#include <algorithm>
#include <atomic>
#include <exception>
#include <thread>
#include <vector>
#include <cstddef>
#include <cstdint>

#include "app/SongIndex.h"

namespace tenriff::app {

struct SongIndexMemorySnapshot {
    std::uint64_t available_bytes = 0;
    std::uint64_t total_bytes = 0;
};

SongIndexMemorySnapshot query_song_index_memory_snapshot();

struct SongIndexWorkBudget {
    std::size_t worker_count = 1;
    std::size_t batch_size = 1;
    std::uint64_t reserve_bytes = 0;
    std::uint64_t headroom_bytes = 0;
};

inline SongIndexWorkBudget choose_song_index_work_budget(SongIndexProfile profile,
                                                         unsigned reported_threads,
                                                         std::size_t remaining_items,
                                                         SongIndexMemorySnapshot memory) {
    const bool fast_profile = profile == SongIndexProfile::Fast;
    const std::uint64_t min_reserve_bytes =
        fast_profile ? (1536ull * 1024ull * 1024ull) : (2ull * 1024ull * 1024ull * 1024ull);
    const std::uint64_t estimated_worker_bytes =
        fast_profile ? (160ull * 1024ull * 1024ull) : (192ull * 1024ull * 1024ull);
    const std::uint64_t estimated_batch_item_bytes = fast_profile ? (160ull * 1024ull) : (128ull * 1024ull);

    const std::size_t items = std::max<std::size_t>(remaining_items, 1);
    const std::size_t threads = (reported_threads > 0) ? static_cast<std::size_t>(reported_threads) : 4u;
    std::size_t worker_target = std::min(threads, items);

    SongIndexWorkBudget budget;
    budget.reserve_bytes = (memory.total_bytes > 0)
                               ? std::max<std::uint64_t>(min_reserve_bytes, memory.total_bytes / (fast_profile ? 8ull : 6ull))
                               : 0ull;
    std::size_t max_batch_by_memory = items;
    if (memory.total_bytes > 0) {
        budget.headroom_bytes = memory.available_bytes > budget.reserve_bytes
                                    ? memory.available_bytes - budget.reserve_bytes : 0ull;
        const std::size_t max_workers_by_memory =
            std::max<std::size_t>(1u, static_cast<std::size_t>(budget.headroom_bytes / estimated_worker_bytes));
        worker_target = std::min(worker_target, max_workers_by_memory);
        max_batch_by_memory = std::max<std::size_t>(
            worker_target, static_cast<std::size_t>(budget.headroom_bytes / estimated_batch_item_bytes));
    }

    // Both metadata profiles use every available CPU thread. Several charts per
    // worker amortize thread startup while batches stay bounded in memory.
    const bool large_scan = items >= 8192u;
    const std::size_t items_per_worker = fast_profile ? (large_scan ? 24u : 32u) : (large_scan ? 12u : 24u);
    const std::size_t batch_cap = std::max<std::size_t>(worker_target, fast_profile ? 1024u : 512u);
    const std::size_t preferred_batch = std::min(worker_target * items_per_worker, batch_cap);
    budget.worker_count = worker_target;
    budget.batch_size = std::min({items, preferred_batch, max_batch_by_memory});
    return budget;
}


// Work stays local to this call. Join every worker before propagating an item error.
template<class Function>
void run_song_index_workers(std::size_t worker_count, std::size_t items, Function&& process) {
    if (items == 0) return;
    worker_count = std::max<std::size_t>(1, std::min(worker_count, items));
    std::atomic<std::size_t> next{0};
    std::vector<std::exception_ptr> errors(items);
    const auto work = [&] {
        for (;;) {
            const auto item = next.fetch_add(1, std::memory_order_relaxed);
            if (item >= items) return;
            try { process(item); }
            catch (...) { errors[item] = std::current_exception(); }
        }
    };
    std::vector<std::thread> workers;
    workers.reserve(worker_count - 1);
    for (std::size_t i = 1; i < worker_count; ++i) {
        try { workers.emplace_back(work); }
        catch (const std::exception&) { break; } // The started workers and caller drain the same queue.
    }
    work();
    for (auto& worker : workers) worker.join();
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
}

}  // namespace tenriff::app
