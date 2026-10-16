#include "doctest/doctest.h"
#include "app/MemoryReclamation.h"

#include <atomic>
#include <thread>

namespace {
std::atomic<int> collections{0};
void count_collection() { ++collections; }
}

TEST_CASE("forced memory reclamation coalesces gameplay requests until a menu collection") {
    using namespace tenriff::app;
    collections = 0;
    MemoryReclamationGate gate(count_collection);
    GameplayMemoryDeferral gameplay(gate);
    gameplay.begin();
    gameplay.begin(); // Reinitializing a failed load must not leak another deferral.
    std::thread indexer([&] { for (int i = 0; i < 1000; ++i) gate.request(); });
    indexer.join();
    gate.collect_if_allowed();
    CHECK(collections == 0);
    gameplay.end();
    CHECK(collections == 0);
    gate.collect_if_allowed();
    CHECK(collections == 1);
    gate.collect_if_allowed();
    CHECK(collections == 1);
    gameplay.end();
    gate.request();
    CHECK(collections == 2);
}

TEST_CASE("overlapping gameplay lifetimes and failed-load destruction retain the trim barrier") {
    using namespace tenriff::app;
    collections = 0;
    MemoryReclamationGate gate(count_collection);
    {
        GameplayMemoryDeferral outer(gate);
        outer.begin();
        {
            GameplayMemoryDeferral failed_load(gate);
            failed_load.begin();
            gate.request();
        }
        gate.collect_if_allowed();
        CHECK(collections == 0);
    }
    CHECK(collections == 0);
    gate.collect_if_allowed();
    CHECK(collections == 1);
}
