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
    }

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
