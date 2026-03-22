        const float header_left = 84.0f;
        const float header_top = 42.0f;
        const float header_right = kBaseWidth - 84.0f;
        const float header_safe_right =
            data.performance.visible
                ? std::min(header_right, performance_overlay_safe_left(28.0f))
                : header_right;
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
        constexpr double kGameplayTimingIndicatorRangeMs = 80.0;
        constexpr float kGameplayTimingIndicatorHalfWidth = 124.0f;
        constexpr float kGameplayTimingIndicatorHeight = 8.0f;

        auto sample_to_y = [&](int64_t sample) -> float {
            return static_cast<float>(compute_gameplay_note_y_normalized(sample,
                                                                         display_sample,
                                                                         data.gameplay.lookahead_samples,
                                                                         data.gameplay.past_samples,
                                                                         judgement_line_position));
        };

        auto draw_timing_indicator = [&](float indicator_left,
                                         float indicator_right,
                                         float combo_anchor_y,
                                         const std::array<double, kGameplayTimingHistoryMaxEntries>& timing_history,
                                         std::size_t timing_history_count,
                                         bool has_live_feedback,
                                         double live_feedback_delta_ms) {
            if (!d2d_->body_format || !d2d_->text_brush) {
                return;
            }
            if (timing_history_count == 0 && !has_live_feedback) {
                return;
            }

            constexpr wchar_t kFastIndicatorText[] = L"\uBE60\uB984";
            constexpr wchar_t kSlowIndicatorText[] = L"\uB290\uB9BC";
            const float feedback_center_x = (indicator_left + indicator_right) * 0.5f;
            const D2D1_RECT_F indicator_rect =
                D2D1::RectF(feedback_center_x - kGameplayTimingIndicatorHalfWidth,
                            combo_anchor_y + 16.0f,
                            feedback_center_x + kGameplayTimingIndicatorHalfWidth,
                            combo_anchor_y + 16.0f + kGameplayTimingIndicatorHeight);
            const D2D1_RECT_F fast_rect =
                D2D1::RectF(indicator_left + 60.0f,
                            combo_anchor_y + 8.0f,
                            indicator_rect.left - 18.0f,
                            combo_anchor_y + 38.0f);
            const D2D1_RECT_F slow_rect =
                D2D1::RectF(indicator_rect.right + 18.0f,
                            combo_anchor_y + 8.0f,
                            indicator_right - 60.0f,
                            combo_anchor_y + 38.0f);
            const double reference_delta_ms =
                timing_history_count > 0 ? timing_history[timing_history_count - 1] : live_feedback_delta_ms;
            const bool timing_fast = reference_delta_ms < -0.05;
            const bool timing_slow = reference_delta_ms > 0.05;

            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.75f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(indicator_rect, 4.0f, 4.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                const auto saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.26f);
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x5DA9FF));
                ctx->FillRectangle(D2D1::RectF(indicator_rect.left, indicator_rect.top,
                                               feedback_center_x, indicator_rect.bottom),
                                   d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(D2D1::ColorF(0xFF5A6B));
                ctx->FillRectangle(D2D1::RectF(feedback_center_x, indicator_rect.top,
                                               indicator_rect.right, indicator_rect.bottom),
                                   d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(indicator_rect, 4.0f, 4.0f),
                                          d2d_->button_border_brush.Get(),
                                          1.0f);
            }

            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const float indicator_center_y = (indicator_rect.top + indicator_rect.bottom) * 0.5f;
            d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.92f));
            ctx->FillRectangle(D2D1::RectF(feedback_center_x - 1.0f, indicator_rect.top - 3.0f,
                                           feedback_center_x + 1.0f, indicator_rect.bottom + 3.0f),
                               d2d_->text_brush.Get());

            for (std::size_t i = 0; i < timing_history_count; ++i) {
                const double delta_ms = timing_history[i];
                const float history_weight =
                    static_cast<float>(i + 1) / static_cast<float>(std::max<std::size_t>(1, timing_history_count));
                const float marker_center_x =
                    feedback_center_x +
                    static_cast<float>(std::clamp(delta_ms / kGameplayTimingIndicatorRangeMs, -1.0, 1.0)) *
                        kGameplayTimingIndicatorHalfWidth;
                const float marker_half_width = 0.8f + 0.8f * history_weight;
                const float marker_half_height = 1.4f + 2.4f * history_weight;
                D2D1_COLOR_F marker_color = D2D1::ColorF(0xF7FAFD, 0.12f + 0.50f * history_weight * history_weight);
                if (delta_ms < -0.05) {
                    marker_color = D2D1::ColorF(0x5DA9FF, 0.12f + 0.50f * history_weight * history_weight);
                } else if (delta_ms > 0.05) {
                    marker_color = D2D1::ColorF(0xFF5A6B, 0.12f + 0.50f * history_weight * history_weight);
                }
                d2d_->text_brush->SetColor(marker_color);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(marker_center_x - marker_half_width,
                                                 indicator_center_y - marker_half_height,
                                                 marker_center_x + marker_half_width,
                                                 indicator_center_y + marker_half_height),
                                      marker_half_width,
                                      marker_half_width),
                    d2d_->text_brush.Get());
            }

            if (has_live_feedback) {
                const float live_marker_center_x =
                    feedback_center_x +
                    static_cast<float>(std::clamp(live_feedback_delta_ms / kGameplayTimingIndicatorRangeMs,
                                                  -1.0,
                                                  1.0)) *
                        kGameplayTimingIndicatorHalfWidth;
                d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.98f));
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(live_marker_center_x, indicator_center_y), 5.0f, 5.0f),
                                 d2d_->text_brush.Get());
            }

            d2d_->text_brush->SetColor(D2D1::ColorF(0x5DA9FF, timing_fast ? 0.82f : 0.28f));
            draw_text_clipped_aligned(std::wstring(kFastIndicatorText),
                                      d2d_->body_format.Get(),
                                      fast_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_TRAILING);

            d2d_->text_brush->SetColor(D2D1::ColorF(0xFF5A6B, timing_slow ? 0.82f : 0.28f));
            draw_text_clipped_aligned(std::wstring(kSlowIndicatorText),
                                      d2d_->body_format.Get(),
                                      slow_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_LEADING);
            d2d_->text_brush->SetColor(saved_text_color);
        };

        auto draw_gameplay_header = [&]() {
            if (d2d_->title_format && d2d_->text_brush) {
                const D2D1_RECT_F title_rect =
                    D2D1::RectF(header_left, header_top, header_right * 0.60f, header_top + 52.0f);
                draw_text_clipped(gameplay_hud_cache_.title_text,
                                  d2d_->title_format.Get(),
                                  title_rect,
                                  d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const D2D1_RECT_F artist_rect =
                    D2D1::RectF(header_left, header_top + 46.0f, header_right * 0.60f, header_top + 84.0f);
                draw_text_clipped(gameplay_hud_cache_.artist_text,
                                  d2d_->body_format.Get(),
                                  artist_rect,
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F speed_rect =
                    D2D1::RectF(header_left, header_top + 82.0f, header_right * 0.68f, header_top + 118.0f);
                draw_text_clipped(gameplay_hud_cache_.speed_text,
                                  d2d_->hud_format.Get(),
                                  speed_rect,
                                  d2d_->muted_brush.Get());
            }
            if (!data.gameplay.ghost_visible && d2d_->title_format && d2d_->text_brush) {
                const D2D1_RECT_F score_rect =
                    D2D1::RectF(std::max(header_left + 700.0f, header_safe_right - 610.0f),
                                header_top,
                                header_safe_right,
                                header_top + 52.0f);
                draw_text_clipped_aligned(gameplay_hud_cache_.score_text,
                                          d2d_->title_format.Get(),
                                          score_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            if (!data.gameplay.ghost_visible && d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F combo_rect =
                    D2D1::RectF(std::max(header_left + 620.0f, header_safe_right - 690.0f),
                                header_top + 52.0f,
                                header_safe_right,
                                header_top + 84.0f);
                draw_text_clipped_aligned(gameplay_hud_cache_.combo_text,
                                          d2d_->body_format.Get(),
                                          combo_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            if (!data.gameplay.ghost_visible && d2d_->hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F judge_stats_rect =
                    D2D1::RectF(std::max(header_left + 620.0f, header_safe_right - 690.0f),
                                header_top + 82.0f,
                                header_safe_right,
                                header_top + 116.0f);
                draw_text_clipped_aligned(gameplay_hud_cache_.judge_stats_text,
                                          d2d_->hud_format.Get(),
                                          judge_stats_rect,
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
        };

        if (gameplay_hud_cache_.text_revision != data.gameplay.text_revision) {
            const std::string title = data.gameplay.title.empty() ? loc("Unknown Track", "알 수 없는 곡") : data.gameplay.title;
            const std::string artist = data.gameplay.artist.empty() ? loc("Unknown Artist", "알 수 없는 아티스트") : data.gameplay.artist;
            std::string gauge_label = data.gameplay.gauge_label;
            if (ui_korean) {
                if (gauge_label == "HARD") {
                    gauge_label = "하드";
                } else if (gauge_label == "EASY") {
                    gauge_label = "이지";
                } else if (gauge_label == "NORMAL") {
                    gauge_label = "노말";
                }
            }
            std::string ghost_gauge_label = data.gameplay.ghost_gauge_label;
            if (ui_korean) {
                if (ghost_gauge_label == "HARD") {
                    ghost_gauge_label = "하드";
                } else if (ghost_gauge_label == "EASY") {
                    ghost_gauge_label = "이지";
                } else if (ghost_gauge_label == "NORMAL") {
                    ghost_gauge_label = "노말";
                }
            }
            gameplay_hud_cache_.title_text = to_wide(title);
            gameplay_hud_cache_.artist_text = to_wide(artist);
            gameplay_hud_cache_.speed_text =
                to_wide(loc("RATE x", "RATE x") + format_decimal(data.gameplay.rate, 2) +
                        " / HS " + format_decimal(data.gameplay.hispeed, 2) +
                        " / BPM " + std::to_string(static_cast<int>(std::llround(data.gameplay.bpm))) +
                        " / " + loc("Scroll ", "스크롤 ") + std::to_string(static_cast<int>(std::llround(data.gameplay.scroll_speed))));
            gameplay_hud_cache_.score_text =
                to_wide(loc("SCORE  ", "점수  ") + format_int_with_commas(data.gameplay.score));
            gameplay_hud_cache_.combo_text =
                to_wide(loc("COMBO ", "콤보 ") + std::to_string(data.gameplay.combo) +
                        "   " + loc("MAX ", "최대 ") + std::to_string(data.gameplay.max_combo) +
                        "   " + loc("ACC ", "정확도 ") + format_decimal(data.gameplay.accuracy, 2) + "%");
            gameplay_hud_cache_.judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.pg) +
                        "  GR " + std::to_string(data.gameplay.gr) +
                        "  G " + std::to_string(data.gameplay.gd) +
                        "  BAD " + std::to_string(data.gameplay.bd) +
                        "  PR " + std::to_string(data.gameplay.pr));
            gameplay_hud_cache_.gauge_label_text = to_wide(gauge_label);
            gameplay_hud_cache_.gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.gauge))) + "%");
            gameplay_hud_cache_.ghost_score_text =
                to_wide(loc("SCORE  ", "점수  ") + format_int_with_commas(data.gameplay.ghost_score));
            gameplay_hud_cache_.ghost_combo_text =
                to_wide(loc("COMBO ", "콤보 ") + std::to_string(data.gameplay.ghost_combo) +
                        "   " + loc("MAX ", "최대 ") + std::to_string(data.gameplay.ghost_max_combo) +
                        "   " + loc("ACC ", "정확도 ") + format_decimal(data.gameplay.ghost_accuracy, 2) + "%");
            gameplay_hud_cache_.ghost_judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.ghost_pg) +
                        "  GR " + std::to_string(data.gameplay.ghost_gr) +
                        "  G " + std::to_string(data.gameplay.ghost_gd) +
                        "  BAD " + std::to_string(data.gameplay.ghost_bd) +
                        "  PR " + std::to_string(data.gameplay.ghost_pr));
            gameplay_hud_cache_.ghost_gauge_label_text = to_wide(ghost_gauge_label);
            gameplay_hud_cache_.ghost_gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.ghost_gauge))) + "%");

            if (data.gameplay.has_feedback) {
                gameplay_hud_cache_.feedback_text = gameplay_feedback_overlay_text(data.gameplay.feedback);
            } else {
                gameplay_hud_cache_.feedback_text.clear();
            }
            if (data.gameplay.ghost_has_feedback) {
                gameplay_hud_cache_.ghost_feedback_text =
                    gameplay_feedback_overlay_text(data.gameplay.ghost_feedback);
            } else {
                gameplay_hud_cache_.ghost_feedback_text.clear();
            }
            gameplay_hud_cache_.text_revision = data.gameplay.text_revision;
        }

        if (data.gameplay.loading && !data.gameplay.active) {
            draw_gameplay_header();
            const D2D1_RECT_F panel_rect =
                fit_rect_below_performance_overlay(D2D1::RectF(620.0f, 360.0f, 1300.0f, 620.0f),
                                                   kBaseHeight - 140.0f,
                                                   22.0f);
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 24.0f, 24.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.92f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.6f);
            }

            const std::wstring loading_w = wloc("LOADING CHART", "차트 로딩 중");
            const std::wstring stage_w =
                to_wide(data.gameplay.loading_stage.empty() ? loc("Preparing gameplay", "게임 준비 중")
                                                            : data.gameplay.loading_stage);
            const std::wstring percent_w =
                to_wide(std::to_string(std::clamp(data.gameplay.loading_percent, 0, 100)) + "%");
            if (d2d_->title_format && d2d_->text_brush) {
                draw_text_clipped_aligned(loading_w,
                                          d2d_->title_format.Get(),
                                          D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 24.0f,
                                                      panel_rect.right - 32.0f, panel_rect.top + 72.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped_aligned(percent_w,
                                          d2d_->title_format.Get(),
                                          D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 74.0f,
                                                      panel_rect.right - 32.0f, panel_rect.top + 124.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_text_clipped_aligned(stage_w,
                                          d2d_->body_format.Get(),
                                          D2D1::RectF(panel_rect.left + 32.0f, panel_rect.top + 134.0f,
                                                      panel_rect.right - 32.0f, panel_rect.top + 174.0f),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
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

            const D2D1_RECT_F panel_rect =
                fit_rect_below_performance_overlay(D2D1::RectF(700.0f, 300.0f, 1220.0f, 760.0f),
                                                   kBaseHeight - 120.0f,
                                                   22.0f);
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
                const std::wstring ready_w = wloc("GET READY", "준비");
                draw_text_clipped_aligned(ready_w,
                                          d2d_->body_format.Get(),
                                          D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 30.0f,
                                                      panel_rect.right - 24.0f, panel_rect.top + 76.0f),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            const D2D1_RECT_F countdown_rect =
                D2D1::RectF(panel_rect.left + 24.0f,
                            panel_rect.top + 86.0f,
                            panel_rect.right - 24.0f,
                            panel_rect.bottom - 116.0f);
            ID2D1Brush* countdown_brush = d2d_->logo_brush
                                              ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                              : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), countdown_rect);
            }
            if (d2d_->rank_format && countdown_brush) {
                const std::wstring countdown_w =
                    to_wide(std::to_string(std::max(1, data.gameplay.countdown_value)));
                Microsoft::WRL::ComPtr<IDWriteTextLayout> countdown_layout;
                bool countdown_layout_ready = false;
                if (d2d_->dwrite_factory &&
                    SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(
                        countdown_w.c_str(),
                        static_cast<UINT32>(countdown_w.size()),
                        d2d_->rank_format.Get(),
                        std::max(1.0f, countdown_rect.right - countdown_rect.left),
                        std::max(1.0f, countdown_rect.bottom - countdown_rect.top),
                        &countdown_layout)) &&
                    countdown_layout) {
                    DWRITE_TEXT_METRICS countdown_metrics{};
                    if (SUCCEEDED(countdown_layout->GetMetrics(&countdown_metrics))) {
                        const float countdown_width =
                            std::ceil(std::max(countdown_metrics.width,
                                               countdown_metrics.widthIncludingTrailingWhitespace));
                        const float countdown_height = std::ceil(countdown_metrics.height);
                        const D2D1_POINT_2F countdown_origin =
                            D2D1::Point2F(
                                std::floor(countdown_rect.left +
                                           ((countdown_rect.right - countdown_rect.left) - countdown_width) * 0.5f),
                                std::floor(countdown_rect.top +
                                           ((countdown_rect.bottom - countdown_rect.top) - countdown_height) * 0.5f));
                        ctx->DrawTextLayout(countdown_origin,
                                            countdown_layout.Get(),
                                            countdown_brush,
                                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
                        countdown_layout_ready = true;
                    }
                }
                if (!countdown_layout_ready) {
                    draw_text_clipped_aligned(countdown_w,
                                              d2d_->rank_format.Get(),
                                              countdown_rect,
                                              countdown_brush,
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }

            if (d2d_->title_format && d2d_->text_brush) {
                const std::wstring start_w = L"START";
                draw_text_clipped_aligned(start_w,
                                          d2d_->title_format.Get(),
                                          D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 132.0f,
                                                      panel_rect.right - 24.0f, panel_rect.bottom - 64.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            return;
        }

        const int lane_count = std::clamp(data.gameplay.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
        const GameplaySurfaceLayout surface_layout =
            build_gameplay_surface_layout(
                lane_count,
                note_width_scale,
                data.gameplay.lane_width_scale_count,
                data.gameplay.lane_width_scales,
                data.gameplay.lane_spacing_scale_count,
                data.gameplay.lane_spacing_scales,
                data.gameplay.ghost_visible,
                data.gameplay.lane_center_gap_scale);
        const GameplayFieldLayout field_layout = surface_layout.player_field;
        const float field_left = field_layout.left;
        const float field_right = field_layout.right;
        const float field_top = field_layout.top;
        const float field_bottom = field_layout.bottom;
        const float field_height = field_layout.height;
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
            const float lane_center = gameplay_lane_center(field_layout, lane);
            const float lane_width = gameplay_lane_width(field_layout, lane);
            const float note_width = gameplay_note_width(field_layout, lane);
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
            const float lane_center = gameplay_lane_center(field_layout, lane - 1);
            const float note_width = gameplay_note_width(field_layout, lane - 1);
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

        const bool show_feedback_overlay =
            data.gameplay.has_feedback && !gameplay_hud_cache_.feedback_text.empty();
        const bool has_timing_history = data.gameplay.timing_history_count > 0;
        const bool reserve_timing_overlay_space = show_feedback_overlay || has_timing_history;
        const float combo_anchor_top_safe = reserve_timing_overlay_space ? 74.0f : 44.0f;
        const float combo_anchor_bottom_safe = reserve_timing_overlay_space ? 82.0f : 44.0f;
        const float combo_anchor_y =
            gameplay_combo_anchor_y(field_layout, combo_position, combo_anchor_top_safe, combo_anchor_bottom_safe);
        if ((show_feedback_overlay || has_timing_history) && d2d_->text_brush) {
            if (show_feedback_overlay && d2d_->header_format) {
                const D2D1_RECT_F feedback_rect =
                    gameplay_centered_overlay_rect(field_layout, combo_anchor_y - 34.0f, 40.0f, -24.0f);
                const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();

                d2d_->text_brush->SetColor(D2D1::ColorF(0x061118, 0.78f));
                const D2D1_RECT_F feedback_shadow_rect =
                    D2D1::RectF(feedback_rect.left + 3.0f, feedback_rect.top + 3.0f,
                                feedback_rect.right + 3.0f, feedback_rect.bottom + 3.0f);
                draw_text_clipped_aligned(gameplay_hud_cache_.feedback_text,
                                          d2d_->header_format.Get(),
                                          feedback_shadow_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);

                d2d_->text_brush->SetColor(gameplay_feedback_color(data.gameplay.feedback));
                draw_text_clipped_aligned(gameplay_hud_cache_.feedback_text,
                                          d2d_->header_format.Get(),
                                          feedback_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->text_brush->SetColor(saved_text_color);
            }

            draw_timing_indicator(field_left,
                                  field_right,
                                  combo_anchor_y,
                                  data.gameplay.timing_history_delta_ms,
                                  data.gameplay.timing_history_count,
                                  data.gameplay.has_feedback,
                                  data.gameplay.feedback_delta_ms);
        }

        if (data.gameplay.combo > 0 && d2d_->accent_brush &&
            (show_feedback_overlay ? (d2d_->title_format.Get() != nullptr)
                                   : (d2d_->header_format.Get() != nullptr))) {
            const std::wstring combo_overlay_w = to_wide(std::to_string(data.gameplay.combo));
            if (show_feedback_overlay) {
                const D2D1_RECT_F combo_overlay_rect =
                    gameplay_combo_overlay_rect(field_layout,
                                                combo_position,
                                                22.0f,
                                                combo_anchor_top_safe,
                                                combo_anchor_bottom_safe,
                                                60.0f,
                                                8.0f);
                d2d_->accent_brush->SetOpacity(0.92f);
                draw_text_clipped_aligned(combo_overlay_w,
                                          d2d_->title_format.Get(),
                                          combo_overlay_rect,
                                          d2d_->accent_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetOpacity(1.0f);
            } else {
                const D2D1_RECT_F combo_overlay_rect =
                    gameplay_combo_overlay_rect(field_layout,
                                                combo_position,
                                                44.0f,
                                                combo_anchor_top_safe,
                                                combo_anchor_bottom_safe,
                                                0.0f,
                                                8.0f);
                draw_text_clipped_aligned(combo_overlay_w,
                                          d2d_->header_format.Get(),
                                          combo_overlay_rect,
                                          d2d_->accent_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }

        draw_gameplay_header();
        if (surface_layout.ghost_visible && d2d_->body_format && d2d_->title_format) {
            auto draw_field_summary = [&](const GameplayFieldLayout& summary_layout,
                                          const std::wstring& label_text,
                                          const std::wstring& score_text,
                                          const std::wstring& combo_text,
                                          const std::wstring& judge_stats_text) {
                const D2D1_RECT_F summary_rect =
                    D2D1::RectF(summary_layout.left + 18.0f, 118.0f, summary_layout.right - 18.0f, 246.0f);
                if (d2d_->panel_brush) {
                    d2d_->panel_brush->SetOpacity(0.76f);
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(summary_rect, 18.0f, 18.0f), d2d_->panel_brush.Get());
                    d2d_->panel_brush->SetOpacity(1.0f);
                }
                if (d2d_->button_border_brush) {
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(summary_rect, 18.0f, 18.0f),
                                              d2d_->button_border_brush.Get(),
                                              1.2f);
                }
                if (d2d_->body_format && d2d_->muted_brush) {
                    draw_text_clipped_aligned(label_text,
                                              d2d_->body_format.Get(),
                                              D2D1::RectF(summary_rect.left + 16.0f, summary_rect.top + 10.0f,
                                                          summary_rect.right - 16.0f, summary_rect.top + 40.0f),
                                              d2d_->muted_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                if (d2d_->title_format && d2d_->text_brush) {
                    draw_text_clipped_aligned(score_text,
                                              d2d_->title_format.Get(),
                                              D2D1::RectF(summary_rect.left + 16.0f, summary_rect.top + 34.0f,
                                                          summary_rect.right - 16.0f, summary_rect.top + 82.0f),
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                if (d2d_->body_format && d2d_->text_brush) {
                    draw_text_clipped_aligned(combo_text,
                                              d2d_->body_format.Get(),
                                              D2D1::RectF(summary_rect.left + 16.0f, summary_rect.top + 76.0f,
                                                          summary_rect.right - 16.0f, summary_rect.top + 108.0f),
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                if (d2d_->hud_format && d2d_->muted_brush) {
                    draw_text_clipped_aligned(judge_stats_text,
                                              d2d_->hud_format.Get(),
                                              D2D1::RectF(summary_rect.left + 16.0f, summary_rect.top + 108.0f,
                                                          summary_rect.right - 16.0f, summary_rect.bottom - 12.0f),
                                              d2d_->muted_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            };
            draw_field_summary(surface_layout.player_field,
                               wloc("LIVE", "실플레이"),
                               gameplay_hud_cache_.score_text,
                               gameplay_hud_cache_.combo_text,
                               gameplay_hud_cache_.judge_stats_text);
            draw_field_summary(surface_layout.ghost_field,
                               wloc("GHOST", "고스트"),
                               gameplay_hud_cache_.ghost_score_text,
                               gameplay_hud_cache_.ghost_combo_text,
                               gameplay_hud_cache_.ghost_judge_stats_text);
        }

        if (d2d_->accent_brush && data.gameplay.lane_activity_count > 0) {
            const std::size_t count =
                std::min(data.gameplay.lane_activity_count, static_cast<std::size_t>(lane_count));
            for (std::size_t lane = 0; lane < count; ++lane) {
                const float activity = std::clamp(data.gameplay.lane_activity[lane], 0.0f, 1.0f);
                if (activity <= 0.01f) {
                    continue;
                }
                const float lane_center = gameplay_lane_center(field_layout, static_cast<int>(lane));
                const float note_width = gameplay_note_width(field_layout, static_cast<int>(lane));
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

        const float gauge_left = surface_layout.player_gauge_left;
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
                d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        if (surface_layout.ghost_visible) {
            const GameplayFieldLayout ghost_field_layout = surface_layout.ghost_field;
            const float ghost_field_left = ghost_field_layout.left;
            const float ghost_field_right = ghost_field_layout.right;
            const float ghost_field_top = ghost_field_layout.top;
            const float ghost_field_bottom = ghost_field_layout.bottom;
            const float ghost_field_height = ghost_field_layout.height;
            const D2D1_RECT_F ghost_field_clip_rect =
                D2D1::RectF(ghost_field_left + 2.0f, ghost_field_top + 2.0f,
                            ghost_field_right - 2.0f, ghost_field_bottom - 2.0f);
            const float ghost_hit_line_y =
                gameplay_field_y(ghost_field_top, ghost_field_height, judgement_line_position);

            const D2D1_ANTIALIAS_MODE ghost_saved_antialias = ctx->GetAntialiasMode();
            ctx->PushAxisAlignedClip(ghost_field_clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
            ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            for (int lane = 0; lane < lane_count; ++lane) {
                const std::size_t lane_index = static_cast<std::size_t>(lane);
                ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
                if (lane_index < data.gameplay.ghost_lane_activity_count &&
                    data.gameplay.ghost_lane_activity[lane_index] > 0.05f &&
                    d2d_->lane_key_pressed_bitmaps[lane_index]) {
                    key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
                }
                if (!key_bitmap) {
                    continue;
                }
                const D2D1_SIZE_F bitmap_size = key_bitmap->GetSize();
                const float lane_center = gameplay_lane_center(ghost_field_layout, lane);
                const float ghost_lane_width = gameplay_lane_width(ghost_field_layout, lane);
                const float ghost_note_width = gameplay_note_width(ghost_field_layout, lane);
                const D2D1_RECT_F receptor_rect =
                    gameplay_key_bitmap_rect(ghost_field_layout,
                                             ghost_hit_line_y,
                                             note_height_scale,
                                             lane_center,
                                             ghost_lane_width,
                                             ghost_note_width,
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
            for (std::size_t note_index = 0; note_index < data.gameplay.ghost_note_count; ++note_index) {
                const auto& note = data.gameplay.ghost_notes[note_index];
                if (!should_render_gameplay_note(note.start_sample, note.head_visible, display_sample)) {
                    continue;
                }
                const int lane = std::clamp(note.lane, 1, lane_count);
                const float lane_center = gameplay_lane_center(ghost_field_layout, lane - 1);
                const float ghost_note_width = gameplay_note_width(ghost_field_layout, lane - 1);
                const float x0 = lane_center - ghost_note_width * 0.5f;
                const float x1 = lane_center + ghost_note_width * 0.5f;
                const int64_t render_sample =
                    gameplay_note_render_sample(note.start_sample, note.hold, note.head_visible, display_sample);
                const float y = gameplay_field_y(ghost_field_top, ghost_field_height, sample_to_y(render_sample));
                const float tail_y =
                    gameplay_field_y(ghost_field_top, ghost_field_height, sample_to_y(note.tail_sample));
                const float head_half_h = gameplay_note_head_half_height(note_height_scale);
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
                    const float head_body_inset =
                        gameplay_hold_body_cap_inset(note_shape, gameplay_note_head_half_height(note_height_scale));
                    const float body_top = std::min(y, tail_y);
                    const float body_bottom = std::max(y, tail_y) - (note.head_visible ? head_body_inset : 0.0f);
                    const float hold_half_width = std::max(4.0f, ghost_note_width * 0.5f * hold_body_width_scale);
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
                    } else if (note_fill) {
                        draw_note_primitive(ctx, note_rect, note_fill, note_border, 1.3f,
                                            note_shape, note_border_enabled);
                    }
                }
            }
            ctx->PopAxisAlignedClip();
            ctx->SetAntialiasMode(ghost_saved_antialias);

            const bool show_ghost_feedback_overlay =
                data.gameplay.ghost_has_feedback && !gameplay_hud_cache_.ghost_feedback_text.empty();
            const bool ghost_has_timing_history = data.gameplay.ghost_timing_history_count > 0;
            const bool reserve_ghost_timing_overlay_space =
                show_ghost_feedback_overlay || ghost_has_timing_history;
            const float ghost_combo_anchor_top_safe = reserve_ghost_timing_overlay_space ? 74.0f : 44.0f;
            const float ghost_combo_anchor_bottom_safe = reserve_ghost_timing_overlay_space ? 82.0f : 44.0f;
            const float ghost_combo_anchor_y =
                gameplay_combo_anchor_y(ghost_field_layout,
                                        combo_position,
                                        ghost_combo_anchor_top_safe,
                                        ghost_combo_anchor_bottom_safe);
            if ((show_ghost_feedback_overlay || ghost_has_timing_history) && d2d_->text_brush) {
                if (show_ghost_feedback_overlay && d2d_->header_format) {
                    const D2D1_RECT_F feedback_rect = gameplay_centered_overlay_rect(
                        ghost_field_layout, ghost_combo_anchor_y - 34.0f, 40.0f, -24.0f);
                    const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();

                    d2d_->text_brush->SetColor(D2D1::ColorF(0x061118, 0.78f));
                    const D2D1_RECT_F feedback_shadow_rect =
                        D2D1::RectF(feedback_rect.left + 3.0f, feedback_rect.top + 3.0f,
                                    feedback_rect.right + 3.0f, feedback_rect.bottom + 3.0f);
                    draw_text_clipped_aligned(gameplay_hud_cache_.ghost_feedback_text,
                                              d2d_->header_format.Get(),
                                              feedback_shadow_rect,
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);

                    d2d_->text_brush->SetColor(gameplay_feedback_color(data.gameplay.ghost_feedback));
                    draw_text_clipped_aligned(gameplay_hud_cache_.ghost_feedback_text,
                                              d2d_->header_format.Get(),
                                              feedback_rect,
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->text_brush->SetColor(saved_text_color);
                }

                draw_timing_indicator(ghost_field_left,
                                      ghost_field_right,
                                      ghost_combo_anchor_y,
                                      data.gameplay.ghost_timing_history_delta_ms,
                                      data.gameplay.ghost_timing_history_count,
                                      data.gameplay.ghost_has_feedback,
                                      data.gameplay.ghost_feedback_delta_ms);
            }
            if (data.gameplay.ghost_combo > 0 && d2d_->accent_brush &&
                (show_ghost_feedback_overlay ? (d2d_->title_format.Get() != nullptr)
                                             : (d2d_->header_format.Get() != nullptr))) {
                const std::wstring combo_overlay_w = to_wide(std::to_string(data.gameplay.ghost_combo));
                if (show_ghost_feedback_overlay) {
                    const D2D1_RECT_F combo_overlay_rect =
                        gameplay_combo_overlay_rect(ghost_field_layout,
                                                    combo_position,
                                                    22.0f,
                                                    ghost_combo_anchor_top_safe,
                                                    ghost_combo_anchor_bottom_safe,
                                                    60.0f,
                                                    8.0f);
                    d2d_->accent_brush->SetOpacity(0.92f);
                    draw_text_clipped_aligned(combo_overlay_w,
                                              d2d_->title_format.Get(),
                                              combo_overlay_rect,
                                              d2d_->accent_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->accent_brush->SetOpacity(1.0f);
                } else {
                    const D2D1_RECT_F combo_overlay_rect =
                        gameplay_combo_overlay_rect(ghost_field_layout,
                                                    combo_position,
                                                    44.0f,
                                                    ghost_combo_anchor_top_safe,
                                                    ghost_combo_anchor_bottom_safe,
                                                    0.0f,
                                                    8.0f);
                    draw_text_clipped_aligned(combo_overlay_w,
                                              d2d_->header_format.Get(),
                                              combo_overlay_rect,
                                              d2d_->accent_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }

            if (d2d_->accent_brush && data.gameplay.ghost_lane_activity_count > 0) {
                const std::size_t count =
                    std::min(data.gameplay.ghost_lane_activity_count, static_cast<std::size_t>(lane_count));
                for (std::size_t lane = 0; lane < count; ++lane) {
                    const float activity = std::clamp(data.gameplay.ghost_lane_activity[lane], 0.0f, 1.0f);
                if (activity <= 0.01f) {
                    continue;
                }
                const float lane_center = gameplay_lane_center(ghost_field_layout, static_cast<int>(lane));
                const float ghost_note_width = gameplay_note_width(ghost_field_layout, static_cast<int>(lane));
                const float x0 = lane_center - ghost_note_width * 0.5f;
                const float x1 = lane_center + ghost_note_width * 0.5f;
                    const float glow_half_h = 8.0f + 14.0f * activity;
                    const D2D1_RECT_F glow_rect =
                        D2D1::RectF(x0,
                                    std::max(ghost_field_top + 2.0f, ghost_hit_line_y - glow_half_h),
                                    x1,
                                    std::min(ghost_field_bottom - 2.0f, ghost_hit_line_y + glow_half_h));
                    d2d_->accent_brush->SetOpacity(0.12f + 0.38f * activity);
                    ctx->FillRectangle(glow_rect, d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetOpacity(1.0f);
                }
            }

            const float ghost_gauge_left = surface_layout.ghost_gauge_left;
            const float ghost_gauge_top = kGameplayGaugeTop;
            const float ghost_gauge_bottom = kGameplayGaugeBottom;
            const float ghost_gauge_width = kGameplayGaugeWidth;
            const float ghost_gauge_ratio =
                static_cast<float>(std::clamp(data.gameplay.ghost_gauge / 100.0, 0.0, 1.0));
            const float ghost_fill_top =
                ghost_gauge_bottom - (ghost_gauge_bottom - ghost_gauge_top) * ghost_gauge_ratio;
            if (d2d_->accent_brush) {
                D2D1_COLOR_F gauge_color = D2D1::ColorF(0xFFB703, 0.90f);
                if (data.gameplay.ghost_gauge_label == "HARD") {
                    gauge_color = D2D1::ColorF(0xFF4D6D, 0.92f);
                } else if (data.gameplay.ghost_gauge_label == "EASY") {
                    gauge_color = D2D1::ColorF(0x89D185, 0.92f);
                }
                d2d_->accent_brush->SetColor(gauge_color);
                const D2D1_RECT_F fill =
                    D2D1::RectF(ghost_gauge_left + 4.0f,
                                ghost_fill_top + 4.0f,
                                ghost_gauge_left + ghost_gauge_width - 4.0f,
                                ghost_gauge_bottom - 4.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            }

            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F label_rect =
                    D2D1::RectF(ghost_gauge_left - 90.0f, ghost_gauge_top - 38.0f,
                                ghost_gauge_left + 140.0f, ghost_gauge_top - 8.0f);
                const D2D1_RECT_F value_rect =
                    D2D1::RectF(ghost_gauge_left - 90.0f, ghost_gauge_bottom + 10.0f,
                                ghost_gauge_left + 140.0f, ghost_gauge_bottom + 42.0f);
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(gameplay_hud_cache_.ghost_gauge_label_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.ghost_gauge_label_text.size()),
                              d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
                ctx->DrawText(gameplay_hud_cache_.ghost_gauge_value_text.c_str(),
                              static_cast<UINT32>(gameplay_hud_cache_.ghost_gauge_value_text.size()),
                              d2d_->body_format.Get(), value_rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }

            if (data.gameplay.ghost_game_over && d2d_->panel_brush) {
                const D2D1_RECT_F overlay =
                    D2D1::RectF(ghost_field_left - 10.0f, ghost_field_top - 10.0f,
                                ghost_field_right + 10.0f, ghost_field_bottom + 10.0f);
                d2d_->panel_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(overlay, 18.0f, 18.0f), d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
                if (d2d_->header_format && d2d_->accent_brush) {
                    const std::wstring over_w = L"GAME OVER";
                    const D2D1_RECT_F over_rect =
                        D2D1::RectF(ghost_field_left, (ghost_field_top + ghost_field_bottom) * 0.5f - 50.0f,
                                    ghost_field_right, (ghost_field_top + ghost_field_bottom) * 0.5f + 50.0f);
                    d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    ctx->DrawText(over_w.c_str(), static_cast<UINT32>(over_w.size()),
                                  d2d_->header_format.Get(), over_rect, d2d_->accent_brush.Get());
                    d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            }
        }
