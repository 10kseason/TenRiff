#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "app/SongIndex.h"

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
    const bool large_scan = items >= 8192u;
    std::size_t worker_target = 1u;
    if (fast_profile) {
        if (threads >= 12u) {
            worker_target = 4u;
        } else if (threads >= 8u) {
            worker_target = 3u;
        } else if (threads >= 4u) {
            worker_target = 2u;
        }
    } else {
        worker_target = (!large_scan && threads >= 6u) ? 2u : 1u;
    }
    worker_target = std::clamp<std::size_t>(worker_target, 1u, fast_profile ? 4u : 2u);

    SongIndexWorkBudget budget;
    budget.reserve_bytes = (memory.total_bytes > 0)
                               ? std::max<std::uint64_t>(min_reserve_bytes, memory.total_bytes / (fast_profile ? 8ull : 6ull))
                               : 0ull;

    if (memory.available_bytes > 0 && memory.total_bytes > 0 && memory.available_bytes > budget.reserve_bytes) {
        budget.headroom_bytes = memory.available_bytes - budget.reserve_bytes;
        const std::size_t max_workers_by_memory =
            std::max<std::size_t>(1u, static_cast<std::size_t>(budget.headroom_bytes / estimated_worker_bytes));
        worker_target = std::min(worker_target, max_workers_by_memory);

        const std::size_t max_batch_by_memory =
            std::max<std::size_t>(worker_target,
                                  static_cast<std::size_t>(budget.headroom_bytes / estimated_batch_item_bytes));
        const std::size_t preferred_batch =
            fast_profile ? (large_scan ? std::clamp<std::size_t>(worker_target * 24u, worker_target, 96u)
                                       : std::clamp<std::size_t>(worker_target * 32u, worker_target, 128u))
                         : (large_scan ? std::clamp<std::size_t>(worker_target * 12u, worker_target, 24u)
                                       : std::clamp<std::size_t>(worker_target * 24u, worker_target, 48u));
        budget.batch_size = std::min({items, preferred_batch, max_batch_by_memory});
    } else {
        budget.batch_size = std::min<std::size_t>(
            items,
            fast_profile ? (large_scan ? std::clamp<std::size_t>(worker_target * 16u, 16u, 48u)
                                       : std::clamp<std::size_t>(worker_target * 20u, 20u, 64u))
                         : (large_scan ? std::clamp<std::size_t>(worker_target * 8u, 8u, 16u)
                                       : std::clamp<std::size_t>(worker_target * 12u, 16u, 24u)));
    }

    budget.worker_count = std::min<std::size_t>(worker_target, items);
    budget.batch_size = std::max<std::size_t>(budget.batch_size, budget.worker_count);
    return budget;
}

}  // namespace tenriff::app
