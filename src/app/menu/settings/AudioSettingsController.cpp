#include "app/menu/settings/AudioSettingsController.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace tenriff::app::menu::settings {
namespace {

constexpr NumericSettingRange kMasterVolumeRange{0.0, 1.0, 0.05};
constexpr NumericSettingRange kChartMixVolumeRange{0.0, 2.0, 0.05};
constexpr NumericSettingRange kSoundOffsetRange{
    config::kSoundOffsetMin,
    config::kSoundOffsetMax,
    1.0,
};

std::string to_lower_ascii(std::string_view value) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return normalized;
}

double snap_to_range(double value, const NumericSettingRange& range) {
    if (!std::isfinite(value)) {
        return range.minimum;
    }
    const double clamped = std::clamp(value, range.minimum, range.maximum);
    const double steps = std::round((clamped - range.minimum) / range.step);
    return std::clamp(range.minimum + steps * range.step, range.minimum, range.maximum);
}

double value_from_ratio(double ratio, const NumericSettingRange& range) {
    const double safe_ratio = std::isfinite(ratio) ? std::clamp(ratio, 0.0, 1.0) : 0.0;
    return snap_to_range(
        range.minimum + safe_ratio * (range.maximum - range.minimum),
        range);
}

bool set_numeric_value(double& destination, double value, const NumericSettingRange& range) {
    const double next = snap_to_range(value, range);
    if (destination == next) {
        return false;
    }
    destination = next;
    return true;
}

void apply_audio_preset(config::RuntimeConfig& runtime) {
    if (runtime.audio_ui.preset == "basic") {
        runtime.audio.frames_per_buffer = 256;
        runtime.audio.periods = 3;
    } else {
        runtime.audio.frames_per_buffer = 320;
        runtime.audio.periods = 3;
    }
}

std::string cycle_keysound_policy(std::string_view current, int direction) {
    static constexpr std::array<std::string_view, 3> kPolicies{
        "follow",
        "autoplay",
        "ignore",
    };
    std::size_t index = 0;
    const std::string normalized = to_lower_ascii(current);
    for (std::size_t candidate = 0; candidate < kPolicies.size(); ++candidate) {
        if (normalized == kPolicies[candidate]) {
            index = candidate;
            break;
        }
    }

    if (direction < 0) {
        index = index == 0 ? kPolicies.size() - 1 : index - 1;
    } else {
        index = (index + 1) % kPolicies.size();
    }
    return std::string(kPolicies[index]);
}

}  // namespace

std::optional<std::size_t> audio_setting_index(AudioSettingId id) noexcept {
    for (std::size_t index = 0; index < kAudioSettingOrder.size(); ++index) {
        if (kAudioSettingOrder[index] == id) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<AudioSettingId> audio_setting_id_at(std::size_t index) noexcept {
    if (index >= kAudioSettingOrder.size()) {
        return std::nullopt;
    }
    return kAudioSettingOrder[index];
}

std::optional<NumericSettingRange> audio_setting_numeric_range(AudioSettingId id) noexcept {
    switch (id) {
        case AudioSettingId::MasterVolume:
            return kMasterVolumeRange;
        case AudioSettingId::BgmVolume:
        case AudioSettingId::KeysoundVolume:
            return kChartMixVolumeRange;
        case AudioSettingId::SoundOffset:
            return kSoundOffsetRange;
        case AudioSettingId::Preset:
        case AudioSettingId::KeysoundMode:
        case AudioSettingId::BackgroundSound:
        case AudioSettingId::Back:
            return std::nullopt;
    }
    return std::nullopt;
}

AudioSettingId AudioSettingsController::selected_id() const noexcept {
    return selected_id_;
}

bool AudioSettingsController::dirty() const noexcept {
    return dirty_;
}

void AudioSettingsController::reset(AudioSettingId selected) noexcept {
    selected_id_ = audio_setting_index(selected).has_value() ? selected : AudioSettingId::Preset;
    dirty_ = false;
}

MenuEffectFlags AudioSettingsController::select(AudioSettingId target) noexcept {
    if (!audio_setting_index(target).has_value() || selected_id_ == target) {
        return {};
    }
    selected_id_ = target;
    return MenuEffectFlags{true, false, false, false};
}

MenuEffectFlags AudioSettingsController::handle(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    std::optional<AudioSettingId> target) {
    if (action.kind == MenuActionKind::Back) {
        return leave_screen();
    }
    if (action.kind == MenuActionKind::Move) {
        return move_selection(action.direction);
    }

    MenuEffectFlags effects;
    if (target.has_value()) {
        if (!audio_setting_index(*target).has_value()) {
            return effects;
        }
        effects.merge(select(*target));
    }
    effects.merge(apply_selected_action(action, runtime));
    return effects;
}

MenuEffectFlags AudioSettingsController::move_selection(int direction) noexcept {
    if (direction == 0) {
        return {};
    }
    const auto current_index = audio_setting_index(selected_id_);
    if (!current_index.has_value()) {
        selected_id_ = AudioSettingId::Preset;
        return MenuEffectFlags{true, false, false, false};
    }

    const std::size_t next_index = direction < 0
        ? (*current_index == 0 ? 0 : *current_index - 1)
        : std::min(*current_index + 1, kAudioSettingOrder.size() - 1);
    if (next_index == *current_index) {
        return {};
    }
    selected_id_ = kAudioSettingOrder[next_index];
    return MenuEffectFlags{true, false, false, false};
}

MenuEffectFlags AudioSettingsController::apply_selected_action(
    const MenuAction& action,
    config::RuntimeConfig& runtime) {
    const bool is_adjust = action.kind == MenuActionKind::Adjust && action.direction != 0;
    const bool is_activate = action.kind == MenuActionKind::Activate;
    const bool is_set_ratio = action.kind == MenuActionKind::SetRatio;
    bool changed = false;

    switch (selected_id_) {
        case AudioSettingId::Preset:
            if (is_adjust) {
                runtime.audio_ui.preset = runtime.audio_ui.preset == "basic" ? "high" : "basic";
                apply_audio_preset(runtime);
                changed = true;
            }
            break;
        case AudioSettingId::KeysoundMode:
            if (is_adjust) {
                const std::string next = cycle_keysound_policy(
                    runtime.audio_ui.bms_keysound_policy,
                    action.direction);
                if (runtime.audio_ui.bms_keysound_policy != next) {
                    runtime.audio_ui.bms_keysound_policy = next;
                    changed = true;
                }
            }
            break;
        case AudioSettingId::BackgroundSound:
            if (is_adjust || is_activate) {
                runtime.audio_ui.background_sound_enabled =
                    !runtime.audio_ui.background_sound_enabled;
                changed = true;
            }
            break;
        case AudioSettingId::MasterVolume:
            if (is_adjust) {
                changed = set_numeric_value(
                    runtime.audio_ui.master_volume,
                    runtime.audio_ui.master_volume +
                        static_cast<double>(action.direction) * kMasterVolumeRange.step,
                    kMasterVolumeRange);
            } else if (is_set_ratio) {
                changed = set_numeric_value(
                    runtime.audio_ui.master_volume,
                    value_from_ratio(action.ratio, kMasterVolumeRange),
                    kMasterVolumeRange);
            }
            break;
        case AudioSettingId::BgmVolume:
            if (is_adjust) {
                changed = set_numeric_value(
                    runtime.audio_ui.bgm_volume,
                    runtime.audio_ui.bgm_volume +
                        static_cast<double>(action.direction) * kChartMixVolumeRange.step,
                    kChartMixVolumeRange);
            } else if (is_set_ratio) {
                changed = set_numeric_value(
                    runtime.audio_ui.bgm_volume,
                    value_from_ratio(action.ratio, kChartMixVolumeRange),
                    kChartMixVolumeRange);
            }
            break;
        case AudioSettingId::KeysoundVolume:
            if (is_adjust) {
                changed = set_numeric_value(
                    runtime.audio_ui.keysound_volume,
                    runtime.audio_ui.keysound_volume +
                        static_cast<double>(action.direction) * kChartMixVolumeRange.step,
                    kChartMixVolumeRange);
            } else if (is_set_ratio) {
                changed = set_numeric_value(
                    runtime.audio_ui.keysound_volume,
                    value_from_ratio(action.ratio, kChartMixVolumeRange),
                    kChartMixVolumeRange);
            }
            break;
        case AudioSettingId::SoundOffset:
            if (is_adjust) {
                changed = set_numeric_value(
                    runtime.sound_offset_ms,
                    runtime.sound_offset_ms +
                        static_cast<double>(action.direction) * kSoundOffsetRange.step,
                    kSoundOffsetRange);
            }
            break;
        case AudioSettingId::Back:
            if (is_activate) {
                return leave_screen();
            }
            break;
    }

    return changed ? mark_changed() : MenuEffectFlags{};
}

MenuEffectFlags AudioSettingsController::leave_screen() noexcept {
    const bool was_dirty = dirty_;
    selected_id_ = AudioSettingId::Preset;
    dirty_ = false;
    return MenuEffectFlags{
        true,
        was_dirty,
        was_dirty,
        true,
    };
}

MenuEffectFlags AudioSettingsController::mark_changed() noexcept {
    dirty_ = true;
    return MenuEffectFlags{true, false, false, false};
}

}  // namespace tenriff::app::menu::settings
