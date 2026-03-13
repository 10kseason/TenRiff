#include "doctest/doctest.h"

#include <limits>
#include <vector>

#include "app/ChartAudioStreaming.h"

TEST_CASE("chart audio budget choice clamps startup and runtime budgets") {
    const auto budgets = tenriff::app::choose_chart_audio_budgets(
        tenriff::app::SystemMemorySnapshot{16ull * 1024ull * 1024ull * 1024ull, 32ull * 1024ull * 1024ull * 1024ull});

    CHECK(budgets.startup_preload_bytes == 384ull * 1024ull * 1024ull);
    CHECK(budgets.runtime_cache_bytes == 768ull * 1024ull * 1024ull);
}

TEST_CASE("chart audio startup plan always includes BGM and early assets before deferred assets") {
    using tenriff::app::ChartAudioStartupCandidate;

    std::vector<ChartAudioStartupCandidate> candidates(4);
    candidates[0].has_bgm = true;
    candidates[0].first_use_sample = 40'000;
    candidates[0].estimated_decoded_bytes = 32ull * 1024ull * 1024ull;
    candidates[0].use_count = 1;

    candidates[1].first_use_sample = 2'000;
    candidates[1].estimated_decoded_bytes = 16ull * 1024ull * 1024ull;
    candidates[1].use_count = 1;

    candidates[2].first_use_sample = 50'000;
    candidates[2].estimated_decoded_bytes = 40ull * 1024ull * 1024ull;
    candidates[2].use_count = 1;

    candidates[3].first_use_sample = 70'000;
    candidates[3].estimated_decoded_bytes = 40ull * 1024ull * 1024ull;
    candidates[3].use_count = 1;

    const auto plan = tenriff::app::build_chart_audio_startup_plan(
        candidates,
        3'000,
        96ull * 1024ull * 1024ull);

    REQUIRE(plan.required_assets.size() == candidates.size());
    REQUIRE(plan.queued_assets.size() == candidates.size());
    CHECK(plan.required_assets[0] == 1);
    CHECK(plan.required_assets[1] == 1);
    CHECK(plan.queued_assets[0] == 1);
    CHECK(plan.queued_assets[1] == 1);
    CHECK(plan.queued_assets[2] == 1);
    CHECK(plan.queued_assets[3] == 0);
    CHECK(plan.deferred_count == 1u);
}

TEST_CASE("chart audio startup plan ignores unused assets") {
    using tenriff::app::ChartAudioStartupCandidate;

    std::vector<ChartAudioStartupCandidate> candidates(1);
    candidates[0].first_use_sample = (std::numeric_limits<int64_t>::max)();
    candidates[0].estimated_decoded_bytes = 32ull * 1024ull * 1024ull;
    candidates[0].use_count = 0;

    const auto plan = tenriff::app::build_chart_audio_startup_plan(
        candidates,
        3'000,
        96ull * 1024ull * 1024ull);

    CHECK(plan.required_assets[0] == 0);
    CHECK(plan.queued_assets[0] == 0);
    CHECK(plan.deferred_count == 0u);
}
