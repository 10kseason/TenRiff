#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::gameplay {

enum class KeyModeConversionAlgorithm {
    Krrcream,
    NK2,
    NK3,
};

enum class Nk2Preset {
    Native,
    Transform,
    // 65% support budget aimed at keeping the source placement readable.
    Remaster,
};

enum class KeyMode {
    Auto,
    Keys4,
    Keys5,
    Keys6,
    Keys7,
    Keys8,
    Keys9,
    Keys10,
    Keys12,
    Keys14,
    Keys16,
};

enum class GaugeMode {
    ExHard,
    Hard,
    Normal,
    Shift,
    Easy,
};

enum class RandomMode {
    Off,
    Mirror,
    RotateRandom,
    FullRandom,
    SuperRandom,
};

struct ModeSettings {
    KeyMode key_mode = KeyMode::Auto;
    KeyModeConversionAlgorithm key_conversion_algorithm = KeyModeConversionAlgorithm::Krrcream;
    Nk2Preset key_conversion_nk2_preset = Nk2Preset::Native;
    GaugeMode gauge = GaugeMode::Normal;
    RandomMode random = RandomMode::Off;
    uint32_t random_seed = 0;
    bool dp_flip = false;
};

std::string to_string(KeyModeConversionAlgorithm algorithm);
std::string to_string(Nk2Preset preset);
std::string to_string(KeyMode mode);
std::string to_string(GaugeMode mode);
std::string to_string(RandomMode mode);

std::optional<KeyModeConversionAlgorithm> parse_key_mode_conversion_algorithm(std::string_view token);
std::optional<Nk2Preset> parse_nk2_preset(std::string_view token);
std::optional<KeyMode> parse_key_mode(std::string_view token);
std::optional<GaugeMode> parse_gauge_mode(std::string_view token);
std::optional<RandomMode> parse_random_mode(std::string_view token);

}  // namespace tenriff::gameplay
