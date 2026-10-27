#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::render {

inline constexpr std::uint32_t kClearEasyRgb = 0x00ED57;
inline constexpr std::uint32_t kClearNormalRgb = 0xFFF000;
inline constexpr std::uint32_t kClearHardRgb = 0xFF3838;
inline constexpr std::uint32_t kClearExRgb = 0xFF44EF; // Fallback when a gradient cannot be allocated.
inline constexpr std::uint32_t kGradeLowRgb = 0xC2A0FF;
inline constexpr std::array<std::uint32_t, 7> kClearRainbowRgb{
    0xFF32ED, 0xFF4848, 0xFFF000, 0x00ED57, 0x00E9FF, 0x7755FF, 0xFF32ED};
inline constexpr std::array<float, 7> kClearRainbowStops{0.0f, 0.18f, 0.36f, 0.54f, 0.72f, 0.88f, 1.0f};

inline bool grade_uses_clear_rainbow(std::wstring_view grade) {
    return grade == L"S+" || grade == L"S++" || grade == L"S+++" ||
           grade == L"SS" || grade == L"SSS" || grade == L"AA" || grade == L"AAA";
}

inline std::uint32_t grade_display_color(std::wstring_view grade) {
    if (grade_uses_clear_rainbow(grade)) return kClearExRgb;
    if (grade == L"S") return kClearHardRgb;
    if (grade == L"A+" || grade == L"A") return kClearNormalRgb;
    if (grade == L"B+" || grade == L"B" || grade == L"C+" || grade == L"C" || grade == L"D") return kClearEasyRgb;
    if (grade == L"E" || grade == L"F") return kGradeLowRgb;
    return 0xB6C0D0; // AUTO / no record / abort retain their neutral label.
}

inline bool gameplay_gauge_is_ex(std::string_view gauge_token) {
    // Live MenuApp HUDs use EX; keep the older preview label compatible.
    return gauge_token == "EX" || gauge_token == "EX-HARD";
}

inline std::uint32_t gameplay_gauge_color(std::string_view gauge_token) {
    if (gameplay_gauge_is_ex(gauge_token)) {
        return kClearExRgb;
    }
    if (gauge_token == "HARD") {
        return kClearHardRgb;
    }
    if (gauge_token == "EASY") {
        return kClearEasyRgb;
    }
    return kClearNormalRgb;
}

inline std::uint32_t song_select_gauge_text_color(std::string_view gauge_tier) {
    if (gauge_tier == "ex_hard") {
        return kClearExRgb;
    }
    if (gauge_tier == "hard") {
        return kClearHardRgb;
    }
    if (gauge_tier == "easy") {
        return kClearEasyRgb;
    }
    return kClearNormalRgb;
}

struct SongSelectLampPresentation {
    std::string label;
    std::optional<std::uint32_t> color;
    bool tint_card = false;
    std::string badge_label;
    std::uint32_t badge_rgb = 0;
    bool rainbow_badge = false;
};

inline SongSelectLampPresentation song_select_lamp_presentation(std::string_view status) {
    SongSelectLampPresentation result{std::string(status), std::nullopt, false};
    constexpr std::string_view shift = "GAUGE SHIFT ";
    // Presentation only: stored statuses still distinguish Gauge Shift records.
    // Keep assisted-play prefixes visible and never tint them as a normal clear.
    const bool assisted = status.substr(0, 7) == "ASSIST ";
    const auto shift_at = result.label.find(shift);
    if (shift_at == 0 || (assisted && shift_at != std::string::npos)) {
        result.label.erase(shift_at, shift.size());
    }
    const std::string_view label(result.label);
    if (label == "EX CLEAR" || label == "EX-HARD CLEAR") {
        result.color = song_select_gauge_text_color("ex_hard");
        result.badge_label = "EX CLEAR";
        result.badge_rgb = kClearExRgb;
        result.rainbow_badge = true;
    } else if (label == "HARD CLEAR") {
        result.color = song_select_gauge_text_color("hard");
        result.badge_label = "HD CLEAR";
        result.badge_rgb = kClearHardRgb;
    } else if (label == "EASY CLEAR") {
        result.color = song_select_gauge_text_color("easy");
        result.badge_label = "EZ CLEAR";
        result.badge_rgb = kClearEasyRgb;
    } else if (label == "CLEAR" || label == "NORMAL CLEAR") {
        result.color = song_select_gauge_text_color("normal");
        result.badge_label = "NM CLEAR";
        result.badge_rgb = kClearNormalRgb;
    }
    result.tint_card = result.color.has_value();
    return result;
}

}  // namespace tenriff::render
