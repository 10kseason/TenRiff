#include "doctest/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "app/BmsKeyConverter.h"
#include "app/ChartLoader.h"

namespace {

struct TempDirGuard {
    std::filesystem::path path;

    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_bms_key_converter_tests";
    std::filesystem::create_directories(base);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto candidate = base / ("case_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

}  // namespace

TEST_CASE("bms key converter writes a reparsable converted chart") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto input_path = temp.path / "source.bms";
    const auto output_path = temp.path / "converted.bms";

    {
        std::ofstream chart_file(input_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Convert Me\n"
                      "#BPM 120\n"
                      "#00111:01\n"
                      "#00212:01\n"
                      "#00313:01\n"
                      "#00414:01\n"
                      "#00515:01\n"
                      "#00621:01\n"
                      "#00722:01\n"
                      "#00823:01\n"
                      "#00924:01\n"
                      "#01025:01\n";
    }

    tenriff::app::BmsKeyConverterOptions options;
    options.input_path = input_path.u8string();
    options.output_path = output_path.u8string();
    options.target_lane_count = 4;

    const auto convert_result = tenriff::app::convert_bms_chart_file(options);
    CHECK(convert_result.success);
    CHECK(convert_result.note_count > 0u);

    tenriff::app::ChartLoader loader;
    const auto load_result = loader.load(output_path.u8string(), 44100, 1.0, "ignore");

    CHECK(load_result.success());
    CHECK(load_result.chart.lane_count == 4);
    CHECK_FALSE(load_result.chart.notes.empty());
}

TEST_CASE("bms key converter keeps long notes via LNOBJ output") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto input_path = temp.path / "source_ln.bms";
    const auto output_path = temp.path / "converted_ln.bms";

    {
        std::ofstream chart_file(input_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Convert LN\n"
                      "#BPM 120\n"
                      "#LNOBJ AA\n"
                      "#00111:01AA\n"
                      "#00212:01\n"
                      "#00313:01\n"
                      "#00414:01\n"
                      "#00515:01\n"
                      "#00621:01\n"
                      "#00722:01\n"
                      "#00823:01\n"
                      "#00924:01\n"
                      "#01025:01\n";
    }

    tenriff::app::BmsKeyConverterOptions options;
    options.input_path = input_path.u8string();
    options.output_path = output_path.u8string();
    options.target_lane_count = 6;

    const auto convert_result = tenriff::app::convert_bms_chart_file(options);
    CHECK(convert_result.success);
    CHECK(convert_result.hold_count == 1u);

    tenriff::app::ChartLoader loader;
    const auto load_result = loader.load(output_path.u8string(), 44100, 1.0, "ignore");

    CHECK(load_result.success());
    CHECK(load_result.chart.lane_count == 6);
    CHECK_FALSE(load_result.chart.notes.empty());
    const auto hold_count = static_cast<std::size_t>(std::count_if(load_result.chart.notes.begin(),
                                                                   load_result.chart.notes.end(),
                                                                   [](const auto& note) {
                                                                       return note.end_sample.has_value();
                                                                   }));
    CHECK(hold_count >= 1u);
}

TEST_CASE("bms key converter rejects unsupported target lane counts") {
    tenriff::app::BmsKeyConverterOptions options;
    options.input_path = "input.bms";
    options.output_path = "output.bms";
    options.target_lane_count = 7;

    const auto result = tenriff::app::convert_bms_chart_file(options);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("Unsupported target lane count") != std::string::npos);
}

TEST_CASE("bms key converter exposes original toolkit presets") {
    const auto& presets = tenriff::app::bms_key_converter_presets();
    CHECK(presets.size() >= 9u);

    tenriff::app::BmsKeyConverterPreset preset;
    REQUIRE(tenriff::app::find_bms_key_converter_preset("a10k", preset));
    CHECK(preset.target_lane_count == 10);
    CHECK(preset.max_keys == 7);
    CHECK(preset.min_keys == 7);
    CHECK(preset.transform_speed_slot == 2);
    CHECK(preset.supported_output);

    REQUIRE(tenriff::app::find_bms_key_converter_preset("down-to-6k", preset));
    CHECK(preset.target_lane_count == 6);
    CHECK(preset.max_keys == 6);
    CHECK(preset.min_keys == 4);
    CHECK(preset.transform_speed_slot == 2);

    REQUIRE(tenriff::app::find_bms_key_converter_preset("7k", preset));
    CHECK_FALSE(preset.supported_output);
}
