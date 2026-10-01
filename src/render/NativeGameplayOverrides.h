#pragma once
#include "app/ImportedGameplaySkin.h"
#include <algorithm>
#include <cmath>
#include <memory>
namespace tenriff::render {
inline const app::NativeGameplaySkinStyle& native_gameplay_style(
    const std::shared_ptr<const app::ImportedGameplaySkinDefinition>& skin, bool enabled=true) {
    static const app::NativeGameplaySkinStyle empty;
    return enabled && skin && skin->native_renderer ? skin->native : empty;
}
inline float native_gameplay_number(const app::NativeGameplaySkinStyle& style, const char* key, bool motion=false) {
    const auto& values=motion?style.motion:style.metrics;
    auto resolve=[&](const auto& slots) {
        for (const auto& slot:slots) if(std::string_view(key)==slot.key) {
            const auto it=values.find(key);
            return it==values.end() || !std::isfinite(it->second) ? slot.value : std::clamp(it->second,slot.minimum,slot.maximum);
        }
        return 0.0f;
    };
    return motion?resolve(app::kNativeGameplayMotion):resolve(app::kNativeGameplayMetrics);
}
// Keep an authored native line width at the default note height, then scale both
// together. Imported note-sized judgement rectangles already use this ratio.
inline float native_gameplay_judgement_line_width(
    const app::NativeGameplaySkinStyle& style, double note_height_scale) {
    const double scale = std::isfinite(note_height_scale)
        ? std::clamp(note_height_scale, 0.5, 4.0) : 1.8;
    return native_gameplay_number(style, "judgement_line_width") * static_cast<float>(scale / 1.8);
}
// The key backdrop is independent of hit-burst brightness and static lane tint.
// Releasing a key removes it even when a hit animation is still fading out.
inline float gameplay_key_backdrop_alpha(bool enabled, bool pressed, double opacity) {
    return enabled && pressed && std::isfinite(opacity)
        ? static_cast<float>(std::clamp(opacity, 0.0, 1.0)) : 0.0f;
}
// Brightness changes RGB, not alpha: it remains independent of backdrop opacity.
inline std::uint32_t gameplay_key_backdrop_color(std::uint32_t rgb, double brightness) {
    const double gain = std::isfinite(brightness) ? std::clamp(brightness, 0.0, 2.0) : 1.0;
    const auto channel = [&](int shift) {
        return static_cast<std::uint32_t>(std::lround(std::min(255.0,
            static_cast<double>((rgb >> shift) & 0xFFu) * gain)));
    };
    return (channel(16) << 16u) | (channel(8) << 8u) | channel(0);
}
// Anchor to the field bottom so 100% exactly preserves the original full lane.
inline float gameplay_key_backdrop_top(float field_top, float field_bottom, double height) {
    const float ratio = static_cast<float>(std::isfinite(height) ? std::clamp(height, 0.0, 1.0) : 1.0);
    return field_bottom - std::max(0.0f, field_bottom - field_top) * ratio;
}
inline std::array<float,4> native_gameplay_color(const app::NativeGameplaySkinStyle& style,const char* key,
                                                std::array<float,4> fallback) {
    const auto it=style.colors.find(key); return it==style.colors.end()?fallback:it->second;
}
inline float native_gameplay_alpha(const app::NativeGameplaySkinStyle& style,const char* key) {
    const auto it=style.colors.find(key); return it==style.colors.end()?1.0f:std::clamp(it->second[3],0.0f,1.0f);
}
inline std::uint32_t native_gameplay_rgb(const app::NativeGameplaySkinStyle& style,const char* key,std::uint32_t fallback) {
    const auto it=style.colors.find(key); if(it==style.colors.end()) return fallback;
    const auto byte=[](float f){return static_cast<std::uint32_t>(std::lround(std::clamp(f,0.0f,1.0f)*255));};
    return (byte(it->second[0])<<16u)|(byte(it->second[1])<<8u)|byte(it->second[2]);
}
inline std::array<float,4> native_gameplay_rect(const app::NativeGameplaySkinStyle& style,const char* key,std::array<float,4> rect) {
    const auto it=style.rects.find(key); if(it==style.rects.end()) return rect;
    const auto& d=it->second; for(float v:d) if(!std::isfinite(v)) return rect;
    // Gauge bevels reserve four pixels per edge; collapsed edits still need a valid fill.
    const float minimum=std::string_view(key)=="gauge"?12.0f:1.0f;
    const float w=std::max(minimum,rect[2]-rect[0]+d[2]),h=std::max(minimum,rect[3]-rect[1]+d[3]);
    return {rect[0]+d[0],rect[1]+d[1],rect[0]+d[0]+w,rect[1]+d[1]+h};
}
} // namespace tenriff::render
