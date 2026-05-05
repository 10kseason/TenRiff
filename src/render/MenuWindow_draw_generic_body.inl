        const ScreenContentBands bands =
            make_screen_content_bands(48.0f, 72.0f, false, 20.0f, 18.0f);
        const float left = 80.0f;
        const float top = bands.body_top;
        const float right = kBaseWidth - 80.0f;
        const float bottom = bands.body_bottom;

        if (d2d_->card_brush) {
            D2D1_ROUNDED_RECT card =
                D2D1::RoundedRect(D2D1::RectF(left, top, right, bottom), 18.0f, 18.0f);
            ctx->FillRoundedRectangle(card, d2d_->card_brush.Get());
        }

        std::string header = "TenRiff";
        if (!data.generic.heading.empty()) {
            header += " / " + data.generic.heading;
        } else if (!data.screen_title.empty()) {
            header += " / " + data.screen_title;
        }
        const std::wstring header_wide = to_wide(header);
        D2D1_RECT_F header_rect = D2D1::RectF(left, 48.0f, right, 120.0f);
        if (d2d_->title_format && d2d_->accent_brush) {
            draw_text_clipped(header_wide, d2d_->title_format.Get(), header_rect, d2d_->accent_brush.Get());
        }

        const bool has_skin_preview = data.generic.skin_preview.visible;
        const float preview_gap = has_skin_preview ? 28.0f : 0.0f;
        const float preview_width = has_skin_preview ? 620.0f : 0.0f;

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

            const std::wstring title_w = wloc("LIVE PREVIEW", "실시간 미리보기");
            const std::wstring mode_w =
                to_wide(preview.mode_label + " / " + loc("Lane ", "레인 ") + std::to_string(std::max(1, preview.selected_lane)));
            const std::wstring color_w = to_wide(loc("Color: ", "색상: ") + preview.selected_color_label);
            if (d2d_->title_format && d2d_->text_brush) {
                draw_text_clipped(title_w,
                                  d2d_->title_format.Get(),
                                  D2D1::RectF(rect.left + 24.0f, rect.top + 18.0f, rect.right - 24.0f, rect.top + 60.0f),
                                  d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_text_clipped(mode_w,
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(rect.left + 24.0f, rect.top + 56.0f, rect.right - 24.0f, rect.top + 88.0f),
                                  d2d_->muted_brush.Get());
                draw_text_clipped(color_w,
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(rect.left + 24.0f, rect.top + 86.0f, rect.right - 24.0f, rect.top + 118.0f),
                                  d2d_->muted_brush.Get());
            }

            const float preview_bounds_left = rect.left + 28.0f;
            const float preview_bounds_right = rect.right - 28.0f;
            const float field_top = rect.top + 132.0f;
            const float field_bottom = rect.bottom - 152.0f;
            const int lane_count = std::clamp(preview.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
            std::array<float, kGameplayHudMaxLanes> imported_lane_divider_widths{};
            std::size_t imported_lane_divider_width_count = 0;
            float imported_note_width_ratio = 1.0f;
            float imported_note_height_ratio = 1.0f;
            const bool use_imported_metrics = normalize_gameplay_skin_source(preview.skin_source) != "native";
            if (use_imported_metrics) {
                const auto skin = app::resolve_imported_gameplay_skin(
                    preview.skin_source,
                    preview.external_skin_root,
                    preview.external_skin_name,
                    lane_count,
                    preview.lr2_resolution_override);
                imported_note_width_ratio = skin.imported_note_width_ratio;
                imported_note_height_ratio = skin.imported_note_height_ratio;
                imported_lane_divider_width_count =
                    (std::min)(skin.lane_divider_widths.size(), imported_lane_divider_widths.size());
                for (std::size_t divider = 0; divider < imported_lane_divider_width_count; ++divider) {
                    imported_lane_divider_widths[divider] = skin.lane_divider_widths[divider];
                }
            }
            const float note_width_scale = effective_gameplay_note_width_scale(
                preview.note_width_scale,
                imported_note_width_ratio,
                use_imported_metrics);
            const float note_height_scale = effective_gameplay_note_height_scale(
                preview.note_height_scale,
                imported_note_height_ratio,
                use_imported_metrics);
            const float lane_center_gap_scale = clamp_gameplay_lane_center_gap_scale(preview.lane_center_gap_scale);
            const GameplayFieldLayout field_layout = build_gameplay_field_layout(
                preview_bounds_left,
                preview_bounds_right,
                field_top,
                field_bottom,
                lane_count,
                note_width_scale,
                preview.lane_width_scale_count,
                preview.lane_width_scales,
                preview.lane_spacing_scale_count,
                preview.lane_spacing_scales,
                lane_center_gap_scale);
            const float field_height = field_layout.height;
            const float field_left = field_layout.left;
            const float field_right = field_layout.right;
            const float hit_line_y =
                gameplay_field_y(field_layout.top,
                                 field_height,
                                 clamp_gameplay_judgement_line(preview.judgement_line_position));
            const float head_half_h = gameplay_note_head_half_height(note_height_scale);
            const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
            std::array<float, kGameplayHudMaxLanes> preview_lane_divider_widths{};
            const std::size_t preview_lane_divider_width_count = resolve_gameplay_lane_divider_widths(
                lane_count,
                preview.lane_divider_width_scale,
                imported_lane_divider_width_count,
                imported_lane_divider_widths,
                preview_lane_divider_widths);
            const float hold_body_width_scale =
                clamp_gameplay_hold_body_width_scale(preview.hold_body_width_scale);
            const double combo_position = clamp_gameplay_combo_position(preview.combo_position);
            const std::string note_shape = normalize_gameplay_note_shape(preview.note_shape);
            const bool note_border_enabled = preview.note_border_enabled;
            const float preview_visual_opacity =
                static_cast<float>(std::clamp(preview.visual_opacity, 0.20, 1.0));
            const float preview_outline_opacity =
                static_cast<float>(std::clamp(preview.note_outline_opacity, 0.0, 1.0) * preview_visual_opacity);
            const float preview_hold_body_opacity =
                static_cast<float>(std::clamp(preview.hold_body_opacity, 0.05, 0.60) * preview_visual_opacity);
            const float preview_lane_bg_opacity =
                static_cast<float>(std::clamp(preview.lane_background_opacity, 0.0, 0.45) * preview_visual_opacity);
            const std::string preview_key_label_position =
                config::normalize_skin_key_label_position_token(preview.key_label_position);

            const D2D1_ROUNDED_RECT field_rr =
                D2D1::RoundedRect(D2D1::RectF(field_left, field_layout.top, field_right, field_layout.bottom),
                                  14.0f,
                                  14.0f);
            if (d2d_->card_brush) {
                ctx->FillRoundedRectangle(field_rr, d2d_->card_brush.Get());
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(field_rr, d2d_->button_border_brush.Get(), 1.1f);
            }

            for (int lane = 0; lane < lane_count; ++lane) {
                const float x0 = gameplay_lane_left(field_layout, lane);
                const float x1 = gameplay_lane_right(field_layout, lane);
                const uint32_t rgb = preview.lane_colors[static_cast<std::size_t>(lane)];
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(
                        gameplay_lane_preview_fill(rgb, lane + 1 == preview.selected_lane, preview_lane_bg_opacity));
                    ctx->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(x0 + 2.0f,
                                                      field_layout.top + 2.0f,
                                                      x1 - 2.0f,
                                                      field_layout.bottom - 2.0f),
                                          5.0f,
                                          5.0f),
                        d2d_->note_fill_brush.Get());
                }
                if (preview.show_lane_dividers &&
                    d2d_->lane_divider_brush &&
                    static_cast<std::size_t>(lane) < preview_lane_divider_width_count) {
                    if (gameplay_is_center_gap_divider(field_layout, static_cast<std::size_t>(lane))) {
                        continue;
                    }
                    const float divider_width = preview_lane_divider_widths[static_cast<std::size_t>(lane)];
                    if (divider_width <= 0.01f) {
                        continue;
                    }
                    const float divider_x = gameplay_lane_divider_x(field_layout, static_cast<std::size_t>(lane));
                    ctx->DrawLine(D2D1::Point2F(divider_x, field_layout.top), D2D1::Point2F(divider_x, field_layout.bottom),
                                  d2d_->lane_divider_brush.Get(), divider_width);
                }
            }
            if (preview.selected_gap > 0 && d2d_->accent_brush) {
                const int gap_index = std::min(preview.selected_gap - 1, lane_count - 2);
                const float gap_left = gameplay_lane_right(field_layout, gap_index);
                const float gap_width = gameplay_lane_gap_after(field_layout, gap_index);
                if (gap_width > 1.0f) {
                    d2d_->accent_brush->SetOpacity(0.24f);
                    ctx->FillRectangle(D2D1::RectF(gap_left,
                                                   field_layout.top + 12.0f,
                                                   gap_left + gap_width,
                                                   field_layout.bottom - 12.0f),
                                       d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetOpacity(1.0f);
                }
            }

            if (preview.show_judgement_line && d2d_->judgement_line_brush) {
                const D2D1_RECT_F hit_line_rect =
                    gameplay_judgement_line_rect(field_layout, hit_line_y, note_height_scale);
                if (preview.judgement_line_glow_enabled) {
                    const float saved_opacity = d2d_->judgement_line_brush->GetOpacity();
                    d2d_->judgement_line_brush->SetOpacity(0.20f * preview_visual_opacity);
                    ctx->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(field_left + 5.0f,
                                                      hit_line_rect.top - 11.0f,
                                                      field_right - 5.0f,
                                                      hit_line_rect.bottom + 11.0f),
                                          10.0f,
                                          10.0f),
                        d2d_->judgement_line_brush.Get());
                    d2d_->judgement_line_brush->SetOpacity(saved_opacity);
                }
                ctx->FillRoundedRectangle(D2D1::RoundedRect(hit_line_rect, 5.0f, 5.0f),
                                          d2d_->judgement_line_brush.Get());
                ctx->DrawLine(D2D1::Point2F(field_left, hit_line_y), D2D1::Point2F(field_right, hit_line_y),
                              d2d_->judgement_line_brush.Get(), 1.4f);
            }
            if (preview.show_gear_boundary_line && d2d_->button_border_brush) {
                const float gear_top = gameplay_osu_gear_top(field_layout, hit_line_y, note_height_scale);
                ctx->DrawLine(D2D1::Point2F(field_left, gear_top), D2D1::Point2F(field_right, gear_top),
                              d2d_->button_border_brush.Get(), 1.1f);
            }

            for (int lane = 0; lane < lane_count; ++lane) {
                const uint32_t rgb = preview.lane_colors[static_cast<std::size_t>(lane)];
                const float lane_center = gameplay_lane_center(field_layout, lane);
                const float note_width = gameplay_note_width(field_layout, lane);
                const float x0 = lane_center - note_width * 0.5f;
                const float x1 = lane_center + note_width * 0.5f;
                const float default_y =
                    field_layout.top + field_height * (0.16f + 0.09f * static_cast<float>((lane + 1) % 4));
                const bool draw_selected_hold_preview = (lane + 1 == preview.selected_lane) && d2d_->note_hold_brush;
                const float y = draw_selected_hold_preview ? hit_line_y : default_y;
                if (d2d_->note_fill_brush) {
                    d2d_->note_fill_brush->SetColor(gameplay_note_fill_color(rgb, preview_visual_opacity));
                }
                if (d2d_->note_border_brush) {
                    d2d_->note_border_brush->SetColor(gameplay_note_border_color(rgb, preview_outline_opacity));
                }
                if (d2d_->note_hold_brush) {
                    d2d_->note_hold_brush->SetColor(gameplay_note_hold_color(rgb, preview_hold_body_opacity));
                }

                if (draw_selected_hold_preview) {
                    const float tail_y =
                        std::max(field_layout.top + 20.0f, hit_line_y - field_height * 0.18f);
                    const float hold_half_width = std::max(4.0f, note_width * 0.5f * hold_body_width_scale);
                    const float head_body_inset = gameplay_hold_body_cap_inset(note_shape, head_half_h);
                    const D2D1_RECT_F hold_rect =
                        D2D1::RectF(lane_center - hold_half_width,
                                    tail_y,
                                    lane_center + hold_half_width,
                                    y - head_body_inset);
                    if (hold_rect.bottom > hold_rect.top) {
                        draw_gameplay_hold_body(ctx,
                                                d2d_->d2d_factory.Get(),
                                                hold_rect,
                                                lane_center,
                                                y,
                                                tail_y,
                                                hold_half_width,
                                                preview.hold_tail_taper_enabled,
                                                d2d_->note_hold_brush.Get());
                    }
                }

                const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
                if (d2d_->note_fill_brush) {
                    draw_note_primitive(ctx, note_rect, d2d_->note_fill_brush.Get(), d2d_->note_border_brush.Get(),
                                        0.85f, note_shape, note_border_enabled);
                }
            }

            if (preview_key_label_position != "off" && d2d_->hud_format && d2d_->text_brush) {
                const bool top_labels = preview_key_label_position == "top";
                const float label_top = top_labels ? field_layout.top + 8.0f : field_layout.bottom - 30.0f;
                const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.38f * preview_visual_opacity));
                for (int lane = 0; lane < lane_count; ++lane) {
                    const std::wstring lane_w = to_wide(std::to_string(lane + 1));
                    draw_text_clipped_aligned(
                        lane_w,
                        d2d_->hud_format.Get(),
                        D2D1::RectF(gameplay_lane_left(field_layout, lane) + 2.0f,
                                    label_top,
                                    gameplay_lane_right(field_layout, lane) - 2.0f,
                                    label_top + 22.0f),
                        d2d_->text_brush.Get(),
                        DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                d2d_->text_brush->SetColor(saved_text_color);
            }

            if (d2d_->header_format && d2d_->accent_brush) {
                const std::wstring combo_w = L"123";
                const D2D1_RECT_F combo_rect =
                    gameplay_combo_overlay_rect(field_layout,
                                                combo_position,
                                                40.0f,
                                                40.0f,
                                                40.0f,
                                                0.0f,
                                                8.0f);
                draw_text_clipped_aligned(combo_w,
                                          d2d_->header_format.Get(),
                                          combo_rect,
                                          d2d_->accent_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            const float swatch_top = rect.bottom - 96.0f;
            const float swatch_height = 54.0f;
            for (int lane = 0; lane < lane_count; ++lane) {
                const float x0 = gameplay_lane_left(field_layout, lane);
                const float x1 = gameplay_lane_right(field_layout, lane);
                const D2D1_RECT_F swatch_rect = D2D1::RectF(x0 + 4.0f, swatch_top, x1 - 4.0f, swatch_top + swatch_height);
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
                    draw_text_clipped_aligned(lane_w,
                                              d2d_->body_format.Get(),
                                              swatch_rect,
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }
        };

        if (!data.generic.rows.empty() || !data.generic.notes.empty() ||
            !data.generic.footer_notes.empty() || data.generic.footer_reserved_lines > 0) {
            const float row_left = left + 24.0f;
            const float base_row_right = has_skin_preview ? (right - preview_width - preview_gap) : (right - 24.0f);
            const float row_safe_right =
                (!has_skin_preview && data.performance.visible)
                    ? std::min(base_row_right, performance_overlay_safe_left(24.0f))
                    : base_row_right;
            const bool roomy_option_layout = data.generic.rows.size() <= 12;
            const float row_height = roomy_option_layout ? 54.0f : 48.0f;
            const float row_gap = roomy_option_layout ? 10.0f : 8.0f;
            const float row_step = row_height + row_gap;
            const float value_width = has_skin_preview ? (roomy_option_layout ? 270.0f : 240.0f)
                                                       : (roomy_option_layout ? 360.0f : 340.0f);
            const float action_width = roomy_option_layout ? 62.0f : 56.0f;
            const float action_gap = 10.0f;
            const float note_line_height = roomy_option_layout ? 34.0f : 28.0f;
            const float note_section_gap = data.generic.notes.empty() ? 0.0f : (roomy_option_layout ? 18.0f : 14.0f);
            const bool has_footer_notes =
                !data.generic.footer_notes.empty() || data.generic.footer_reserved_lines > 0;
            const float footer_section_gap = has_footer_notes ? (roomy_option_layout ? 18.0f : 14.0f) : 0.0f;
            const float list_top = top + 24.0f;
            const float list_bottom_limit = bottom - 20.0f;
            IDWriteTextFormat* row_format =
                roomy_option_layout && d2d_->option_format ? d2d_->option_format.Get() : d2d_->body_format.Get();

            if (has_skin_preview) {
                const D2D1_RECT_F preview_rect = fit_rect_below_performance_overlay(
                    D2D1::RectF(base_row_right + preview_gap, top + 24.0f, right - 24.0f, bottom - 24.0f),
                    bottom - 24.0f,
                    22.0f);
                if (preview_rect.bottom - preview_rect.top > 180.0f) {
                    draw_skin_preview_panel(data.generic.skin_preview, preview_rect);
                }
            }

            int displayed_note_count = static_cast<int>(data.generic.notes.size());
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

            int visible_row_count = 0;
            if (!data.generic.rows.empty()) {
                visible_row_count = std::max(1, static_cast<int>(
                                                    std::floor((row_region_bottom - list_top + row_gap) / row_step)));
                visible_row_count = std::min<int>(visible_row_count, static_cast<int>(data.generic.rows.size()));
            }

            int row_window_start = 0;
            if (!data.generic.rows.empty() && visible_row_count < static_cast<int>(data.generic.rows.size())) {
                const int max_window_start = static_cast<int>(data.generic.rows.size()) - visible_row_count;
                row_window_start = std::clamp(selected_row_index - visible_row_count / 2, 0, max_window_start);
            }
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
                const bool highlight = row.selected || row.activatable || row.adjustable;
                const D2D1_RECT_F row_rect = D2D1::RectF(row_left, row_y, row_right, row_y + row_height);
                const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(row_rect, 12.0f, 12.0f);
                if (highlight) {
                    ID2D1SolidColorBrush* fill =
                        row.selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
                    if (fill) {
                        ctx->FillRoundedRectangle(rr, fill);
                    }
                    ID2D1SolidColorBrush* border =
                        row.selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
                    if (border) {
                        ctx->DrawRoundedRectangle(rr, border, row.selected ? 2.0f : 1.0f);
                    }
                }

                const std::wstring label_w = to_wide(row.label);
                float label_right = row_right - value_width - 18.0f;
                if (row.adjustable) {
                    label_right = row_right - value_width - action_width * 2.0f - action_gap * 2.0f - 18.0f;
                } else if (row.value.empty()) {
                    label_right = row_right - 18.0f;
                }
                const D2D1_RECT_F label_rect =
                    D2D1::RectF(row_left + 18.0f, row_y + 8.0f, std::max(row_left + 160.0f, label_right), row_y + row_height - 8.0f);
                if (row_format && d2d_->text_brush) {
                    draw_text_clipped(label_w, row_format, label_rect, d2d_->text_brush.Get());
                }

                if (!row.value.empty() && row_format && d2d_->text_brush) {
                    const std::wstring value_w = to_wide(row.value);
                    if (row.adjustable) {
                        const float plus_left = row_right - action_width;
                        const float minus_left = plus_left - action_gap - action_width;
                        const float value_right = minus_left - action_gap;
                        const D2D1_RECT_F value_rect =
                            D2D1::RectF(std::max(label_rect.right + 12.0f, row_left + 320.0f),
                                        row_y + 8.0f,
                                        value_right,
                                        row_y + row_height - 8.0f);
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
                                draw_text_clipped_aligned(buffer,
                                                          d2d_->title_format.Get(),
                                                          rect,
                                                          d2d_->text_brush.Get(),
                                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                            }
                        };

                        const D2D1_RECT_F minus_rect =
                            D2D1::RectF(minus_left, row_y + 6.0f, minus_left + action_width, row_y + row_height - 6.0f);
                        const D2D1_RECT_F plus_rect =
                            D2D1::RectF(plus_left, row_y + 6.0f, plus_left + action_width, row_y + row_height - 6.0f);
                        draw_action(minus_rect, L'-', MenuHitPart::Decrement, row.decrement_enabled);
                        draw_action(plus_rect, L'+', MenuHitPart::Increment, row.increment_enabled);
                    } else {
                        const D2D1_RECT_F value_rect =
                            D2D1::RectF(std::max(label_rect.right + 12.0f, row_left + 320.0f),
                                        row_y + 8.0f,
                                        row_right - 18.0f,
                                        row_y + row_height - 8.0f);
                        draw_text_clipped_aligned(value_w,
                                                  row_format,
                                                  value_rect,
                                                  d2d_->text_brush.Get(),
                                                  DWRITE_TEXT_ALIGNMENT_TRAILING);
                    }
                }

                if (row.activatable) {
                    register_hit(row_rect, row.target_kind, row.row_index, MenuHitPart::Activate);
                }
                row_y += row_step;
            }

            if (show_scrollbar && d2d_->button_border_brush) {
                const float track_top = list_top + 4.0f;
                const float track_bottom = row_region_bottom - 4.0f;
                const D2D1_RECT_F track_rect =
                    D2D1::RectF(row_right + scrollbar_gap, track_top, row_right + scrollbar_gap + scrollbar_width, track_bottom);
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
                        D2D1::RectF(track_rect.left + 1.0f, thumb_top, track_rect.right - 1.0f, thumb_top + thumb_height);
                    if (d2d_->accent_brush) {
                        d2d_->accent_brush->SetOpacity(0.92f);
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(thumb_rect, scrollbar_width * 0.5f, scrollbar_width * 0.5f),
                                                  d2d_->accent_brush.Get());
                        d2d_->accent_brush->SetOpacity(1.0f);
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
                    const std::wstring note_w = to_wide(data.generic.notes[static_cast<std::size_t>(i)]);
                    const D2D1_RECT_F note_rect =
                        D2D1::RectF(row_left + 6.0f, note_y, row_right - 6.0f, note_y + 30.0f);
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
                        D2D1::RectF(row_left + 6.0f, footer_y, row_right - 6.0f, footer_y + 30.0f);
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
        const float line_height = 26.0f;
        for (const auto& line : data.lines) {
            if (line_y + line_height > bottom - 16.0f) {
                break;
            }
            const bool selected = line_is_selected(line);
            const bool is_option = line_has_prefix(line);
            const std::wstring text = to_wide(strip_prefix(line));
            D2D1_RECT_F line_rect =
                D2D1::RectF(line_left, line_y, line_right, line_y + line_height);

            if (is_option) {
                D2D1_RECT_F button_rect = D2D1::RectF(line_left - 12.0f, line_y - 4.0f,
                                                      line_right, line_y + line_height + 4.0f);
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

