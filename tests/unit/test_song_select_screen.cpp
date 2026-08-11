#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <thread>

#include "app/SongSelectScreen.h"

using tenriff::app::SongSelectScreen;

TEST_CASE("SongSelectScreen clears preview ownership when the screen is left") {
    SongSelectScreen screen;
    screen.set_active(true);
    screen.set_preview_target("chart-a", 1234);
    screen.set_preview_active_path("preview.wav");
    screen.preview_gain().store(0.75f);

    CHECK(screen.active());
    CHECK(screen.preview_pending());
    CHECK(screen.preview_selection_key() == "chart-a");

    screen.set_active(false);
    CHECK_FALSE(screen.active());
    CHECK_FALSE(screen.preview_pending());
    CHECK(screen.preview_selection_key().empty());
    CHECK(screen.preview_due_ns() == 0);
}

TEST_CASE("SongSelectScreen drains a completed preview decode future") {
    const std::filesystem::path missing_chart =
        std::filesystem::temp_directory_path() /
        ("tenriff_missing_preview_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) /
        "missing-chart.bms";

    SongSelectScreen screen;
    screen.set_active(true);
    screen.set_preview_target("missing-chart", 0);
    screen.begin_preview_decode("missing-chart", missing_chart.string(), "", 44100);

    std::optional<SongSelectScreen::PreviewDecodeResult> decoded;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!decoded && std::chrono::steady_clock::now() < deadline) {
        decoded = screen.take_ready_preview_decode();
        if (!decoded) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    REQUIRE(decoded.has_value());
    CHECK(decoded->selection_key == "missing-chart");
    CHECK_FALSE(decoded->error.empty());
    CHECK_FALSE(screen.preview_decode_in_flight());
}
