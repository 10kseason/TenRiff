#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "app/BmsKeyConverter.h"

namespace {

bool parse_int(const char* text, int& value) {
    if (!text) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool parse_u32(const char* text, uint32_t& value) {
    if (!text) {
        return false;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return parsed == value;
}

bool parse_sample_rate(const char* text, int& value) {
    if (!text) {
        return false;
    }
    std::string token(text);
    for (char& ch : token) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch + ('a' - 'A'));
        }
    }
    if (token == "auto" || token == "detect" || token == "0") {
        value = 0;
        return true;
    }
    return parse_int(text, value) && value > 0;
}

void print_usage() {
    std::vector<std::string> preset_tokens;
    for (const auto& preset : tenriff::app::bms_key_converter_presets()) {
        if (!preset.supported_output) {
            continue;
        }
        preset_tokens.push_back(preset.token);
    }

    std::cout
        << "Usage: bms_key_converter --input <file> --output <file> --target-keys <4|5|6|8|9|10|16>\n"
        << "                         [--preset <";
    for (std::size_t i = 0; i < preset_tokens.size(); ++i) {
        if (i != 0) {
            std::cout << '|';
        }
        std::cout << preset_tokens[i];
    }
    std::cout << ">]\n"
        << "                         [--max-keys <n>] [--min-keys <n>]\n"
        << "                         [--transform-speed-slot <0-8>] [--seed <u32>]\n"
        << "                         [--sample-rate <hz|auto>] [--algorithm <krrcream|nk2>]\n"
        << "Preset applies the original krrcream Toolkit target/max/min/speed defaults.\n"
        << "Preset 10k uses target=10, max=10, min=1, speed slot 5 (2 bars), and fixed seed 0.\n"
        << "Sample rate defaults to auto and is detected from referenced BMS keysounds before falling back to 44100 Hz.\n"
        << "Algorithm defaults to krrcream; nk2 uses its deterministic native 50/50 profile.\n"
        << "Krrcream tuning flags are accepted but ignored when --algorithm nk2 is selected.\n"
        << "Explicit --target-keys/--max-keys/--min-keys/--transform-speed-slot override preset values.\n";
}

}  // namespace

int main(int argc, char** argv) {
    tenriff::app::BmsKeyConverterOptions options;
    std::string preset_token;
    bool target_keys_specified = false;
    bool max_keys_specified = false;
    bool min_keys_specified = false;
    bool speed_slot_specified = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            options.input_path = argv[++i];
            continue;
        }
        if (arg == "--output" && i + 1 < argc) {
            options.output_path = argv[++i];
            continue;
        }
        if (arg == "--target-keys" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.target_lane_count)) {
                std::cerr << "Invalid --target-keys value.\n";
                return 1;
            }
            target_keys_specified = true;
            continue;
        }
        if (arg == "--preset" && i + 1 < argc) {
            preset_token = argv[++i];
            continue;
        }
        if (arg == "--max-keys" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.max_keys)) {
                std::cerr << "Invalid --max-keys value.\n";
                return 1;
            }
            max_keys_specified = true;
            continue;
        }
        if (arg == "--min-keys" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.min_keys)) {
                std::cerr << "Invalid --min-keys value.\n";
                return 1;
            }
            min_keys_specified = true;
            continue;
        }
        if (arg == "--transform-speed-slot" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.transform_speed_slot)) {
                std::cerr << "Invalid --transform-speed-slot value.\n";
                return 1;
            }
            speed_slot_specified = true;
            continue;
        }
        if (arg == "--sample-rate" && i + 1 < argc) {
            if (!parse_sample_rate(argv[++i], options.sample_rate)) {
                std::cerr << "Invalid --sample-rate value. Use a positive integer or auto.\n";
                return 1;
            }
            continue;
        }
        if (arg == "--algorithm" && i + 1 < argc) {
            options.conversion_algorithm = argv[++i];
            continue;
        }
        if (arg == "--seed" && i + 1 < argc) {
            if (!parse_u32(argv[++i], options.seed)) {
                std::cerr << "Invalid --seed value.\n";
                return 1;
            }
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        print_usage();
        return 1;
    }

    if (!preset_token.empty()) {
        tenriff::app::BmsKeyConverterPreset preset;
        if (!tenriff::app::find_bms_key_converter_preset(preset_token, preset)) {
            std::cerr << "Unknown --preset value: " << preset_token << '\n';
            print_usage();
            return 1;
        }
        if (!preset.supported_output) {
            std::cerr << "Preset " << preset.token << " is defined in the original toolkit but not available in the current standalone BMS writer.\n";
            return 1;
        }
        if (!target_keys_specified) {
            options.target_lane_count = preset.target_lane_count;
        }
        if (!max_keys_specified) {
            options.max_keys = preset.max_keys;
        }
        if (!min_keys_specified) {
            options.min_keys = preset.min_keys;
        }
        if (!speed_slot_specified) {
            options.transform_speed_slot = preset.transform_speed_slot;
        }
        if (preset.fixed_seed.has_value()) {
            options.seed = preset.fixed_seed.value();
        }
    }

    const auto result = tenriff::app::convert_bms_chart_file(options);
    for (const auto& warning : result.warnings) {
        std::cerr << "[warning] " << warning << '\n';
    }
    if (!result.success) {
        std::cerr << "[error] " << result.error << '\n';
        return 1;
    }

    std::cout << "Converted " << result.source_lane_count << "K -> " << result.target_lane_count
              << "K, notes=" << result.note_count << ", holds=" << result.hold_count
              << ", sample_rate=" << result.sample_rate << (result.sample_rate_auto ? " (auto)" : "") << '\n';
    return 0;
}
