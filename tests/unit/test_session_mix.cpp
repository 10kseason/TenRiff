#include "doctest/doctest.h"

#include <set>
#include <vector>

#include "app/SessionMix.h"

using tenriff::app::SessionMixCandidate;
using tenriff::app::SessionMixDraftEntry;
using tenriff::app::SessionMixPhase;
using tenriff::app::build_session_mix_draft_plan;
using tenriff::app::build_session_mix_plan;
using tenriff::app::cycle_session_mix_minutes;
using tenriff::app::session_mix_phase_counts;

TEST_CASE("session mix exposes the three supported target lengths") {
    CHECK(cycle_session_mix_minutes(15, 1) == 30);
    CHECK(cycle_session_mix_minutes(30, 1) == 60);
    CHECK(cycle_session_mix_minutes(60, 1) == 15);
    CHECK(cycle_session_mix_minutes(15, -1) == 60);

    CHECK(session_mix_phase_counts(15, 99).warmup == 1);
    CHECK(session_mix_phase_counts(15, 99).challenge == 3);
    CHECK(session_mix_phase_counts(15, 99).cooldown == 1);
    CHECK(session_mix_phase_counts(30, 99).total() == 10);
    CHECK(session_mix_phase_counts(60, 99).total() == 20);
}

TEST_CASE("session mix draft plan preserves authored order and repeats") {
    const std::vector<SessionMixDraftEntry> draft = {
        {"first.bms", "a", "First", 7, 3},
        {"second.bms", "b", "Second", 7, 7},
        {"first.bms", "a", "First", 7, 3},
    };

    const auto plan = build_session_mix_draft_plan(draft);
    REQUIRE(plan.entries.size() == 3u);
    CHECK(plan.entries[0].chart_id == "first.bms");
    CHECK(plan.entries[1].chart_id == "second.bms");
    CHECK(plan.entries[2].chart_id == "first.bms");
    CHECK(plan.entries[0].phase == SessionMixPhase::Challenge);
}

TEST_CASE("session mix keeps every selected chart unique and preserves phase order") {
    std::vector<SessionMixCandidate> candidates;
    for (int level = 1; level <= 20; ++level) {
        candidates.push_back(SessionMixCandidate{
            "chart-" + std::to_string(level), level, level <= 8, level <= 8, 93.0});
    }
    candidates.push_back(candidates.front());

    const auto plan = build_session_mix_plan(candidates, 30, 77);
    REQUIRE(plan.entries.size() == 10);

    std::set<std::string> ids;
    bool challenge_seen = false;
    bool cooldown_seen = false;
    for (const auto& entry : plan.entries) {
        CHECK(ids.insert(entry.chart_id).second);
        if (entry.phase == SessionMixPhase::Challenge) {
            challenge_seen = true;
        }
        if (entry.phase == SessionMixPhase::Cooldown) {
            cooldown_seen = true;
        }
        if (challenge_seen) {
            CHECK(entry.phase != SessionMixPhase::Warmup);
        }
        if (cooldown_seen) {
            CHECK(entry.phase == SessionMixPhase::Cooldown);
        }
    }
}

TEST_CASE("session mix uses local clears to place challenge songs above warmup") {
    std::vector<SessionMixCandidate> candidates;
    for (int level = 1; level <= 18; ++level) {
        const bool cleared = level <= 8;
        candidates.push_back(SessionMixCandidate{
            "level-" + std::to_string(level), level, cleared, cleared, cleared ? 94.0 : 0.0});
    }

    const auto plan = build_session_mix_plan(candidates, 30, 1234);
    REQUIRE(plan.entries.size() == 10);

    int warmup_sum = 0;
    int warmup_count = 0;
    int challenge_sum = 0;
    int challenge_count = 0;
    for (const auto& entry : plan.entries) {
        if (entry.phase == SessionMixPhase::Warmup) {
            warmup_sum += entry.difficulty;
            ++warmup_count;
        } else if (entry.phase == SessionMixPhase::Challenge) {
            challenge_sum += entry.difficulty;
            ++challenge_count;
        }
    }
    REQUIRE(warmup_count > 0);
    REQUIRE(challenge_count > 0);
    CHECK(challenge_sum * warmup_count > warmup_sum * challenge_count);
}

TEST_CASE("session mix gracefully shrinks to a small filtered library") {
    const std::vector<SessionMixCandidate> candidates = {
        {"one", 4, true, true, 95.0},
        {"two", 5, false, false, 0.0},
    };
    const auto plan = build_session_mix_plan(candidates, 60, 1);
    REQUIRE(plan.entries.size() == 2);
    CHECK(plan.entries[0].phase == SessionMixPhase::Warmup);
    CHECK(plan.entries[1].phase == SessionMixPhase::Challenge);
}
