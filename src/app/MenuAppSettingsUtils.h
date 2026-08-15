#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/MenuAppSkinUtils.h"
#include "app/ModeManager.h"
#include "config/Config.h"

namespace tenriff::app {

inline constexpr double kVisualOffsetMin = config::kVisualOffsetMin;
inline constexpr double kVisualOffsetMax = config::kVisualOffsetMax;
inline constexpr double kVisualOffsetStep = 1.0;
inline constexpr double kSoundOffsetMin = config::kSoundOffsetMin;
inline constexpr double kSoundOffsetMax = config::kSoundOffsetMax;
inline constexpr double kSoundOffsetStep = 1.0;
inline constexpr double kVolumeMin = 0.0;
inline constexpr double kVolumeMax = 1.0;
inline constexpr double kVolumeStep = 0.05;
inline constexpr double kChartMixVolumeMin = 0.0;
inline constexpr double kChartMixVolumeMax = 2.0;
inline constexpr double kChartMixVolumeStep = 0.05;
inline constexpr double kRateMin = 0.5;
inline constexpr double kRateMax = 2.0;
inline constexpr double kRateStep = 0.05;
inline constexpr double kHiSpeedMin = 0.5;
inline constexpr double kHiSpeedMax = 50.0;
// Menu and Song Select adjustments are fine-grained. In-play tuning keeps its
// independent 0.25 / 10.0 steps in GameSession.
inline constexpr double kHiSpeedStep = 0.01;
inline constexpr int kSeedMin = 0;
inline constexpr int kSeedMax = 9999;
inline constexpr double kPacemakerAccuracyStep = 0.5;
inline constexpr int64_t kPacemakerScoreStep = 100;
inline std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

inline std::string preset_label(const std::string& preset) {
    if (preset == "high") {
        return "High";
    }
    return "Basic";
}

inline std::string cycle_pacemaker_mode(std::string_view mode, int direction) {
    const std::string normalized = config::normalize_pacemaker_mode_token(mode);
    if (direction < 0) {
        if (normalized == "off") return "score";
        if (normalized == "score") return "accuracy";
        return "off";
    }
    if (normalized == "off") return "accuracy";
    if (normalized == "accuracy") return "score";
    return "off";
}

inline void apply_audio_preset(config::RuntimeConfig& config) {
    if (config.audio_ui.preset == "basic") {
        config.audio.frames_per_buffer = 256;
        config.audio.periods = 3;
    } else {
        config.audio.frames_per_buffer = 320;
        config.audio.periods = 3;
    }
}

inline std::string format_multiplier(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(2);
    stream << value << "x";
    return stream.str();
}

inline std::string format_decimal(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(2);
    stream << value;
    return stream.str();
}

inline std::string mode_score_summary(const std::vector<std::string>& mods, double rate) {
    return mode_mod_summary(mods) + " / " + format_multiplier(final_score_multiplier(mods, rate));
}

inline std::vector<std::string> cycle_mode_mod_category(const std::vector<std::string>& active_mods,
                                                        const ModeModCategoryDescriptor& category,
                                                        int direction) {
    std::vector<std::string> normalized = normalize_mode_mod_tokens(active_mods);
    int current_index = 0;
    for (std::size_t i = 0; i < category.mods.size(); ++i) {
        if (std::find(normalized.begin(), normalized.end(), std::string(category.mods[i]->token)) != normalized.end()) {
            current_index = static_cast<int>(i) + 1;
            break;
        }
    }

    const int option_count = static_cast<int>(category.mods.size()) + 1;
    int next_index = current_index + direction;
    if (next_index < 0) {
        next_index = option_count - 1;
    } else if (next_index >= option_count) {
        next_index = 0;
    }

    normalized.erase(std::remove_if(normalized.begin(),
                                    normalized.end(),
                                    [&](const std::string& token) {
                                        const auto* descriptor = find_mode_mod_descriptor(token);
                                        return descriptor && descriptor->category_token == category.token;
                                    }),
                     normalized.end());
    if (next_index > 0) {
        normalized.push_back(std::string(category.mods[static_cast<std::size_t>(next_index - 1)]->token));
    }
    return normalize_mode_mod_tokens(normalized);
}

inline std::string format_pixels(double value) {
    return std::to_string(static_cast<int>(std::lround(value))) + " px";
}

inline std::string format_signed_offset_ms(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(1);
    if (value >= 0.0) {
        stream << '+';
    }
    stream << value << " ms";
    return stream.str();
}

inline std::string display_label(const std::string& mode) {
    if (mode == "windowed") {
        return "Windowed";
    }
    if (mode == "fullscreen") {
        return "Fullscreen";
    }
    return "Borderless";
}

inline std::string normalize_display_mode(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "windowed" || value == "fullscreen") {
        return value;
    }
    return "borderless";
}

inline std::string cycle_display_mode(std::string current, int direction) {
    static constexpr const char* kDisplayModes[] = {"borderless", "windowed", "fullscreen"};
    const int option_count = static_cast<int>(sizeof(kDisplayModes) / sizeof(kDisplayModes[0]));
    current = normalize_display_mode(std::move(current));
    int current_index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (current == kDisplayModes[i]) {
            current_index = i;
            break;
        }
    }
    current_index += direction;
    if (current_index < 0) {
        current_index = option_count - 1;
    } else if (current_index >= option_count) {
        current_index = 0;
    }
    return kDisplayModes[current_index];
}

inline std::string normalize_resolution_preset(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "720p" || value == "1080p" || value == "qhd") {
        return value;
    }
    return "native";
}

inline std::string cycle_resolution_preset(std::string current, int direction) {
    static constexpr const char* kResolutionPresets[] = {"native", "720p", "1080p", "qhd"};
    const int option_count = static_cast<int>(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]));
    current = normalize_resolution_preset(std::move(current));
    int current_index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (current == kResolutionPresets[i]) {
            current_index = i;
            break;
        }
    }
    current_index += direction;
    if (current_index < 0) {
        current_index = option_count - 1;
    } else if (current_index >= option_count) {
        current_index = 0;
    }
    return kResolutionPresets[current_index];
}

inline std::pair<int, int> resolution_dimensions(std::string_view preset) {
    if (preset == "720p") {
        return {1280, 720};
    }
    if (preset == "1080p") {
        return {1920, 1080};
    }
    if (preset == "qhd") {
        return {2560, 1440};
    }
    return {0, 0};
}

inline std::string resolution_label(std::string_view preset) {
    if (preset == "720p") {
        return "1280x720";
    }
    if (preset == "1080p") {
        return "1920x1080";
    }
    if (preset == "qhd") {
        return "2560x1440";
    }
    return "Monitor Native";
}

inline std::string song_index_profile_label(const std::string& value) {
    const std::string normalized = config::normalize_song_index_profile_token(value);
    if (normalized == "fast") {
        return "Fast (Minimal)";
    }
    return "Safe";
}

inline std::string cycle_song_index_profile(std::string current, int direction) {
    static constexpr const char* kProfiles[] = {"safe", "fast"};
    current = config::normalize_song_index_profile_token(current);
    int current_index = 0;
    for (int i = 0; i < static_cast<int>(sizeof(kProfiles) / sizeof(kProfiles[0])); ++i) {
        if (current == kProfiles[i]) {
            current_index = i;
            break;
        }
    }
    current_index += direction;
    if (current_index < 0) {
        current_index = static_cast<int>(sizeof(kProfiles) / sizeof(kProfiles[0])) - 1;
    } else if (current_index >= static_cast<int>(sizeof(kProfiles) / sizeof(kProfiles[0]))) {
        current_index = 0;
    }
    return kProfiles[current_index];
}

inline bool ensure_difficulty_table_indexing(config::RuntimeConfig& runtime) {
    if (config::normalize_song_index_profile_token(runtime.mode.song_index_profile) != "fast") {
        return false;
    }
    // Fast deliberately omits file hashes, so a BMSTable can download successfully
    // while matching zero charts. Selecting a table means the player chose matching.
    runtime.mode.song_index_profile = "safe";
    return true;
}

inline std::string normalize_key_conversion_algorithm(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "nk2" || value == "nativek2" || value == "keyweaver" ||
        value == "keyweaver_nk2") {
        return "nk2";
    }
    return "krrcream";
}

inline std::string cycle_key_conversion_algorithm(std::string_view current) {
    return normalize_key_conversion_algorithm(std::string(current)) == "nk2"
               ? "krrcream"
               : "nk2";
}

inline std::string normalize_key_conversion_nk2_preset(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "transform" || value == "transform35") {
        return "transform";
    }
    if (value == "remaster" || value == "remaster65" || value == "rm") {
        return "remaster";
    }
    return "native";
}

inline std::string cycle_key_conversion_nk2_preset(std::string_view current) {
    const std::string normalized = normalize_key_conversion_nk2_preset(std::string(current));
    if (normalized == "native") {
        return "transform";
    }
    if (normalized == "transform") {
        return "remaster";
    }
    return "native";
}

inline std::string normalize_runtime_key_mode(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "none" || value == "auto") {
        return "none";
    }
    if (value == "4k" || value == "5k" || value == "6k" || value == "7k" || value == "8k" ||
        value == "9k" || value == "10k" || value == "12k" || value == "14k" || value == "16k") {
        return value;
    }
    return "none";
}

inline std::string cycle_runtime_key_mode(std::string_view current, int direction, bool allow_auto) {
    static constexpr const char* kAutoModes[] = {"none", "4k", "5k", "6k", "7k", "8k", "9k", "10k", "12k", "14k", "16k"};
    static constexpr const char* kConcreteModes[] = {"4k", "5k", "6k", "7k", "8k", "9k", "10k", "12k", "14k", "16k"};
    const auto* options = allow_auto ? kAutoModes : kConcreteModes;
    const int option_count = allow_auto ? static_cast<int>(sizeof(kAutoModes) / sizeof(kAutoModes[0]))
                                        : static_cast<int>(sizeof(kConcreteModes) / sizeof(kConcreteModes[0]));
    std::string normalized = normalize_runtime_key_mode(std::string(current));
    if (!allow_auto && normalized == "none") {
        normalized = "10k";
    }

    int index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (normalized == options[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = option_count - 1;
    } else if (index >= option_count) {
        index = 0;
    }
    return options[index];
}

inline std::string gauge_label(const std::string& value) {
    const std::string normalized = to_lower_ascii(value);
    if (normalized == "ex_hard" || normalized == "ex-hard" || normalized == "exhard") {
        return "EX";
    }
    if (normalized == "shift" || normalized == "gauge_shift" || normalized == "gauge-shift") {
        return "EX";
    }
    if (normalized == "hard") {
        return "Hard";
    }
    if (normalized == "easy") {
        return "Easy";
    }
    return "Normal";
}

inline std::string normalize_gauge_mode(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "ex_hard" || value == "ex-hard" || value == "exhard") {
        return "ex_hard";
    }
    if (value == "shift" || value == "gauge_shift" || value == "gauge-shift") {
        // Legacy Gauge Shift selections now mean an EX starting tier. Gauge
        // Shift itself is always enabled by the session runtime.
        return "ex_hard";
    }
    if (value == "hard" || value == "easy") {
        return value;
    }
    return "normal";
}

inline std::string cycle_gauge_mode(std::string current, int direction) {
    // Gauge Shift is always active; this selection chooses its starting tier.
    static constexpr const char* kGauges[] = {"easy", "normal", "hard", "ex_hard"};
    const int option_count = static_cast<int>(sizeof(kGauges) / sizeof(kGauges[0]));
    current = normalize_gauge_mode(std::move(current));
    int index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (current == kGauges[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = option_count - 1;
    } else if (index >= option_count) {
        index = 0;
    }
    return kGauges[index];
}

inline std::string random_label(const std::string& value) {
    const std::string normalized = to_lower_ascii(value);
    if (normalized == "mirror") {
        return "Mirror";
    }
    if (normalized == "fr") {
        return "Random";
    }
    if (normalized == "rr") {
        return "R-Random";
    }
    if (normalized == "sr") {
        return "S-Random";
    }
    return "Off";
}

inline std::string cycle_random_mode(std::string current, int direction) {
    static constexpr const char* kRandomModes[] = {"off", "mirror", "fr", "rr", "sr"};
    const int option_count = static_cast<int>(sizeof(kRandomModes) / sizeof(kRandomModes[0]));
    current = to_lower_ascii(std::move(current));
    int index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (current == kRandomModes[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = option_count - 1;
    } else if (index >= option_count) {
        index = 0;
    }
    return kRandomModes[index];
}

inline bool adjust_song_quick_setting(config::RuntimeConfig& runtime, int setting_index, int direction) {
    const int step_direction = direction < 0 ? -1 : 1;
    switch (setting_index) {
        case 0:
            runtime.visual_offset_ms = clamp_step_value(
                runtime.visual_offset_ms + static_cast<double>(step_direction) * kVisualOffsetStep,
                kVisualOffsetMin, kVisualOffsetMax, kVisualOffsetStep);
            break;
        case 1:
            runtime.speed.hi_speed = clamp_step_value(
                runtime.speed.hi_speed + static_cast<double>(step_direction) * kHiSpeedStep,
                kHiSpeedMin, kHiSpeedMax, kHiSpeedStep);
            break;
        case 2:
            runtime.mode.gauge = cycle_gauge_mode(runtime.mode.gauge, step_direction);
            break;
        case 3:
            runtime.mode.random = cycle_random_mode(runtime.mode.random, step_direction);
            break;
        default:
            return false;
    }
    return true;
}

inline std::string keysound_policy_label(std::string_view policy) {
    const std::string normalized = to_lower_ascii(std::string(policy));
    if (normalized == "autoplay") {
        return "Autoplay";
    }
    if (normalized == "ignore" || normalized == "off") {
        return "Off";
    }
    return "Follow";
}

inline std::string cycle_bms_keysound_policy(std::string_view current, int direction) {
    static constexpr const char* kPolicies[] = {"follow", "autoplay", "ignore"};
    int index = 0;
    const std::string normalized = to_lower_ascii(std::string(current));
    for (int i = 0; i < 3; ++i) {
        if (normalized == kPolicies[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = 2;
    } else if (index >= 3) {
        index = 0;
    }
    return kPolicies[index];
}

}  // namespace tenriff::app
