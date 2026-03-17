#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "chart/OsuManiaLoader.h"
#include "config/SimpleJson.h"
#include "gameplay/GameplayChart.h"
#include "gameplay/ModeApplier.h"

namespace {

namespace fs = std::filesystem;
using tenriff::chart::OsuManiaLoader;
using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonValue;
using tenriff::gameplay::GameplayChart;
using tenriff::gameplay::KeyMode;
using tenriff::gameplay::ModeApplyContext;
using tenriff::gameplay::ModeSettings;
using tenriff::gameplay::apply_mode_settings;
using tenriff::gameplay::from_osu_mania;

struct Options {
    fs::path chart_path;
    int target_keys = 0;
    std::uint32_t seed = 0;
    int sample_rate = 1000;
    double rate = 1.0;
};

void print_usage(const char* argv0) {
    std::cout << "Usage: " << (argv0 ? argv0 : "n2nc_compare_smoke")
              << " --chart <path> --target <4|5|6|7|8|9|10|16> [--seed <u32>]"
              << " [--sample-rate <hz>] [--rate <multiplier>]\n";
}

bool parse_u32(std::string_view text, std::uint32_t& value) {
    try {
        const auto parsed = static_cast<unsigned long>(std::stoul(std::string(text)));
        value = static_cast<std::uint32_t>(parsed);
        return parsed == value;
    } catch (...) {
        return false;
    }
}

bool parse_int(std::string_view text, int& value) {
    try {
        value = std::stoi(std::string(text));
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_double(std::string_view text, double& value) {
    try {
        value = std::stod(std::string(text));
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--chart") {
            if (i + 1 >= argc) {
                std::cerr << "[error] --chart requires a path.\n";
                return false;
            }
            options.chart_path = fs::u8path(argv[++i]);
            continue;
        }
        if (arg == "--target") {
            if (i + 1 >= argc || !parse_int(argv[i + 1], options.target_keys)) {
                std::cerr << "[error] --target requires an integer key count.\n";
                return false;
            }
            ++i;
            continue;
        }
        if (arg == "--seed") {
            if (i + 1 >= argc || !parse_u32(argv[i + 1], options.seed)) {
                std::cerr << "[error] --seed requires an unsigned integer.\n";
                return false;
            }
            ++i;
            continue;
        }
        if (arg == "--sample-rate") {
            if (i + 1 >= argc || !parse_int(argv[i + 1], options.sample_rate) || options.sample_rate <= 0) {
                std::cerr << "[error] --sample-rate requires a positive integer.\n";
                return false;
            }
            ++i;
            continue;
        }
        if (arg == "--rate") {
            if (i + 1 >= argc || !parse_double(argv[i + 1], options.rate) || options.rate <= 0.0) {
                std::cerr << "[error] --rate requires a positive number.\n";
                return false;
            }
            ++i;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        std::cerr << "[error] Unknown argument: " << arg << '\n';
        return false;
    }

    if (options.chart_path.empty()) {
        std::cerr << "[error] --chart is required.\n";
        return false;
    }
    if (options.target_keys <= 0) {
        std::cerr << "[error] --target is required.\n";
        return false;
    }
    return true;
}

std::optional<KeyMode> key_mode_from_target(int target_keys) {
    switch (target_keys) {
    case 4:
        return KeyMode::Keys4;
    case 5:
        return KeyMode::Keys5;
    case 6:
        return KeyMode::Keys6;
    case 7:
        return KeyMode::Keys7;
    case 8:
        return KeyMode::Keys8;
    case 9:
        return KeyMode::Keys9;
    case 10:
        return KeyMode::Keys10;
    case 16:
        return KeyMode::Keys16;
    default:
        return std::nullopt;
    }
}

bool read_text_file(const fs::path& path, std::string& text, std::string& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Failed to open file: " + path.u8string();
        return false;
    }
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size < 0) {
        error = "Failed to read file size: " + path.u8string();
        return false;
    }
    text.resize(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    if (!text.empty()) {
        stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (!stream && !text.empty()) {
        error = "Failed to read file contents: " + path.u8string();
        return false;
    }
    return true;
}

int64_t sample_to_ms(int64_t sample, int sample_rate) {
    if (sample_rate <= 0) {
        return sample;
    }
    return static_cast<int64_t>((sample * 1000LL + sample_rate / 2) / sample_rate);
}

JsonArray to_json_warnings(const std::vector<std::string>& warnings) {
    JsonArray values;
    values.reserve(warnings.size());
    for (const auto& warning : warnings) {
        values.emplace_back(warning);
    }
    return values;
}

JsonArray to_json_notes(const GameplayChart& chart, int sample_rate) {
    JsonArray notes;
    notes.reserve(chart.notes.size());

    for (std::size_t index = 0; index < chart.notes.size(); ++index) {
        const auto& note = chart.notes[index];
        JsonObject entry;
        entry.emplace("order", JsonValue(static_cast<double>(index)));
        entry.emplace("lane", JsonValue(static_cast<double>(note.lane)));
        entry.emplace("start_ms", JsonValue(static_cast<double>(sample_to_ms(note.start_sample, sample_rate))));
        if (note.end_sample.has_value()) {
            entry.emplace("end_ms", JsonValue(static_cast<double>(sample_to_ms(note.end_sample.value(), sample_rate))));
        } else {
            entry.emplace("end_ms", JsonValue());
        }
        entry.emplace("release_required", JsonValue(note.release_required));
        notes.emplace_back(JsonValue(std::move(entry)));
    }

    return notes;
}

std::size_t hold_count(const GameplayChart& chart) {
    return static_cast<std::size_t>(std::count_if(chart.notes.begin(), chart.notes.end(), [](const auto& note) {
        return note.end_sample.has_value();
    }));
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        return 1;
    }

    const auto key_mode = key_mode_from_target(options.target_keys);
    if (!key_mode.has_value()) {
        std::cerr << "[error] Unsupported target key count: " << options.target_keys << '\n';
        return 1;
    }

    std::error_code exists_error;
    if (!fs::exists(options.chart_path, exists_error)) {
        std::cerr << "[error] Chart does not exist: " << options.chart_path.u8string() << '\n';
        return 1;
    }

    std::string file_text;
    std::string read_error;
    if (!read_text_file(options.chart_path, file_text, read_error)) {
        std::cerr << "[error] " << read_error << '\n';
        return 1;
    }

    OsuManiaLoader loader;
    const auto parsed = loader.parse(file_text);
    if (!parsed.success()) {
        std::cerr << "[error] Failed to parse osu!mania chart.\n";
        for (const auto& message : parsed.messages) {
            std::cerr << "  line " << message.line << ": " << message.text << '\n';
        }
        return 1;
    }

    const GameplayChart source_chart = from_osu_mania(parsed.chart, options.sample_rate, options.rate);

    ModeSettings settings;
    settings.key_mode = key_mode.value();
    settings.random_seed = options.seed;

    ModeApplyContext context;
    context.base_bpm = parsed.chart.base_bpm;
    context.sample_rate = options.sample_rate;

    const auto result = apply_mode_settings(source_chart, settings, context);

    JsonObject root;
    root.emplace("engine", JsonValue("tenriff"));
    root.emplace("chart_path", JsonValue(options.chart_path.u8string()));
    root.emplace("source_key_count", JsonValue(static_cast<double>(source_chart.lane_count)));
    root.emplace("target_key_count", JsonValue(static_cast<double>(result.chart.lane_count)));
    root.emplace("seed", JsonValue(static_cast<double>(options.seed)));
    root.emplace("sample_rate", JsonValue(static_cast<double>(options.sample_rate)));
    root.emplace("base_bpm", JsonValue(parsed.chart.base_bpm));
    root.emplace("source_note_count", JsonValue(static_cast<double>(source_chart.notes.size())));
    root.emplace("source_hold_count", JsonValue(static_cast<double>(hold_count(source_chart))));
    root.emplace("note_count", JsonValue(static_cast<double>(result.chart.notes.size())));
    root.emplace("hold_count", JsonValue(static_cast<double>(hold_count(result.chart))));
    root.emplace("converted", JsonValue(result.chart.lane_count == options.target_keys));
    root.emplace("warnings", JsonValue(to_json_warnings(result.warnings)));
    root.emplace("notes", JsonValue(to_json_notes(result.chart, options.sample_rate)));

    std::cout << tenriff::config::json_stringify(JsonValue(std::move(root)), 2) << '\n';
    return 0;
}
