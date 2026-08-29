#include "app/menu/settings/ModeSettingsController.h"

#include <algorithm>

#include "app/MenuAppSettingsUtils.h"

namespace tenriff::app::menu::settings {

std::optional<std::size_t> mode_setting_index(ModeSettingId id) noexcept {
    for (std::size_t index = 0; index < kModeSettingOrder.size(); ++index) {
        if (kModeSettingOrder[index] == id) return index;
    }
    return std::nullopt;
}

std::optional<ModeSettingId> mode_setting_id_at(std::size_t index) noexcept {
    if (index >= kModeSettingOrder.size()) return std::nullopt;
    return kModeSettingOrder[index];
}

bool ModeSettingsEffects::empty() const noexcept {
    return menu.empty() && !refresh_song_library && !show_mod_manager;
}

void ModeSettingsEffects::merge(const ModeSettingsEffects& other) noexcept {
    menu.merge(other.menu);
    refresh_song_library = refresh_song_library || other.refresh_song_library;
    show_mod_manager = show_mod_manager || other.show_mod_manager;
}

ModeSettingId ModeSettingsController::selected_id() const noexcept {
    return selected_id_;
}

std::size_t ModeSettingsController::selected_mod_category() const noexcept {
    return selected_mod_category_;
}

bool ModeSettingsController::dirty() const noexcept {
    return dirty_;
}

bool ModeSettingsController::library_dirty() const noexcept {
    return library_dirty_;
}

void ModeSettingsController::reset() noexcept {
    selected_id_ = ModeSettingId::Indexing;
    selected_mod_category_ = 0;
    dirty_ = false;
    library_dirty_ = false;
}

ModeSettingsEffects ModeSettingsController::select(ModeSettingId target) noexcept {
    if (!mode_setting_index(target).has_value() || selected_id_ == target) return {};
    selected_id_ = target;
    ModeSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

ModeSettingsEffects ModeSettingsController::handle(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    std::optional<ModeSettingId> target) {
    if (action.kind == MenuActionKind::Back) return leave_screen();
    if (action.kind == MenuActionKind::Move) return move_selection(action.direction);
    ModeSettingsEffects effects;
    if (target.has_value()) {
        if (!mode_setting_index(*target).has_value()) return effects;
        effects.merge(select(*target));
    }
    effects.merge(apply_selected_action(action, runtime));
    return effects;
}

ModeSettingsEffects ModeSettingsController::handle_mod_manager(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    std::optional<std::size_t> target_category) {
    const auto& categories = mode_mod_categories();
    ModeSettingsEffects effects;
    if (target_category.has_value()) {
        const std::size_t clamped = std::min(*target_category, categories.size());
        if (clamped != selected_mod_category_) {
            selected_mod_category_ = clamped;
            effects.menu.render_changed = true;
        }
    }
    if (action.kind == MenuActionKind::Move && action.direction != 0) {
        const std::size_t next = action.direction < 0
            ? (selected_mod_category_ == 0 ? 0 : selected_mod_category_ - 1)
            : std::min(selected_mod_category_ + 1, categories.size());
        if (next != selected_mod_category_) {
            selected_mod_category_ = next;
            effects.menu.render_changed = true;
        }
        return effects;
    }
    if (action.kind == MenuActionKind::Adjust && action.direction != 0 &&
        selected_mod_category_ < categories.size()) {
        runtime.mode.mods = cycle_mode_mod_category(
            runtime.mode.mods, categories[selected_mod_category_], action.direction);
        effects.merge(mark_changed());
        return effects;
    }
    if (action.kind == MenuActionKind::Back || action.kind == MenuActionKind::Activate) {
        selected_id_ = ModeSettingId::Mods;
        selected_mod_category_ = 0;
        effects.menu.render_changed = true;
        effects.menu.navigate_back = true;
    }
    return effects;
}

ModeSettingsEffects ModeSettingsController::move_selection(int direction) noexcept {
    if (direction == 0) return {};
    const auto current = mode_setting_index(selected_id_);
    const std::size_t index = current.value_or(0);
    const std::size_t next = direction < 0
        ? (index == 0 ? 0 : index - 1)
        : std::min(index + 1, kModeSettingOrder.size() - 1);
    if (next == index) return {};
    selected_id_ = kModeSettingOrder[next];
    ModeSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

ModeSettingsEffects ModeSettingsController::apply_selected_action(
    const MenuAction& action,
    config::RuntimeConfig& runtime) {
    const bool adjust = action.kind == MenuActionKind::Adjust && action.direction != 0;
    const bool activate = action.kind == MenuActionKind::Activate;
    if (selected_id_ == ModeSettingId::Back && activate) return leave_screen();
    if (selected_id_ == ModeSettingId::Mods && activate) {
        selected_mod_category_ = 0;
        ModeSettingsEffects effects;
        effects.menu.render_changed = true;
        effects.show_mod_manager = true;
        return effects;
    }
    if (activate) return leave_screen();
    if (!adjust) return {};
    const int direction = action.direction;

    switch (selected_id_) {
        case ModeSettingId::Indexing:
            runtime.mode.song_index_profile = cycle_song_index_profile(
                runtime.mode.song_index_profile, direction);
            return mark_changed(true);
        case ModeSettingId::IndexDifficulty:
            runtime.mode.calculate_song_index_difficulty =
                !runtime.mode.calculate_song_index_difficulty;
            return mark_changed(true);
        case ModeSettingId::GhostBattle:
            runtime.mode.ghost_battle_enabled = !runtime.mode.ghost_battle_enabled;
            return mark_changed();
        case ModeSettingId::Autoplay:
            runtime.mode.autoplay_enabled = !runtime.mode.autoplay_enabled;
            return mark_changed();
        case ModeSettingId::PracticeNoFail:
            runtime.mode.practice_no_fail_enabled = !runtime.mode.practice_no_fail_enabled;
            if (runtime.mode.practice_no_fail_enabled) {
                runtime.mode.one_miss_fail_enabled = false;
                runtime.mode.pacemaker_mode = "off";
            }
            return mark_changed();
        case ModeSettingId::SuddenDeath:
            runtime.mode.one_miss_fail_enabled = !runtime.mode.one_miss_fail_enabled;
            if (runtime.mode.one_miss_fail_enabled) {
                runtime.mode.practice_no_fail_enabled = false;
                runtime.mode.pacemaker_mode = "off";
            }
            return mark_changed();
        case ModeSettingId::Pacemaker:
            runtime.mode.pacemaker_mode = cycle_pacemaker_mode(
                runtime.mode.pacemaker_mode, direction);
            if (config::normalize_pacemaker_mode_token(runtime.mode.pacemaker_mode) != "off") {
                runtime.mode.practice_no_fail_enabled = false;
                runtime.mode.one_miss_fail_enabled = false;
            }
            return mark_changed();
        case ModeSettingId::PacemakerTarget: {
            const std::string mode =
                config::normalize_pacemaker_mode_token(runtime.mode.pacemaker_mode);
            if (mode == "accuracy") {
                runtime.mode.pacemaker_target_accuracy = clamp_step_value(
                    runtime.mode.pacemaker_target_accuracy + direction * kPacemakerAccuracyStep,
                    config::kPacemakerAccuracyMin,
                    config::kPacemakerAccuracyMax,
                    kPacemakerAccuracyStep);
                return mark_changed();
            }
            if (mode == "score") {
                runtime.mode.pacemaker_target_score = std::clamp(
                    runtime.mode.pacemaker_target_score +
                        static_cast<std::int64_t>(direction) * kPacemakerScoreStep,
                    config::kPacemakerScoreMin,
                    config::kPacemakerScoreMax);
                return mark_changed();
            }
            return {};
        }
        case ModeSettingId::KeyMode:
            runtime.mode.key_mode = cycle_runtime_key_mode(
                runtime.mode.key_mode, direction, true);
            return mark_changed();
        case ModeSettingId::KeyConverter:
            runtime.mode.key_conversion_algorithm = cycle_key_conversion_algorithm(
                runtime.mode.key_conversion_algorithm);
            return mark_changed();
        case ModeSettingId::Nk2Preset:
            if (normalize_key_conversion_algorithm(runtime.mode.key_conversion_algorithm) ==
                "krrcream") return {};
            runtime.mode.key_conversion_nk2_preset = cycle_key_conversion_nk2_preset(
                runtime.mode.key_conversion_nk2_preset);
            return mark_changed();
        case ModeSettingId::Gauge:
            runtime.mode.gauge = cycle_gauge_mode(runtime.mode.gauge, direction);
            return mark_changed();
        case ModeSettingId::Random:
            runtime.mode.random = cycle_random_mode(runtime.mode.random, direction);
            return mark_changed();
        case ModeSettingId::RandomSeed: {
            int next = static_cast<int>(runtime.mode.random_seed) + direction;
            runtime.mode.random_seed = static_cast<std::uint32_t>(
                clamp_int(next, kSeedMin, kSeedMax));
            return mark_changed();
        }
        case ModeSettingId::Rate:
            runtime.speed.rate = clamp_step_value(
                runtime.speed.rate + direction * kRateStep,
                kRateMin,
                kRateMax,
                kRateStep);
            return mark_changed();
        case ModeSettingId::HiSpeed:
            runtime.speed.hi_speed = clamp_step_value(
                runtime.speed.hi_speed + direction * kHiSpeedStep,
                kHiSpeedMin,
                kHiSpeedMax,
                kHiSpeedStep);
            return mark_changed();
        case ModeSettingId::Mods:
        case ModeSettingId::Back:
            return {};
    }
    return {};
}

ModeSettingsEffects ModeSettingsController::leave_screen() noexcept {
    const bool was_dirty = dirty_;
    const bool was_library_dirty = library_dirty_;
    selected_id_ = ModeSettingId::Indexing;
    selected_mod_category_ = 0;
    dirty_ = false;
    library_dirty_ = false;
    ModeSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.menu.persist_config = was_dirty;
    effects.menu.navigate_back = true;
    effects.refresh_song_library = was_dirty && was_library_dirty;
    return effects;
}

ModeSettingsEffects ModeSettingsController::mark_changed(bool library_changed) noexcept {
    dirty_ = true;
    library_dirty_ = library_dirty_ || library_changed;
    ModeSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

}  // namespace tenriff::app::menu::settings
