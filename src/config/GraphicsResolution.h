#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tenriff::config {

// Keep the old profile tokens; explicit dimensions use the same string field.
inline std::pair<int, int> graphics_resolution_dimensions(std::string_view value) {
    const auto trim = [](std::string_view text) {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
        return text;
    };
    value = trim(value);
    std::string lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lower == "720p") return {1280, 720};
    if (lower == "900p") return {1600, 900};
    if (lower == "1080p") return {1920, 1080};
    if (lower == "qhd" || lower == "1440p") return {2560, 1440};
    if (lower == "4k" || lower == "uhd" || lower == "2160p") return {3840, 2160};
    const auto separator = lower.find('x');
    if (separator == std::string::npos) return {0, 0};
    const auto width_text = trim(std::string_view(lower).substr(0, separator));
    const auto height_text = trim(std::string_view(lower).substr(separator + 1));
    int width = 0, height = 0;
    const auto parse = [](std::string_view text, int& number) {
        if (text.empty()) return false;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
        // Bound explicit allocations while allowing portrait and 8K displays.
        return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
               number >= 320 && number <= 8192;
    };
    if (!parse(width_text, width) || !parse(height_text, height)) return {0, 0};
    return {width, height};
}

inline std::string normalize_graphics_resolution(std::string_view value) {
    const auto [width, height] = graphics_resolution_dimensions(value);
    if (width == 0 || height == 0) return "native";
    if (width == 1280 && height == 720) return "720p";
    if (width == 1920 && height == 1080) return "1080p";
    if (width == 2560 && height == 1440) return "qhd";
    return std::to_string(width) + "x" + std::to_string(height);
}

inline std::vector<std::string> graphics_resolution_choices(
    const std::vector<std::pair<int, int>>& display_modes = {}) {
    static constexpr std::array<std::string_view, 22> common{
        "640x480", "800x600", "1024x768", "1152x864", "720p", "1280x800",
        "1280x1024", "1360x768", "1366x768", "1440x900", "1600x900",
        "1600x1200", "1680x1050", "1080p", "1920x1200", "2560x1080",
        "qhd", "2560x1600", "3440x1440", "3840x1080", "3840x1600", "3840x2160"};
    std::vector<std::string> choices;
    for (const auto value : common) choices.emplace_back(value);
    for (const auto [width, height] : display_modes) {
        const auto token = normalize_graphics_resolution(std::to_string(width) + "x" + std::to_string(height));
        if (token != "native") choices.push_back(token);
    }
    std::sort(choices.begin(), choices.end(), [](const auto& left, const auto& right) {
        return graphics_resolution_dimensions(left) < graphics_resolution_dimensions(right);
    });
    choices.erase(std::unique(choices.begin(), choices.end()), choices.end());
    choices.insert(choices.begin(), "native");
    return choices;
}

inline std::string cycle_graphics_resolution(std::string_view current, int direction,
                                            std::vector<std::string> choices = graphics_resolution_choices()) {
    const auto token = normalize_graphics_resolution(current);
    if (direction == 0) return token;
    // A saved custom size remains a choice even when the monitor changes.
    if (std::find(choices.begin(), choices.end(), token) == choices.end()) {
        choices.push_back(token);
        std::sort(choices.begin(), choices.end(), [](const auto& left, const auto& right) {
            return graphics_resolution_dimensions(left) < graphics_resolution_dimensions(right);
        });
    }
    const auto index = static_cast<std::size_t>(std::find(choices.begin(), choices.end(), token) - choices.begin());
    const auto next = direction < 0 ? (index == 0 ? choices.size() - 1 : index - 1)
                                    : (index + 1) % choices.size();
    return choices[next];
}

}  // namespace tenriff::config
