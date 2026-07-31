#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

struct BmsKeyConverterPreset {
    std::string token;
    std::string display_name;
    int target_lane_count = 0;
    int max_keys = 0;
    int min_keys = 0;
    int transform_speed_slot = 0;
    bool supported_output = true;
    std::optional<uint32_t> fixed_seed;
};

struct BmsKeyConverterOptions {
    std::string input_path;
    std::string output_path;
    int target_lane_count = 0;
    int max_keys = 0;
    int min_keys = 0;
    int transform_speed_slot = 4;
    std::string conversion_algorithm = "krrcream";
    uint32_t seed = 0;
    int sample_rate = 0;
};

struct BmsKeyConverterResult {
    bool success = false;
    std::string error;
    int source_lane_count = 0;
    int target_lane_count = 0;
    int sample_rate = 0;
    bool sample_rate_auto = false;
    std::size_t note_count = 0;
    std::size_t hold_count = 0;
    std::vector<std::string> warnings;
};

[[nodiscard]] const std::vector<BmsKeyConverterPreset>& bms_key_converter_presets();
[[nodiscard]] bool find_bms_key_converter_preset(std::string_view token, BmsKeyConverterPreset& preset);
[[nodiscard]] BmsKeyConverterResult convert_bms_chart_file(const BmsKeyConverterOptions& options);

}  // namespace tenriff::app
