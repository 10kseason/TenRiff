#include "app/menu/settings/SkinSettingsController.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "app/MenuAppSettingsUtils.h"

namespace tenriff::app::menu::settings {
namespace {

constexpr double kJudgementLinePositionStep = 0.01;
constexpr double kComboPositionStep = 0.02;
constexpr double kNoteSizeScaleStep = 0.05;
constexpr double kLaneDividerScaleStep = 0.05;
constexpr double kSkinOpacityStep = 0.05;

std::string cycle_lr2_resolution_mode(std::string_view value, int direction) {
    static constexpr std::array<std::string_view, 4> kModes{"auto", "sd", "hd", "fhd"};
    const std::string normalized =
        config::normalize_skin_lr2_resolution_mode_token(value);
    auto current = std::find(kModes.begin(), kModes.end(), normalized);
    std::size_t index = current == kModes.end()
        ? 0
        : static_cast<std::size_t>(std::distance(kModes.begin(), current));
    index = direction < 0
        ? (index == 0 ? kModes.size() - 1 : index - 1)
        : (index + 1) % kModes.size();
    return std::string(kModes[index]);
}

std::string cycle_hit_burst_style(std::string_view value, int direction) {
    static constexpr std::array<std::string_view, 3> kStyles{"prism", "ring", "spark"};
    const std::string normalized = config::normalize_skin_hit_burst_style_token(value);
    auto current = std::find(kStyles.begin(), kStyles.end(), normalized);
    std::size_t index = current == kStyles.end()
        ? 0
        : static_cast<std::size_t>(std::distance(kStyles.begin(), current));
    index = direction < 0
        ? (index == 0 ? kStyles.size() - 1 : index - 1)
        : (index + 1) % kStyles.size();
    return std::string(kStyles[index]);
}

bool cycle_selected_name(
    const std::vector<std::string>& names,
    int direction,
    std::string& selected_name) {
    if (names.empty()) {
        return false;
    }
    auto current = std::find(names.begin(), names.end(), selected_name);
    std::size_t index = current == names.end()
        ? 0
        : static_cast<std::size_t>(std::distance(names.begin(), current));
    index = direction < 0
        ? (index == 0 ? names.size() - 1 : index - 1)
        : (index + 1) % names.size();
    selected_name = names[index];
    return true;
}

}  // namespace

bool SkinSettingsEffects::empty() const noexcept {
    return menu.empty() && !refresh_lr2_skins && !refresh_tenriff_skins &&
           !increment_skin_revision && boundary_action == SkinBoundaryAction::None;
}

void SkinSettingsEffects::merge(const SkinSettingsEffects& other) noexcept {
    menu.merge(other.menu);
    refresh_lr2_skins = refresh_lr2_skins || other.refresh_lr2_skins;
    refresh_tenriff_skins = refresh_tenriff_skins || other.refresh_tenriff_skins;
    increment_skin_revision =
        increment_skin_revision || other.increment_skin_revision;
    if (other.boundary_action != SkinBoundaryAction::None) {
        boundary_action = other.boundary_action;
    }
}

std::optional<SkinSettingsRowId> skin_setting_id_at(
    std::size_t index,
    bool lr2_source) noexcept {
    std::size_t visible_index = 0;
    for (const auto id : kSkinSettingsRowOrder) {
        if (!lr2_source && id == SkinSettingsRowId::Lr2Resolution) {
            continue;
        }
        if (visible_index == index) {
            return id;
        }
        ++visible_index;
    }
    return std::nullopt;
}

SkinSettingsRowId SkinSettingsController::selected_id() const noexcept {
    return selected_id_;
}

std::string_view SkinSettingsController::edit_mode() const noexcept {
    return edit_mode_;
}

int SkinSettingsController::edit_lane() const noexcept {
    return edit_lane_;
}

int SkinSettingsController::edit_gap() const noexcept {
    return edit_gap_;
}

bool SkinSettingsController::dirty() const noexcept {
    return dirty_;
}

void SkinSettingsController::reset(std::string_view runtime_key_mode) {
    selected_id_ = SkinSettingsRowId::KeyMode;
    edit_mode_ = normalize_skin_edit_mode(std::string(runtime_key_mode));
    edit_lane_ = 0;
    edit_gap_ = 0;
    dirty_ = false;
    clamp_edit_targets();
}

SkinSettingsEffects SkinSettingsController::select(
    SkinSettingsRowId target,
    bool lr2_source) noexcept {
    if (SkinSettingsRows{lr2_source}.index_of(target) < 0 || selected_id_ == target) {
        return {};
    }
    selected_id_ = target;
    SkinSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

SkinSettingsEffects SkinSettingsController::handle(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    const std::vector<std::string>& available_lr2_skin_names,
    const std::vector<std::string>& available_tenriff_skin_names,
    std::optional<SkinSettingsRowId> target) {
    const bool lr2_source =
        config::normalize_skin_source_token(runtime.skin.source) == "lr2";
    if (action.kind == MenuActionKind::Back) {
        return leave_screen();
    }
    if (action.kind == MenuActionKind::Move) {
        return move_selection(action.direction, lr2_source);
    }
    SkinSettingsEffects effects;
    if (target.has_value()) {
        if (SkinSettingsRows{lr2_source}.index_of(*target) < 0) {
            return effects;
        }
        effects.merge(select(*target, lr2_source));
    }
    effects.merge(apply_selected_action(
        action, runtime, available_lr2_skin_names, available_tenriff_skin_names));
    return effects;
}

SkinSettingsEffects SkinSettingsController::request_reload() const noexcept {
    SkinSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.boundary_action = SkinBoundaryAction::ReloadSkin;
    return effects;
}

SkinSettingsEffects SkinSettingsController::mark_external_change() noexcept {
    return mark_changed();
}

SkinSettingsEffects SkinSettingsController::move_selection(
    int direction,
    bool lr2_source) noexcept {
    if (direction == 0) {
        return {};
    }
    const SkinSettingsRows rows{lr2_source};
    const int current = rows.index_of(selected_id_);
    const int next = std::clamp(
        (current < 0 ? 0 : current) + (direction < 0 ? -1 : 1),
        0,
        rows.count() - 1);
    const auto next_id = skin_setting_id_at(static_cast<std::size_t>(next), lr2_source);
    if (!next_id.has_value() || *next_id == selected_id_) {
        return {};
    }
    selected_id_ = *next_id;
    SkinSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

SkinSettingsEffects SkinSettingsController::apply_selected_action(
    const MenuAction& action,
    config::RuntimeConfig& runtime,
    const std::vector<std::string>& available_lr2_skin_names,
    const std::vector<std::string>& available_tenriff_skin_names) {
    const bool is_adjust = action.kind == MenuActionKind::Adjust && action.direction != 0;
    const bool is_activate = action.kind == MenuActionKind::Activate;
    if (selected_id_ == SkinSettingsRowId::Back && is_activate) {
        return leave_screen();
    }
    if (is_activate) {
        SkinSettingsEffects effects;
        effects.menu.render_changed = true;
        switch (selected_id_) {
            case SkinSettingsRowId::ImportSkin:
                effects.boundary_action = SkinBoundaryAction::ImportSkin;
                return effects;
            case SkinSettingsRowId::CreateSkin:
                effects.boundary_action = SkinBoundaryAction::CreateSkin;
                return effects;
            case SkinSettingsRowId::OpenSkinFolder:
                effects.boundary_action = SkinBoundaryAction::OpenSkinFolder;
                return effects;
            case SkinSettingsRowId::ReloadSkin:
                effects.boundary_action = SkinBoundaryAction::ReloadSkin;
                return effects;
            case SkinSettingsRowId::VisualLatency:
                break;
            default:
                return {};
        }
    }
    if (!is_adjust && !(is_activate && selected_id_ == SkinSettingsRowId::VisualLatency)) {
        return {};
    }
    const int direction = is_adjust ? action.direction : 1;
    const std::string active_source =
        config::normalize_skin_source_token(runtime.skin.source);

    switch (selected_id_) {
        case SkinSettingsRowId::KeyMode: {
            edit_mode_ = cycle_skin_edit_mode(edit_mode_, direction);
            clamp_edit_targets();
            editable_skin_lane_colors(runtime.skin, edit_mode_);
            editable_skin_lane_width_scales(runtime.skin, edit_mode_);
            editable_skin_lane_spacing_scales(runtime.skin, edit_mode_);
            auto effects = mark_changed();
            effects.refresh_tenriff_skins = active_source == "tenriff";
            return effects;
        }
        case SkinSettingsRowId::ScratchPosition:
            if (config::normalize_skin_mode_token(edit_mode_) != "7+1") return {};
            runtime.skin.scratch_position =
                config::normalize_skin_scratch_position_token(runtime.skin.scratch_position) == "left"
                    ? "right"
                    : "left";
            return mark_changed();
        case SkinSettingsRowId::SkinSource: {
            runtime.skin.source = cycle_skin_source(runtime.skin.source, direction);
            auto effects = mark_changed();
            effects.refresh_lr2_skins = true;
            effects.refresh_tenriff_skins = true;
            effects.increment_skin_revision = true;
            return effects;
        }
        case SkinSettingsRowId::ImportedSkin: {
            bool changed = false;
            SkinSettingsEffects effects;
            if (active_source == "lr2") {
                changed = cycle_selected_name(
                    available_lr2_skin_names, direction, runtime.skin.lr2_skin_name);
                effects.refresh_lr2_skins = changed;
            } else if (active_source == "tenriff") {
                changed = cycle_selected_name(
                    available_tenriff_skin_names, direction, runtime.skin.tenriff_skin_name);
                effects.refresh_tenriff_skins = changed;
                effects.increment_skin_revision = changed;
            }
            if (changed) effects.merge(mark_changed());
            return effects;
        }
        case SkinSettingsRowId::Lr2Resolution:
            runtime.skin.lr2_resolution_mode =
                cycle_lr2_resolution_mode(runtime.skin.lr2_resolution_mode, direction);
            return mark_changed();
        case SkinSettingsRowId::TargetLane: {
            const int lane_count = lane_count_for_skin_mode(edit_mode_);
            edit_lane_ = (edit_lane_ + direction + lane_count) % lane_count;
            SkinSettingsEffects effects;
            effects.menu.render_changed = true;
            return effects;
        }
        case SkinSettingsRowId::TargetGap: {
            const int gap_count = std::max(0, lane_count_for_skin_mode(edit_mode_) - 1);
            if (gap_count <= 0) return {};
            edit_gap_ = (edit_gap_ + direction + gap_count) % gap_count;
            SkinSettingsEffects effects;
            effects.menu.render_changed = true;
            return effects;
        }
        case SkinSettingsRowId::LaneColor: {
            if (config::normalize_skin_single_color_token(runtime.skin.single_color) != "off") {
                return {};
            }
            auto& colors = editable_skin_lane_colors(runtime.skin, edit_mode_);
            const auto palette = config::supported_skin_color_tokens();
            const std::string current = config::normalize_skin_color_token(
                colors[static_cast<std::size_t>(edit_lane_)]);
            auto found = std::find(palette.begin(), palette.end(), current);
            std::size_t index = found == palette.end()
                ? 0
                : static_cast<std::size_t>(std::distance(palette.begin(), found));
            index = direction < 0
                ? (index == 0 ? palette.size() - 1 : index - 1)
                : (index + 1) % palette.size();
            colors[static_cast<std::size_t>(edit_lane_)] = palette[index];
            return mark_changed();
        }
        case SkinSettingsRowId::SingleColor: {
            std::vector<std::string> options{"off"};
            const auto palette = config::supported_skin_color_tokens();
            options.insert(options.end(), palette.begin(), palette.end());
            const std::string current =
                config::normalize_skin_single_color_token(runtime.skin.single_color);
            auto found = std::find(options.begin(), options.end(), current);
            std::size_t index = found == options.end()
                ? 0
                : static_cast<std::size_t>(std::distance(options.begin(), found));
            index = direction < 0
                ? (index == 0 ? options.size() - 1 : index - 1)
                : (index + 1) % options.size();
            runtime.skin.single_color = options[index];
            return mark_changed();
        }
        case SkinSettingsRowId::NoteShape: {
            static constexpr std::array<std::string_view, 8> kShapes{
                "rect", "square", "circle", "diamond", "arrow", "triangle", "pentagon", "hexagon"};
            const std::string current = config::normalize_skin_note_shape_token(runtime.skin.note_shape);
            auto found = std::find(kShapes.begin(), kShapes.end(), current);
            std::size_t index = found == kShapes.end()
                ? 0
                : static_cast<std::size_t>(std::distance(kShapes.begin(), found));
            index = direction < 0
                ? (index == 0 ? kShapes.size() - 1 : index - 1)
                : (index + 1) % kShapes.size();
            runtime.skin.note_shape = std::string(kShapes[index]);
            return mark_changed();
        }
        case SkinSettingsRowId::NoteBorder:
            runtime.skin.note_border_enabled = !runtime.skin.note_border_enabled;
            return mark_changed();
        case SkinSettingsRowId::ImageAspect:
            runtime.skin.note_image_aspect =
                cycle_skin_note_image_aspect(runtime.skin.note_image_aspect, direction);
            runtime.skin.preserve_note_image_aspect_ratio =
                runtime.skin.note_image_aspect != "stretch";
            return mark_changed();
        case SkinSettingsRowId::LaneDividers:
            runtime.skin.show_lane_dividers = !runtime.skin.show_lane_dividers;
            return mark_changed();
        case SkinSettingsRowId::JudgementLine:
            runtime.skin.show_judgement_line = !runtime.skin.show_judgement_line;
            return mark_changed();
        case SkinSettingsRowId::GearBoundary:
            runtime.skin.show_gear_boundary_line = !runtime.skin.show_gear_boundary_line;
            return mark_changed();
        case SkinSettingsRowId::ShowHoldTail:
            runtime.skin.show_hold_tail = !runtime.skin.show_hold_tail;
            return mark_changed();
        case SkinSettingsRowId::LnTailTaper:
            runtime.skin.hold_tail_taper_enabled = !runtime.skin.hold_tail_taper_enabled;
            return mark_changed();
        case SkinSettingsRowId::VisualPreset:
            config::apply_skin_visual_preset(
                runtime.skin,
                cycle_skin_visual_preset(runtime.skin.visual_preset, direction));
            return mark_changed();
        case SkinSettingsRowId::LaneBackgroundOpacity:
            runtime.skin.lane_background_opacity = clamp_step_value(
                runtime.skin.lane_background_opacity + direction * kSkinOpacityStep,
                config::kSkinLaneBackgroundOpacityMin,
                config::kSkinLaneBackgroundOpacityMax,
                kSkinOpacityStep);
            return mark_changed();
        case SkinSettingsRowId::VisualOpacity:
            runtime.skin.visual_opacity = clamp_step_value(
                runtime.skin.visual_opacity + direction * kSkinOpacityStep,
                config::kSkinVisualOpacityMin,
                config::kSkinVisualOpacityMax,
                kSkinOpacityStep);
            return mark_changed();
        case SkinSettingsRowId::NoteOutlineOpacity:
            runtime.skin.note_outline_opacity = clamp_step_value(
                runtime.skin.note_outline_opacity + direction * kSkinOpacityStep,
                config::kSkinNoteOutlineOpacityMin,
                config::kSkinNoteOutlineOpacityMax,
                kSkinOpacityStep);
            return mark_changed();
        case SkinSettingsRowId::LnBodyOpacity:
            runtime.skin.hold_body_opacity = clamp_step_value(
                runtime.skin.hold_body_opacity + direction * kSkinOpacityStep,
                config::kSkinHoldBodyOpacityMin,
                config::kSkinHoldBodyOpacityMax,
                kSkinOpacityStep);
            return mark_changed();
        case SkinSettingsRowId::JudgementLineGlow:
            runtime.skin.judgement_line_glow_enabled = !runtime.skin.judgement_line_glow_enabled;
            return mark_changed();
        case SkinSettingsRowId::HitBurstStyle:
            runtime.skin.hit_burst_style = cycle_hit_burst_style(runtime.skin.hit_burst_style, direction);
            return mark_changed();
        case SkinSettingsRowId::KeyPulse:
            runtime.skin.key_pulse_brightness = clamp_step_value(
                runtime.skin.key_pulse_brightness + direction * kSkinOpacityStep,
                config::kSkinKeyPulseBrightnessMin,
                config::kSkinKeyPulseBrightnessMax,
                kSkinOpacityStep);
            runtime.skin.key_pulse_enabled = runtime.skin.key_pulse_brightness > 0.0;
            return mark_changed();
        case SkinSettingsRowId::KeyLabelPosition:
            runtime.skin.key_label_position =
                cycle_skin_key_label_position(runtime.skin.key_label_position, direction);
            return mark_changed();
        case SkinSettingsRowId::JudgeLinePosition:
            runtime.skin.judgement_line_position = clamp_step_value(
                runtime.skin.judgement_line_position + direction * kJudgementLinePositionStep,
                config::kJudgementLinePositionMin,
                config::kJudgementLinePositionMax,
                kJudgementLinePositionStep);
            return mark_changed();
        case SkinSettingsRowId::LaneWidth: {
            auto& scales = editable_skin_lane_width_scales(runtime.skin, edit_mode_);
            if (static_cast<std::size_t>(edit_lane_) >= scales.size()) return {};
            scales[static_cast<std::size_t>(edit_lane_)] = clamp_step_value(
                scales[static_cast<std::size_t>(edit_lane_)] + direction * kNoteSizeScaleStep,
                config::kLaneWidthScaleMin,
                config::kLaneWidthScaleMax,
                kNoteSizeScaleStep);
            return mark_changed();
        }
        case SkinSettingsRowId::NoteWidth: {
            auto& value = editable_skin_note_width_scale(runtime.skin, edit_mode_);
            value = clamp_step_value(
                value + direction * kNoteSizeScaleStep,
                config::kNoteWidthScaleMin,
                config::kNoteWidthScaleMax,
                kNoteSizeScaleStep);
            return mark_changed();
        }
        case SkinSettingsRowId::LaneSpacing: {
            auto& scales = editable_skin_lane_spacing_scales(runtime.skin, edit_mode_);
            if (static_cast<std::size_t>(edit_gap_) >= scales.size()) return {};
            scales[static_cast<std::size_t>(edit_gap_)] = clamp_step_value(
                scales[static_cast<std::size_t>(edit_gap_)] + direction * kLaneDividerScaleStep,
                config::kLaneSpacingScaleMin,
                config::kLaneSpacingScaleMax,
                kLaneDividerScaleStep);
            return mark_changed();
        }
        case SkinSettingsRowId::DividerWidth: {
            auto& value = editable_skin_lane_divider_width_scale(runtime.skin, edit_mode_);
            value = clamp_step_value(
                value + direction * kLaneDividerScaleStep,
                config::kLaneDividerWidthScaleMin,
                config::kLaneDividerWidthScaleMax,
                kLaneDividerScaleStep);
            return mark_changed();
        }
        case SkinSettingsRowId::CenterGap: {
            if (config::normalize_skin_mode_token(edit_mode_) != "16k") return {};
            auto& value = editable_skin_lane_center_gap_scale(runtime.skin, edit_mode_);
            value = clamp_step_value(
                value + direction * kLaneDividerScaleStep,
                config::kLaneCenterGapScaleMin,
                config::kLaneCenterGapScaleMax,
                kLaneDividerScaleStep);
            return mark_changed();
        }
        case SkinSettingsRowId::LnBodyWidth:
            runtime.skin.hold_body_width_scale = clamp_step_value(
                runtime.skin.hold_body_width_scale + direction * kNoteSizeScaleStep,
                config::kHoldBodyWidthScaleMin,
                config::kHoldBodyWidthScaleMax,
                kNoteSizeScaleStep);
            return mark_changed();
        case SkinSettingsRowId::NoteHeight: {
            auto& value = editable_skin_note_height_scale(runtime.skin, edit_mode_);
            value = clamp_step_value(
                value + direction * kNoteSizeScaleStep,
                config::kNoteHeightScaleMin,
                config::kNoteHeightScaleMax,
                kNoteSizeScaleStep);
            return mark_changed();
        }
        case SkinSettingsRowId::ComboY:
            runtime.skin.combo_position = clamp_step_value(
                runtime.skin.combo_position + direction * kComboPositionStep,
                config::kComboPositionMin,
                config::kComboPositionMax,
                kComboPositionStep);
            return mark_changed();
        case SkinSettingsRowId::BlackPlayfield:
            runtime.skin.black_playfield_enabled = !runtime.skin.black_playfield_enabled;
            return mark_changed();
        case SkinSettingsRowId::UiFont:
            runtime.skin.ui_font = cycle_skin_ui_font(runtime.skin.ui_font, direction);
            return mark_changed();
        case SkinSettingsRowId::VisualLatency:
            runtime.visual_offset_ms = clamp_step_value(
                runtime.visual_offset_ms + direction * kVisualOffsetStep,
                kVisualOffsetMin,
                kVisualOffsetMax,
                kVisualOffsetStep);
            return mark_changed();
        case SkinSettingsRowId::NoteGap:
            runtime.skin.note_divider_gap_px = clamp_step_value(
                runtime.skin.note_divider_gap_px + direction * config::kNoteDividerGapPxStep,
                config::kNoteDividerGapPxMin,
                config::kNoteDividerGapPxMax,
                config::kNoteDividerGapPxStep);
            return mark_changed();
        case SkinSettingsRowId::GameplayCursor:
            runtime.ui.show_cursor_in_gameplay = !runtime.ui.show_cursor_in_gameplay;
            return mark_changed();
        case SkinSettingsRowId::TimingFeedback:
            runtime.skin.show_timing_feedback = !runtime.skin.show_timing_feedback;
            return mark_changed();
        case SkinSettingsRowId::ImportSkin:
        case SkinSettingsRowId::CreateSkin:
        case SkinSettingsRowId::OpenSkinFolder:
        case SkinSettingsRowId::ReloadSkin:
        case SkinSettingsRowId::Back:
            return {};
    }
    return {};
}

SkinSettingsEffects SkinSettingsController::leave_screen() noexcept {
    const bool was_dirty = dirty_;
    selected_id_ = SkinSettingsRowId::KeyMode;
    dirty_ = false;
    SkinSettingsEffects effects;
    effects.menu.render_changed = true;
    effects.menu.persist_config = was_dirty;
    effects.menu.navigate_back = true;
    return effects;
}

SkinSettingsEffects SkinSettingsController::mark_changed() noexcept {
    dirty_ = true;
    SkinSettingsEffects effects;
    effects.menu.render_changed = true;
    return effects;
}

void SkinSettingsController::clamp_edit_targets() noexcept {
    const int lane_count = lane_count_for_skin_mode(edit_mode_);
    edit_lane_ = std::clamp(edit_lane_, 0, lane_count - 1);
    edit_gap_ = std::clamp(edit_gap_, 0, std::max(0, lane_count - 2));
}

}  // namespace tenriff::app::menu::settings
