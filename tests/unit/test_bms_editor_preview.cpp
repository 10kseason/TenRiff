#include "doctest/doctest.h"

#include "app/BmsEditorPreview.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

namespace tenriff::app {

namespace {
struct PreviewFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("tenriff-editor-preview-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path chart = root / "preview.bms";
    std::filesystem::path wav = root / "hit.wav";

    PreviewFixture() {
        std::filesystem::create_directories(root);
        std::ofstream out(wav, std::ios::binary);
        const auto le = [&](std::uint32_t value, int bytes) {
            for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 0xffu));
        };
        constexpr std::uint32_t frames = 64;
        out.write("RIFF", 4); le(36u + frames * 2u, 4); out.write("WAVEfmt ", 8);
        le(16, 4); le(1, 2); le(1, 2); le(48'000, 4); le(96'000, 4);
        le(2, 2); le(16, 2); out.write("data", 4); le(frames * 2u, 4);
        for (std::uint32_t i = 0; i < frames; ++i) le(i == 0 ? 20'000u : 0u, 2);
    }
    ~PreviewFixture() { std::error_code ec; std::filesystem::remove_all(root, ec); }
};
}  // namespace

TEST_CASE("BMS editor preview maps cursor through BPM and STOP") {
    BmsEditorPreviewRequest request;
    request.chart_text =
        "#PLAYER 1\n"
        "#BPM 120\n"
        "#STOP01 48\n"
        "#00009:0100\n";
    request.cursor_measure = 1;
    request.cursor_fraction = 0.0;
    request.sample_rate = 48'000;
    request.max_duration_seconds = 2;

    const auto result = build_bms_editor_preview(request);
    REQUIRE(result.success());
    // One measure at 120 BPM is two seconds, and the 48-tick STOP adds half a
    // beat (0.5 seconds) before the cursor.
    CHECK(result.cursor_sample == 120'000);
    REQUIRE(result.stereo_samples);
    CHECK(result.stereo_samples->size() == 2u * 48'000u * 2u);
}

TEST_CASE("BMS editor preview cancellation is bounded and does not return audio") {
    BmsEditorPreviewRequest request;
    request.chart_text = "#BPM 120\n#00111:0100\n";
    auto cancel = std::make_shared<std::atomic<bool>>(true);

    const auto result = build_bms_editor_preview(request, cancel);
    CHECK_FALSE(result.success());
    CHECK(result.error == "cancelled");
    CHECK_FALSE(result.stereo_samples);
}

TEST_CASE("BMS editor preview mixes a note keysound from the edited buffer") {
    PreviewFixture fixture;
    BmsEditorPreviewRequest request;
    request.chart_text =
        "#BPM 120\n"
        "#WAV01 hit.wav\n"
        "#00011:01\n";
    request.source_chart_path = fixture.chart.u8string();
    request.sample_rate = 48'000;
    request.max_duration_seconds = 1;

    const auto result = build_bms_editor_preview(request);
    REQUIRE(result.success());
    REQUIRE(result.stereo_samples);
    CHECK(std::any_of(result.stereo_samples->begin(), result.stereo_samples->end(),
                      [](float sample) { return std::abs(sample) > 0.01f; }));
}

TEST_CASE("BMS editor preview resolves a keysound through a sibling audio extension") {
    PreviewFixture fixture;
    BmsEditorPreviewRequest request;
    request.chart_text = "#BPM 120\n#WAV01 hit.ogg\n";
    request.source_chart_path = fixture.chart.u8string();
    request.sample_rate = 48'000;
    request.max_duration_seconds = 1;
    request.keysound_id = "01";
    request.keysound_only = true;

    const auto result = build_bms_editor_preview(request);
    REQUIRE(result.success());
    REQUIRE(result.stereo_samples);
    CHECK(std::any_of(result.stereo_samples->begin(), result.stereo_samples->end(),
                      [](float sample) { return std::abs(sample) > 0.01f; }));
}

}  // namespace tenriff::app
