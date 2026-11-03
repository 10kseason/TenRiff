#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "app/SongSelectScreen.h"

using tenriff::app::SongSelectScreen;

namespace {
struct DensityFixture {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("tenriff_density_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bms");
    ~DensityFixture() { std::error_code ec; std::filesystem::remove(path, ec); }
    void write(const char* notes) const {
        std::ofstream out(path, std::ios::binary);
        out << "#TITLE Density\n#BPM 240\n#4K\n" << notes;
    }
};

bool wait_density(SongSelectScreen& screen, const std::string& key, const std::string& path, int64_t now_ns) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        // A true return is what causes the owner to publish an idle snapshot.
        if (screen.service_nps_profile(key, path, now_ns) && screen.nps_profile_for(key)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}
}  // namespace

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
    screen.begin_preview_decode("missing-chart", missing_chart.u8string(), "", 44100);

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

TEST_CASE("SongSelectScreen density completes with audio disabled and invalidates on chart revision") {
    DensityFixture chart;
    chart.write("#00111:01010101\n#00312:01\n");
    SongSelectScreen screen;
    const auto path = chart.path.u8string();
    CHECK_FALSE(screen.active());
    CHECK(screen.service_nps_profile("revision-1", path, 0));
    CHECK(screen.nps_profile_for("revision-1") == nullptr);
    CHECK_FALSE(screen.service_nps_profile("revision-1", {}, 149'999'999));
    CHECK(screen.nps_profile_for("revision-1") == nullptr);
    REQUIRE(wait_density(screen, "revision-1", {}, 150'000'000));
    const auto* profile = screen.nps_profile_for("revision-1");
    REQUIRE(profile);
    REQUIRE(profile->available);
    CHECK(profile->average == doctest::Approx(1.25));
    CHECK(profile->minimum == doctest::Approx(0));
    chart.write("#00111:01\n");
    CHECK_FALSE(screen.service_nps_profile("revision-1", path, 200'000'000));
    CHECK(profile->average == doctest::Approx(1.25));
    CHECK(screen.service_nps_profile("revision-2", path, 200'000'000));
    CHECK(screen.nps_profile_for("revision-1") == nullptr);
    CHECK(screen.nps_profile_for("revision-2") == nullptr);
    REQUIRE(wait_density(screen, "revision-2", path, 350'000'000));
    REQUIRE(screen.nps_profile_for("revision-2"));
    CHECK(screen.nps_profile_for("revision-2")->average == doctest::Approx(0.5));
    screen.shutdown();
    CHECK(screen.nps_profile_for("revision-2") == nullptr);
}

TEST_CASE("SongSelectScreen density rejects stale completion after switching or leaving") {
    DensityFixture chart;
    chart.write("#00111:01010101\n");
    SongSelectScreen screen;
    const auto path = chart.path.u8string();
    CHECK(screen.service_nps_profile("chart-a", path, 0));
    (void)screen.service_nps_profile("chart-a", path, 150'000'000);
    CHECK(screen.service_nps_profile("chart-b", path + ".missing", 150'000'001));
    REQUIRE(wait_density(screen, "chart-b", path + ".missing", 350'000'000));
    CHECK(screen.nps_profile_for("chart-a") == nullptr);
    REQUIRE(screen.nps_profile_for("chart-b"));
    CHECK_FALSE(screen.nps_profile_for("chart-b")->available);
    CHECK_FALSE(screen.service_nps_profile("chart-b", path + ".missing", 400'000'000));
    CHECK(screen.service_nps_profile("chart-a", path, 400'000'000));
    (void)screen.service_nps_profile("chart-a", path, 550'000'000);
    CHECK(screen.service_nps_profile({}, {}, 550'000'001));
    screen.shutdown();
    CHECK(screen.nps_profile_for("chart-a") == nullptr);
}
