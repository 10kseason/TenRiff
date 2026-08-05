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

void write_minimal_wav(const std::filesystem::path& path, int sample_rate) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());

    auto write_u16 = [&](uint16_t value) {
        const char bytes[2] = {
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu),
        };
        out.write(bytes, 2);
    };
    auto write_u32 = [&](uint32_t value) {
        const char bytes[4] = {
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu),
            static_cast<char>((value >> 16u) & 0xffu),
            static_cast<char>((value >> 24u) & 0xffu),
        };
        out.write(bytes, 4);
    };

    out.write("RIFF", 4);
    write_u32(38);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(16);
    write_u16(1);
    write_u16(1);
    write_u32(static_cast<uint32_t>(sample_rate));
    write_u32(static_cast<uint32_t>(sample_rate * 2));
    write_u16(2);
    write_u16(16);
    out.write("data", 4);
    write_u32(2);
    write_u16(0);
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
                      "#SCROLL01 -1.5\n"
                      "#000SC:01\n"
                      "#00111:01\n"
                      "#00212:01\n"
                      "#00313:01\n"
                      "#00414:01\n"
                      "#00515:01\n"
                      "#005E5:0A\n"
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
    REQUIRE(load_result.chart.mines.size() == 1u);
    CHECK(load_result.chart.mines[0].lane == 4);
    CHECK(load_result.chart.mines[0].damage_percent == doctest::Approx(5.0));
    CHECK_FALSE(load_result.chart.scroll_segments.empty());
    CHECK(load_result.chart.visual_velocity_at(0) < 0.0);
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

TEST_CASE("bms key converter auto-detects sample rate from BMS keysounds") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto input_path = temp.path / "source_keysound_rate.bms";
    const auto output_path = temp.path / "converted_keysound_rate.bms";
    write_minimal_wav(temp.path / "key.wav", 48000);

    {
        std::ofstream chart_file(input_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Auto Rate\n"
                      "#BPM 120\n"
                      "#WAV01 key.wav\n"
                      "#00111:01\n"
                      "#00212:01\n"
                      "#00313:01\n"
                      "#00414:01\n";
    }

    tenriff::app::BmsKeyConverterOptions options;
    options.input_path = input_path.u8string();
    options.output_path = output_path.u8string();
    options.target_lane_count = 10;

    const auto auto_result = tenriff::app::convert_bms_chart_file(options);
    CHECK(auto_result.success);
    CHECK(auto_result.sample_rate_auto);
    CHECK(auto_result.sample_rate == 48000);

    options.output_path = (temp.path / "converted_keysound_rate_manual.bms").u8string();
    options.sample_rate = 22050;
    const auto manual_result = tenriff::app::convert_bms_chart_file(options);
    CHECK(manual_result.success);
    CHECK_FALSE(manual_result.sample_rate_auto);
    CHECK(manual_result.sample_rate == 22050);
}

TEST_CASE("bms key converter exposes original toolkit presets") {
    const auto& presets = tenriff::app::bms_key_converter_presets();
    CHECK(presets.size() >= 9u);

    tenriff::app::BmsKeyConverterPreset preset;
    REQUIRE(tenriff::app::find_bms_key_converter_preset("10k", preset));
    CHECK(preset.target_lane_count == 10);
    CHECK(preset.max_keys == 10);
    CHECK(preset.min_keys == 1);
    CHECK(preset.transform_speed_slot == 5);
    REQUIRE(preset.fixed_seed.has_value());
    CHECK(preset.fixed_seed.value() == 0u);

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
TEST_CASE("bms key converter rejects unknown conversion algorithms") {
    tenriff::app::BmsKeyConverterOptions options;
    options.input_path = "input.bms";
    options.output_path = "output.bms";
    options.target_lane_count = 4;
    options.conversion_algorithm = "unknown";

    const auto result = tenriff::app::convert_bms_chart_file(options);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("Unsupported conversion algorithm") != std::string::npos);
}

TEST_CASE("bms key converter writes reparsable nK2 output") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto input_path = temp.path / "source_nk2.bms";
    const auto output_path = temp.path / "converted_nk2.bms";

    {
        std::ofstream chart_file(input_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE nK2 Convert\n"
                      "#BPM 120\n"
                      "#4K\n"
                      "#00111:01\n"
                      "#00212:01\n"
                      "#00313:01\n"
                      "#00414:01\n"
                      "#00511:01\n"
                      "#00612:01\n"
                      "#00713:01\n"
                      "#00814:01\n";
    }

    tenriff::app::BmsKeyConverterOptions options;
    options.input_path = input_path.u8string();
    options.output_path = output_path.u8string();
    options.target_lane_count = 8;
    options.conversion_algorithm = "nk2";

    const auto convert_result = tenriff::app::convert_bms_chart_file(options);
    REQUIRE(convert_result.success);
    CHECK(convert_result.target_lane_count == 8);
    CHECK(convert_result.note_count >= 8u);
    CHECK(std::any_of(convert_result.warnings.begin(), convert_result.warnings.end(), [](const std::string& warning) {
        return warning.find("Conversion algorithm: nK2") != std::string::npos;
    }));

    tenriff::app::ChartLoader loader;
    const auto load_result = loader.load(output_path.u8string(), 44100, 1.0, "ignore");
    CHECK(load_result.success());
    CHECK(load_result.chart.lane_count == 8);
    CHECK_FALSE(load_result.chart.notes.empty());
}
