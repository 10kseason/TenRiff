#pragma once

#include <algorithm>
#include <cmath>
#include <functional>

#include "render/MenuWindow.h"

namespace tenriff::render {

inline uint64_t skin_gameplay_preview_text_revision(const SkinPreviewData& preview,
                                                    ui::Language language) {
    // The preview has no session publisher to increment text_revision. Hash only
    // inputs used by cached HUD strings, so settings/language changes refresh
    // them without rebuilding text layouts for every animated key frame.
    uint64_t revision = 14695981039346656037ULL;
    const auto mix = [&](uint64_t value) { revision = (revision ^ value) * 1099511628211ULL; };
    for (const unsigned char ch : preview.mode_label) mix(ch);
    mix(0xFF);
    for (const unsigned char ch : preview.hud_layout) mix(ch);
    mix(static_cast<uint64_t>(std::clamp(preview.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes))));
    mix(static_cast<uint64_t>(language));
    mix(static_cast<uint64_t>(std::hash<double>{}(preview.judgement_line_position)));
    return revision | (uint64_t{1} << 63);  // Zero remains the cache's invalid sentinel.
}

// Only the sample chart is synthetic. Every visual setting travels through the
// gameplay renderer, including imported lane geometry, gear art and font roles.
inline GameplayHudData make_skin_gameplay_preview(const SkinPreviewData& preview, int64_t now_ns = 0,
                                                   ui::Language language = ui::Language::English) {
    GameplayHudData hud;
    hud.active = true;
    hud.show_cursor_in_gameplay = true;
    hud.lane_count = std::clamp(preview.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    hud.title = preview.mode_label;
    hud.artist = "TenRiff";
    hud.text_revision = skin_gameplay_preview_text_revision(preview, language);
    hud.current_sample = hud.sample_rate;
    hud.duration_samples = hud.sample_rate * 120;
    hud.current_visual_position = 1.0;
    hud.visual_velocity = 1.0 / hud.sample_rate;
    hud.lookahead_samples = hud.sample_rate;
    hud.past_samples = hud.sample_rate / 10;
    hud.combo = hud.max_combo = 123;
    hud.has_feedback = true;
    hud.feedback = "GR";
    hud.feedback_delta_ms = -18.0;
    hud.timing_history_count = 3;
    hud.timing_history_delta_ms = {-24.0, 12.0, -18.0};
    hud.gauge = 75.0;
    hud.gauge_label = "NORMAL";
    hud.show_timing_feedback = preview.show_timing_feedback;
    hud.show_timing_bar = preview.show_timing_bar;
    hud.note_fade_in = preview.note_fade_in;
    hud.note_fade_out = preview.note_fade_out;
    hud.timing_text_offset_x = preview.timing_text_offset_x;
    hud.timing_text_offset_y = preview.timing_text_offset_y;
    hud.timing_bar_offset_x = preview.timing_bar_offset_x;
    hud.timing_bar_offset_y = preview.timing_bar_offset_y;


    hud.judgement_line_position = preview.judgement_line_position;
    hud.gameplay_field_offset_x = preview.gameplay_field_offset_x;
    hud.combo_position = preview.combo_position;
    hud.judgement_position = preview.judgement_position;
    hud.judgement_offset_x = preview.judgement_offset_x;
    hud.combo_offset_x = preview.combo_offset_x;
    hud.combo_font_scale = preview.combo_font_scale;
    hud.judgement_font_scale = preview.judgement_font_scale;
    hud.lane_width_scale_count = preview.lane_width_scale_count;
    hud.lane_width_scales = preview.lane_width_scales;
    hud.note_width_scale = preview.note_width_scale;
    hud.lane_spacing_scale_count = preview.lane_spacing_scale_count;
    hud.lane_spacing_scales = preview.lane_spacing_scales;
    hud.note_height_scale = preview.note_height_scale;
    hud.lane_divider_width_scale = preview.lane_divider_width_scale;
    hud.lane_center_gap_scale = preview.lane_center_gap_scale;
    hud.hold_body_width_scale = preview.hold_body_width_scale;
    hud.show_lane_dividers = preview.show_lane_dividers;
    hud.note_divider_gap_px = preview.note_divider_gap_px;
    hud.show_judgement_line = preview.show_judgement_line;
    hud.show_gear_boundary_line = preview.show_gear_boundary_line;
    hud.show_hold_tail = preview.show_hold_tail;
    hud.hold_tail_taper_enabled = preview.hold_tail_taper_enabled;
    hud.judgement_line_glow_enabled = preview.judgement_line_glow_enabled;
    hud.key_pulse_enabled = preview.key_pulse_enabled;
    hud.key_pulse_brightness = preview.key_pulse_brightness;
    hud.key_backdrop_enabled = preview.key_backdrop_enabled;
    hud.key_backdrop_opacity = preview.key_backdrop_opacity;
    hud.key_backdrop_brightness = preview.key_backdrop_brightness;
    hud.key_backdrop_height = preview.key_backdrop_height;
    hud.hit_burst_style = preview.hit_burst_style;
    hud.hud_layout = preview.hud_layout;
    hud.key_label_position = preview.key_label_position;
    hud.note_border_enabled = preview.note_border_enabled;
    hud.note_shape = preview.note_shape;
    hud.note_image_aspect = preview.note_image_aspect;
    hud.skin_source = preview.skin_source;
    hud.external_skin_root = preview.external_skin_root;
    hud.external_skin_name = preview.external_skin_name;
    hud.skin_revision = preview.skin_revision;
    hud.resolved_tenriff_skin = preview.resolved_tenriff_skin;
    hud.skin_background_path = preview.skin_background_path;
    hud.skin_background_opacity = preview.skin_background_opacity;
    hud.lr2_resolution_override = preview.lr2_resolution_override;
    hud.lane_background_opacity = preview.lane_background_opacity;
    hud.black_playfield_enabled = preview.black_playfield_enabled;
    hud.visual_opacity = preview.visual_opacity;
    hud.note_outline_opacity = preview.note_outline_opacity;
    hud.hold_body_opacity = preview.hold_body_opacity;
    hud.lane_colors = preview.lane_colors;
    hud.key_labels = preview.key_labels;

    hud.lane_color_count = hud.key_label_count = static_cast<std::size_t>(hud.lane_count);
    hud.lane_pressed_count = static_cast<std::size_t>(hud.lane_count);
    hud.lane_activity_count = static_cast<std::size_t>(hud.lane_count);
    hud.activity_publish_time_ns = now_ns;
    if (preview.selected_lane > 0 && preview.selected_lane <= hud.lane_count) {
        // A synthetic tap every 900ms uses the real hit-burst path, including the
        // selected prism/ring/spark style, opacity and imported lane positions.
        const double phase = static_cast<double>(std::max<int64_t>(0, now_ns) % 900'000'000LL) / 1e9;
        const auto lane = static_cast<std::size_t>(preview.selected_lane - 1);
        hud.lane_activity[lane] = static_cast<float>(std::max(0.0, 1.0 - phase / 0.32));
        hud.lane_pressed[lane] = phase < 0.12 ? 1 : 0;
    }
    for (int lane = 0; lane < hud.lane_count; ++lane) {
        auto& note = hud.notes[hud.note_count++];
        note.lane = lane + 1;
        note.hold = note.lane == preview.selected_lane;
        // Keep sample notes in the upper part of the field at every judge line.
        const double y = std::max(0.04, std::clamp(preview.judgement_line_position, 0.0, 1.0) *
                                               (0.28 + 0.12 * (lane % 4)));
        note.visual_position = hud.current_visual_position + (preview.judgement_line_position - y) / (preview.judgement_line_position + 0.12);
        note.start_sample = static_cast<int64_t>(note.visual_position * hud.sample_rate);
        note.tail_visual_position = note.visual_position + (note.hold ? 0.15 : 0.0);
        note.tail_sample = static_cast<int64_t>(note.tail_visual_position * hud.sample_rate);
    }
    return hud;
}

}  // namespace tenriff::render
