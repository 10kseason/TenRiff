#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace tenriff::render {

inline float menu_ease_out(float progress) {
    const float t = 1.0f - std::clamp(progress, 0.0f, 1.0f);
    return 1.0f - t * t * t;
}

// UI magnification is presentation-only. Entering gameplay restores the
// original shared formats, including headers reused by the playfield HUD.
inline float menu_ui_text_scale(float requested, bool gameplay) {
    return gameplay || !std::isfinite(requested) ? 1.0f : std::clamp(requested, 1.0f, 1.30f);
}

struct NativeWordmarkMotion {
    float bloom = 0.10f;
    float scan = 0.35f;
    float sparkle = 0.0f;
    float sparkle_x = 0.0f;
    float glitch = 0.0f;
};

inline NativeWordmarkMotion native_wordmark_motion(double seconds, bool moving) {
    NativeWordmarkMotion value;
    if (!moving || !std::isfinite(seconds) || seconds < 0.0) return value;
    // One short, low-amplitude interference burst per 9.2 seconds. Reduced
    // motion never enters this path; it retains the still holographic tint.
    const double cycle = std::fmod(seconds, 9.2);
    value.bloom = 0.10f + 0.025f * static_cast<float>(std::sin(seconds * 1.3));
    value.scan = static_cast<float>(std::fmod(seconds * 0.12 + 0.35, 1.0));
    value.sparkle_x = static_cast<float>(std::fmod(seconds * 0.085 + 0.19, 1.0));
    value.sparkle = 0.42f * std::pow(std::max(0.0f, static_cast<float>(std::sin(seconds * 1.7))), 8.0f);
    if (cycle >= 6.8 && cycle < 6.98) {
        value.glitch = static_cast<float>(std::sin((cycle - 6.8) * 95.0)) * 2.6f;
    }
    return value;
}

// Render-thread state only. Never changes navigation, audio, scores or hit targets.
// Fixed slots avoid allocations while scrolling large libraries.
class NativeMenuMotion {
public:
    void begin(int screen, int64_t now_ns, bool enabled, bool reduced,
               float speed = 1.0f, float entry_seconds = 0.42f) {
        speed_ = std::isfinite(speed) ? std::clamp(speed, 0.0f, 4.0f) : 1.0f;
        entry_scale_ = std::isfinite(entry_seconds) ? std::clamp(entry_seconds, 0.05f, 2.0f) / 0.42f : 1.0f;
        reduced = reduced || speed_ == 0.0f;
        const bool changed = !active_ || screen != screen_ || reduced != reduced_;
        dt_ = active_ && now_ns >= previous_ns_
            ? std::clamp(static_cast<float>((now_ns - previous_ns_) * 1e-9), 0.0f, 0.05f) : 0.0f;
        if (!enabled) { active_ = false; return; }
        if (changed || now_ns < previous_ns_) {
            entered_ns_ = now_ns;
            slots_ = {};
            dt_ = 0.0f;
        }
        active_ = true;
        screen_ = screen;
        reduced_ = reduced;
        previous_ns_ = now_ns;
        seconds_ = std::max(0.0, (now_ns - entered_ns_) * 1e-9);
    }
    float entrance(float delay = 0.0f, float duration = 0.42f) const {
        if (!active_ || reduced_) return 1.0f;
        return menu_ease_out(static_cast<float>((seconds_ * speed_ - delay) / std::max(0.01f, duration * entry_scale_)));
    }
    double seconds() const { return reduced_ || !active_ ? 0.0 : seconds_ * speed_; }
    bool moving() const { return active_ && !reduced_; }
    float focus(std::size_t slot, int identity, bool selected) {
        auto& value = slots_[slot % slots_.size()];
        const float target = selected ? 1.0f : 0.0f;
        if (!value.initialized || value.identity != identity || reduced_ || !active_) {
            value = {identity, target, true};
        } else {
            value.level += (target - value.level) * (1.0f - std::exp(-dt_ * speed_ * 18.0f));
        }
        return value.level;
    }
private:
    struct Slot { int identity = 0; float level = 0; bool initialized = false; };
    std::array<Slot, 128> slots_{};
    bool active_ = false;
    bool reduced_ = false;
    int screen_ = -1;
    int64_t entered_ns_ = 0;
    int64_t previous_ns_ = 0;
    double seconds_ = 0;
    float dt_ = 0;
    float speed_ = 1;
    float entry_scale_ = 1;
};

} // namespace tenriff::render
