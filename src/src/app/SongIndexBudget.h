#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace tenriff::app {

struct SongIndexMemorySnapshot {
    std::uint64_t available_bytes = 0;
    std::uint64_t total_bytes = 0;
};

struct SongIndexWorkBudget {
    std::size_t worker_count = 1;
    std::size_t batch_size = 1;
    std::uint64_t reserve_bytes = 0;
    std::uint64_t headroom_bytes = 0;
};

inline SongIndexWorkBudget choose_song_index_work_budget(unsigned reported_threads,
                                                         std::size_t remaining_items,
                                                         SongIndexMemorySnapshot memory) {
    constexpr std::uint64_t kMinReserveBytes = 768ull * 1024ull * 1024ull;
    constexpr std::uint64_t kEstimatedWorkerBytes = 32ull * 1024ull * 1024ull;
    constexpr std::uint64_t kEstimatedBatchItemBytes = 8ull * 1024ull;

    const std::size_t items = std::max<std::size_t>(remaining_items, 1);
    const std::size_t threads = (reported_threads > 0) ? static_cast<std::size_t>(reported_threads) : 4u;
    std::size_t worker_target = (threads <= 2u) ? 1u : (threads - 1u);
    worker_target = std::clamp<std::size_t>(worker_target, 1u, 16u);

    SongIndexWorkBudget budget;
    budget.reserve_bytes = (memory.total_bytes > 0)
                               ? std::max<std::uint64_t>(kMinReserveBytes, memory.total_bytes / 8ull)
                               : 0ull;

    if (memory.available_bytes > 0 && memory.total_bytes > 0 && memory.available_bytes > budget.reserve_bytes) {
        budget.headroom_bytes = memory.available_bytes - budget.reserve_bytes;
        const std::size_t max_workers_by_memory =
            std::max<std::size_t>(1u, static_cast<std::size_t>(budget.headroom_bytes / kEstimatedWorkerBytes));
        worker_target = std::min(worker_target, max_workers_by_memory);

        const std::size_t max_batch_by_memory =
            std::max<std::size_t>(worker_target,
                                  static_cast<std::size_t>(budget.headroom_bytes / kEstimatedBatchItemBytes));
        const std::size_t preferred_batch = std::clamp<std::size_t>(worker_target * 256u, worker_target, 4096u);
        budget.batch_size = std::min({items, preferred_batch, max_batch_by_memory});
    } else {
        budget.batch_size = std::min<std::size_t>(items, std::clamp<std::size_t>(worker_target * 256u, 256u, 4096u));
    }

    budget.worker_count = std::min<std::size_t>(worker_target, items);
    budget.batch_size = std::max<std::size_t>(budget.batch_size, budget.worker_count);
    return budget;
}

}  // namespace tenriff::app
