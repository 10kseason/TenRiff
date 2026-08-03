#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gameplay/GameplayChart.h"
#include "gameplay/ModeSettings.h"

namespace tenriff::gameplay {

struct KeyModeConverterOptions {
  int target_lane_count = 0;
  int max_keys = 0;
  int min_keys = 0;
  int transform_speed_slot = 4;
  uint32_t seed = 0;
  double base_bpm = 180.0;
  int sample_rate = 44100;
  KeyModeConversionAlgorithm algorithm = KeyModeConversionAlgorithm::Krrcream;
  Nk2Preset nk2_preset = Nk2Preset::Native;
};

struct KeyModeConverterResult {
  GameplayChart chart;
  bool converted = false;
  std::vector<std::string> warnings;
};

[[nodiscard]] KeyModeConverterResult
convert_key_mode_chart(const GameplayChart &chart,
                       const KeyModeConverterOptions &options);

} // namespace tenriff::gameplay
