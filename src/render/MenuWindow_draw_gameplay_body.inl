        const float header_left = 84.0f;
        const float header_top = 42.0f;
        const float header_right = kBaseWidth - 84.0f;
        const double judgement_line_position = clamp_gameplay_judgement_line(data.gameplay.judgement_line_position);
        const double combo_position = clamp_gameplay_combo_position(data.gameplay.combo_position);
        const bool use_imported_metrics = normalize_gameplay_skin_source(data.gameplay.skin_source) != "native";
        const float note_width_scale = effective_gameplay_note_width_scale(
            data.gameplay.note_width_scale,
            gameplay_note_sprite_cache_.imported_note_width_ratio,
            use_imported_metrics);
        const float note_height_scale = effective_gameplay_note_height_scale(
            data.gameplay.note_height_scale,
            gameplay_note_sprite_cache_.imported_note_height_ratio,
            use_imported_metrics);
        const float hold_body_width_scale =
            clamp_gameplay_hold_body_width_scale(data.gameplay.hold_body_width_scale);
        const bool note_border_enabled = data.gameplay.note_border_enabled;
        const std::string note_shape = normalize_gameplay_note_shape(data.gameplay.note_shape);

        const GameplayMotionDiagnostics motion_diagnostics = compute_gameplay_motion_diagnostics(
            GameplayMotionState{
                data.gameplay.current_sample,
                data.gameplay.duration_samples,
                data.gameplay.sample_rate,
                data.gameplay.audio_sample_time_ns,
                0,
                data.gameplay.audio_buffer_frames,
                data.gameplay.visual_offset_ms,
                data.gameplay.finished,
                data.gameplay.game_over,
            },
            timing::HighResClock::now_ns());
        const int64_t display_sample = motion_diagnostics.display_sample;

        auto sample_to_y = [&](int64_t sample) -> float {
            return static_cast<float>(compute_gameplay_note_y_normalized(sample,
                                                                         display_sample,
                                                                         data.gameplay.lookahead_samples,
                                                                         data.gameplay.past_samples,
                                                                         judgement_line_position));
        };

        auto draw_gameplay_header = [&]() {
            if (d2d_->title_format && d2d_->text_brush) {
                const D2D1_RECT_F title_rect =
                    D2D1::RectF(header_left, header_top, header_right * 0.60f, header_top + 52.0f);
                ctx->DrawText(gameplay_hud_cache_.title_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.title_text.size()),
                              d2d_->title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const D2D1_RECT_F artist_rect =
                    D2D1::RectF(header_left, header_top + 46.0f, header_right * 0.60f, header_top + 84.0f);
                ctx->DrawText(gameplay_hud_cache_.artist_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.artist_text.size()),
                              d2d_->body_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F speed_rect =
                    D2D1::RectF(header_left, header_top + 82.0f, header_right * 0.68f, header_top + 118.0f);
                ctx->DrawText(gameplay_hud_cache_.speed_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.speed_text.size()),
                              d2d_->hud_format.Get(), speed_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->title_format && d2d_->text_brush) {
                const D2D1_RECT_F score_rect =
                    D2D1::RectF(kBaseWidth - 700.0f, header_top, kBaseWidth - 90.0f, header_top + 52.0f);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(gameplay_hud_cache_.score_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.score_text.size()),
                              d2d_->title_format.Get(), score_rect, d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F combo_rect =
                    D2D1::RectF(kBaseWidth - 780.0f, header_top + 52.0f, kBaseWidth - 90.0f, header_top + 84.0f);
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(gameplay_hud_cache_.combo_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.combo_text.size()),
                              d2d_->body_format.Get(), combo_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F judge_stats_rect =
                    D2D1::RectF(kBaseWidth - 780.0f, header_top + 82.0f, kBaseWidth - 90.0f, header_top + 116.0f);
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                ctx->DrawText(gameplay_hud_cache_.judge_stats_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.judge_stats_text.size()),
                              d2d_->hud_format.Get(), judge_stats_rect, d2d_->muted_brush.Get());
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        };

        if (gameplay_hud_cache_.text_revision != data.gameplay.text_revision) {
            const std::string title = data.gameplay.title.empty() ? "Unknown Track" : data.gameplay.title;
            const std::string artist = data.gameplay.artist.empty() ? "Unknown Artist" : data.gameplay.artist;
            gameplay_hud_cache_.title_text = to_wide(title);
            gameplay_hud_cache_.artist_text = to_wide(artist);
            gameplay_hud_cache_.speed_text =
                to_wide("RATE x" + format_decimal(data.gameplay.rate, 2) +
                        " / HS " + format_decimal(data.gameplay.hispeed, 2) +
                        " / BPM " + std::to_string(static_cast<int>(std::llround(data.gameplay.bpm))) +
                        " / Scroll " + std::to_string(static_cast<int>(std::llround(data.gameplay.scroll_speed))));
            gameplay_hud_cache_.score_text =
                to_wide("SCORE  " + format_int_with_commas(data.gameplay.score));
            gameplay_hud_cache_.combo_text =
                to_wide("COMBO " + std::to_string(data.gameplay.combo) +
                        "   MAX " + std::to_string(data.gameplay.max_combo) +
                        "   ACC " + format_decimal(data.gameplay.accuracy, 2) + "%");
            gameplay_hud_cache_.judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.pg) +
                        "  GR " + std::to_string(data.gameplay.gr) +
                        "  G " + std::to_string(data.gameplay.gd) +
                        "  BAD " + std::to_string(data.gameplay.bd));
            gameplay_hud_cache_.gauge_label_text = to_wide(data.gameplay.gauge_label);
            gameplay_hud_cache_.gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.gauge))) + "%");

            if (data.gameplay.has_feedback) {
                gameplay_hud_cache_.feedback_text = gameplay_feedback_overlay_text(data.gameplay.feedback);
            } else {
                gameplay_hud_cache_.feedback_text.clear();
            }
            gameplay_hud_cache_.text_revision = data.gameplay.text_revision;
        }

        if (data.gameplay.loading && !data.gameplay.active) {
            draw_gameplay_header();
            const D2D1_RECT_F panel_rect = D2D1::RectF(620.0f, 360.0f, 1300.0f, 620.0f);
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 24.0f, 24.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.92f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.6f);
            }

            const std::wstring loading_w = L"LOADING CHART";
            const std::wstring stage_w =
                to_wide(data.gameplay.loading_stage.empty() ? std::string("Preparing gameplay")
                                                            : data.gameplay.loading_stage);
            const std::wstring percent_w =
                to_wide(std::to_string(std::clamp(data.gameplay.loading_percent, 0, 100)) + "%");
            if (d2d_->title_format && d2d_->text_brush) {
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(loading_w.c_str(), static_cast<UINT32>(loading_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 24.0f,
                                          panel_rect.right - 32.0f, panel_rect.top + 72.0f),
                              d2d_->text_brush.Get());
                ctx->DrawText(percent_w.c_str(), static_cast<UINT32>(percent_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 74.0f,
                                          panel_rect.right - 32.0f, panel_rect.top + 124.0f),
                              d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(stage_w.c_str(), static_cast<UINT32>(stage_w.size()),
                              d2d_->body_format.Get(),
                              D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 134.0f,
                                          panel_rect.right - 32.0f, panel_rect.top + 174.0f),
                              d2d_->muted_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            const D2D1_RECT_F progress_track =
                D2D1::RectF(panel_rect.left + 74.0f, panel_rect.bottom - 82.0f,
                            panel_rect.right - 74.0f, panel_rect.bottom - 54.0f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_track, 10.0f, 10.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                const float fill_ratio =
                    std::clamp(static_cast<float>(data.gameplay.loading_percent) / 100.0f, 0.0f, 1.0f);
                const D2D1_RECT_F progress_fill =
                    D2D1::RectF(progress_track.left + 4.0f,
                                progress_track.top + 4.0f,
                                progress_track.left + 4.0f +
                                    (progress_track.right - progress_track.left - 8.0f) * fill_ratio,
                                progress_track.bottom - 4.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
            }
            return;
        }

        if (data.gameplay.countdown_active) {
            if (d2d_->gameplay_static_command_list) {
                ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
            }
            draw_gameplay_header();

            const D2D1_RECT_F panel_rect = D2D1::RectF(700.0f, 300.0f, 1220.0f, 760.0f);
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 28.0f, 28.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.90f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->accent_brush.Get(), 1.8f);
            }

            if (d2d_->body_format && d2d_->muted_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                const std::wstring ready_w = L"GET READY";
                ctx->DrawText(ready_w.c_str(), static_cast<UINT32>(ready_w.size()),
                              d2d_->body_format.Get(),
                              D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 30.0f,
                                          panel_rect.right - 24.0f, panel_rect.top + 76.0f),
                              d2d_->muted_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            ID2D1Brush* countdown_brush = d2d_->logo_brush
                                              ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                              : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), panel_rect);
            }
            if (d2d_->rank_format && countdown_brush) {
                const std::wstring countdown_w =
                    to_wide(std::to_string(std::max(1, data.gameplay.countdown_value)));
                ctx->DrawText(countdown_w.c_str(), static_cast<UINT32>(countdown_w.size()),
                              d2d_->rank_format.Get(),
                              D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 86.0f,
                                          panel_rect.right - 24.0f, panel_rect.top + 270.0f),
                              countdown_brush);
            }

            if (d2d_->title_format && d2d_->text_brush) {
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                const std::wstring start_w = L"START";
                ctx->DrawText(start_w.c_str(), static_cast<UINT32>(start_w.size()),
                              d2d_->title_format.Get(),
                              D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 132.0f,
                                          panel_rect.right - 24.0f, panel_rect.bottom - 64.0f),
                              d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            return;
        }

        const int lane_count = std::clamp(data.gameplay.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
        const GameplayFieldLayout field_layout = build_gameplay_field_layout(
            kGameplayFieldLeft,
            kGameplayFieldRight,
            kGameplayFieldTop,
            kGameplayFieldBottom,
            lane_count,
            note_width_scale);
        const float field_left = field_layout.left;
        const float field_right = field_layout.right;
        const float field_top = field_layout.top;
        const float field_bottom = field_layout.bottom;
        const float field_height = field_layout.height;
        const float lane_width = field_layout.lane_width;
        const float note_width = field_layout.note_width;
        const D2D1_RECT_F field_clip_rect =
            D2D1::RectF(field_left + 2.0f, field_top + 2.0f, field_right - 2.0f, field_bottom - 2.0f);
        const float hit_line_y = gameplay_field_y(field_top, field_height, judgement_line_position);
        if (d2d_->gameplay_static_command_list) {
            ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
        }

        const D2D1_ANTIALIAS_MODE saved_antialias = ctx->GetAntialiasMode();
        ctx->PushAxisAlignedClip(field_clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
        ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (int lane = 0; lane < lane_count; ++lane) {
            const std::size_t lane_index = static_cast<std::size_t>(lane);
            ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
            if (lane_index < data.gameplay.lane_activity_count &&
                data.gameplay.lane_activity[lane_index] > 0.05f &&
                d2d_->lane_key_pressed_bitmaps[lane_index]) {
                key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
            }
            if (!key_bitmap) {
                continue;
            }
            const D2D1_SIZE_F bitmap_size = key_bitmap->GetSize();
            const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
            const D2D1_RECT_F receptor_rect =
                gameplay_key_bitmap_rect(field_layout,
                                         hit_line_y,
                                         note_height_scale,
                                         lane_center,
                                         lane_width,
                                         note_width,
                                         bitmap_size,
                                         gameplay_note_sprite_cache_.use_full_lane_receptor_layout);
            const D2D1_RECT_F* key_source_rect = nullptr;
            if (lane_index < d2d_->lane_key_pressed_source_rects.size() &&
                key_bitmap == d2d_->lane_key_pressed_bitmaps[lane_index].Get()) {
                key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_pressed_source_rects[lane_index]);
            } else if (lane_index < d2d_->lane_key_idle_source_rects.size()) {
                key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_idle_source_rects[lane_index]);
            }
            ctx->DrawBitmap(key_bitmap, receptor_rect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, key_source_rect);
        }
        for (std::size_t note_index = 0; note_index < data.gameplay.note_count; ++note_index) {
            const auto& note = data.gameplay.notes[note_index];
            if (!should_render_gameplay_note(note.start_sample, note.head_visible, display_sample)) {
                continue;
            }
            const int lane = std::clamp(note.lane, 1, lane_count);
            const float lane_center = field_left + (static_cast<float>(lane) - 0.5f) * lane_width;
            const float x0 = lane_center - note_width * 0.5f;
            const float x1 = lane_center + note_width * 0.5f;
            const int64_t render_sample =
                gameplay_note_render_sample(note.start_sample, note.hold, note.head_visible, display_sample);
            const float y = gameplay_field_y(field_top, field_height, sample_to_y(render_sample));
            const float tail_y = gameplay_field_y(field_top, field_height, sample_to_y(note.tail_sample));
            const float head_half_h = gameplay_note_head_half_height(note_height_scale);
            const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
            uint32_t lane_color = 0xF6F8FF;
            if (static_cast<std::size_t>(lane - 1) < data.gameplay.lane_color_count) {
                lane_color = data.gameplay.lane_colors[static_cast<std::size_t>(lane - 1)];
            } else if (!gameplay_lane_uses_white_note(lane)) {
                lane_color = 0x4F80FF;
            }
            ID2D1SolidColorBrush* note_fill = d2d_->note_fill_brush.Get();
            ID2D1SolidColorBrush* note_border = d2d_->note_border_brush.Get();
            ID2D1SolidColorBrush* note_hold_fill = d2d_->note_hold_brush.Get();
            if (note_fill) {
                note_fill->SetColor(gameplay_note_fill_color(lane_color));
            }
            if (note_border) {
                note_border->SetColor(gameplay_note_border_color(lane_color));
            }
            if (note_hold_fill) {
                note_hold_fill->SetColor(gameplay_note_hold_color(lane_color));
            }
            ID2D1Bitmap* note_head_bitmap = d2d_->lane_note_head_bitmaps[static_cast<std::size_t>(lane - 1)].Get();
            ID2D1Bitmap* note_hold_head_bitmap =
                d2d_->lane_note_hold_head_bitmaps[static_cast<std::size_t>(lane - 1)].Get();
            ID2D1Bitmap* note_hold_body_bitmap =
                d2d_->lane_note_hold_body_bitmaps[static_cast<std::size_t>(lane - 1)].Get();

            if (note.hold && note_hold_fill) {
                const float head_body_inset = gameplay_hold_body_cap_inset(note_shape, head_half_h);
                const float body_top = std::min(y, tail_y);
                const float body_bottom = std::max(y, tail_y) - (note.head_visible ? head_body_inset : 0.0f);
                const float hold_half_width = std::max(4.0f, note_width * 0.5f * hold_body_width_scale);
                const D2D1_RECT_F hold_body =
                    D2D1::RectF(lane_center - hold_half_width, body_top, lane_center + hold_half_width, body_bottom);
                if (body_bottom > body_top) {
                    if (note_hold_body_bitmap && !data.gameplay.hold_tail_taper_enabled) {
                        const D2D1_RECT_F* hold_body_source_rect =
                            bitmap_source_rect_or_null(
                                d2d_->lane_note_hold_body_source_rects[static_cast<std::size_t>(lane - 1)]);
                        ctx->DrawBitmap(note_hold_body_bitmap, hold_body, 1.0f,
                                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                        hold_body_source_rect);
                    } else {
                        draw_gameplay_hold_body(ctx,
                                                d2d_->d2d_factory.Get(),
                                                hold_body,
                                                lane_center,
                                                y,
                                                tail_y,
                                                hold_half_width,
                                                data.gameplay.hold_tail_taper_enabled,
                                                note_hold_fill);
                    }
                }
            }

            const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
            if (note.head_visible) {
                ID2D1Bitmap* head_bitmap = note.hold && note_hold_head_bitmap ? note_hold_head_bitmap : note_head_bitmap;
                if (head_bitmap) {
                    const D2D1_RECT_F* head_source_rect =
                        note.hold
                            ? bitmap_source_rect_or_null(
                                  d2d_->lane_note_hold_head_source_rects[static_cast<std::size_t>(lane - 1)])
                            : bitmap_source_rect_or_null(
                                  d2d_->lane_note_head_source_rects[static_cast<std::size_t>(lane - 1)]);
                    const D2D1_RECT_F bitmap_rect =
                        gameplay_note_bitmap_dest_rect(note_rect,
                                                       head_bitmap,
                                                       head_source_rect,
                                                       note_shape,
                                                       data.gameplay.preserve_note_image_aspect_ratio);
                    ctx->DrawBitmap(head_bitmap, bitmap_rect, 1.0f,
                                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, head_source_rect);
                } else {
                    if (note_fill) {
                        draw_note_primitive(ctx, note_rect, note_fill, note_border, 1.3f,
                                            note_shape, note_border_enabled);
                    }
                }
            }
        }
        ctx->PopAxisAlignedClip();
        ctx->SetAntialiasMode(saved_antialias);

        const float combo_y = gameplay_field_y(field_top, field_height, combo_position);
        const bool show_feedback_overlay =
            data.gameplay.has_feedback && !gameplay_hud_cache_.feedback_text.empty();
        if (show_feedback_overlay && d2d_->header_format && d2d_->text_brush) {
            const float feedback_center_x = (field_left + field_right) * 0.5f;
            const D2D1_RECT_F feedback_rect =
                D2D1::RectF(field_left - 24.0f, combo_y - 74.0f, field_right + 24.0f, combo_y + 6.0f);
            const bool show_timing_indicator =
                data.gameplay.feedback != "PG" &&
                std::abs(data.gameplay.feedback_delta_ms) >= 0.05;
            const bool timing_fast = show_timing_indicator && data.gameplay.feedback_delta_ms < 0.0;
            const bool timing_slow = show_timing_indicator && data.gameplay.feedback_delta_ms > 0.0;

            d2d_->text_brush->SetColor(D2D1::ColorF(0x061118, 0.78f));
            d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            const D2D1_RECT_F feedback_shadow_rect =
                D2D1::RectF(feedback_rect.left + 3.0f, feedback_rect.top + 3.0f,
                            feedback_rect.right + 3.0f, feedback_rect.bottom + 3.0f);
            ctx->DrawText(gameplay_hud_cache_.feedback_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.feedback_text.size()),
                          d2d_->header_format.Get(), feedback_shadow_rect, d2d_->text_brush.Get());

            d2d_->text_brush->SetColor(gameplay_feedback_color(data.gameplay.feedback));
            ctx->DrawText(gameplay_hud_cache_.feedback_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.feedback_text.size()),
                          d2d_->header_format.Get(), feedback_rect, d2d_->text_brush.Get());

            if (show_timing_indicator && d2d_->body_format) {
                constexpr wchar_t kFastIndicatorText[] = L"\uBE60\uB984";
                constexpr wchar_t kSlowIndicatorText[] = L"\uB290\uB9BC";
                const D2D1_RECT_F fast_rect =
                    D2D1::RectF(field_left + 60.0f, combo_y + 8.0f, feedback_center_x - 220.0f, combo_y + 38.0f);
                const D2D1_RECT_F slow_rect =
                    D2D1::RectF(feedback_center_x + 220.0f, combo_y + 8.0f, field_right - 60.0f, combo_y + 38.0f);

                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(D2D1::ColorF(0x5DA9FF, timing_fast ? 0.82f : 0.28f));
                ctx->DrawText(kFastIndicatorText, 2, d2d_->body_format.Get(), fast_rect, d2d_->text_brush.Get());

                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                d2d_->text_brush->SetColor(D2D1::ColorF(0xFF5A6B, timing_slow ? 0.82f : 0.28f));
                ctx->DrawText(kSlowIndicatorText, 2, d2d_->body_format.Get(), slow_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->text_brush->SetColor(D2D1::ColorF(0xE8ECF1));
        }

        if (data.gameplay.combo > 0 && d2d_->accent_brush &&
            (show_feedback_overlay ? (d2d_->title_format.Get() != nullptr)
                                   : (d2d_->header_format.Get() != nullptr))) {
            const std::wstring combo_overlay_w = to_wide(std::to_string(data.gameplay.combo));
            if (show_feedback_overlay) {
                const D2D1_RECT_F combo_overlay_rect =
                    D2D1::RectF(field_left, combo_y + 38.0f, field_right, combo_y + 82.0f);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetOpacity(0.92f);
                ctx->DrawText(combo_overlay_w.c_str(), static_cast<UINT32>(combo_overlay_w.size()),
                              d2d_->title_format.Get(), combo_overlay_rect, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(1.0f);
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            } else {
                const D2D1_RECT_F combo_overlay_rect =
                    D2D1::RectF(field_left, combo_y - 44.0f, field_right, combo_y + 44.0f);
                ctx->DrawText(combo_overlay_w.c_str(), static_cast<UINT32>(combo_overlay_w.size()),
                              d2d_->header_format.Get(), combo_overlay_rect, d2d_->accent_brush.Get());
            }
        }

        draw_gameplay_header();

        if (d2d_->accent_brush && data.gameplay.lane_activity_count > 0) {
            const std::size_t count =
                std::min(data.gameplay.lane_activity_count, static_cast<std::size_t>(lane_count));
            for (std::size_t lane = 0; lane < count; ++lane) {
                const float activity = std::clamp(data.gameplay.lane_activity[lane], 0.0f, 1.0f);
                if (activity <= 0.01f) {
                    continue;
                }
                const float lane_center = field_left + (static_cast<float>(lane) + 0.5f) * lane_width;
                const float x0 = lane_center - note_width * 0.5f;
                const float x1 = lane_center + note_width * 0.5f;
                const float glow_half_h = 8.0f + 14.0f * activity;
                const D2D1_RECT_F glow_rect =
                    D2D1::RectF(x0,
                                std::max(field_top + 2.0f, hit_line_y - glow_half_h),
                                x1,
                                std::min(field_bottom - 2.0f, hit_line_y + glow_half_h));
                d2d_->accent_brush->SetOpacity(0.12f + 0.38f * activity);
                ctx->FillRectangle(glow_rect, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(1.0f);
            }
        }

        const float gauge_left = kGameplayGaugeLeft;
        const float gauge_top = kGameplayGaugeTop;
        const float gauge_bottom = kGameplayGaugeBottom;
        const float gauge_width = kGameplayGaugeWidth;

        const float gauge_ratio = static_cast<float>(std::clamp(data.gameplay.gauge / 100.0, 0.0, 1.0));
        const float fill_top = gauge_bottom - (gauge_bottom - gauge_top) * gauge_ratio;
        if (d2d_->accent_brush) {
            D2D1_COLOR_F gauge_color = D2D1::ColorF(0xFFB703, 0.90f);
            if (data.gameplay.gauge_label == "HARD") {
                gauge_color = D2D1::ColorF(0xFF4D6D, 0.92f);
            } else if (data.gameplay.gauge_label == "EASY") {
                gauge_color = D2D1::ColorF(0x89D185, 0.92f);
            }
            d2d_->accent_brush->SetColor(gauge_color);
            const D2D1_RECT_F fill =
                D2D1::RectF(gauge_left + 4.0f, fill_top + 4.0f, gauge_left + gauge_width - 4.0f, gauge_bottom - 4.0f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
        }

        if (d2d_->body_format && d2d_->text_brush) {
            const D2D1_RECT_F label_rect =
                D2D1::RectF(gauge_left - 90.0f, gauge_top - 38.0f, gauge_left + 140.0f, gauge_top - 8.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(gauge_left - 90.0f, gauge_bottom + 10.0f, gauge_left + 140.0f, gauge_bottom + 42.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            ctx->DrawText(gameplay_hud_cache_.gauge_label_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.gauge_label_text.size()),
                          d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
            ctx->DrawText(gameplay_hud_cache_.gauge_value_text.c_str(),
                          static_cast<UINT32>(gameplay_hud_cache_.gauge_value_text.size()),
                          d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (data.gameplay.game_over && d2d_->panel_brush) {
            const D2D1_RECT_F overlay = D2D1::RectF(field_left - 10.0f, field_top - 10.0f,
                                                    field_right + 10.0f, field_bottom + 10.0f);
            d2d_->panel_brush->SetOpacity(0.78f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(overlay, 18.0f, 18.0f), d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(1.0f);
            if (d2d_->header_format && d2d_->accent_brush) {
                const std::wstring over_w = L"GAME OVER";
                const D2D1_RECT_F over_rect =
                    D2D1::RectF(field_left, (field_top + field_bottom) * 0.5f - 50.0f, field_right,
                                (field_top + field_bottom) * 0.5f + 50.0f);
                d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(over_w.c_str(), static_cast<UINT32>(over_w.size()),
                              d2d_->header_format.Get(), over_rect, d2d_->accent_brush.Get());
                d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
