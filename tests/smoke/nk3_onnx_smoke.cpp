#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "gameplay/KeyModeConverter.h"

namespace {

using tenriff::gameplay::GameplayChart;
using tenriff::gameplay::KeyModeConversionAlgorithm;
using tenriff::gameplay::KeyModeConverterOptions;
using tenriff::gameplay::NoteEvent;

GameplayChart make_dense_long_note_chart() {
    GameplayChart chart;
    chart.lane_count = 7;
    chart.duration_samples = 40'000;
    for (int index = 0; index < 448; ++index) {
        NoteEvent note;
        note.lane = index % chart.lane_count + 1;
        note.start_sample = 100 + static_cast<int64_t>(index) * 75;
        note.note_id = static_cast<std::size_t>(index + 1);
        if (index % 2 == 0) {
            note.end_sample = note.start_sample + 225;
            note.release_required = index % 4 == 0;
        }
        chart.duration_samples =
            std::max(chart.duration_samples, note.end_sample.value_or(note.start_sample));
        chart.notes.push_back(note);
    }
    return chart;
}

GameplayChart make_18k_compression_chart() {
    GameplayChart chart;
    chart.lane_count = 18;
    for (int chord = 0; chord < 2; ++chord) {
        for (int lane = 0; lane < chart.lane_count; ++lane) {
            NoteEvent note;
            note.lane = lane + 1;
            note.start_sample = 100 + static_cast<int64_t>(chord) * 1'000;
            note.note_id = static_cast<std::size_t>(chord * chart.lane_count + lane + 1);
            chart.notes.push_back(note);
        }
    }
    chart.duration_samples = 2'000;
    return chart;
}

GameplayChart make_sub_millisecond_same_lane_chart() {
    GameplayChart chart;
    chart.lane_count = 1;
    for (int index = 0; index < 2; ++index) {
        NoteEvent note;
        note.lane = 1;
        note.start_sample = 1'000 + index;
        note.note_id = static_cast<std::size_t>(index + 1);
        chart.notes.push_back(note);
    }
    chart.duration_samples = 2'000;
    return chart;
}

GameplayChart make_matrix_chart(int source_keys) {
    GameplayChart chart;
    chart.lane_count = source_keys;
    const int note_count = std::max(8, source_keys * 2);
    for (int index = 0; index < note_count; ++index) {
        NoteEvent note;
        note.lane = index % source_keys + 1;
        note.start_sample = 100 + static_cast<int64_t>(index) * 250;
        note.note_id = static_cast<std::size_t>(index + 1);
        if (index % 3 == 0) {
            note.end_sample = note.start_sample + 120;
            note.release_required = index % 2 == 0;
        }
        chart.duration_samples =
            std::max(chart.duration_samples, note.end_sample.value_or(note.start_sample));
        chart.notes.push_back(note);
    }
    return chart;
}

bool structurally_safe(const GameplayChart& chart, int target_keys) {
    if (chart.lane_count != target_keys || chart.notes.empty()) {
        return false;
    }
    std::vector<int64_t> lane_end(static_cast<std::size_t>(target_keys),
                                  std::numeric_limits<int64_t>::min());
    int64_t previous_time = std::numeric_limits<int64_t>::min();
    int previous_lane = 0;
    for (const auto& note : chart.notes) {
        if (note.lane <= 0 || note.lane > target_keys ||
            note.start_sample < previous_time ||
            (note.start_sample == previous_time && note.lane < previous_lane)) {
            return false;
        }
        const std::size_t lane = static_cast<std::size_t>(note.lane - 1);
        if (note.start_sample <= lane_end[lane]) {
            return false;
        }
        lane_end[lane] = note.end_sample.value_or(note.start_sample);
        previous_time = note.start_sample;
        previous_lane = note.lane;
    }
    return true;
}

bool warning_contains(const std::vector<std::string>& warnings,
                      const std::string& text) {
    return std::any_of(warnings.begin(), warnings.end(), [&](const std::string& warning) {
        return warning.find(text) != std::string::npos;
    });
}

bool warning_reports_effective_batching(const std::vector<std::string>& warnings) {
    for (const auto& warning : warnings) {
        if (warning.find("generalized pattern MLP") == std::string::npos) {
            continue;
        }
        const auto inference_pos = warning.find("inferences=");
        const auto states_pos = warning.find("evaluated-states=");
        if (inference_pos == std::string::npos || states_pos == std::string::npos ||
            warning.find("batch-capacity=32") == std::string::npos) {
            return false;
        }
        const auto inference_count = std::stoull(
            warning.substr(inference_pos + std::string("inferences=").size()));
        const auto evaluated_states = std::stoull(
            warning.substr(states_pos + std::string("evaluated-states=").size()));
        return inference_count > 0 && evaluated_states > inference_count;
    }
    return false;
}

bool warning_field_greater_than(const std::vector<std::string>& warnings,
                                const std::string& field,
                                std::uint64_t threshold) {
    for (const auto& warning : warnings) {
        const auto position = warning.find(field);
        if (position != std::string::npos &&
            std::stoull(warning.substr(position + field.size())) > threshold) {
            return true;
        }
    }
    return false;
}

void set_device_environment(const char* value) {
#ifdef _WIN32
    _putenv_s("TENRIFF_NK3_DEVICE", value ? value : "");
#else
    if (value) {
        setenv("TENRIFF_NK3_DEVICE", value, 1);
    } else {
        unsetenv("TENRIFF_NK3_DEVICE");
    }
#endif
}

}  // namespace

int main() {
    const GameplayChart source = make_dense_long_note_chart();
    const std::array<int, 5> targets{4, 5, 7, 8, 10};
    for (const int target : targets) {
        KeyModeConverterOptions options;
        options.algorithm = KeyModeConversionAlgorithm::NK3;
        options.target_lane_count = target;
        options.sample_rate = 1000;
        options.base_bpm = 180.0;

        const auto converted = tenriff::gameplay::convert_key_mode_chart(source, options);
        for (const auto& warning : converted.warnings) {
            std::cout << target << "K: " << warning << '\n';
        }
        if (!converted.converted || !structurally_safe(converted.chart, target)) {
            std::cerr << "NK3 smoke failed for 7K -> " << target << "K\n";
            return 1;
        }
        if (!warning_contains(converted.warnings, "NK3 P64 hybrid ONNX") ||
            !warning_contains(converted.warnings,
                              std::to_string(target) + "K generalized pattern MLP") ||
            !warning_contains(converted.warnings, "schema=v3 features=28 roles=2") ||
            !warning_reports_effective_batching(converted.warnings)) {
            std::cerr << "Generalized pattern MLP route missing for " << target << "K\n";
            return 1;
        }
    }

    const GameplayChart compression_source = make_18k_compression_chart();
    for (const int target : {8, 10}) {
        KeyModeConverterOptions options;
        options.algorithm = KeyModeConversionAlgorithm::NK3;
        options.target_lane_count = target;
        options.sample_rate = 1000;
        options.base_bpm = 180.0;

        const auto converted =
            tenriff::gameplay::convert_key_mode_chart(compression_source, options);
        if (!converted.converted || !structurally_safe(converted.chart, target) ||
            !warning_field_greater_than(converted.warnings, "safety-retry=", 0) ||
            !warning_field_greater_than(converted.warnings,
                                        "max-requested-states=", 32)) {
            std::cerr << "NK3 batched retry smoke failed for 18K -> "
                      << target << "K\n";
            return 1;
        }
    }

    KeyModeConverterOptions precise_time_options;
    precise_time_options.algorithm = KeyModeConversionAlgorithm::NK3;
    precise_time_options.target_lane_count = 1;
    precise_time_options.sample_rate = 44'100;
    precise_time_options.base_bpm = 180.0;
    const auto precise_time_converted = tenriff::gameplay::convert_key_mode_chart(
        make_sub_millisecond_same_lane_chart(), precise_time_options);
    if (!precise_time_converted.converted || precise_time_converted.chart.notes.size() != 2 ||
        !structurally_safe(precise_time_converted.chart, 1) ||
        !warning_contains(precise_time_converted.warnings,
                          "generalized pattern MLP off for 1K")) {
        std::cerr << "NK3 sub-millisecond timing smoke failed\n";
        return 1;
    }
    auto exact_collision_source = make_sub_millisecond_same_lane_chart();
    exact_collision_source.notes[1].start_sample = exact_collision_source.notes[0].start_sample;
    const auto exact_collision_converted = tenriff::gameplay::convert_key_mode_chart(
        exact_collision_source, precise_time_options);
    if (exact_collision_converted.converted ||
        !warning_contains(exact_collision_converted.warnings, "collisions=1")) {
        std::cerr << "NK3 exact-sample collision guard smoke failed\n";
        return 1;
    }

    int matrix_routes = 0;
    for (int source_keys = 1; source_keys <= 18; ++source_keys) {
        const GameplayChart matrix_source = make_matrix_chart(source_keys);
        for (int target_keys = 1; target_keys <= 18; ++target_keys) {
            KeyModeConverterOptions options;
            options.algorithm = KeyModeConversionAlgorithm::NK3;
            options.target_lane_count = target_keys;
            options.sample_rate = 1000;
            options.base_bpm = 180.0;
            const auto converted =
                tenriff::gameplay::convert_key_mode_chart(matrix_source, options);
            const bool expected_route = target_keys == 1
                                            ? warning_contains(
                                                  converted.warnings,
                                                  "generalized pattern MLP off for 1K")
                                            : warning_contains(
                                                  converted.warnings,
                                                  std::to_string(target_keys) +
                                                      "K generalized pattern MLP");
            if (!converted.converted ||
                !structurally_safe(converted.chart, target_keys) ||
                !expected_route) {
                std::cerr << "NK3 all-to-all matrix failed for " << source_keys
                          << "K -> " << target_keys << "K\n";
                for (const auto& warning : converted.warnings) {
                    std::cerr << warning << '\n';
                }
                return 1;
            }
            ++matrix_routes;
        }
    }
    std::cout << "NK3_ALL_TO_ALL=PASS routes=" << matrix_routes << '\n';

    const char* configured_device = std::getenv("TENRIFF_NK3_DEVICE");
    const std::optional<std::string> saved_device =
        configured_device ? std::optional<std::string>{configured_device} : std::nullopt;
    set_device_environment("NPU");
    KeyModeConverterOptions rejected_options;
    rejected_options.algorithm = KeyModeConversionAlgorithm::NK3;
    rejected_options.target_lane_count = 8;
    rejected_options.sample_rate = 1000;
    rejected_options.base_bpm = 180.0;
    const auto rejected =
        tenriff::gameplay::convert_key_mode_chart(source, rejected_options);
    if (saved_device.has_value()) {
        set_device_environment(saved_device->c_str());
    } else {
        set_device_environment(nullptr);
    }
    const bool explains_device_contract =
        std::any_of(rejected.warnings.begin(), rejected.warnings.end(),
                    [](const std::string& warning) {
                        return warning.find("must be GPU or CPU") != std::string::npos;
                    });
    if (rejected.converted || !explains_device_contract) {
        std::cerr << "NK3 accepted the unsupported NPU device\n";
        return 1;
    }
    std::cout << "NK3_ONNX_SMOKE=PASS\n";
    return 0;
}
