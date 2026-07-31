#include "gameplay/ModeSettings.h"

#include <algorithm>

namespace tenriff::gameplay {

std::string to_string(KeyMode mode) {
    switch (mode) {
        case KeyMode::Keys4: return "4K";
        case KeyMode::Keys5: return "5K";
        case KeyMode::Keys6: return "6K";
        case KeyMode::Keys7: return "7K";
        case KeyMode::Keys8: return "8K";
        case KeyMode::Keys9: return "9K";
        case KeyMode::Keys10: return "10K";
        case KeyMode::Keys12: return "12K";
        case KeyMode::Keys14: return "14K";
        case KeyMode::Keys16: return "16K";
        case KeyMode::Auto: default: return "NONE";
    }
}

std::string to_string(GaugeMode mode) {
    switch (mode) {
        case GaugeMode::ExHard: return "EX-HARD";
        case GaugeMode::Hard: return "HARD";
        case GaugeMode::Shift: return "SHIFT";
        case GaugeMode::Easy: return "EASY";
        case GaugeMode::Normal: default: return "NORMAL";
    }
}

std::string to_string(RandomMode mode) {
    switch (mode) {
        case RandomMode::Mirror: return "MIRROR";
        case RandomMode::RotateRandom: return "R-RANDOM";
        case RandomMode::FullRandom: return "FR";
        case RandomMode::SuperRandom: return "SR";
        case RandomMode::Off: default: return "OFF";
    }
}

namespace {

std::string normalize(std::string_view token) {
    std::string out(token);
    out.erase(std::remove_if(out.begin(), out.end(), [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }), out.end());
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return out;
}

}  // namespace

std::optional<KeyMode> parse_key_mode(std::string_view token) {
    std::string normalized = normalize(token);
    if (normalized == "AUTO" || normalized == "NONE" || normalized == "NATIVE") {
        return KeyMode::Auto;
    }
    if (normalized == "4K" || normalized == "4KEY" || normalized == "KEYS4") {
        return KeyMode::Keys4;
    }
    if (normalized == "5K" || normalized == "5KEY" || normalized == "KEYS5") {
        return KeyMode::Keys5;
    }
    if (normalized == "6K" || normalized == "6KEY" || normalized == "KEYS6") {
        return KeyMode::Keys6;
    }
    if (normalized == "7K" || normalized == "7KEY" || normalized == "KEYS7") {
        return KeyMode::Keys7;
    }
    if (normalized == "8K" || normalized == "8KEY" || normalized == "KEYS8") {
        return KeyMode::Keys8;
    }
    if (normalized == "9K" || normalized == "9KEY" || normalized == "KEYS9") {
        return KeyMode::Keys9;
    }
    if (normalized == "10K" || normalized == "10KEY" || normalized == "KEYS10") {
        return KeyMode::Keys10;
    }
    if (normalized == "12K" || normalized == "12KEY" || normalized == "KEYS12") {
        return KeyMode::Keys12;
    }
    if (normalized == "14K" || normalized == "14KEY" || normalized == "KEYS14") {
        return KeyMode::Keys14;
    }
    if (normalized == "16K" || normalized == "16KEY" || normalized == "KEYS16") {
        return KeyMode::Keys16;
    }
    return std::nullopt;
}

std::optional<GaugeMode> parse_gauge_mode(std::string_view token) {
    std::string normalized = normalize(token);
    if (normalized == "EXHARD" || normalized == "EX-HARD" || normalized == "EX_HARD") {
        return GaugeMode::ExHard;
    }
    if (normalized == "HARD") {
        return GaugeMode::Hard;
    }
    if (normalized == "NORMAL") {
        return GaugeMode::Normal;
    }
    if (normalized == "SHIFT" || normalized == "GAUGESHIFT" || normalized == "GAUGE_SHIFT") {
        return GaugeMode::Shift;
    }
    if (normalized == "EASY") {
        return GaugeMode::Easy;
    }
    return std::nullopt;
}

std::optional<RandomMode> parse_random_mode(std::string_view token) {
    std::string normalized = normalize(token);
    if (normalized == "OFF" || normalized == "NONE") {
        return RandomMode::Off;
    }
    if (normalized == "MIRROR") {
        return RandomMode::Mirror;
    }
    if (normalized == "RR" || normalized == "R-RANDOM" || normalized == "ROTATERANDOM") {
        return RandomMode::RotateRandom;
    }
    if (normalized == "FR" || normalized == "FULLRANDOM" || normalized == "FULL_RANDOM") {
        return RandomMode::FullRandom;
    }
    if (normalized == "SR" || normalized == "SUPERRANDOM" || normalized == "SUPER_RANDOM") {
        return RandomMode::SuperRandom;
    }
    return std::nullopt;
}

}  // namespace tenriff::gameplay
