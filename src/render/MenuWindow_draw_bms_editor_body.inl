const auto& editor = data.bms_editor;
const float bgm_left = 72.0f;
const float bgm_right = 194.0f;
const float grid_left = 214.0f;
const float grid_top = 184.0f;
const float grid_right = 1770.0f;
const float grid_bottom = 900.0f;
const float grid_width = grid_right - grid_left;
const float grid_height = grid_bottom - grid_top;
const int lane_count = std::clamp(editor.lane_count, 1, 16);
const int measure_count = std::max(1, editor.measure_count);
const int view_count = std::max(1, std::min(std::clamp(editor.view_measure_count, 1, 32), measure_count));
const int view_start = editor.view_start_measure >= 0
    ? std::clamp(editor.view_start_measure, 0, std::max(0, measure_count - view_count))
    : std::clamp(editor.cursor_measure - view_count / 2, 0,
                 std::max(0, measure_count - view_count));
const float measure_height = grid_height / static_cast<float>(view_count);
std::array<float, 16> editor_lane_widths{};
double editor_lane_units = 0.0;
for (int lane = 0; lane < lane_count; ++lane) {
    const double scale = lane < static_cast<int>(editor.lane_width_scale_count)
        ? std::clamp(editor.lane_width_scales[static_cast<std::size_t>(lane)], 0.25, 4.0)
        : 1.0;
    editor_lane_units += scale;
}
const float editor_unit_width = grid_width / static_cast<float>(std::max(0.01, editor_lane_units));
for (int lane = 0; lane < lane_count; ++lane) {
    const double scale = lane < static_cast<int>(editor.lane_width_scale_count)
        ? std::clamp(editor.lane_width_scales[static_cast<std::size_t>(lane)], 0.25, 4.0)
        : 1.0;
    editor_lane_widths[static_cast<std::size_t>(lane)] = editor_unit_width * static_cast<float>(scale);
}
const auto lane_left = [&](int lane) {
    float left = grid_left;
    for (int index = 1; index < lane; ++index) left += editor_lane_widths[static_cast<std::size_t>(index - 1)];
    return left;
};

if (d2d_->panel_brush) {
    ctx->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(48.0f, 36.0f, 1872.0f, 140.0f), 18.0f, 18.0f), d2d_->panel_brush.Get());
}
if (d2d_->hud_format && d2d_->muted_brush) {
    draw_text_clipped_aligned(to_wide(data.ui_korean ? "배경음" : "BGM"), d2d_->hud_format.Get(),
                              D2D1::RectF(bgm_left, grid_top - 32.0f, bgm_right, grid_top - 8.0f),
                              d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
}
if (d2d_->title_format && d2d_->text_brush) {
    draw_text_clipped(to_wide(editor.title.empty() ? (data.ui_korean ? "BMS 편집기" : "BMS Editor") : editor.title),
                      d2d_->title_format.Get(), D2D1::RectF(82.0f, 52.0f, 1050.0f, 98.0f),
                      d2d_->text_brush.Get());
}
if (d2d_->body_format && d2d_->muted_brush) {
    const std::string subtitle = (editor.artist.empty() ? std::string{} : editor.artist + "  |  ") +
        "BPM " + std::to_string(editor.base_bpm) + "  |  " + editor.path;
    draw_text_clipped(to_wide(subtitle), d2d_->body_format.Get(),
                      D2D1::RectF(82.0f, 96.0f, 1420.0f, 126.0f), d2d_->muted_brush.Get());
}
if (d2d_->hud_format && d2d_->accent_brush) {
    const std::string mode_label = editor.silent_note_mode
        ? (data.ui_korean ? "무키음" : "SILENT")
        : (data.ui_korean ? "키음" : "KEYSOUND");
    const std::string tool_label = editor.tool == "move"
        ? (data.ui_korean ? "이동" : "MOVE")
        : editor.tool == "remove"
            ? (data.ui_korean ? "제거" : "REMOVE")
            : (data.ui_korean ? "무키음 배치" : "PLACE SILENT");
    const std::string state = (editor.dirty ? "* " : "") + editor.status +
        "  |  " + mode_label +
        "  |  T:" + tool_label +
        "  |  " + (data.ui_korean ? (editor.auto_align_bgm ? "BGM 자동" : "BGM 수동")
                                  : (editor.auto_align_bgm ? "BGM-AUTO" : "BGM-MANUAL")) +
        "  |  " + (data.ui_korean ? "스냅 1/" : "SNAP 1/") + std::to_string(std::max(1, editor.snap_division)) +
        "  |  " + std::to_string(view_count) + (data.ui_korean ? "마디" : " MEAS") +
        (editor.x_axis_lock ? (data.ui_korean ? "  |  X축 잠금" : "  |  X-LOCK") : "");
    draw_text_clipped_aligned(to_wide(state), d2d_->hud_format.Get(),
                              D2D1::RectF(1180.0f, 58.0f, 1828.0f, 92.0f),
                              d2d_->accent_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
}

if (d2d_->panel_brush) {
    ctx->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(grid_left - 18.0f, grid_top - 40.0f, grid_right + 18.0f, grid_bottom + 18.0f),
        16.0f, 16.0f), d2d_->panel_brush.Get());
}

const std::uint32_t lane_colors[] = {
    0x6EE7F2, 0xF59E0B, 0xA78BFA, 0x34D399, 0xFB7185, 0x60A5FA,
    0xFBBF24, 0xC084FC, 0x2DD4BF, 0xF472B6, 0x93C5FD, 0x86EFAC,
    0xFDBA74, 0xA5B4FC, 0x5EEAD4, 0xFDA4AF
};
constexpr std::size_t lane_color_count = sizeof(lane_colors) / sizeof(lane_colors[0]);
for (int lane = 1; lane <= lane_count; ++lane) {
    const float left = lane_left(lane);
    const float lane_width = editor_lane_widths[static_cast<std::size_t>(lane - 1)];
    const float right = left + lane_width;
    const D2D1_RECT_F column = D2D1::RectF(left, grid_top, right, grid_bottom);
    if (lane == editor.cursor_lane && d2d_->button_selected_brush) {
        ctx->FillRectangle(column, d2d_->button_selected_brush.Get());
    }
    register_hit(column, MenuHitTargetKind::BmsEditorGrid, lane, MenuHitPart::SetValue);
    if (d2d_->hud_format && d2d_->muted_brush) {
        draw_text_clipped_aligned(to_wide("K" + std::to_string(lane)), d2d_->hud_format.Get(),
                                  D2D1::RectF(left + 2.0f, grid_top - 32.0f, right - 2.0f, grid_top - 8.0f),
                                  d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    if (d2d_->lane_divider_brush) {
        ctx->DrawLine(D2D1::Point2F(left, grid_top), D2D1::Point2F(left, grid_bottom),
                      d2d_->lane_divider_brush.Get(), lane == 1 ? 2.0f : 1.0f);
    }
}
if (d2d_->lane_divider_brush) {
    ctx->DrawLine(D2D1::Point2F(grid_right, grid_top), D2D1::Point2F(grid_right, grid_bottom),
                  d2d_->lane_divider_brush.Get(), 2.0f);
    for (int measure = 0; measure <= view_count; ++measure) {
        const float y = grid_top + measure_height * static_cast<float>(measure);
        ctx->DrawLine(D2D1::Point2F(grid_left, y), D2D1::Point2F(grid_right, y),
                      d2d_->lane_divider_brush.Get(), measure == 0 || measure == view_count ? 2.0f : 1.6f);
        if (d2d_->hud_format && d2d_->muted_brush && measure < view_count) {
            draw_text_clipped(std::to_wstring(view_start + measure + 1), d2d_->hud_format.Get(),
                              D2D1::RectF(grid_right + 8.0f, y + 3.0f, grid_right + 72.0f, y + 28.0f),
                              d2d_->muted_brush.Get());
        }
        const int subdivision = std::clamp(editor.snap_division, 1, 192);
        for (int sub = 1; sub < subdivision; ++sub) {
            const float sub_y = y + measure_height * static_cast<float>(sub) /
                static_cast<float>(subdivision);
            ctx->DrawLine(D2D1::Point2F(grid_left, sub_y), D2D1::Point2F(grid_right, sub_y),
                          d2d_->button_border_brush.Get(), sub % 4 == 0 ? 0.8f : 0.3f);
        }
    }
}

for (const auto& marker : editor.markers) {
    if (marker.measure < view_start || marker.measure >= view_start + view_count) continue;
    const float y = grid_top + (static_cast<float>(marker.measure - view_start) +
        static_cast<float>(marker.slice) / static_cast<float>(std::max(1, marker.slice_count))) * measure_height;
    if (d2d_->judgement_line_brush) {
        ctx->DrawLine(D2D1::Point2F(grid_left, y), D2D1::Point2F(grid_right, y),
                      d2d_->judgement_line_brush.Get(), 1.5f);
    }
    if (d2d_->hud_format && d2d_->muted_brush) {
        draw_text_clipped(to_wide(marker.label), d2d_->hud_format.Get(),
                          D2D1::RectF(grid_left - 112.0f, y - 12.0f, grid_left - 8.0f, y + 12.0f),
                          d2d_->muted_brush.Get());
    }
}

for (std::size_t bgm_index = 0; bgm_index < editor.bgm.size(); ++bgm_index) {
    const auto& bgm = editor.bgm[bgm_index];
    if (bgm.measure < view_start || bgm.measure >= view_start + view_count) continue;
    const float y = grid_top + (static_cast<float>(bgm.measure - view_start) +
        static_cast<float>(bgm.slice) / static_cast<float>(std::max(1, bgm.slice_count))) * measure_height;
    const D2D1_RECT_F bgm_rect = D2D1::RectF(bgm_left + 8.0f, y - 5.0f, bgm_right - 8.0f, y + 5.0f);
    register_hit(bgm_rect, MenuHitTargetKind::BmsEditorBgm, static_cast<int>(bgm_index),
                 MenuHitPart::Activate);
    if (d2d_->accent_brush) {
        const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
        const float saved_opacity = d2d_->accent_brush->GetOpacity();
        d2d_->accent_brush->SetColor(D2D1::ColorF(bgm.selected ? 0xFFFFFF : 0xF59E0B));
        d2d_->accent_brush->SetOpacity(bgm.selected ? 1.0f : 0.72f);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(bgm_rect, 3.0f, 3.0f), d2d_->accent_brush.Get());
        d2d_->accent_brush->SetColor(saved_color);
        d2d_->accent_brush->SetOpacity(saved_opacity);
    }
}

for (std::size_t note_index = 0; note_index < editor.notes.size(); ++note_index) {
    const auto& note = editor.notes[note_index];
    if (note.measure < view_start || note.measure >= view_start + view_count ||
        note.lane < 1 || note.lane > lane_count) continue;
    const float x = lane_left(note.lane);
    const float lane_width = editor_lane_widths[static_cast<std::size_t>(note.lane - 1)];
    const float y = grid_top + (static_cast<float>(note.measure - view_start) +
        static_cast<float>(note.slice) / static_cast<float>(std::max(1, note.slice_count))) * measure_height;
    const float note_width = std::clamp(lane_width * 0.72f *
        static_cast<float>(std::clamp(editor.note_width_scale, 0.5, 1.4)), 8.0f, lane_width * 0.95f);
    const float note_height = std::clamp(measure_height / 72.0f *
        static_cast<float>(std::clamp(editor.note_height_scale / 1.8, 0.5, 4.0)), 5.0f, 28.0f);
    const auto color = lane_colors[static_cast<std::size_t>(note.lane - 1) % lane_color_count];
    ID2D1SolidColorBrush* brush = note.editable ? d2d_->accent_brush.Get() : d2d_->muted_brush.Get();
    if (brush == d2d_->accent_brush.Get() && brush) {
        const D2D1_COLOR_F saved_color = brush->GetColor();
        const float saved_opacity = brush->GetOpacity();
        brush->SetColor(D2D1::ColorF(color));
        brush->SetOpacity(note.token == "00" ? 0.30f : (note.long_note ? 0.88f : 1.0f));
        if (note.long_note) {
            const float end_y = grid_top + (static_cast<float>(note.end_measure - view_start) +
                static_cast<float>(note.end_slice) / static_cast<float>(std::max(1, note.end_slice_count))) * measure_height;
            ctx->FillRectangle(D2D1::RectF(x + lane_width * 0.36f, y + note_height * 0.5f,
                                            x + lane_width * 0.64f, std::max(y + note_height, end_y)), brush);
        }
        ctx->FillRoundedRectangle(D2D1::RoundedRect(
            D2D1::RectF(x + (lane_width - note_width) * 0.5f, y - note_height * 0.5f,
                        x + (lane_width + note_width) * 0.5f, y + note_height * 0.5f), 3.0f, 3.0f), brush);
        brush->SetColor(saved_color);
        brush->SetOpacity(saved_opacity);
    } else if (brush) {
        ctx->FillRectangle(D2D1::RectF(x + (lane_width - note_width) * 0.5f, y - note_height * 0.5f,
                                       x + (lane_width + note_width) * 0.5f, y + note_height * 0.5f), brush);
    }
    register_hit(D2D1::RectF(x + (lane_width - note_width) * 0.5f - 3.0f, y - note_height * 0.5f - 3.0f,
                             x + (lane_width + note_width) * 0.5f + 3.0f, y + note_height * 0.5f + 3.0f),
                 MenuHitTargetKind::BmsEditorNote, static_cast<int>(note_index), MenuHitPart::Activate);
    if (note.selected && d2d_->text_brush) {
        ctx->DrawRectangle(D2D1::RectF(x + (lane_width - note_width) * 0.5f - 4.0f,
                                       y - note_height * 0.5f - 4.0f,
                                       x + (lane_width + note_width) * 0.5f + 4.0f,
                                       y + note_height * 0.5f + 4.0f),
                           d2d_->text_brush.Get(), 1.5f);
    }
}

const float cursor_x = lane_left(std::clamp(editor.cursor_lane, 1, lane_count));
const float cursor_lane_width = editor_lane_widths[static_cast<std::size_t>(std::clamp(editor.cursor_lane, 1, lane_count) - 1)];
const float cursor_y = grid_top + (static_cast<float>(editor.cursor_measure - view_start) +
    static_cast<float>(editor.cursor_slice) / static_cast<float>(std::max(1, editor.cursor_slice_count))) * measure_height;
if (d2d_->accent_brush) {
    const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
    d2d_->accent_brush->SetColor(D2D1::ColorF(0xFFFFFF));
    ctx->DrawLine(D2D1::Point2F(grid_left, cursor_y), D2D1::Point2F(grid_right, cursor_y),
                  d2d_->accent_brush.Get(), 2.0f);
    ctx->DrawRectangle(D2D1::RectF(cursor_x + 2.0f, grid_top, cursor_x + cursor_lane_width - 2.0f, grid_bottom),
                       d2d_->accent_brush.Get(), 1.5f);
    d2d_->accent_brush->SetColor(saved_color);
}

if (d2d_->body_format && d2d_->muted_brush) {
    draw_text_clipped(to_wide(data.ui_korean ? "T: 무키음 배치/이동/제거   방향키: 레인/시간   노트 드래그: 이동   Ctrl+클릭: 다중 선택   Ctrl+Y: BGM 자동정렬   Ctrl+↑/↓: BGM 이동   휠: 1초 스크롤" : "T: place silent/move/remove   Arrow: lane/time   Drag note: move   Ctrl+click: multi-select   Ctrl+Y: auto-align BGM   Ctrl+Up/Down: move BGM   Wheel: 1s scroll"),
                      d2d_->body_format.Get(), D2D1::RectF(72.0f, 952.0f, 1530.0f, 1002.0f),
                      d2d_->muted_brush.Get());
    draw_text_clipped_aligned(to_wide(data.ui_korean ? "O: 여기서 10초   CTRL+O: 처음부터 끝까지   P: 이 구간 연습   CTRL+S: 다른 이름 저장   ESC: 뒤로" : "O: 10s here   CTRL+O: start to end   P: practice here   CTRL+S: Save As   ESC: back"),
                              d2d_->body_format.Get(), D2D1::RectF(1370.0f, 952.0f, 1840.0f, 1002.0f),
                              d2d_->accent_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
}
