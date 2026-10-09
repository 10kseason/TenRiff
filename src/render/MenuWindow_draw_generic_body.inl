        const ScreenContentBands bands =
            make_screen_content_bands(48.0f, 72.0f, false, 20.0f, 18.0f);
        const D2D1_RECT_F content_rect = skin_layout_rect(
            data, "generic.content",
            modern_settings_screen ? native_rect("generic.rect.001", D2D1::RectF(64.0f, 154.0f, kBaseWidth - 64.0f, 958.0f))
                                   : native_rect("generic.rect.002", D2D1::RectF(80.0f, bands.body_top, kBaseWidth - 80.0f, bands.body_bottom)));
        const float left = content_rect.left;
        const float top = content_rect.top;
        const float right = content_rect.right;
        const float bottom = content_rect.bottom;

        if (d2d_->card_brush && !modern_settings_screen) {
            D2D1_ROUNDED_RECT card =
                D2D1::RoundedRect(native_rect("generic.rect.003", D2D1::RectF(left, top, right, bottom)), 18.0f, 18.0f);
            ctx->FillRoundedRectangle(card, d2d_->card_brush.Get());
        }

        std::string header = "TenRiff";
        if (!data.generic.heading.empty()) {
            header += " / " + data.generic.heading;
        } else if (!data.screen_title.empty()) {
            header += " / " + data.screen_title;
        }
        const std::wstring header_wide = to_wide(header);
        D2D1_RECT_F header_rect = native_rect("generic.rect.004", D2D1::RectF(left, 48.0f, right, 120.0f));
        if (modern_settings_screen) {
            if (d2d_->panel_brush) ctx->FillRectangle(native_rect("generic.rect.005", D2D1::RectF(0, 0, kBaseWidth, 126)), d2d_->panel_brush.Get());
            draw_native_asset(native_menu_assets::kMark, D2D1::RectF(64, 30, 112, 88));
            draw_native_wordmark(d2d_->header_format.Get(),
                                native_rect("generic.rect.006", D2D1::RectF(128, 24, 384, 94)));
            draw_text_clipped(to_wide(data.generic.heading.empty() ? data.screen_title : data.generic.heading),
                              d2d_->title_format.Get(), native_rect("generic.rect.007", D2D1::RectF(420, 34, 1470, 78)), d2d_->text_brush.Get());
            draw_text_clipped(wloc("SETTINGS & TOOLS", "설정 및 도구"), d2d_->hud_format.Get(),
                              native_rect("generic.rect.008", D2D1::RectF(422, 82, 1470, 110)), d2d_->muted_brush.Get());
            draw_glass_panel(native_rect("generic.rect.009", D2D1::RectF(left, 984, right, 1048)), 12, 0.9f, 0, false, 0);
            draw_text_clipped(wloc("ESC / BACKSPACE    Back or cancel", "ESC / BACKSPACE    뒤로 / 취소"),
                              d2d_->body_format.Get(), native_rect("generic.rect.010", D2D1::RectF(left + 24, 1004, left + 530, 1034)), d2d_->text_brush.Get());
            draw_text_clipped_aligned(wloc("UP / DOWN  Select     LEFT / RIGHT  Adjust     ENTER  Open     F1  Help",
                                           "위 / 아래  선택     좌 / 우  조절     ENTER  열기     F1  도움말"),
                                      d2d_->body_format.Get(), native_rect("generic.rect.011", D2D1::RectF(left + 560, 1004, right - 24, 1034)),
                                      d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
        } else if (d2d_->title_format && d2d_->accent_brush) {
            draw_text_clipped(header_wide, d2d_->title_format.Get(), header_rect, d2d_->accent_brush.Get());
        }

        const bool has_skin_preview = data.generic.skin_preview.visible;
        const float preview_gap = has_skin_preview ? 28.0f : 0.0f;
        const float preview_width = has_skin_preview
                                        ? std::max(360.0f, ((right - left - 48.0f) - preview_gap) * 0.5f)
                                        : 0.0f;

#include "MenuWindow_draw_generic_help.inl"

        auto draw_skin_preview_panel = [&](const SkinPreviewData& preview, const D2D1_RECT_F& rect) {
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.90f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.2f);
            }

            const auto fullscreen_rect = D2D1::RectF(rect.right - 304.0f, rect.top + 18.0f, rect.right - 24.0f, rect.top + 60.0f);
            if (d2d_->button_brush) ctx->FillRoundedRectangle(D2D1::RoundedRect(fullscreen_rect, 6, 6), d2d_->button_brush.Get());
            if (d2d_->button_border_brush) ctx->DrawRoundedRectangle(D2D1::RoundedRect(fullscreen_rect, 6, 6), d2d_->button_border_brush.Get(), 1.0f);
            draw_text_clipped_aligned(wloc("Fullscreen Preview  [F6]", "전체화면 미리보기  [F6]"),
                d2d_->body_format.Get(), fullscreen_rect, d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            register_hit(fullscreen_rect, MenuHitTargetKind::SkinPreviewButton, 0, MenuHitPart::Activate);
            const std::wstring title_w = wloc("LIVE PREVIEW", "실시간 미리보기");
            const std::wstring mode_w =
                to_wide(preview.mode_label + " / " + loc("Lane ", "레인 ") + std::to_string(std::max(1, preview.selected_lane)));
            const std::wstring color_w = to_wide(loc("Color: ", "색상: ") + preview.selected_color_label);
            if (d2d_->title_format && d2d_->text_brush) {
                draw_text_clipped(title_w,
                                  d2d_->title_format.Get(),
                                  native_rect("generic.rect.012", D2D1::RectF(rect.left + 24.0f, rect.top + 18.0f, rect.right - 316.0f, rect.top + 60.0f)),
                                  d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_text_clipped(mode_w,
                                  d2d_->body_format.Get(),
                                  native_rect("generic.rect.013", D2D1::RectF(rect.left + 24.0f, rect.top + 56.0f, rect.right - 24.0f, rect.top + 88.0f)),
                                  d2d_->muted_brush.Get());
                draw_text_clipped(color_w,
                                  d2d_->body_format.Get(),
                                  native_rect("generic.rect.014", D2D1::RectF(rect.left + 24.0f, rect.top + 86.0f, rect.right - 24.0f, rect.top + 118.0f)),
                                  d2d_->muted_brush.Get());
            }

            const int lane_count = std::clamp(preview.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
            const D2D1_RECT_F viewport = D2D1::RectF(rect.left + 28.0f, rect.top + 132.0f,
                                                   rect.right - 28.0f, rect.bottom - 152.0f);
            const float scene_scale = std::min((viewport.right - viewport.left) / kBaseWidth,
                                               (viewport.bottom - viewport.top) / kBaseHeight);
            const float scene_left = viewport.left + ((viewport.right - viewport.left) - kBaseWidth * scene_scale) * 0.5f;
            const float scene_top = viewport.top + ((viewport.bottom - viewport.top) - kBaseHeight * scene_scale) * 0.5f;
            D2D1_MATRIX_3X2_F saved_transform{};
            ctx->GetTransform(&saved_transform);
            ctx->PushAxisAlignedClip(viewport, D2D1_ANTIALIAS_MODE_ALIASED);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scene_scale, scene_scale) *
                              D2D1::Matrix3x2F::Translation(scene_left, scene_top) * saved_transform);
            const D2D1_RECT_F scene_rect = D2D1::RectF(0, 0, kBaseWidth, kBaseHeight);
            if (d2d_->bg_brush) ctx->FillRectangle(scene_rect, d2d_->bg_brush.Get());
            if (auto* bitmap = find_song_card_preview_bitmap(preview.skin_background_path)) {
                const auto source = centered_bitmap_source_rect(bitmap->GetSize(), scene_rect);
                ctx->DrawBitmap(bitmap, scene_rect, std::clamp(preview.skin_background_opacity, 0.0f, 1.0f),
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
            }
            // The surrounding menu uses a different palette. Preview the exact
            // gameplay brush defaults, then restore the settings chrome.
            struct PreviewBrush { ID2D1SolidColorBrush* brush; const char* key; D2D1_COLOR_F color; };
            std::array<PreviewBrush, 10> preview_brushes{{
                {d2d_->text_brush.Get(), "text", D2D1::ColorF(0xE8ECF1)},
                {d2d_->accent_brush.Get(), "accent", D2D1::ColorF(0x6EE7F2)},
                {d2d_->muted_brush.Get(), "muted", D2D1::ColorF(0xB7C4D4)},
                {d2d_->card_brush.Get(), "card", D2D1::ColorF(0x1F2130)},
                {d2d_->panel_brush.Get(), "panel", D2D1::ColorF(0x14141C, 0.72f)},
                {d2d_->footer_brush.Get(), "footer", D2D1::ColorF(0x0B0B10, 0.75f)},
                {d2d_->button_brush.Get(), "button", D2D1::ColorF(0x242638)},
                {d2d_->button_selected_brush.Get(), "button_selected", D2D1::ColorF(0x6EE7F2, 0.22f)},
                {d2d_->button_border_brush.Get(), "border", D2D1::ColorF(0x31344A)},
                {d2d_->lane_divider_brush.Get(), "lane_divider", D2D1::ColorF(0xF6F8FF, 0.85f)}
            }};
            for (auto& item : preview_brushes) {
                if (!item.brush) continue;
                auto color = item.color;
                const auto custom = data.lobby_skin.theme_colors.find(item.key);
                if (data.lobby_skin.enabled && !data.lobby_skin.native_menu_renderer &&
                    custom != data.lobby_skin.theme_colors.end()) {
                    const auto& rgba = custom->second;
                    color = D2D1::ColorF(rgba[0], rgba[1], rgba[2], rgba[3]);
                }
                item.color = item.brush->GetColor();
                item.brush->SetColor(color);
            }
            draw_gameplay_hud(*skin_preview_scene, true);
            for (const auto& item : preview_brushes) if (item.brush) item.brush->SetColor(item.color);
            ctx->SetTransform(saved_transform);
            ctx->PopAxisAlignedClip();
            if (d2d_->button_border_brush)
                ctx->DrawRectangle(D2D1::RectF(scene_left, scene_top, scene_left + kBaseWidth * scene_scale,
                                              scene_top + kBaseHeight * scene_scale), d2d_->button_border_brush.Get());
            draw_text_clipped_aligned(wloc("Actual gameplay proportions", "실제 인게임 비율"), d2d_->body_format.Get(),
                                      D2D1::RectF(viewport.left, rect.bottom - 140.0f, viewport.right, rect.bottom - 110.0f),
                                      d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            const float preview_visual_opacity = static_cast<float>(std::clamp(preview.visual_opacity, 0.20, 1.0));
            const float swatch_top = rect.bottom - 96.0f;
            const float swatch_height = native_metric("generic.swatch_height", 54.0f);
            for (int lane = 0; lane < lane_count; ++lane) {
                const float x0 = viewport.left + (viewport.right - viewport.left) * lane / lane_count;
                const float x1 = viewport.left + (viewport.right - viewport.left) * (lane + 1) / lane_count;
                const D2D1_RECT_F swatch_rect = native_rect("generic.rect.025", D2D1::RectF(x0 + 4.0f, swatch_top, x1 - 4.0f, swatch_top + swatch_height));
                const D2D1_ROUNDED_RECT swatch_rr = D2D1::RoundedRect(swatch_rect, 10.0f, 10.0f);
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(
                        gameplay_note_fill_color(preview.lane_colors[static_cast<std::size_t>(lane)],
                                                 preview_visual_opacity));
                    ctx->FillRoundedRectangle(swatch_rr, d2d_->note_fill_brush.Get());
                }
                ID2D1SolidColorBrush* border =
                    (lane + 1 == preview.selected_lane) ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                if (border) {
                    ctx->DrawRoundedRectangle(swatch_rr, border, lane + 1 == preview.selected_lane ? 2.0f : 1.0f);
                }
                if (d2d_->body_format && d2d_->text_brush) {
                    const std::wstring lane_w = to_wide(std::to_string(lane + 1));
                    const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
                    const auto saved_paragraph = d2d_->body_format->GetParagraphAlignment();
                    if (modern_settings_screen && d2d_->note_fill_brush) {
                        const auto color = d2d_->note_fill_brush->GetColor();
                        const float luminance = color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
                        d2d_->text_brush->SetColor(D2D1::ColorF(luminance > 0.55f ? 0x101820 : 0xF3F6FA));
                        d2d_->body_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    }
                    draw_text_clipped_aligned(lane_w,
                                              d2d_->body_format.Get(),
                                              swatch_rect,
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->text_brush->SetColor(saved_text_color);
                    d2d_->body_format->SetParagraphAlignment(saved_paragraph);
                }
            }
        };

        if (data.generic.card_grid && data.generic.rows.size() == 8) {
#include "MenuWindow_draw_options_grid.inl"
            return;
        }

        if (data.generic.keymap_keyboard) {
#include "MenuWindow_draw_keymap_body.inl"
            return;
        }

        if (!data.generic.rows.empty() || !data.generic.notes.empty() ||
            !data.generic.footer_notes.empty() || data.generic.footer_reserved_lines > 0) {
            const float native_list_right = has_skin_preview ? right - preview_width - preview_gap
                                                            : right - 648.0f;
            const float row_left = left + (modern_settings_screen ? 20.0f : 24.0f);
            const float base_row_right = modern_settings_screen ? native_list_right - 20.0f
                : has_skin_preview ? (right - preview_width - preview_gap) : (right - 24.0f);
            const float row_safe_right =
                (!modern_settings_screen && !has_skin_preview && data.performance.visible)
                    ? std::min(base_row_right, performance_overlay_safe_left(24.0f))
                    : base_row_right;
            const bool roomy_option_layout = data.generic.rows.size() <= 12;
            const float row_height = modern_settings_screen ? 58.0f : roomy_option_layout ? 54.0f : 48.0f;
            const float row_gap = modern_settings_screen ? 8.0f : roomy_option_layout ? 10.0f : 8.0f;
            const float row_step = row_height + row_gap;
            const float value_width = has_skin_preview ? (roomy_option_layout ? 270.0f : 240.0f)
                                                       : (roomy_option_layout ? 360.0f : 340.0f);
            const float action_width = modern_settings_screen ? 44.0f : roomy_option_layout ? 62.0f : 56.0f;
            const float action_gap = native_metric("generic.action_gap", 10.0f);
            const float note_line_height = roomy_option_layout ? 34.0f : 28.0f;
            const float note_section_gap = data.generic.notes.empty() ? 0.0f : (roomy_option_layout ? 18.0f : 14.0f);
            const bool has_footer_notes =
                !modern_settings_screen && !has_skin_preview &&
                (!data.generic.footer_notes.empty() || data.generic.footer_reserved_lines > 0);
            const float footer_section_gap = has_footer_notes ? (roomy_option_layout ? 18.0f : 14.0f) : 0.0f;
            const float list_top = top + (modern_settings_screen ? 64.0f : 24.0f);
            const float list_bottom_limit = has_skin_preview ? bottom - 280.0f : bottom - 20.0f;
            IDWriteTextFormat* row_format =
                (modern_settings_screen || roomy_option_layout) && d2d_->option_format ? d2d_->option_format.Get() : d2d_->body_format.Get();

            if (modern_settings_screen) {
                draw_glass_panel(native_rect("generic.rect.026", D2D1::RectF(left, top, native_list_right, list_bottom_limit + 20.0f)),
                                 14, 0.92f, 0, false, 0);
                draw_text_clipped(wloc("PREFERENCES", "설정 항목"), d2d_->hud_format.Get(),
                                  native_rect("generic.rect.027", D2D1::RectF(row_left + 6, top + 22, base_row_right, top + 48)), d2d_->muted_brush.Get());
                draw_text_clipped_aligned(to_wide(std::to_string(data.generic.rows.size())) + wloc(" items", "개 항목"),
                                          d2d_->hud_format.Get(), native_rect("generic.rect.028", D2D1::RectF(row_left + 6, top + 22, base_row_right, top + 48)),
                                          d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                D2D1_RECT_F help_rect = has_skin_preview
                    ? native_rect("generic.rect.029", D2D1::RectF(left, bottom - 240.0f, native_list_right, bottom))
                    : fit_rect_below_performance_overlay(native_rect("generic.rect.030", D2D1::RectF(native_list_right + 24.0f, top, right, bottom)), bottom, 20.0f);
                if (data.generic.profile_preview_visible && !has_skin_preview && help_rect.bottom - help_rect.top >= 440.0f) {
                    const float preview_bottom = std::min(help_rect.top + 366.0f, help_rect.bottom - 220.0f);
                    const auto profile_preview = D2D1::RectF(help_rect.left, help_rect.top, help_rect.right, preview_bottom);
                    draw_glass_panel(profile_preview, 14, 0.92f, 0.25f, false, 0);
                    draw_text_clipped(wloc("PROFILE PREVIEW", "프로필 미리보기"), d2d_->hud_format.Get(),
                        D2D1::RectF(profile_preview.left + 26, profile_preview.top + 20, profile_preview.right - 26, profile_preview.top + 50),
                        d2d_->text_brush.Get());
                    const float avatar_size = std::min(224.0f, preview_bottom - profile_preview.top - 104.0f);
                    const float avatar_left = (profile_preview.left + profile_preview.right - avatar_size) * 0.5f;
                    const auto avatar = D2D1::RectF(avatar_left, profile_preview.top + 66, avatar_left + avatar_size, profile_preview.top + 66 + avatar_size);
                    if (auto* bitmap = find_song_card_preview_bitmap(data.generic.profile_avatar_path)) {
                        const auto source = centered_bitmap_source_rect(bitmap->GetSize(), avatar);
                        ctx->DrawBitmap(bitmap, avatar, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
                    } else {
                        draw_native_asset(native_menu_assets::kMark, avatar, 0.65f);
                    }
                    draw_native_panel_edge(avatar, 6, 0x9BCEDC, 0.8f);
                    help_rect.top = preview_bottom + 22.0f;
                }
                draw_generic_help(help_rect, data.generic.heading, data.generic.notes, data.generic.footer_notes);
            }

            if (!modern_settings_screen && has_skin_preview) {
                // Imported menu skins also need bounded, paginated help. Long
                // tips must not consume nearly every selectable settings row.
                draw_generic_help(D2D1::RectF(left + 16.0f, bottom - 240.0f,
                                             base_row_right, bottom - 16.0f),
                                  data.generic.heading, data.generic.notes, data.generic.footer_notes);
            }

            if (has_skin_preview) {
                const D2D1_RECT_F configured_preview = skin_layout_rect(
                    data, "generic.preview",
                    native_rect("generic.rect.031", D2D1::RectF(base_row_right + preview_gap + (modern_settings_screen ? 20.0f : 0.0f),
                                top + (modern_settings_screen ? 0.0f : 24.0f),
                                right - (modern_settings_screen ? 0.0f : 24.0f), bottom - (modern_settings_screen ? 0.0f : 24.0f))));
                const D2D1_RECT_F preview_rect = fit_rect_below_performance_overlay(
                    configured_preview,
                    bottom - 24.0f,
                    22.0f);
                if (preview_rect.bottom - preview_rect.top > 180.0f) {
                    draw_skin_preview_panel(data.generic.skin_preview, preview_rect);
                }
            }

            int displayed_note_count = modern_settings_screen || has_skin_preview
                ? 0 : static_cast<int>(data.generic.notes.size());
            float notes_height = 0.0f;
            if (displayed_note_count > 0) {
                notes_height = note_section_gap + note_line_height * static_cast<float>(displayed_note_count);
            }

            const int footer_reserved_line_count =
                std::max(static_cast<int>(data.generic.footer_notes.size()), std::max(0, data.generic.footer_reserved_lines));
            const float footer_notes_height =
                has_footer_notes
                    ? (footer_section_gap + note_line_height * static_cast<float>(footer_reserved_line_count))
                    : 0.0f;
            const float note_region_bottom = list_bottom_limit - footer_notes_height;
            float row_region_bottom = note_region_bottom - notes_height;
            const float minimum_row_region_height = data.generic.rows.empty() ? 0.0f : row_height;
            if (minimum_row_region_height > 0.0f && row_region_bottom - list_top < minimum_row_region_height && displayed_note_count > 0) {
                displayed_note_count = std::min(displayed_note_count, 3);
                notes_height = note_section_gap + note_line_height * static_cast<float>(displayed_note_count);
                row_region_bottom = note_region_bottom - notes_height;
            }
            if (minimum_row_region_height > 0.0f && row_region_bottom - list_top < minimum_row_region_height) {
                displayed_note_count = 0;
                notes_height = 0.0f;
                row_region_bottom = note_region_bottom;
            }

            int selected_row_index = 0;
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                if (data.generic.rows[i].selected) {
                    selected_row_index = static_cast<int>(i);
                    break;
                }
            }

            const float category_height = modern_settings_screen ? 28.0f : 24.0f;
            const auto list_window = settings_list_window(data.generic.rows, selected_row_index,
                row_region_bottom - list_top + row_gap, row_step, category_height);
            const int visible_row_count = list_window.count;
            const int row_window_start = list_window.start;
            const bool show_scrollbar =
                !data.generic.rows.empty() && visible_row_count < static_cast<int>(data.generic.rows.size());
            const float scrollbar_gap = show_scrollbar ? 12.0f : 0.0f;
            const float scrollbar_width = show_scrollbar ? 10.0f : 0.0f;
            const float row_right = row_safe_right - scrollbar_gap - scrollbar_width;
            float row_y = list_top;

            const int row_window_end = std::min<int>(static_cast<int>(data.generic.rows.size()),
                                                     row_window_start + visible_row_count);
            for (int row_list_index = row_window_start; row_list_index < row_window_end; ++row_list_index) {
                const auto& row = data.generic.rows[static_cast<std::size_t>(row_list_index)];
                if (settings_category_heading(data.generic.rows, row_list_index, row_window_start)) {
                    draw_text_clipped(to_wide(row.category), d2d_->hud_format.Get(),
                        D2D1::RectF(row_left + 8, row_y, row_right, row_y + category_height - 4),
                        d2d_->muted_brush.Get());
                    row_y += category_height;
                }
                const int64_t flash_age_ns = render_now_ns - row.change_flash_started_ns;
                const bool change_flash = row.change_flash_started_ns > 0 &&
                    flash_age_ns >= 0 && flash_age_ns < 900'000'000LL &&
                    ((flash_age_ns / 150'000'000LL) % 2 == 0);
                const bool visually_selected = row.selected || change_flash;
                const bool highlight = row.selected || row.activatable || row.adjustable;
                const D2D1_RECT_F row_rect = native_rect("generic.rect.032", D2D1::RectF(row_left, row_y, row_right, row_y + row_height));
                const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(row_rect, 12.0f, 12.0f);
                if (highlight || modern_settings_screen) {
                    ID2D1SolidColorBrush* fill =
                        visually_selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
                    if (fill) {
                        const float saved = fill->GetOpacity();
                        if (modern_settings_screen && !visually_selected) fill->SetOpacity(0.48f);
                        ctx->FillRoundedRectangle(rr, fill);
                        fill->SetOpacity(saved);
                    }
                    ID2D1SolidColorBrush* border =
                        visually_selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                    if (border) {
                        if (!modern_settings_screen) {
                            ctx->DrawRoundedRectangle(rr, border, visually_selected ? 2.0f : 1.0f);
                        } else if (visually_selected) {
                            ctx->FillRoundedRectangle(D2D1::RoundedRect(
                                native_rect("generic.rect.033", D2D1::RectF(row_left, row_y + 10, row_left + 3, row_y + row_height - 10)), 1.5f, 1.5f), border);
                        }
                    }
                }

                // Broad row hits must precede specific +/- and slider hits.
                if (row.activatable || row.adjustable || row.slider || !row.enabled) {
                    register_hit(row_rect, row.target_kind, row.row_index,
                        !row.enabled || row.adjustable || row.slider ? MenuHitPart::SelectOnly : MenuHitPart::Activate);
                }
                const float saved_text_opacity = d2d_->text_brush->GetOpacity();
                const float saved_accent_opacity = d2d_->accent_brush->GetOpacity();
                if (!row.enabled) {
                    d2d_->text_brush->SetOpacity(saved_text_opacity * 0.40f);
                    d2d_->accent_brush->SetOpacity(saved_accent_opacity * 0.30f);
                }
                const auto saved_row_paragraph = row_format ? row_format->GetParagraphAlignment() : DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
                draw_native_focus(row_rect, 12, 64 + static_cast<std::size_t>(row_list_index - row_window_start),
                                  row.row_index, visually_selected);
                if (modern_settings_screen && row_format) row_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

                const std::wstring label_w = to_wide(row.label);
                float label_right = row_right - value_width - 18.0f;
                if (row.slider) {
                    label_right = row_right - value_width - 18.0f;
                } else if (row.adjustable) {
                    label_right = row_right - value_width - action_width * 2.0f - action_gap * 2.0f - 18.0f;
                } else if (row.value.empty()) {
                    label_right = row_right - 18.0f;
                }
                const D2D1_RECT_F label_rect =
                    native_rect("generic.rect.034", D2D1::RectF(row_left + 18.0f, row_y + 8.0f, std::max(row_left + 160.0f, label_right), row_y + row_height - 8.0f));
                if (row_format && d2d_->text_brush) {
                    draw_text_clipped(label_w, row_format, label_rect, d2d_->text_brush.Get());
                }

                if (!row.value.empty() && row_format && d2d_->text_brush) {
                    const std::wstring value_w = to_wide(row.value);
                    if (row.slider) {
                        const float slider_group_left = row_right - value_width;
                        const float value_left = row_right - 92.0f;
                        const float track_left = slider_group_left + 12.0f;
                        const float track_right = value_left - 18.0f;
                        const float track_center_y = row_y + row_height * 0.5f;
                        const D2D1_RECT_F track_rect =
                            native_rect("generic.rect.035", D2D1::RectF(track_left, track_center_y - 4.0f,
                                        track_right, track_center_y + 4.0f));
                        const float slider_ratio =
                            static_cast<float>(std::clamp(row.slider_ratio, 0.0, 1.0));
                        const float knob_x =
                            track_rect.left + (track_rect.right - track_rect.left) * slider_ratio;
                        if (d2d_->card_brush) {
                            ctx->FillRoundedRectangle(
                                D2D1::RoundedRect(track_rect, 4.0f, 4.0f),
                                d2d_->card_brush.Get());
                        }
                        if (d2d_->button_border_brush) {
                            ctx->DrawRoundedRectangle(
                                D2D1::RoundedRect(track_rect, 4.0f, 4.0f),
                                d2d_->button_border_brush.Get(), 1.0f);
                        }
                        if (knob_x > track_rect.left && d2d_->accent_brush) {
                            const D2D1_RECT_F fill_rect =
                                native_rect("generic.rect.036", D2D1::RectF(track_rect.left, track_rect.top,
                                            knob_x, track_rect.bottom));
                            ctx->FillRoundedRectangle(
                                D2D1::RoundedRect(fill_rect, 4.0f, 4.0f),
                                d2d_->accent_brush.Get());
                        }
                        if (d2d_->accent_brush) {
                            ctx->FillEllipse(
                                D2D1::Ellipse(D2D1::Point2F(knob_x, track_center_y),
                                              9.0f, 9.0f),
                                d2d_->accent_brush.Get());
                        }
                        if (row.enabled) register_hit(
                            native_rect("generic.rect.037", D2D1::RectF(track_rect.left, row_y + 6.0f,
                                        track_rect.right, row_y + row_height - 6.0f)),
                            row.target_kind, row.row_index, MenuHitPart::SetValue);
                        const D2D1_RECT_F value_rect =
                            native_rect("generic.rect.038", D2D1::RectF(value_left, row_y + 8.0f,
                                        row_right - 18.0f, row_y + row_height - 8.0f));
                        draw_text_clipped_aligned(value_w,
                                                  row_format,
                                                  value_rect,
                                                  d2d_->text_brush.Get(),
                                                  DWRITE_TEXT_ALIGNMENT_TRAILING);
                    } else if (row.adjustable) {
                        const float plus_left = row_right - action_width;
                        const float minus_left = plus_left - action_gap - action_width;
                        const float value_right = minus_left - action_gap;
                        const D2D1_RECT_F value_rect =
                            native_rect("generic.rect.039", D2D1::RectF(std::max(label_rect.right + 12.0f, row_left + 320.0f),
                                        row_y + 8.0f,
                                        value_right,
                                        row_y + row_height - 8.0f));
                        draw_text_clipped_aligned(value_w,
                                                  row_format,
                                                  value_rect,
                                                  d2d_->text_brush.Get(),
                                                  DWRITE_TEXT_ALIGNMENT_TRAILING);

                        const auto draw_action = [&](const D2D1_RECT_F& rect, wchar_t symbol, MenuHitPart part, bool enabled) {
                            const D2D1_ROUNDED_RECT action_rr = D2D1::RoundedRect(rect, 10.0f, 10.0f);
                            ID2D1SolidColorBrush* fill = enabled ? d2d_->button_brush.Get() : d2d_->card_brush.Get();
                            if (fill) {
                                ctx->FillRoundedRectangle(action_rr, fill);
                            }
                            ID2D1SolidColorBrush* border = enabled ? d2d_->button_border_brush.Get() : d2d_->muted_brush.Get();
                            if (border) {
                                ctx->DrawRoundedRectangle(action_rr, border, 1.0f);
                            }
                            if (enabled) {
                                register_hit(rect, row.target_kind, row.row_index, part);
                            }
                            if (d2d_->title_format && d2d_->text_brush) {
                                const wchar_t buffer[2] = {symbol, L'\0'};
                                const auto saved_paragraph = d2d_->title_format->GetParagraphAlignment();
                                if (modern_settings_screen) d2d_->title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                                draw_text_clipped_aligned(buffer,
                                                          d2d_->title_format.Get(),
                                                          rect,
                                                          d2d_->text_brush.Get(),
                                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                                d2d_->title_format->SetParagraphAlignment(saved_paragraph);
                            }
                        };

                        const D2D1_RECT_F minus_rect =
                            native_rect("generic.rect.040", D2D1::RectF(minus_left, row_y + 6.0f, minus_left + action_width, row_y + row_height - 6.0f));
                        const D2D1_RECT_F plus_rect =
                            native_rect("generic.rect.041", D2D1::RectF(plus_left, row_y + 6.0f, plus_left + action_width, row_y + row_height - 6.0f));
                        // Selecting a setting never changes its value; only controls do.
                        register_hit(native_rect("generic.rect.042", D2D1::RectF(row_rect.left, row_rect.top,
                                                 minus_rect.left - action_gap * 0.5f, row_rect.bottom)),
                                     row.target_kind, row.row_index, MenuHitPart::SelectOnly);
                        draw_action(minus_rect, L'-', MenuHitPart::Decrement, row.enabled && row.decrement_enabled);
                        draw_action(plus_rect, L'+', MenuHitPart::Increment, row.enabled && row.increment_enabled);
                    } else {
                        const D2D1_RECT_F value_rect =
                            native_rect("generic.rect.043", D2D1::RectF(std::max(label_rect.right + 12.0f, row_left + 320.0f),
                                        row_y + 8.0f,
                                        row_right - 18.0f,
                                        row_y + row_height - 8.0f));
                        draw_text_clipped_aligned(value_w,
                                                  row_format,
                                                  value_rect,
                                                  d2d_->text_brush.Get(),
                                                  DWRITE_TEXT_ALIGNMENT_TRAILING);
                    }
                }

                d2d_->text_brush->SetOpacity(saved_text_opacity);
                d2d_->accent_brush->SetOpacity(saved_accent_opacity);
                if (row_format) row_format->SetParagraphAlignment(saved_row_paragraph);
                row_y += row_step;
            }

            if (show_scrollbar && d2d_->button_border_brush) {
                const float track_top = list_top + 4.0f;
                const float track_bottom = row_region_bottom - 4.0f;
                const D2D1_RECT_F track_rect =
                    native_rect("generic.rect.044", D2D1::RectF(row_right + scrollbar_gap, track_top, row_right + scrollbar_gap + scrollbar_width, track_bottom));
                if (track_rect.bottom > track_rect.top) {
                    if (d2d_->card_brush) {
                        d2d_->card_brush->SetOpacity(0.70f);
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(track_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                                  d2d_->card_brush.Get());
                        d2d_->card_brush->SetOpacity(1.0f);
                    }
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(track_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                              d2d_->button_border_brush.Get(), 1.0f);

                    const float track_height = track_rect.bottom - track_rect.top;
                    const float total_rows = static_cast<float>(data.generic.rows.size());
                    const float visible_rows = static_cast<float>(visible_row_count);
                    const float thumb_height =
                        std::min(track_height, std::max(44.0f, track_height * (visible_rows / total_rows)));
                    const int max_window_start = static_cast<int>(data.generic.rows.size()) - visible_row_count;
                    const float scroll_ratio = (max_window_start <= 0)
                                                   ? 0.0f
                                                   : static_cast<float>(row_window_start) /
                                                         static_cast<float>(max_window_start);
                    const float thumb_top = track_rect.top + (track_height - thumb_height) * scroll_ratio;
                    const D2D1_RECT_F thumb_rect =
                        native_rect("generic.rect.045", D2D1::RectF(track_rect.left + 1.0f, thumb_top, track_rect.right - 1.0f, thumb_top + thumb_height));
                    if (d2d_->accent_brush) {
                        d2d_->accent_brush->SetOpacity(0.92f);
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(thumb_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                                  d2d_->accent_brush.Get());
                        d2d_->accent_brush->SetOpacity(1.0f);
                    }

                    // The generic scrollbar used to be visual-only. Divide its
                    // enlarged hit area into absolute row slots so a click can
                    // move selection without also activating or adjusting it.
                    const float hit_left = track_rect.left - 8.0f;
                    const float hit_right = track_rect.right + 8.0f;
                    for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                        const auto& target_row = data.generic.rows[i];
                        if (target_row.target_kind != MenuHitTargetKind::SettingsRow &&
                            target_row.target_kind != MenuHitTargetKind::OptionsItem) {
                            continue;
                        }
                        const float slot_top =
                            track_rect.top + track_height * static_cast<float>(i) / total_rows;
                        const float slot_bottom =
                            track_rect.top + track_height * static_cast<float>(i + 1) / total_rows;
                        register_hit(native_rect("generic.rect.046", D2D1::RectF(hit_left, slot_top, hit_right, slot_bottom)),
                                     target_row.target_kind,
                                     target_row.row_index,
                                     MenuHitPart::SelectOnly);
                    }
                }
            }

            if (displayed_note_count > 0) {
                const float notes_top = note_region_bottom - notes_height;
                float note_y = notes_top + note_section_gap;
                if (d2d_->button_border_brush) {
                    d2d_->button_border_brush->SetOpacity(0.35f);
                    ctx->DrawLine(D2D1::Point2F(row_left, notes_top), D2D1::Point2F(row_right, notes_top),
                                  d2d_->button_border_brush.Get(), 1.0f);
                    d2d_->button_border_brush->SetOpacity(1.0f);
                }
                for (int i = 0; i < displayed_note_count; ++i) {
                    std::string note_text = data.generic.notes[static_cast<std::size_t>(i)];
                    if (displayed_note_count >= 2 &&
                        displayed_note_count < static_cast<int>(data.generic.notes.size()) &&
                        i == displayed_note_count - 1) {
                        const int hidden_count =
                            static_cast<int>(data.generic.notes.size()) - (displayed_note_count - 1);
                        note_text = loc("F1: ", "F1: 도움말 ") + std::to_string(hidden_count) +
                                    loc(" more help lines", "줄 더 보기");
                    }
                    const std::wstring note_w = to_wide(note_text);
                    const D2D1_RECT_F note_rect =
                        native_rect("generic.rect.047", D2D1::RectF(row_left + 6.0f, note_y, row_right - 6.0f, note_y + 30.0f));
                    if (row_format && d2d_->muted_brush) {
                        draw_text_clipped(note_w, row_format, note_rect, d2d_->muted_brush.Get());
                    }
                    note_y += note_line_height;
                }
            }
            if (has_footer_notes) {
                const float footer_top = list_bottom_limit - footer_notes_height;
                float footer_y = footer_top + footer_section_gap;
                if (d2d_->button_border_brush) {
                    d2d_->button_border_brush->SetOpacity(0.35f);
                    ctx->DrawLine(D2D1::Point2F(row_left, footer_top), D2D1::Point2F(row_right, footer_top),
                                  d2d_->button_border_brush.Get(), 1.0f);
                    d2d_->button_border_brush->SetOpacity(1.0f);
                }
                for (const auto& note : data.generic.footer_notes) {
                    const std::wstring note_w = to_wide(note);
                    const D2D1_RECT_F note_rect =
                        native_rect("generic.rect.048", D2D1::RectF(row_left + 6.0f, footer_y, row_right - 6.0f, footer_y + 30.0f));
                    if (row_format && d2d_->muted_brush) {
                        draw_text_clipped(note_w, row_format, note_rect, d2d_->muted_brush.Get());
                    }
                    footer_y += note_line_height;
                }
            }
            return;
        }

        const float line_left = left + 24.0f;
        const float line_right =
            data.performance.visible ? std::min(right - 24.0f, performance_overlay_safe_left(24.0f)) : (right - 24.0f);
        float line_y = top + 24.0f;
        const float line_height = native_metric("generic.line_height", 26.0f);
        for (const auto& line : data.lines) {
            if (line_y + line_height > bottom - 16.0f) {
                break;
            }
            const bool selected = line_is_selected(line);
            const bool is_option = line_has_prefix(line);
            const std::wstring text = to_wide(strip_prefix(line));
            D2D1_RECT_F line_rect =
                native_rect("generic.rect.049", D2D1::RectF(line_left, line_y, line_right, line_y + line_height));

            if (is_option) {
                D2D1_RECT_F button_rect = native_rect("generic.rect.050", D2D1::RectF(line_left - 12.0f, line_y - 4.0f,
                                                      line_right, line_y + line_height + 4.0f));
                D2D1_ROUNDED_RECT button = D2D1::RoundedRect(button_rect, 10.0f, 10.0f);
                ID2D1SolidColorBrush* fill =
                    selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
                if (fill) {
                    ctx->FillRoundedRectangle(button, fill);
                }
                ID2D1SolidColorBrush* border =
                    selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                if (border) {
                    ctx->DrawRoundedRectangle(button, border, selected ? 2.0f : 1.0f);
                }
            }

            ID2D1SolidColorBrush* brush = d2d_->text_brush.Get();
            if (brush && d2d_->body_format) {
                draw_text_clipped(text, d2d_->body_format.Get(), line_rect, brush);
            }
            line_y += line_height + 8.0f;
        }
