#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::gameplay {

enum class ChartFormatMode {
    Auto,
    Bms,
    Osu,
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
    Keys16,
};

enum class GaugeMode {
    ExHard,
    Hard,
    Normal,
    Easy,
};

enum class RandomMode {
    Off,
    Mirror,
    FullRandom,
    SuperRandom,
};

struct ModeSettings {
    ChartFormatMode format = ChartFormatMode::Auto;
    KeyMode key_mode = KeyMode::Auto;
    GaugeMode gauge = GaugeMode::Normal;
    RandomMode random = RandomMode::Off;
    uint32_t random_seed = 0;
};

std::string to_string(ChartFormatMode mode);
std::string to_string(KeyMode mode);
std::string to_string(GaugeMode mode);
std::string to_string(RandomMode mode);

std::optional<ChartFormatMode> parse_chart_format(std::string_view token);
std::optional<KeyMode> parse_key_mode(std::string_view token);
std::optional<GaugeMode> parse_gauge_mode(std::string_view token);
std::optional<RandomMode> parse_random_mode(std::string_view token);

}  // namespace tenriff::gameplay
