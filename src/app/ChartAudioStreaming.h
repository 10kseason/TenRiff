#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "app/MemoryDiagnostics.h"

namespace tenriff::app {

struct ChartAudioBudgetChoice {
    std::uint64_t startup_preload_bytes = 96u * 1024u * 1024u;
    std::uint64_t runtime_cache_bytes = 192u * 1024u * 1024u;
};

struct ChartAudioStartupCandidate {
    int64_t first_use_sample = (std::numeric_limits<int64_t>::max)();
    std::uint64_t estimated_decoded_bytes = 0;
    int use_count = 0;
};

struct ChartAudioStartupPlan {
    std::vector<uint8_t> required_assets;
    std::vector<uint8_t> queued_assets;
    std::size_t deferred_count = 0;
    std::uint64_t estimated_startup_bytes = 0;
};

inline ChartAudioBudgetChoice choose_chart_audio_budgets(SystemMemorySnapshot snapshot) {
    auto clamp_budget = [](std::uint64_t value, std::uint64_t min_value, std::uint64_t max_value) {
        return (std::min)((std::max)(value, min_value), max_value);
    };

    ChartAudioBudgetChoice choice;
    choice.startup_preload_bytes = clamp_budget((snapshot.available_bytes > 0) ? (snapshot.available_bytes / 16u) : 0u,
                                                96u * 1024u * 1024u,
                                                384u * 1024u * 1024u);
    choice.runtime_cache_bytes = clamp_budget((snapshot.available_bytes > 0) ? (snapshot.available_bytes / 8u) : 0u,
                                              192u * 1024u * 1024u,
                                              768u * 1024u * 1024u);
    return choice;
}

inline ChartAudioStartupPlan build_chart_audio_startup_plan(const std::vector<ChartAudioStartupCandidate>& candidates,
                                                            int64_t startup_window_samples,
                                                            std::uint64_t startup_budget_bytes) {
    ChartAudioStartupPlan plan;
    plan.required_assets.assign(candidates.size(), 0);
    plan.queued_assets.assign(candidates.size(), 0);

    std::vector<std::size_t> order(candidates.size(), 0u);
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&candidates](std::size_t lhs, std::size_t rhs) {
        if (candidates[lhs].first_use_sample != candidates[rhs].first_use_sample) {
            return candidates[lhs].first_use_sample < candidates[rhs].first_use_sample;
        }
        return lhs < rhs;
    });

    for (std::size_t asset_id : order) {
        const auto& candidate = candidates[asset_id];
        // Only assets audible inside the startup window may block chart launch.
        // Later BGM is queued by first-use order here and by the runtime prefetcher
        // afterwards, so a chart with many late BGM slices does not decode the
        // entire song before the countdown can begin.
        const bool required = candidate.first_use_sample != (std::numeric_limits<int64_t>::max)() &&
                              candidate.first_use_sample <= startup_window_samples;
        if (!required || candidate.use_count <= 0) {
            continue;
        }
        plan.required_assets[asset_id] = 1;
        plan.queued_assets[asset_id] = 1;
        plan.estimated_startup_bytes += candidate.estimated_decoded_bytes;
    }

    for (std::size_t asset_id : order) {
        if (plan.required_assets[asset_id] != 0) {
            continue;
        }
        const auto& candidate = candidates[asset_id];
        if (candidate.use_count <= 0) {
            continue;
        }
        if (plan.estimated_startup_bytes + candidate.estimated_decoded_bytes > startup_budget_bytes) {
            ++plan.deferred_count;
            continue;
        }
        plan.queued_assets[asset_id] = 1;
        plan.estimated_startup_bytes += candidate.estimated_decoded_bytes;
    }

    return plan;
}

}  // namespace tenriff::app
