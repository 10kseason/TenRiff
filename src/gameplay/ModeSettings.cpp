#include "gameplay/ModeSettings.h"

#include <algorithm>

namespace tenriff::gameplay {

std::string to_string(KeyModeConversionAlgorithm algorithm) {
    switch (algorithm) {
        case KeyModeConversionAlgorithm::NK3: return "nk3";
        case KeyModeConversionAlgorithm::NK2: return "nk2";
        case KeyModeConversionAlgorithm::Krrcream: default: return "krrcream";
    }
}

std::string to_string(Nk2Preset preset) {
    switch (preset) {
        case Nk2Preset::Transform: return "transform";
        case Nk2Preset::Remaster: return "remaster";
        case Nk2Preset::Native: break;
    }
    return "native";
}

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

std::optional<KeyModeConversionAlgorithm> parse_key_mode_conversion_algorithm(std::string_view token) {
    const std::string normalized = normalize(token);
    if (normalized.empty() || normalized == "KRR" || normalized == "KRRCREAM" ||
        normalized == "LEGACY" || normalized == "N2NC") {
        return KeyModeConversionAlgorithm::Krrcream;
    }
    if (normalized == "NK2" || normalized == "NATIVEK2" ||
        normalized == "KEYWEAVER" || normalized == "KEYWEAVERNK2" ||
        normalized == "KEYWEAVER_NK2") {
        return KeyModeConversionAlgorithm::NK2;
    }
    if (normalized == "NK3" || normalized == "KEYWEAVERNK3" ||
        normalized == "KEYWEAVER_NK3" || normalized == "VCRR") {
        return KeyModeConversionAlgorithm::NK3;
    }
    return std::nullopt;
}

std::optional<Nk2Preset> parse_nk2_preset(std::string_view token) {
    const std::string normalized = normalize(token);
    if (normalized.empty() || normalized == "NATIVE" || normalized == "NATIVE12") {
        return Nk2Preset::Native;
    }
    if (normalized == "TRANSFORM" || normalized == "TRANSFORM35") {
        return Nk2Preset::Transform;
    }
    if (normalized == "REMASTER" || normalized == "REMASTER65" || normalized == "RM") {
        return Nk2Preset::Remaster;
    }
    return std::nullopt;
}

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
    if (normalized == "FR" || normalized == "FULLRANDOM" || normalized == "FULL_RANDOM" ||
        normalized == "FRNS" || normalized == "FR_NO_SCRATCH" ||
        normalized == "RANDOM_NO_SCRATCH") {
        return RandomMode::FullRandom;
    }
    if (normalized == "SR" || normalized == "SUPERRANDOM" || normalized == "SUPER_RANDOM") {
        return RandomMode::SuperRandom;
    }
    return std::nullopt;
}

}  // namespace tenriff::gameplay
