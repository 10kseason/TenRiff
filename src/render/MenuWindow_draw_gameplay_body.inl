        if (!gameplay_field_drag_state_.active) {
            gameplay_field_drag_state_.visible = false;
        }
        const int64_t render_now_ns = timing::HighResClock::now_ns();
        const float header_left = 84.0f;
        const float header_top = 42.0f;
        const float header_right = kBaseWidth - 84.0f;
        const float header_safe_right =
            data.performance.visible
                ? std::min(header_right, performance_overlay_safe_left(28.0f))
                : header_right;
        const bool use_imported_metrics = normalize_gameplay_skin_source(data.gameplay.skin_source) != "native";
        const double judgement_line_position =
            use_imported_metrics && gameplay_note_sprite_cache_.has_imported_judgement_line_position
                ? gameplay_note_sprite_cache_.imported_judgement_line_position
                : clamp_gameplay_judgement_line(data.gameplay.judgement_line_position);
        const double combo_position = clamp_gameplay_combo_position(data.gameplay.combo_position);
        const float note_width_scale = clamp_gameplay_note_width_scale(data.gameplay.note_width_scale);
        const float note_art_width_ratio = effective_gameplay_note_art_width_ratio(
            gameplay_note_sprite_cache_.imported_note_width_ratio, use_imported_metrics);
        const float note_height_scale = effective_gameplay_note_height_scale(
            data.gameplay.note_height_scale,
            gameplay_note_sprite_cache_.imported_note_height_ratio,
            use_imported_metrics);
        const float hold_body_width_scale =
            clamp_gameplay_hold_body_width_scale(data.gameplay.hold_body_width_scale);
        const bool note_border_enabled = data.gameplay.note_border_enabled;
        // A skin that declares note_aspect knows its own art, so it wins over the
        // player's Image Aspect toggle; otherwise that toggle picks contain/stretch.
        const NoteImageAspect note_image_aspect =
            use_imported_metrics && gameplay_note_sprite_cache_.has_imported_note_aspect
                ? gameplay_note_sprite_cache_.imported_note_aspect
                : (data.gameplay.preserve_note_image_aspect_ratio ? NoteImageAspect::Contain
                                                                  : NoteImageAspect::Stretch);
        // Hit-burst brightness, 0% turns the explosion off entirely.
        const float key_pulse_brightness =
            std::clamp(data.gameplay.key_pulse_brightness, 0.0f, 1.0f);
        const std::string note_shape = normalize_gameplay_note_shape(data.gameplay.note_shape);
        ID2D1Geometry* note_polygon_geometry = gameplay_note_polygon_geometry(
            d2d_->gameplay_note_shape_geometries, note_shape);
        const float visual_opacity =
            static_cast<float>(std::clamp(data.gameplay.visual_opacity, 0.20, 1.0));
        const float note_outline_opacity =
            static_cast<float>(std::clamp(data.gameplay.note_outline_opacity, 0.0, 1.0) * visual_opacity);
        const float hold_body_opacity =
            static_cast<float>(std::clamp(data.gameplay.hold_body_opacity, 0.05, 0.60) * visual_opacity);
        const float native_hold_body_opacity =
            gameplay_native_hold_body_opacity(hold_body_opacity, visual_opacity);
        const std::string key_label_position =
            config::normalize_skin_key_label_position_token(data.gameplay.key_label_position);

        const GameplayMotionDiagnostics motion_diagnostics = compute_gameplay_motion_diagnostics(
            GameplayMotionState{
                data.gameplay.current_sample,
                data.gameplay.duration_samples,
                data.gameplay.sample_rate,
                data.gameplay.audio_sample_time_ns,
                0,
                data.gameplay.audio_buffer_frames,
                data.gameplay.visual_offset_ms,
                data.gameplay.finished || data.gameplay.paused,
                data.gameplay.game_over && !data.gameplay.spectating_peer,
            },
            render_now_ns);
        const int64_t display_sample = motion_diagnostics.display_sample;
        const int64_t hold_handoff_grace_samples = gameplay_hold_handoff_grace_samples(
            data.gameplay.sample_rate,
            motion_diagnostics.extrapolation_limit_samples);
        constexpr double kGameplayTimingIndicatorRangeMs = 80.0;
        constexpr float kGameplayTimingIndicatorHalfWidth = 124.0f;
        constexpr float kGameplayTimingIndicatorHeight = 8.0f;

        const double display_visual_position =
            data.gameplay.current_visual_position +
            data.gameplay.visual_velocity *
                static_cast<double>(display_sample - data.gameplay.current_sample);
        auto visual_to_y = [&](double position) -> float {
            return static_cast<float>(compute_gameplay_visual_y_normalized(
                position,
                display_visual_position,
                data.gameplay.future_visual_span,
                data.gameplay.past_visual_span,
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

        auto format_progress_clock = [&](int64_t sample_count) -> std::string {
            if (data.gameplay.sample_rate <= 0) {
                return "--:--";
            }
            const int64_t safe_samples = std::max<int64_t>(0, sample_count);
            const int64_t total_seconds =
                std::max<int64_t>(0, static_cast<int64_t>((safe_samples + data.gameplay.sample_rate / 2) /
                                                          data.gameplay.sample_rate));
            const int64_t hours = total_seconds / 3600;
            const int64_t minutes = (total_seconds / 60) % 60;
            const int64_t seconds = total_seconds % 60;
            if (hours > 0) {
                return std::to_string(hours) + ":" +
                       (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
                       (seconds < 10 ? "0" : "") + std::to_string(seconds);
            }
            const int64_t total_minutes = total_seconds / 60;
            return std::to_string(total_minutes) + ":" +
                   (seconds < 10 ? "0" : "") + std::to_string(seconds);
        };

        auto draw_gameplay_progress_bar = [&]() {
            if (!d2d_->hud_format || !d2d_->text_brush || data.gameplay.duration_samples <= 0) {
                return;
            }

            const int64_t progress_sample =
                std::clamp<int64_t>(data.gameplay.current_sample, 0, data.gameplay.duration_samples);
            const int64_t remaining_sample = std::max<int64_t>(0, data.gameplay.duration_samples - progress_sample);
            const float fill_ratio =
                std::clamp(static_cast<float>(static_cast<double>(progress_sample) /
                                             static_cast<double>(std::max<int64_t>(1, data.gameplay.duration_samples))),
                           0.0f,
                           1.0f);
            const D2D1_RECT_F track_rect =
                D2D1::RectF(header_left,
                            16.0f,
                            header_safe_right,
                            36.0f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.72f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(track_rect, 10.0f, 10.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush && fill_ratio > 0.0f) {
                const float inner_left = track_rect.left + 3.0f;
                const float inner_right = track_rect.right - 3.0f;
                const float inner_top = track_rect.top + 3.0f;
                const float inner_bottom = track_rect.bottom - 3.0f;
                const float fill_right = inner_left + (inner_right - inner_left) * fill_ratio;
                const D2D1_RECT_F fill_rect =
                    D2D1::RectF(inner_left, inner_top, std::max(inner_left, fill_right), inner_bottom);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(fill_rect, 8.0f, 8.0f), d2d_->accent_brush.Get());
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(track_rect, 10.0f, 10.0f),
                                          d2d_->button_border_brush.Get(),
                                          1.0f);
            }

            const std::wstring elapsed_total_w =
                to_wide(format_progress_clock(progress_sample) + " / " +
                        format_progress_clock(data.gameplay.duration_samples));
            const std::wstring remaining_w =
                to_wide(loc("LEFT ", "남은 ") + format_progress_clock(remaining_sample));
            const std::wstring percent_w =
                to_wide(std::to_string(static_cast<int>(std::llround(fill_ratio * 100.0f))) + "%");

            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.92f));
            draw_text_clipped(elapsed_total_w,
                              d2d_->hud_format.Get(),
                              D2D1::RectF(track_rect.left + 12.0f, track_rect.top - 1.0f,
                                          track_rect.left + 250.0f, track_rect.bottom + 1.0f),
                              d2d_->text_brush.Get());
            draw_text_clipped_aligned(percent_w,
                                      d2d_->hud_format.Get(),
                                      D2D1::RectF(track_rect.left + 180.0f, track_rect.top - 1.0f,
                                                  track_rect.right - 180.0f, track_rect.bottom + 1.0f),
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
            if (d2d_->muted_brush) {
                draw_text_clipped_aligned(remaining_w,
                                          d2d_->hud_format.Get(),
                                          D2D1::RectF(track_rect.right - 260.0f, track_rect.top - 1.0f,
                                                      track_rect.right - 12.0f, track_rect.bottom + 1.0f),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
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
            const int judgement_count =
                data.gameplay.pg + data.gameplay.gr + data.gameplay.gd +
                data.gameplay.bd + data.gameplay.pr;
            if (!gameplay_hud_cache_.animation_initialized) {
                gameplay_hud_cache_.animation_initialized = true;
            } else {
                if (data.gameplay.combo > 0 &&
                    data.gameplay.combo != gameplay_hud_cache_.animated_combo) {
                    gameplay_hud_cache_.combo_animation_started_ns = render_now_ns;
                }
                if (data.gameplay.has_feedback &&
                    judgement_count > gameplay_hud_cache_.animated_judgement_count) {
                    gameplay_hud_cache_.judgement_animation_started_ns = render_now_ns;
                }
            }
            gameplay_hud_cache_.animated_combo = data.gameplay.combo;
            gameplay_hud_cache_.animated_judgement_count = judgement_count;
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
            std::string score_summary =
                loc("SCORE  ", "점수  ") + format_int_with_commas(data.gameplay.score);
            gameplay_hud_cache_.score_text = to_wide(score_summary);
            gameplay_hud_cache_.combo_text =
                to_wide(loc("COMBO ", "콤보 ") + std::to_string(data.gameplay.combo) +
                        "   " + loc("MAX ", "최대 ") + std::to_string(data.gameplay.max_combo) +
                        "   " + loc("ACC ", "정확도 ") + format_decimal(data.gameplay.accuracy, 2) + "%");
            gameplay_hud_cache_.combo_text +=
                to_wide(" / DETAIL " + format_decimal(data.gameplay.detailed_accuracy, 2) + "%");
            gameplay_hud_cache_.combo_value_text = to_wide(std::to_string(data.gameplay.combo));
            gameplay_hud_cache_.combo_label_text = wloc("COMBO", "콤보");
            gameplay_hud_cache_.judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.pg) +
                        "  GR " + std::to_string(data.gameplay.gr) +
                        "  G " + std::to_string(data.gameplay.gd) +
                        "  BAD " + std::to_string(data.gameplay.bd) +
                        "  PR " + std::to_string(data.gameplay.pr));
            gameplay_hud_cache_.gauge_label_text = to_wide(gauge_label);
            gameplay_hud_cache_.gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.gauge))) + "%");
            gameplay_hud_cache_.peer_name_text =
                to_wide(data.gameplay.peer_name.empty() ? loc("OPPONENT", "\uC0C1\uB300") : data.gameplay.peer_name);
            std::string peer_status = data.gameplay.peer_status;
            if (data.gameplay.peer_disconnected) {
                peer_status = loc("DISCONNECTED", "\uC5F0\uACB0 \uB04A\uAE40");
            } else if (data.gameplay.peer_game_over) {
                peer_status = loc("GAME OVER", "\uAC8C\uC784 \uC624\uBC84");
            } else if (data.gameplay.peer_aborted) {
                peer_status = loc("ABORTED", "\uC911\uB2E8");
            } else if (data.gameplay.peer_finished) {
                peer_status = loc("FINISHED", "\uC644\uB8CC");
            } else if (peer_status.empty()) {
                peer_status = loc("PLAYING", "\uD50C\uB808\uC774 \uC911");
            }
            gameplay_hud_cache_.peer_status_text = to_wide(peer_status);
            gameplay_hud_cache_.peer_score_text =
                to_wide(loc("SCORE  ", "\uC810\uC218  ") + format_int_with_commas(data.gameplay.peer_score));
            gameplay_hud_cache_.peer_combo_text =
                to_wide(loc("COMBO ", "\uCF64\uBCF4 ") + std::to_string(data.gameplay.peer_combo) +
                        "   " + loc("MAX ", "\uCD5C\uB300 ") + std::to_string(data.gameplay.peer_max_combo));
            gameplay_hud_cache_.peer_judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.peer_pg) +
                        "  GR " + std::to_string(data.gameplay.peer_gr) +
                        "  G " + std::to_string(data.gameplay.peer_gd) +
                        "  BAD " + std::to_string(data.gameplay.peer_bd) +
                        "  PR " + std::to_string(data.gameplay.peer_pr));
            gameplay_hud_cache_.peer_gauge_text =
                to_wide(loc("GAUGE ", "\uAC8C\uC774\uC9C0 ") +
                        std::to_string(static_cast<int>(std::llround(data.gameplay.peer_gauge))) + "%");
            if (!data.gameplay.peer_score_available) {
                gameplay_hud_cache_.versus_score_difference_text = L"SYNC";
            } else {
                const std::string difference_prefix =
                    data.gameplay.versus_score_difference > 0 ? "+" : "";
                gameplay_hud_cache_.versus_score_difference_text =
                    to_wide(difference_prefix +
                            format_int_with_commas(data.gameplay.versus_score_difference));
            }
            gameplay_hud_cache_.spectating_text =
                to_wide(loc("SPECTATING ", "\uAD00\uC804 \uC911  ") +
                        (data.gameplay.peer_name.empty()
                             ? loc("OPPONENT", "\uC0C1\uB300")
                             : data.gameplay.peer_name));
            std::string ghost_score_summary =
                loc("SCORE  ", "점수  ") + format_int_with_commas(data.gameplay.ghost_score);
            gameplay_hud_cache_.ghost_score_text = to_wide(ghost_score_summary);
            gameplay_hud_cache_.ghost_combo_text =
                to_wide(loc("COMBO ", "콤보 ") + std::to_string(data.gameplay.ghost_combo) +
                        "   " + loc("MAX ", "최대 ") + std::to_string(data.gameplay.ghost_max_combo) +
                        "   " + loc("ACC ", "정확도 ") + format_decimal(data.gameplay.ghost_accuracy, 2) + "%");
            gameplay_hud_cache_.ghost_combo_text +=
                to_wide(" / DETAIL " + format_decimal(data.gameplay.ghost_detailed_accuracy, 2) + "%");
            gameplay_hud_cache_.ghost_combo_value_text =
                to_wide(std::to_string(data.gameplay.ghost_combo));
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
                gameplay_hud_cache_.feedback_timing_text =
                    gameplay_timing_feedback_text(data.gameplay.feedback_delta_ms);
            } else {
                gameplay_hud_cache_.feedback_text.clear();
                gameplay_hud_cache_.feedback_timing_text.clear();
            }
            if (data.gameplay.ghost_has_feedback) {
                gameplay_hud_cache_.ghost_feedback_text =
                    gameplay_feedback_overlay_text(data.gameplay.ghost_feedback);
                gameplay_hud_cache_.ghost_feedback_timing_text =
                    gameplay_timing_feedback_text(data.gameplay.ghost_feedback_delta_ms);
            } else {
                gameplay_hud_cache_.ghost_feedback_text.clear();
                gameplay_hud_cache_.ghost_feedback_timing_text.clear();
            }
            gameplay_hud_cache_.text_revision = data.gameplay.text_revision;
        }

        constexpr double kComboAnimationDurationMs = 150.0;
        constexpr double kJudgementAnimationDurationMs = 220.0;
        const auto animation_age_ms = [render_now_ns](int64_t started_ns, double duration_ms) {
            if (started_ns <= 0) return duration_ms;
            return std::clamp(
                static_cast<double>(render_now_ns - started_ns) / 1'000'000.0,
                0.0,
                duration_ms);
        };
        const GameplayTextPopAnimation combo_text_animation =
            compute_gameplay_text_pop_animation(
                animation_age_ms(gameplay_hud_cache_.combo_animation_started_ns,
                                 kComboAnimationDurationMs),
                kComboAnimationDurationMs,
                1.16f,
                -5.0f);
        const GameplayTextPopAnimation judgement_text_animation =
            compute_gameplay_text_pop_animation(
                animation_age_ms(gameplay_hud_cache_.judgement_animation_started_ns,
                                 kJudgementAnimationDurationMs),
                kJudgementAnimationDurationMs,
                1.22f,
                -8.0f);

        if (data.gameplay.loading && !data.gameplay.active) {
            draw_gameplay_header();
            const bool waiting_for_peer_result =
                data.gameplay.finished && data.gameplay.peer_visible;
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

            const std::wstring loading_w =
                waiting_for_peer_result
                    ? wloc("WAITING FOR OPPONENT", "상대 결과 대기 중")
                    : wloc("LOADING CHART", "차트 로딩 중");
            const std::wstring stage_w =
                to_wide(data.gameplay.loading_stage.empty() ? loc("Preparing gameplay", "게임 준비 중")
                                                            : data.gameplay.loading_stage);
            const std::wstring percent_w =
                waiting_for_peer_result
                    ? wloc("RESULT SYNC", "결과 동기화")
                    : to_wide(std::to_string(std::clamp(data.gameplay.loading_percent, 0, 100)) + "%");
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

            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring hispeed_hint_w =
                    wloc("HI-SPEED  F3/F4  FINE +/-0.25   F5/F6  COARSE +/-10",
                         "노트 배속  F3/F4  미세 +/-0.25   F5/F6  크게 +/-10");
                draw_text_clipped_aligned(
                    hispeed_hint_w,
                    d2d_->body_format.Get(),
                    D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 62.0f,
                                panel_rect.right - 24.0f, panel_rect.bottom - 20.0f),
                    d2d_->muted_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            return;
        }

        const int lane_count = std::clamp(data.gameplay.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
        std::array<double, kGameplayHudMaxLanes> effective_lane_width_scales{};
        effective_lane_width_scales.fill(kGameplayLaneWidthScaleDefault);
        std::size_t effective_lane_width_scale_count =
            std::min(data.gameplay.lane_width_scale_count, effective_lane_width_scales.size());
        for (std::size_t lane = 0; lane < effective_lane_width_scale_count; ++lane) {
            effective_lane_width_scales[lane] = data.gameplay.lane_width_scales[lane];
        }
        if (use_imported_metrics &&
            gameplay_note_sprite_cache_.imported_lane_width_scale_count ==
                static_cast<std::size_t>(lane_count)) {
            effective_lane_width_scale_count = static_cast<std::size_t>(lane_count);
            for (std::size_t lane = 0; lane < effective_lane_width_scale_count; ++lane) {
                effective_lane_width_scales[lane] = std::clamp(
                    effective_lane_width_scales[lane] *
                        gameplay_note_sprite_cache_.imported_lane_width_scales[lane],
                    kGameplayLaneWidthScaleMin,
                    kGameplayLaneWidthScaleMax);
            }
        }
        std::array<double, kGameplayHudMaxLanes> effective_lane_spacing_scales{};
        effective_lane_spacing_scales.fill(kGameplayLaneSpacingScaleDefault);
        std::size_t effective_lane_spacing_scale_count =
            std::min(data.gameplay.lane_spacing_scale_count, effective_lane_spacing_scales.size());
        for (std::size_t gap = 0; gap < effective_lane_spacing_scale_count; ++gap) {
            effective_lane_spacing_scales[gap] = data.gameplay.lane_spacing_scales[gap];
        }
        if (use_imported_metrics &&
            gameplay_note_sprite_cache_.imported_lane_spacing_scale_count ==
                static_cast<std::size_t>(std::max(0, lane_count - 1))) {
            effective_lane_spacing_scale_count =
                gameplay_note_sprite_cache_.imported_lane_spacing_scale_count;
            for (std::size_t gap = 0; gap < effective_lane_spacing_scale_count; ++gap) {
                effective_lane_spacing_scales[gap] = std::clamp(
                    effective_lane_spacing_scales[gap] +
                        gameplay_note_sprite_cache_.imported_lane_spacing_scales[gap],
                    kGameplayLaneSpacingScaleMin,
                    kGameplayLaneSpacingScaleMax);
            }
        }
        const GameplaySurfaceLayout surface_layout =
            build_gameplay_surface_layout(
                lane_count,
                note_width_scale,
                note_art_width_ratio,
                effective_lane_width_scale_count,
                effective_lane_width_scales,
                effective_lane_spacing_scale_count,
                effective_lane_spacing_scales,
                data.gameplay.ghost_visible,
                data.gameplay.lane_center_gap_scale,
                gameplay_field_drag_state_.has_local_override
                    ? gameplay_field_drag_state_.offset_x
                    : data.gameplay.gameplay_field_offset_x);
        gameplay_field_drag_state_.visible = data.gameplay.active && !data.gameplay.loading;
        gameplay_field_drag_state_.left =
            surface_layout.player_field.right + kGameplayFieldDragHandleGap;
        gameplay_field_drag_state_.top =
            surface_layout.player_field.top + kGameplayFieldDragHandleTop;
        gameplay_field_drag_state_.right =
            gameplay_field_drag_state_.left + kGameplayFieldDragHandleWidth;
        gameplay_field_drag_state_.bottom =
            gameplay_field_drag_state_.top + kGameplayFieldDragHandleHeight;
        gameplay_field_drag_state_.offset_x = surface_layout.offset_x;
        gameplay_field_drag_state_.min_offset_x = surface_layout.min_offset_x;
        gameplay_field_drag_state_.max_offset_x = surface_layout.max_offset_x;
        const GameplayFieldLayout field_layout = surface_layout.player_field;
        const float field_left = field_layout.left;
        const float field_right = field_layout.right;
        const float field_top = field_layout.top;
        const float field_bottom = field_layout.bottom;
        const float field_height = field_layout.height;
        const D2D1_RECT_F field_clip_rect =
            D2D1::RectF(field_left + 2.0f, field_top + 2.0f, field_right - 2.0f, field_bottom - 2.0f);
        const float hit_line_y = gameplay_field_y(field_top, field_height, judgement_line_position);

        auto draw_vertical_gauge = [&](float gauge_left,
                                       double gauge_value,
                                       std::string_view gauge_token,
                                       const std::wstring& gauge_label_text,
                                       const std::wstring& gauge_value_text,
                                       std::size_t gauge_grid_index) {
            const float gauge_ratio =
                static_cast<float>(std::clamp(gauge_value / 100.0, 0.0, 1.0));
            const float fill_top =
                kGameplayGaugeBottom - (kGameplayGaugeBottom - kGameplayGaugeTop) * gauge_ratio;
            const uint32_t gauge_rgb = gameplay_gauge_color(gauge_token);

            if (d2d_->accent_brush && gauge_ratio > 0.0f) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                const float inner_bottom = kGameplayGaugeBottom - 4.0f;
                const float inner_top = std::clamp(fill_top + 4.0f,
                                                   kGameplayGaugeTop + 4.0f,
                                                   inner_bottom);

                d2d_->accent_brush->SetColor(
                    color_from_rgb(gauge_rgb, 0.16f * visual_opacity));
                const D2D1_RECT_F halo =
                    D2D1::RectF(gauge_left + 1.0f,
                                std::max(kGameplayGaugeTop + 1.0f, fill_top - 3.0f),
                                gauge_left + kGameplayGaugeWidth - 1.0f,
                                kGameplayGaugeBottom - 1.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(halo, 10.0f, 10.0f),
                                          d2d_->accent_brush.Get());

                if (inner_top < inner_bottom) {
                    const D2D1_RECT_F fill =
                        D2D1::RectF(gauge_left + 4.0f,
                                    inner_top,
                                    gauge_left + kGameplayGaugeWidth - 4.0f,
                                    inner_bottom);
                    d2d_->accent_brush->SetColor(
                        color_from_rgb(gauge_rgb, 0.80f * visual_opacity));
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f),
                                              d2d_->accent_brush.Get());

                    const D2D1_RECT_F sheen =
                        D2D1::RectF(fill.left + 5.0f,
                                    fill.top + 3.0f,
                                    std::min(fill.right - 4.0f, fill.left + 10.0f),
                                    fill.bottom - 3.0f);
                    if (sheen.bottom > sheen.top && sheen.right > sheen.left) {
                        d2d_->accent_brush->SetColor(
                            color_from_rgb(blend_rgb(gauge_rgb, 0xFFFFFF, 0.72f),
                                           0.46f * visual_opacity));
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(sheen, 3.0f, 3.0f),
                                                  d2d_->accent_brush.Get());
                    }
                }

                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            if (d2d_->button_border_brush &&
                gauge_grid_index < d2d_->gameplay_gauge_grid_geometries.size() &&
                d2d_->gameplay_gauge_grid_geometries[gauge_grid_index]) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.42f * visual_opacity);
                ctx->DrawGeometry(d2d_->gameplay_gauge_grid_geometries[gauge_grid_index].Get(),
                                  d2d_->button_border_brush.Get(),
                                  1.0f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F label_rect =
                D2D1::RectF(gauge_left - 90.0f,
                            kGameplayGaugeTop - 42.0f,
                            gauge_left + 140.0f,
                            kGameplayGaugeTop - 8.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(gauge_left - 90.0f,
                            kGameplayGaugeBottom + 8.0f,
                            gauge_left + 140.0f,
                            kGameplayGaugeBottom + 50.0f);
            if (d2d_->body_format && d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetColor(color_from_rgb(gauge_rgb, 0.92f));
                draw_text_clipped_aligned(gauge_label_text,
                                          d2d_->body_format.Get(),
                                          label_rect,
                                          d2d_->accent_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->title_format && d2d_->text_brush) {
                draw_text_clipped_aligned(gauge_value_text,
                                          d2d_->title_format.Get(),
                                          value_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        };

        auto draw_combo_overlay = [&](const GameplayFieldLayout& combo_field_layout,
                                      const std::wstring& combo_value_text,
                                      const std::wstring& combo_label_text,
                                      float top_safe_margin,
                                      float bottom_safe_margin,
                                      float vertical_offset,
                                      const GameplayTextPopAnimation& animation) {
            if (combo_value_text.empty() || !d2d_->accent_brush || !d2d_->gameplay_combo_format) {
                return;
            }
            constexpr float kComboHalfHeight = 30.0f;
            constexpr float kComboLabelExtension = 18.0f;
            const D2D1_RECT_F combo_rect =
                gameplay_combo_overlay_rect(combo_field_layout,
                                            combo_position,
                                            kComboHalfHeight,
                                            top_safe_margin,
                                            bottom_safe_margin,
                                            vertical_offset,
                                            8.0f,
                                            kComboLabelExtension);
            D2D1_MATRIX_3X2_F saved_transform{};
            ctx->GetTransform(&saved_transform);
            const D2D1_POINT_2F animation_center = D2D1::Point2F(
                (combo_rect.left + combo_rect.right) * 0.5f,
                (combo_rect.top + combo_rect.bottom) * 0.5f);
            const D2D1_MATRIX_3X2_F animation_transform =
                D2D1::Matrix3x2F::Scale(animation.scale, animation.scale, animation_center) *
                D2D1::Matrix3x2F::Translation(0.0f, animation.offset_y) *
                saved_transform;
            ctx->SetTransform(animation_transform);
            if (d2d_->footer_brush) {
                const float saved_footer_opacity = d2d_->footer_brush->GetOpacity();
                d2d_->footer_brush->SetOpacity(saved_footer_opacity * animation.opacity);
                const D2D1_RECT_F shadow_rect =
                    D2D1::RectF(combo_rect.left + 2.0f,
                                combo_rect.top + 3.0f,
                                combo_rect.right + 2.0f,
                                combo_rect.bottom + 3.0f);
                draw_text_clipped_aligned(combo_value_text,
                                          d2d_->gameplay_combo_format.Get(),
                                          shadow_rect,
                                          d2d_->footer_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->footer_brush->SetOpacity(saved_footer_opacity);
            }
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(0.94f * animation.opacity);
            draw_text_clipped_aligned(combo_value_text,
                                      d2d_->gameplay_combo_format.Get(),
                                      combo_rect,
                                      d2d_->accent_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->accent_brush->SetOpacity(saved_opacity);
            if (d2d_->hud_format && d2d_->muted_brush) {
                const float saved_muted_opacity = d2d_->muted_brush->GetOpacity();
                d2d_->muted_brush->SetOpacity(saved_muted_opacity * animation.opacity);
                draw_text_clipped_aligned(combo_label_text,
                                          d2d_->hud_format.Get(),
                                          D2D1::RectF(combo_rect.left,
                                                      combo_rect.bottom - 2.0f,
                                                      combo_rect.right,
                                                      combo_rect.bottom + kComboLabelExtension),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->muted_brush->SetOpacity(saved_muted_opacity);
            }
            ctx->SetTransform(saved_transform);
        };
        if (d2d_->gameplay_static_command_list) {
            ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
        }

        auto draw_key_labels = [&](const GameplayFieldLayout& label_field_layout) {
            if (key_label_position == "off" || !d2d_->hud_format || !d2d_->text_brush ||
                data.gameplay.key_label_count == 0) {
                return;
            }
            const bool top_labels = key_label_position == "top";
            const float label_top = top_labels ? (label_field_layout.top + 8.0f)
                                               : (label_field_layout.bottom - 30.0f);
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.38f * visual_opacity));
            const std::size_t label_count =
                std::min(data.gameplay.key_label_count, static_cast<std::size_t>(label_field_layout.lane_count));
            for (std::size_t lane = 0; lane < label_count && lane < data.gameplay.key_labels.size(); ++lane) {
                const std::string& label = data.gameplay.key_labels[lane];
                if (label.empty()) {
                    continue;
                }
                const int lane_index = static_cast<int>(lane);
                const D2D1_RECT_F label_rect =
                    D2D1::RectF(gameplay_lane_left(label_field_layout, lane_index) + 2.0f,
                                label_top,
                                gameplay_lane_right(label_field_layout, lane_index) - 2.0f,
                                label_top + 22.0f);
                draw_text_clipped_aligned(to_wide(label),
                                          d2d_->hud_format.Get(),
                                          label_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            d2d_->text_brush->SetColor(saved_text_color);
        };

        auto draw_native_digital_key = [&](const GameplayFieldLayout& key_field_layout,
                                           float key_hit_line_y,
                                           int lane,
                                           bool pressed,
                                           float activity) {
            if (use_imported_metrics || !d2d_->note_fill_brush) {
                return;
            }
            const float lane_left = gameplay_lane_left(key_field_layout, lane) + 2.0f;
            const float lane_right = gameplay_lane_right(key_field_layout, lane) - 2.0f;
            const float raw_gear_top = gameplay_osu_gear_top(key_field_layout, key_hit_line_y, note_height_scale);
            const float gear_bottom = key_field_layout.bottom - 3.0f;
            const float key_height = std::clamp(gear_bottom - raw_gear_top, 48.0f, 180.0f);
            const float gear_top = gear_bottom - key_height;
            const NativeDigitalKeyVisual key_visual =
                resolve_native_digital_key_visual(pressed, activity, key_height);
            uint32_t lane_color = 0xF6F8FF;
            const std::size_t lane_index = static_cast<std::size_t>(lane);
            if (lane_index < data.gameplay.lane_color_count) {
                lane_color = data.gameplay.lane_colors[lane_index];
            } else if (!gameplay_lane_uses_white_note(lane + 1)) {
                lane_color = 0x4F80FF;
            }

            ID2D1SolidColorBrush* fill = d2d_->note_fill_brush.Get();
            const D2D1_COLOR_F saved_fill_color = fill->GetColor();
            const float saved_fill_opacity = fill->GetOpacity();
            const D2D1_RECT_F housing =
                D2D1::RectF(lane_left, gear_top + 1.0f, lane_right, gear_bottom);
            fill->SetColor(color_from_rgb(0x03070C, 0.96f * visual_opacity));
            ctx->FillRoundedRectangle(D2D1::RoundedRect(housing, 5.0f, 5.0f), fill);

            const D2D1_RECT_F surface = D2D1::RectF(
                lane_left + 2.0f,
                gear_top + 4.0f + key_visual.press_offset,
                lane_right - 2.0f,
                gear_bottom - 2.0f);
            const uint32_t surface_color = pressed
                ? blend_rgb(lane_color, 0x36E1F2, 0.30f)
                : blend_rgb(lane_color, 0x07131E, 0.78f);
            fill->SetColor(color_from_rgb(surface_color, (pressed ? 0.96f : 0.88f) * visual_opacity));
            ctx->FillRoundedRectangle(D2D1::RoundedRect(surface, 4.0f, 4.0f), fill);

            const float lip_height = pressed ? 2.0f : 5.0f;
            fill->SetColor(color_from_rgb(
                blend_rgb(lane_color, pressed ? 0xFFFFFF : 0x6EE7F2, pressed ? 0.48f : 0.25f),
                (pressed ? 0.86f : 0.52f) * visual_opacity));
            ctx->FillRectangle(D2D1::RectF(surface.left + 2.0f,
                                           surface.top + 2.0f,
                                           surface.right - 2.0f,
                                           std::min(surface.bottom, surface.top + 2.0f + lip_height)),
                               fill);

            fill->SetColor(color_from_rgb(0x9BDDE8, 0.10f * visual_opacity));
            const float scan_step = std::max(8.0f, key_height / 7.0f);
            for (float y = surface.top + 14.0f; y < surface.bottom - 4.0f; y += scan_step) {
                ctx->FillRectangle(D2D1::RectF(surface.left + 4.0f, y, surface.right - 4.0f, y + 1.0f), fill);
            }

            if (key_visual.glitch_strength > 0.02f) {
                const float glitch = key_visual.glitch_strength;
                const float glitch_y = std::clamp(
                    surface.top + 18.0f + static_cast<float>((lane * 29) % 61),
                    surface.top + 8.0f,
                    surface.bottom - 8.0f);
                const float shift = 2.0f + 5.0f * glitch;
                fill->SetColor(color_from_rgb(0x55F5FF, 0.78f * glitch * visual_opacity));
                ctx->FillRectangle(D2D1::RectF(surface.left + 5.0f + shift,
                                               glitch_y,
                                               surface.right - 5.0f,
                                               glitch_y + 3.0f),
                                   fill);
                fill->SetColor(color_from_rgb(0xFF4FD8, 0.58f * glitch * visual_opacity));
                ctx->FillRectangle(D2D1::RectF(surface.left + 5.0f,
                                               glitch_y + 5.0f,
                                               surface.right - 5.0f - shift,
                                               glitch_y + 7.0f),
                                   fill);
            }

            if (d2d_->button_border_brush) {
                const float saved_border_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity((pressed ? 0.82f : 0.42f) * visual_opacity);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(surface, 4.0f, 4.0f),
                                          d2d_->button_border_brush.Get(),
                                          pressed ? 1.8f : 1.0f);
                d2d_->button_border_brush->SetOpacity(saved_border_opacity);
            }
            fill->SetColor(saved_fill_color);
            fill->SetOpacity(saved_fill_opacity);
        };

        // LR2 gear art is a bottom panel with the receptor art baked in; TenRiff
        // skins declare gear as a full-playfield overlay with separate key slots.
        const bool tenriff_gear_overlay =
            gameplay_note_sprite_cache_.skin_source == "tenriff";
        auto draw_imported_gear_overlay = [&](const GameplayFieldLayout& gear_field_layout,
                                              float gear_hit_line_y) {
            ID2D1Bitmap* bitmap = d2d_->gameplay_gear_overlay_bitmap.Get();
            if (!bitmap) {
                return false;
            }
            const D2D1_RECT_F* source_rect =
                bitmap_source_rect_or_null(d2d_->gameplay_gear_overlay_source_rect);
            if (tenriff_gear_overlay) {
                ctx->DrawBitmap(bitmap,
                                D2D1::RectF(gear_field_layout.left,
                                            gear_field_layout.top,
                                            gear_field_layout.right,
                                            gear_field_layout.bottom),
                                visual_opacity,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                source_rect);
                return true;
            }
            const D2D1_SIZE_F bitmap_size = bitmap->GetSize();
            const float source_width = source_rect
                ? source_rect->right - source_rect->left
                : bitmap_size.width;
            const float source_height = source_rect
                ? source_rect->bottom - source_rect->top
                : bitmap_size.height;
            const GameplayGearRect bounds{
                gear_field_layout.left + 2.0f,
                gameplay_osu_gear_top(gear_field_layout,
                                      gear_hit_line_y,
                                      note_height_scale),
                gear_field_layout.right - 2.0f,
                gear_field_layout.bottom - 2.0f,
            };
            const GameplayGearRect fitted =
                fit_gameplay_gear_rect(bounds,
                                       source_width,
                                       source_height,
                                       gameplay_gear_scale_multiplier(note_width_scale));
            ctx->PushAxisAlignedClip(D2D1::RectF(bounds.left,
                                                 bounds.top,
                                                 bounds.right,
                                                 bounds.bottom),
                                     D2D1_ANTIALIAS_MODE_ALIASED);
            ctx->DrawBitmap(bitmap,
                            D2D1::RectF(fitted.left,
                                        fitted.top,
                                        fitted.right,
                                        fitted.bottom),
                            visual_opacity,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            source_rect);
            ctx->PopAxisAlignedClip();
            return true;
        };

        const D2D1_ANTIALIAS_MODE saved_antialias = ctx->GetAntialiasMode();
        ctx->PushAxisAlignedClip(field_clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
        const bool player_has_gear_overlay =
            draw_imported_gear_overlay(field_layout, hit_line_y);
        if (use_imported_metrics) {
            draw_key_labels(field_layout);
        }
        ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (int lane = 0; lane < lane_count; ++lane) {
            const std::size_t lane_index = static_cast<std::size_t>(lane);
            if (player_has_gear_overlay && !tenriff_gear_overlay) {
                continue;
            }
            const bool lane_is_pressed =
                lane_index < data.gameplay.lane_pressed_count &&
                data.gameplay.lane_pressed[lane_index] != 0;
            const float lane_activity = lane_index < data.gameplay.lane_activity_count
                ? data.gameplay.lane_activity[lane_index]
                : 0.0f;
            ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
            if (should_use_imported_pressed_key(lane_activity) &&
                d2d_->lane_key_pressed_bitmaps[lane_index]) {
                key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
            }
            if (!key_bitmap && !use_imported_metrics) {
                draw_native_digital_key(field_layout,
                                        hit_line_y,
                                        lane,
                                        lane_is_pressed,
                                        lane_activity);
                continue;
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
            draw_gameplay_sprite(ctx, key_bitmap, receptor_rect, visual_opacity,
                                 key_source_rect,
                                 gameplay_lane_sprite_rotation(
                                     gameplay_note_sprite_cache_.key_rotations,
                                     gameplay_note_sprite_cache_.key_rotation_count,
                                     lane_index));
        }
        if (!use_imported_metrics) {
            draw_key_labels(field_layout);
        }
        for (std::size_t note_index = 0; note_index < data.gameplay.note_count; ++note_index) {
            const auto& note = data.gameplay.notes[note_index];
            if (!should_render_gameplay_note(
                    note.start_sample,
                    note.tail_sample,
                    note.hold,
                    note.head_visible,
                    note.pending,
                    data.gameplay.current_sample,
                    display_sample,
                    hold_handoff_grace_samples)) {
                continue;
            }
            const bool render_head =
                should_render_gameplay_note_head(note.start_sample, note.head_visible, display_sample);
            const int lane = std::clamp(note.lane, 1, lane_count);
            const std::size_t lane_index = static_cast<std::size_t>(lane - 1);
            const float lane_center = gameplay_lane_center(field_layout, lane - 1);
            const float note_width = gameplay_note_width(field_layout, lane - 1);
            const float x0 = lane_center - note_width * 0.5f;
            const float x1 = lane_center + note_width * 0.5f;
            const double render_visual_position =
                gameplay_note_anchors_to_judgement_line(note.hold, note.head_visible)
                    ? display_visual_position
                    : note.visual_position;
            const float y = gameplay_field_y(field_top, field_height, visual_to_y(render_visual_position));
            const float tail_y =
                gameplay_field_y(field_top, field_height, visual_to_y(note.tail_visual_position));
            const float head_half_h = gameplay_note_head_half_height(note_height_scale);
            const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
            uint32_t lane_color = 0xF6F8FF;
            if (lane_index < data.gameplay.lane_color_count) {
                lane_color = data.gameplay.lane_colors[lane_index];
            } else if (!gameplay_lane_uses_white_note(lane)) {
                lane_color = 0x4F80FF;
            }
            ID2D1SolidColorBrush* note_fill = d2d_->note_fill_brush.Get();
            ID2D1SolidColorBrush* note_border = d2d_->note_border_brush.Get();
            ID2D1SolidColorBrush* note_hold_fill = d2d_->note_hold_brush.Get();
            if (note_fill) {
                note_fill->SetColor(gameplay_note_fill_color(lane_color, visual_opacity));
            }
            if (note_border) {
                note_border->SetColor(gameplay_note_border_color(lane_color, note_outline_opacity));
            }
            if (note_hold_fill) {
                note_hold_fill->SetColor(gameplay_note_hold_color(lane_color, native_hold_body_opacity));
            }
            ID2D1LinearGradientBrush* note_material = d2d_->lane_native_note_brushes[lane_index].Get();
            ID2D1LinearGradientBrush* hold_material = d2d_->lane_native_hold_brushes[lane_index].Get();
            ID2D1Bitmap* note_head_bitmap = d2d_->lane_note_head_bitmaps[lane_index].Get();
            ID2D1Bitmap* note_hold_head_bitmap =
                d2d_->lane_note_hold_head_bitmaps[lane_index].Get();
            ID2D1Bitmap* note_hold_body_bitmap =
                d2d_->lane_note_hold_body_bitmaps[lane_index].Get();
            ID2D1Bitmap* note_hold_tail_bitmap =
                d2d_->lane_note_tail_bitmaps[lane_index].Get();

            if (note.mine) {
                const float mine_half_h = std::max(7.0f, head_half_h * 1.15f);
                const D2D1_RECT_F mine_rect = D2D1::RectF(x0, y - mine_half_h, x1, y + mine_half_h);
                if (note_fill) {
                    note_fill->SetColor(D2D1::ColorF(0.96f, 0.08f, 0.15f, visual_opacity));
                    ctx->FillRectangle(mine_rect, note_fill);
                }
                if (note_border) {
                    note_border->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.20f, visual_opacity));
                    ctx->DrawRectangle(mine_rect, note_border, 2.0f);
                    ctx->DrawLine(D2D1::Point2F(x0, y - mine_half_h),
                                  D2D1::Point2F(x1, y + mine_half_h), note_border, 2.0f);
                    ctx->DrawLine(D2D1::Point2F(x1, y - mine_half_h),
                                  D2D1::Point2F(x0, y + mine_half_h), note_border, 2.0f);
                }
                continue;
            }

            if (note.hold && note_hold_fill) {
                const float head_body_inset = gameplay_hold_body_cap_inset(note_shape, head_half_h);
                const float body_top = std::min(y, tail_y);
                const float body_bottom = std::max(y, tail_y) - (render_head ? head_body_inset : 0.0f);
                const float hold_half_width = std::max(4.0f, note_width * 0.5f * hold_body_width_scale);
                const D2D1_RECT_F hold_body =
                    D2D1::RectF(lane_center - hold_half_width, body_top, lane_center + hold_half_width, body_bottom);
                if (body_bottom > body_top) {
                    if (note_hold_body_bitmap && !data.gameplay.hold_tail_taper_enabled) {
                        const D2D1_RECT_F* hold_body_source_rect =
                            bitmap_source_rect_or_null(d2d_->lane_note_hold_body_source_rects[lane_index]);
                        ctx->DrawBitmap(note_hold_body_bitmap, hold_body, hold_body_opacity,
                                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                        hold_body_source_rect);
                    } else {
                        ID2D1Brush* hold_body_brush = configure_gameplay_material_brush(
                            hold_material, note_hold_fill, hold_body, native_hold_body_opacity, true);
                        draw_gameplay_hold_body(ctx,
                                                d2d_->d2d_factory.Get(),
                                                hold_body,
                                                lane_center,
                                                y,
                                                tail_y,
                                                hold_half_width,
                                                data.gameplay.hold_tail_taper_enabled,
                                                hold_body_brush);
                    }
                }
            }

            if (note.hold && data.gameplay.show_hold_tail) {
                const D2D1_RECT_F tail_rect =
                    D2D1::RectF(x0, tail_y - tail_half_h, x1, tail_y + tail_half_h);
                if (note_hold_tail_bitmap) {
                    const D2D1_RECT_F* tail_source_rect =
                        bitmap_source_rect_or_null(d2d_->lane_note_tail_source_rects[lane_index]);
                    const D2D1_RECT_F tail_bitmap_rect =
                        gameplay_note_bitmap_dest_rect(tail_rect,
                                                       note_hold_tail_bitmap,
                                                       tail_source_rect,
                                                       note_shape,
                                                       note_image_aspect);
                    draw_gameplay_sprite(ctx,
                                         note_hold_tail_bitmap,
                                         tail_bitmap_rect,
                                         visual_opacity,
                                         tail_source_rect,
                                         gameplay_lane_sprite_rotation(
                                             gameplay_note_sprite_cache_.note_rotations,
                                             gameplay_note_sprite_cache_.note_rotation_count,
                                             lane_index));
                } else if (note_fill) {
                    ID2D1Brush* tail_fill = configure_gameplay_material_brush(
                        note_material, note_fill, tail_rect, visual_opacity, false);
                    draw_note_primitive(ctx,
                                        tail_rect,
                                        tail_fill,
                                        nullptr,
                                        0.0f,
                                        note_shape,
                                        false,
                                        note_polygon_geometry);
                }
            }

            const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
            if (render_head) {
                ID2D1Bitmap* head_bitmap = note.hold && note_hold_head_bitmap ? note_hold_head_bitmap : note_head_bitmap;
                if (head_bitmap) {
                    const D2D1_RECT_F* head_source_rect =
                        note.hold
                            ? bitmap_source_rect_or_null(
                                  d2d_->lane_note_hold_head_source_rects[lane_index])
                            : bitmap_source_rect_or_null(
                                  d2d_->lane_note_head_source_rects[lane_index]);
                    const D2D1_RECT_F bitmap_rect =
                        gameplay_note_bitmap_dest_rect(note_rect,
                                                       head_bitmap,
                                                       head_source_rect,
                                                       note_shape,
                                                       note_image_aspect);
                    draw_gameplay_sprite(ctx, head_bitmap, bitmap_rect, visual_opacity,
                                         head_source_rect,
                                         gameplay_lane_sprite_rotation(
                                             gameplay_note_sprite_cache_.note_rotations,
                                             gameplay_note_sprite_cache_.note_rotation_count,
                                             lane_index));
                } else {
                    if (note_fill) {
                        ID2D1Brush* head_fill = configure_gameplay_material_brush(
                            note_material, note_fill, note_rect, visual_opacity, false);
                        draw_note_primitive(ctx, note_rect, head_fill, note_border, 0.85f,
                                            note_shape, note_border_enabled, note_polygon_geometry);
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
                const float saved_text_opacity = d2d_->text_brush->GetOpacity();
                D2D1_MATRIX_3X2_F saved_feedback_transform{};
                ctx->GetTransform(&saved_feedback_transform);
                const D2D1_POINT_2F feedback_animation_center = D2D1::Point2F(
                    (feedback_rect.left + feedback_rect.right) * 0.5f,
                    (feedback_rect.top + feedback_rect.bottom) * 0.5f + 12.0f);
                const D2D1_MATRIX_3X2_F feedback_animation_transform =
                    D2D1::Matrix3x2F::Scale(judgement_text_animation.scale,
                                            judgement_text_animation.scale,
                                            feedback_animation_center) *
                    D2D1::Matrix3x2F::Translation(0.0f, judgement_text_animation.offset_y) *
                    saved_feedback_transform;
                ctx->SetTransform(feedback_animation_transform);
                d2d_->text_brush->SetOpacity(saved_text_opacity * judgement_text_animation.opacity);

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
                if (!gameplay_hud_cache_.feedback_timing_text.empty() && d2d_->body_format) {
                    const D2D1_RECT_F timing_text_rect =
                        D2D1::RectF(feedback_rect.left,
                                    feedback_rect.top + 54.0f,
                                    feedback_rect.right,
                                    feedback_rect.bottom + 8.0f);
                    d2d_->text_brush->SetColor(
                        data.gameplay.feedback_delta_ms < 0.0
                            ? D2D1::ColorF(0x5DA9FF, 0.98f)
                            : D2D1::ColorF(0xFF5A6B, 0.98f));
                    draw_text_clipped_aligned(gameplay_hud_cache_.feedback_timing_text,
                                              d2d_->body_format.Get(),
                                              timing_text_rect,
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                d2d_->text_brush->SetColor(saved_text_color);
                d2d_->text_brush->SetOpacity(saved_text_opacity);
                ctx->SetTransform(saved_feedback_transform);
            }

            draw_timing_indicator(field_left,
                                  field_right,
                                  combo_anchor_y,
                                  data.gameplay.timing_history_delta_ms,
                                  data.gameplay.timing_history_count,
                                  data.gameplay.has_feedback,
                                  data.gameplay.feedback_delta_ms);
        }

        if (data.gameplay.combo > 0) {
            draw_combo_overlay(field_layout,
                               gameplay_hud_cache_.combo_value_text,
                               gameplay_hud_cache_.combo_label_text,
                               combo_anchor_top_safe,
                               combo_anchor_bottom_safe,
                               show_feedback_overlay ? 60.0f : 0.0f,
                               combo_text_animation);
        }

        draw_gameplay_header();
        draw_gameplay_progress_bar();
        if (data.gameplay.peer_visible) {
            const D2D1_RECT_F lead_track =
                D2D1::RectF(field_left + 80.0f, 186.0f, field_right - 80.0f, 202.0f);
            const float lead_center_x = (lead_track.left + lead_track.right) * 0.5f;
            const float lead_position = static_cast<float>(
                std::clamp(data.gameplay.versus_score_position, 0.0, 1.0));
            const float marker_x =
                lead_track.left + (lead_track.right - lead_track.left) * lead_position;

            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.88f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(lead_track, 8.0f, 8.0f),
                                          d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.32f);
                d2d_->accent_brush->SetColor(D2D1::ColorF(0xFF5A6B));
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(lead_track.left,
                                                 lead_track.top,
                                                 lead_center_x,
                                                 lead_track.bottom),
                                      8.0f,
                                      8.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x89D185));
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(lead_center_x,
                                                 lead_track.top,
                                                 lead_track.right,
                                                 lead_track.bottom),
                                      8.0f,
                                      8.0f),
                    d2d_->accent_brush.Get());

                const D2D1_COLOR_F marker_color =
                    lead_position < 0.5f
                        ? D2D1::ColorF(0xFF5A6B, 0.98f)
                        : (lead_position > 0.5f
                               ? D2D1::ColorF(0x89D185, 0.98f)
                               : D2D1::ColorF(0x6EE7F2, 0.98f));
                d2d_->accent_brush->SetColor(marker_color);
                d2d_->accent_brush->SetOpacity(1.0f);
                ctx->DrawLine(D2D1::Point2F(marker_x, lead_track.top - 17.0f),
                              D2D1::Point2F(marker_x, lead_track.top + 2.0f),
                              d2d_->accent_brush.Get(),
                              3.0f);
                ctx->DrawLine(D2D1::Point2F(marker_x, lead_track.top + 2.0f),
                              D2D1::Point2F(marker_x - 7.0f, lead_track.top - 6.0f),
                              d2d_->accent_brush.Get(),
                              3.0f);
                ctx->DrawLine(D2D1::Point2F(marker_x, lead_track.top + 2.0f),
                              D2D1::Point2F(marker_x + 7.0f, lead_track.top - 6.0f),
                              d2d_->accent_brush.Get(),
                              3.0f);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(lead_track, 8.0f, 8.0f),
                                          d2d_->button_border_brush.Get(),
                                          1.0f);
                ctx->DrawLine(D2D1::Point2F(lead_center_x, lead_track.top - 3.0f),
                              D2D1::Point2F(lead_center_x, lead_track.bottom + 3.0f),
                              d2d_->button_border_brush.Get(),
                              1.4f);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(D2D1::ColorF(0xFF5A6B, 0.96f));
                draw_text_clipped(L"LOSS",
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(lead_track.left,
                                              lead_track.top - 31.0f,
                                              lead_track.left + 120.0f,
                                              lead_track.top - 3.0f),
                                  d2d_->text_brush.Get());
                d2d_->text_brush->SetColor(D2D1::ColorF(0x89D185, 0.96f));
                draw_text_clipped_aligned(L"WIN",
                                          d2d_->body_format.Get(),
                                          D2D1::RectF(lead_track.right - 120.0f,
                                                      lead_track.top - 31.0f,
                                                      lead_track.right,
                                                      lead_track.top - 3.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.94f));
                draw_text_clipped_aligned(gameplay_hud_cache_.versus_score_difference_text,
                                          d2d_->body_format.Get(),
                                          D2D1::RectF(lead_center_x - 120.0f,
                                                      lead_track.top - 31.0f,
                                                      lead_center_x + 120.0f,
                                                      lead_track.top - 3.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->text_brush->SetColor(saved_text_color);
            }

            if (data.gameplay.spectating_peer && d2d_->panel_brush &&
                d2d_->header_format && d2d_->text_brush) {
                const D2D1_RECT_F spectator_rect =
                    D2D1::RectF(field_left + 80.0f, 216.0f, field_right - 80.0f, 266.0f);
                d2d_->panel_brush->SetOpacity(0.82f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(spectator_rect, 16.0f, 16.0f),
                                          d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
                draw_text_clipped_aligned(gameplay_hud_cache_.spectating_text,
                                          d2d_->header_format.Get(),
                                          spectator_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
        if (data.gameplay.peer_visible) {
            // The protocol exposes aggregate opponent state, not lane events.
            const D2D1_RECT_F peer_panel_rect = surface_layout.ghost_visible
                ? D2D1::RectF(820.0f, 260.0f, 1100.0f, 494.0f)
                : D2D1::RectF(84.0f, 210.0f, 420.0f, 444.0f);
            const D2D1_ROUNDED_RECT peer_panel =
                D2D1::RoundedRect(peer_panel_rect, 18.0f, 18.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.82f);
                ctx->FillRoundedRectangle(peer_panel, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(peer_panel, d2d_->button_border_brush.Get(), 1.2f);
            }

            D2D1_COLOR_F peer_state_color = D2D1::ColorF(0x6EE7F2, 0.92f);
            if (data.gameplay.peer_disconnected || data.gameplay.peer_game_over) {
                peer_state_color = D2D1::ColorF(0xFF5A6B, 0.94f);
            } else if (data.gameplay.peer_aborted) {
                peer_state_color = D2D1::ColorF(0xFFB703, 0.94f);
            } else if (data.gameplay.peer_finished) {
                peer_state_color = D2D1::ColorF(0x89D185, 0.94f);
            }
            if (d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetColor(peer_state_color);
                d2d_->accent_brush->SetOpacity(1.0f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(peer_panel_rect.left + 16.0f,
                                                 peer_panel_rect.top + 14.0f,
                                                 peer_panel_rect.left + 22.0f,
                                                 peer_panel_rect.top + 34.0f),
                                      3.0f,
                                      3.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            if (d2d_->hud_format && d2d_->muted_brush) {
                draw_text_clipped(wloc("OPPONENT", "\uC0C1\uB300"),
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(peer_panel_rect.left + 30.0f,
                                              peer_panel_rect.top + 10.0f,
                                              peer_panel_rect.right - 14.0f,
                                              peer_panel_rect.top + 38.0f),
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped(gameplay_hud_cache_.peer_name_text,
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(peer_panel_rect.left + 16.0f,
                                              peer_panel_rect.top + 38.0f,
                                              peer_panel_rect.right - 116.0f,
                                              peer_panel_rect.top + 70.0f),
                                  d2d_->text_brush.Get());
                const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(peer_state_color);
                draw_text_clipped_aligned(gameplay_hud_cache_.peer_status_text,
                                          d2d_->body_format.Get(),
                                          D2D1::RectF(peer_panel_rect.right - 112.0f,
                                                      peer_panel_rect.top + 38.0f,
                                                      peer_panel_rect.right - 16.0f,
                                                      peer_panel_rect.top + 70.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(saved_text_color);
            }
            if (d2d_->header_format && d2d_->text_brush) {
                draw_text_clipped(gameplay_hud_cache_.peer_score_text,
                                  d2d_->header_format.Get(),
                                  D2D1::RectF(peer_panel_rect.left + 16.0f,
                                              peer_panel_rect.top + 74.0f,
                                              peer_panel_rect.right - 16.0f,
                                              peer_panel_rect.top + 116.0f),
                                  d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped(gameplay_hud_cache_.peer_combo_text,
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(peer_panel_rect.left + 16.0f,
                                              peer_panel_rect.top + 116.0f,
                                              peer_panel_rect.right - 16.0f,
                                              peer_panel_rect.top + 148.0f),
                                  d2d_->text_brush.Get());
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                draw_text_clipped(gameplay_hud_cache_.peer_judge_stats_text,
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(peer_panel_rect.left + 16.0f,
                                              peer_panel_rect.top + 148.0f,
                                              peer_panel_rect.right - 16.0f,
                                              peer_panel_rect.top + 178.0f),
                                  d2d_->muted_brush.Get());
                draw_text_clipped(gameplay_hud_cache_.peer_gauge_text,
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(peer_panel_rect.left + 16.0f,
                                              peer_panel_rect.top + 180.0f,
                                              peer_panel_rect.right - 16.0f,
                                              peer_panel_rect.top + 208.0f),
                                  d2d_->muted_brush.Get());
            }

            const float peer_gauge_ratio =
                static_cast<float>(std::clamp(data.gameplay.peer_gauge / 100.0, 0.0, 1.0));
            const D2D1_RECT_F peer_gauge_track =
                D2D1::RectF(peer_panel_rect.left + 16.0f,
                            peer_panel_rect.bottom - 18.0f,
                            peer_panel_rect.right - 16.0f,
                            peer_panel_rect.bottom - 10.0f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.86f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(peer_gauge_track, 4.0f, 4.0f),
                                          d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush && peer_gauge_ratio > 0.0f) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetColor(peer_state_color);
                d2d_->accent_brush->SetOpacity(0.90f);
                const float fill_right = peer_gauge_track.left +
                    (peer_gauge_track.right - peer_gauge_track.left) * peer_gauge_ratio;
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(peer_gauge_track.left,
                                                 peer_gauge_track.top,
                                                 fill_right,
                                                 peer_gauge_track.bottom),
                                      4.0f,
                                      4.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
        }
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

        if (key_pulse_brightness > 0.0f && d2d_->note_fill_brush && data.gameplay.lane_activity_count > 0) {
            const std::size_t count =
                std::min(data.gameplay.lane_activity_count, static_cast<std::size_t>(lane_count));
            for (std::size_t lane = 0; lane < count; ++lane) {
                const float activity = std::clamp(data.gameplay.lane_activity[lane], 0.0f, 1.0f);
                const float pulse = activity * activity;
                if (pulse <= 0.02f) {
                    continue;
                }
                const float lane_center = gameplay_lane_center(field_layout, static_cast<int>(lane));
                const float note_width = gameplay_note_width(field_layout, static_cast<int>(lane));
                const float pulse_half_w = note_width * (0.50f + 0.09f * pulse);
                const float pulse_half_h = 5.0f + 8.0f * pulse;
                const float beam_half_h = 20.0f + 38.0f * pulse;
                const D2D1_RECT_F beam_rect =
                    D2D1::RectF(lane_center - pulse_half_w * 0.78f,
                                std::max(field_top + 2.0f, hit_line_y - beam_half_h),
                                lane_center + pulse_half_w * 0.78f,
                                std::min(field_bottom - 2.0f, hit_line_y + beam_half_h));
                const D2D1_RECT_F glow_rect =
                    D2D1::RectF(lane_center - pulse_half_w,
                                std::max(field_top + 2.0f, hit_line_y - pulse_half_h),
                                lane_center + pulse_half_w,
                                std::min(field_bottom - 2.0f, hit_line_y + pulse_half_h));
                uint32_t pulse_color = 0x6EE7F2;
                if (lane < data.gameplay.lane_color_count) {
                    pulse_color = data.gameplay.lane_colors[lane];
                }
                d2d_->note_fill_brush->SetColor(
                    color_from_rgb(blend_rgb(pulse_color, 0x6EE7F2, 0.38f),
                                   (0.025f + 0.085f * pulse) * visual_opacity * key_pulse_brightness));
                ctx->FillRoundedRectangle(D2D1::RoundedRect(beam_rect, 9.0f, 9.0f),
                                          d2d_->note_fill_brush.Get());
                d2d_->note_fill_brush->SetColor(
                    color_from_rgb(blend_rgb(pulse_color, 0xFFFFFF, 0.22f),
                                   (0.10f + 0.24f * pulse) * visual_opacity * key_pulse_brightness));
                ctx->FillRoundedRectangle(D2D1::RoundedRect(glow_rect, 6.0f, 6.0f),
                                          d2d_->note_fill_brush.Get());
            }
        }

        draw_vertical_gauge(surface_layout.player_gauge_left,
                            data.gameplay.gauge,
                            data.gameplay.gauge_label,
                            gameplay_hud_cache_.gauge_label_text,
                            gameplay_hud_cache_.gauge_value_text,
                            0);

        if (data.gameplay.game_over && !data.gameplay.spectating_peer && d2d_->panel_brush) {
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
            const bool ghost_has_gear_overlay =
                draw_imported_gear_overlay(ghost_field_layout, ghost_hit_line_y);
            if (use_imported_metrics) {
                draw_key_labels(ghost_field_layout);
            }
            ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            for (int lane = 0; lane < lane_count; ++lane) {
                const std::size_t lane_index = static_cast<std::size_t>(lane);
                if (ghost_has_gear_overlay && !tenriff_gear_overlay) {
                    continue;
                }
                const bool lane_is_pressed =
                    lane_index < data.gameplay.ghost_lane_pressed_count &&
                    data.gameplay.ghost_lane_pressed[lane_index] != 0;
                const float lane_activity = lane_index < data.gameplay.ghost_lane_activity_count
                    ? data.gameplay.ghost_lane_activity[lane_index]
                    : 0.0f;
                ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
                if (should_use_imported_pressed_key(lane_activity) &&
                    d2d_->lane_key_pressed_bitmaps[lane_index]) {
                    key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
                }
                if (!key_bitmap && !use_imported_metrics) {
                    draw_native_digital_key(ghost_field_layout,
                                            ghost_hit_line_y,
                                            lane,
                                            lane_is_pressed,
                                            lane_activity);
                    continue;
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
                draw_gameplay_sprite(ctx, key_bitmap, receptor_rect, visual_opacity,
                                     key_source_rect,
                                     gameplay_lane_sprite_rotation(
                                         gameplay_note_sprite_cache_.key_rotations,
                                         gameplay_note_sprite_cache_.key_rotation_count,
                                         lane_index));
            }
            if (!use_imported_metrics) {
                draw_key_labels(ghost_field_layout);
            }
            for (std::size_t note_index = 0; note_index < data.gameplay.ghost_note_count; ++note_index) {
                const auto& note = data.gameplay.ghost_notes[note_index];
                if (!should_render_gameplay_note(
                        note.start_sample,
                        note.tail_sample,
                        note.hold,
                        note.head_visible,
                        note.pending,
                        data.gameplay.current_sample,
                        display_sample,
                        hold_handoff_grace_samples)) {
                    continue;
                }
                const bool render_head =
                    should_render_gameplay_note_head(note.start_sample, note.head_visible, display_sample);
                const int lane = std::clamp(note.lane, 1, lane_count);
                const std::size_t lane_index = static_cast<std::size_t>(lane - 1);
                const float lane_center = gameplay_lane_center(ghost_field_layout, lane - 1);
                const float ghost_note_width = gameplay_note_width(ghost_field_layout, lane - 1);
                const float x0 = lane_center - ghost_note_width * 0.5f;
                const float x1 = lane_center + ghost_note_width * 0.5f;
                const double render_visual_position =
                    gameplay_note_anchors_to_judgement_line(note.hold, note.head_visible)
                        ? display_visual_position
                        : note.visual_position;
                const float y =
                    gameplay_field_y(ghost_field_top, ghost_field_height, visual_to_y(render_visual_position));
                const float tail_y = gameplay_field_y(
                    ghost_field_top, ghost_field_height, visual_to_y(note.tail_visual_position));
                const float head_half_h = gameplay_note_head_half_height(note_height_scale);
                const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
                uint32_t lane_color = 0xF6F8FF;
                if (lane_index < data.gameplay.lane_color_count) {
                    lane_color = data.gameplay.lane_colors[lane_index];
                } else if (!gameplay_lane_uses_white_note(lane)) {
                    lane_color = 0x4F80FF;
                }
                ID2D1SolidColorBrush* note_fill = d2d_->note_fill_brush.Get();
                ID2D1SolidColorBrush* note_border = d2d_->note_border_brush.Get();
                ID2D1SolidColorBrush* note_hold_fill = d2d_->note_hold_brush.Get();
                if (note_fill) {
                    note_fill->SetColor(gameplay_note_fill_color(lane_color, visual_opacity));
                }
                if (note_border) {
                    note_border->SetColor(gameplay_note_border_color(lane_color, note_outline_opacity));
                }
                if (note_hold_fill) {
                    note_hold_fill->SetColor(gameplay_note_hold_color(lane_color, native_hold_body_opacity));
                }
                ID2D1LinearGradientBrush* note_material = d2d_->lane_native_note_brushes[lane_index].Get();
                ID2D1LinearGradientBrush* hold_material = d2d_->lane_native_hold_brushes[lane_index].Get();
                ID2D1Bitmap* note_head_bitmap = d2d_->lane_note_head_bitmaps[lane_index].Get();
                ID2D1Bitmap* note_hold_head_bitmap =
                    d2d_->lane_note_hold_head_bitmaps[lane_index].Get();
                ID2D1Bitmap* note_hold_body_bitmap =
                    d2d_->lane_note_hold_body_bitmaps[lane_index].Get();
                ID2D1Bitmap* note_hold_tail_bitmap =
                    d2d_->lane_note_tail_bitmaps[lane_index].Get();

                if (note.mine) {
                    const float mine_half_h = std::max(7.0f, head_half_h * 1.15f);
                    const D2D1_RECT_F mine_rect = D2D1::RectF(x0, y - mine_half_h, x1, y + mine_half_h);
                    if (note_fill) {
                        note_fill->SetColor(D2D1::ColorF(0.96f, 0.08f, 0.15f, visual_opacity));
                        ctx->FillRectangle(mine_rect, note_fill);
                    }
                    if (note_border) {
                        note_border->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.20f, visual_opacity));
                        ctx->DrawRectangle(mine_rect, note_border, 2.0f);
                        ctx->DrawLine(D2D1::Point2F(x0, y - mine_half_h),
                                      D2D1::Point2F(x1, y + mine_half_h), note_border, 2.0f);
                        ctx->DrawLine(D2D1::Point2F(x1, y - mine_half_h),
                                      D2D1::Point2F(x0, y + mine_half_h), note_border, 2.0f);
                    }
                    continue;
                }

                if (note.hold && note_hold_fill) {
                    const float head_body_inset =
                        gameplay_hold_body_cap_inset(note_shape, gameplay_note_head_half_height(note_height_scale));
                    const float body_top = std::min(y, tail_y);
                    const float body_bottom = std::max(y, tail_y) - (render_head ? head_body_inset : 0.0f);
                    const float hold_half_width = std::max(4.0f, ghost_note_width * 0.5f * hold_body_width_scale);
                    const D2D1_RECT_F hold_body =
                        D2D1::RectF(lane_center - hold_half_width, body_top, lane_center + hold_half_width, body_bottom);
                    if (body_bottom > body_top) {
                        if (note_hold_body_bitmap && !data.gameplay.hold_tail_taper_enabled) {
                            const D2D1_RECT_F* hold_body_source_rect =
                                bitmap_source_rect_or_null(d2d_->lane_note_hold_body_source_rects[lane_index]);
                            ctx->DrawBitmap(note_hold_body_bitmap, hold_body, hold_body_opacity,
                                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                            hold_body_source_rect);
                        } else {
                            ID2D1Brush* hold_body_brush = configure_gameplay_material_brush(
                                hold_material, note_hold_fill, hold_body, native_hold_body_opacity, true);
                            draw_gameplay_hold_body(ctx,
                                                    d2d_->d2d_factory.Get(),
                                                    hold_body,
                                                    lane_center,
                                                    y,
                                                    tail_y,
                                                    hold_half_width,
                                                    data.gameplay.hold_tail_taper_enabled,
                                                    hold_body_brush);
                        }
                    }
                }

                if (note.hold && data.gameplay.show_hold_tail) {
                    const D2D1_RECT_F tail_rect =
                        D2D1::RectF(x0, tail_y - tail_half_h, x1, tail_y + tail_half_h);
                    if (note_hold_tail_bitmap) {
                        const D2D1_RECT_F* tail_source_rect =
                            bitmap_source_rect_or_null(d2d_->lane_note_tail_source_rects[lane_index]);
                        const D2D1_RECT_F tail_bitmap_rect =
                            gameplay_note_bitmap_dest_rect(tail_rect,
                                                           note_hold_tail_bitmap,
                                                           tail_source_rect,
                                                           note_shape,
                                                           note_image_aspect);
                        draw_gameplay_sprite(ctx,
                                             note_hold_tail_bitmap,
                                             tail_bitmap_rect,
                                             visual_opacity,
                                             tail_source_rect,
                                             gameplay_lane_sprite_rotation(
                                                 gameplay_note_sprite_cache_.note_rotations,
                                                 gameplay_note_sprite_cache_.note_rotation_count,
                                                 lane_index));
                    } else if (note_fill) {
                        ID2D1Brush* tail_fill = configure_gameplay_material_brush(
                            note_material, note_fill, tail_rect, visual_opacity, false);
                        draw_note_primitive(ctx,
                                            tail_rect,
                                            tail_fill,
                                            nullptr,
                                            0.0f,
                                            note_shape,
                                            false,
                                            note_polygon_geometry);
                    }
                }

                const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
                if (render_head) {
                    ID2D1Bitmap* head_bitmap = note.hold && note_hold_head_bitmap ? note_hold_head_bitmap : note_head_bitmap;
                    if (head_bitmap) {
                        const D2D1_RECT_F* head_source_rect =
                            note.hold
                                ? bitmap_source_rect_or_null(
                                      d2d_->lane_note_hold_head_source_rects[lane_index])
                                : bitmap_source_rect_or_null(
                                      d2d_->lane_note_head_source_rects[lane_index]);
                        const D2D1_RECT_F bitmap_rect =
                            gameplay_note_bitmap_dest_rect(note_rect,
                                                           head_bitmap,
                                                           head_source_rect,
                                                           note_shape,
                                                           note_image_aspect);
                        draw_gameplay_sprite(ctx, head_bitmap, bitmap_rect, visual_opacity,
                                             head_source_rect,
                                             gameplay_lane_sprite_rotation(
                                                 gameplay_note_sprite_cache_.note_rotations,
                                                 gameplay_note_sprite_cache_.note_rotation_count,
                                                 lane_index));
                    } else if (note_fill) {
                        ID2D1Brush* head_fill = configure_gameplay_material_brush(
                            note_material, note_fill, note_rect, visual_opacity, false);
                        draw_note_primitive(ctx, note_rect, head_fill, note_border, 0.85f,
                                            note_shape, note_border_enabled, note_polygon_geometry);
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
                    if (!gameplay_hud_cache_.ghost_feedback_timing_text.empty() && d2d_->body_format) {
                        const D2D1_RECT_F timing_text_rect =
                            D2D1::RectF(feedback_rect.left,
                                        feedback_rect.top + 54.0f,
                                        feedback_rect.right,
                                        feedback_rect.bottom + 8.0f);
                        d2d_->text_brush->SetColor(
                            data.gameplay.ghost_feedback_delta_ms < 0.0
                                ? D2D1::ColorF(0x5DA9FF, 0.98f)
                                : D2D1::ColorF(0xFF5A6B, 0.98f));
                        draw_text_clipped_aligned(gameplay_hud_cache_.ghost_feedback_timing_text,
                                                  d2d_->body_format.Get(),
                                                  timing_text_rect,
                                                  d2d_->text_brush.Get(),
                                                  DWRITE_TEXT_ALIGNMENT_CENTER);
                    }
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
            if (data.gameplay.ghost_combo > 0) {
                draw_combo_overlay(ghost_field_layout,
                                   gameplay_hud_cache_.ghost_combo_value_text,
                                   gameplay_hud_cache_.combo_label_text,
                                   ghost_combo_anchor_top_safe,
                                   ghost_combo_anchor_bottom_safe,
                                   show_ghost_feedback_overlay ? 60.0f : 0.0f,
                                   GameplayTextPopAnimation{});
            }

            if (key_pulse_brightness > 0.0f && d2d_->note_fill_brush && data.gameplay.ghost_lane_activity_count > 0) {
                const std::size_t count =
                    std::min(data.gameplay.ghost_lane_activity_count, static_cast<std::size_t>(lane_count));
                for (std::size_t lane = 0; lane < count; ++lane) {
                    const float activity = std::clamp(data.gameplay.ghost_lane_activity[lane], 0.0f, 1.0f);
                    const float pulse = activity * activity;
                    if (pulse <= 0.02f) {
                        continue;
                    }
                    const float lane_center = gameplay_lane_center(ghost_field_layout, static_cast<int>(lane));
                    const float ghost_note_width = gameplay_note_width(ghost_field_layout, static_cast<int>(lane));
                    const float pulse_half_w = ghost_note_width * (0.50f + 0.09f * pulse);
                    const float pulse_half_h = 5.0f + 8.0f * pulse;
                    const float beam_half_h = 20.0f + 38.0f * pulse;
                    const D2D1_RECT_F beam_rect =
                        D2D1::RectF(lane_center - pulse_half_w * 0.78f,
                                    std::max(ghost_field_top + 2.0f, ghost_hit_line_y - beam_half_h),
                                    lane_center + pulse_half_w * 0.78f,
                                    std::min(ghost_field_bottom - 2.0f, ghost_hit_line_y + beam_half_h));
                    const D2D1_RECT_F glow_rect =
                        D2D1::RectF(lane_center - pulse_half_w,
                                    std::max(ghost_field_top + 2.0f, ghost_hit_line_y - pulse_half_h),
                                    lane_center + pulse_half_w,
                                    std::min(ghost_field_bottom - 2.0f, ghost_hit_line_y + pulse_half_h));
                    uint32_t pulse_color = 0x6EE7F2;
                    if (lane < data.gameplay.lane_color_count) {
                        pulse_color = data.gameplay.lane_colors[lane];
                    }
                    d2d_->note_fill_brush->SetColor(
                        color_from_rgb(blend_rgb(pulse_color, 0x6EE7F2, 0.38f),
                                       (0.025f + 0.085f * pulse) * visual_opacity * key_pulse_brightness));
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(beam_rect, 9.0f, 9.0f),
                                              d2d_->note_fill_brush.Get());
                    d2d_->note_fill_brush->SetColor(
                        color_from_rgb(blend_rgb(pulse_color, 0xFFFFFF, 0.22f),
                                       (0.10f + 0.24f * pulse) * visual_opacity * key_pulse_brightness));
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(glow_rect, 6.0f, 6.0f),
                                              d2d_->note_fill_brush.Get());
                }
            }

            draw_vertical_gauge(surface_layout.ghost_gauge_left,
                                data.gameplay.ghost_gauge,
                                data.gameplay.ghost_gauge_label,
                                gameplay_hud_cache_.ghost_gauge_label_text,
                                gameplay_hud_cache_.ghost_gauge_value_text,
                                1);

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
        if (data.gameplay.paused && d2d_->panel_brush && d2d_->card_brush &&
            d2d_->text_brush && d2d_->header_format && d2d_->body_format) {
            const D2D1_RECT_F screen_overlay = D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight);
            const D2D1_RECT_F pause_panel = D2D1::RectF(650.0f, 210.0f, 1270.0f, 870.0f);
            const float saved_panel_opacity = d2d_->panel_brush->GetOpacity();
            const float saved_card_opacity = d2d_->card_brush->GetOpacity();
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const float saved_text_opacity = d2d_->text_brush->GetOpacity();

            d2d_->panel_brush->SetOpacity(0.76f);
            ctx->FillRectangle(screen_overlay, d2d_->panel_brush.Get());
            d2d_->card_brush->SetOpacity(0.98f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(pause_panel, 28.0f, 28.0f), d2d_->card_brush.Get());
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(pause_panel, 28.0f, 28.0f),
                                          d2d_->button_border_brush.Get(),
                                          2.0f);
            }

            d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 1.0f));
            draw_text_clipped_aligned(L"PAUSED  /  \uC77C\uC2DC\uC815\uC9C0",
                                      d2d_->header_format.Get(),
                                      D2D1::RectF(pause_panel.left + 40.0f,
                                                  pause_panel.top + 42.0f,
                                                  pause_panel.right - 40.0f,
                                                  pause_panel.top + 118.0f),
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);

            constexpr std::array<const wchar_t*, 3> kPauseLabels{
                L"\uACC4\uC18D\uD558\uAE30  /  CONTINUE",
                L"\uC7AC\uC2DC\uC791  /  RESTART",
                L"\uB098\uAC00\uAE30  /  EXIT",
            };
            const int selected_pause_row = std::clamp(data.gameplay.pause_menu_cursor, 0, 2);
            for (int row = 0; row < static_cast<int>(kPauseLabels.size()); ++row) {
                const float top = pause_panel.top + 160.0f + static_cast<float>(row) * 118.0f;
                const D2D1_RECT_F row_rect =
                    D2D1::RectF(pause_panel.left + 72.0f,
                                top,
                                pause_panel.right - 72.0f,
                                top + 86.0f);
                const bool selected = row == selected_pause_row;
                if (selected && d2d_->accent_brush) {
                    const float saved_accent_opacity = d2d_->accent_brush->GetOpacity();
                    d2d_->accent_brush->SetOpacity(0.28f);
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(row_rect, 14.0f, 14.0f),
                                              d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetOpacity(saved_accent_opacity);
                }
                if (d2d_->button_border_brush) {
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(row_rect, 14.0f, 14.0f),
                                              d2d_->button_border_brush.Get(),
                                              selected ? 2.0f : 1.0f);
                }
                d2d_->text_brush->SetColor(
                    selected ? D2D1::ColorF(0xFFFFFF, 1.0f) : D2D1::ColorF(0xAAB7C4, 0.92f));
                draw_text_clipped_aligned(kPauseLabels[static_cast<std::size_t>(row)],
                                          d2d_->body_format.Get(),
                                          row_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            d2d_->text_brush->SetColor(D2D1::ColorF(0x94A3B8, 0.92f));
            draw_text_clipped_aligned(L"\u2191\u2193 \uC120\uD0DD   ENTER \uD655\uC778   ESC \uACC4\uC18D",
                                      d2d_->body_format.Get(),
                                      D2D1::RectF(pause_panel.left + 30.0f,
                                                  pause_panel.bottom - 82.0f,
                                                  pause_panel.right - 30.0f,
                                                  pause_panel.bottom - 30.0f),
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);

            d2d_->panel_brush->SetOpacity(saved_panel_opacity);
            d2d_->card_brush->SetOpacity(saved_card_opacity);
            d2d_->text_brush->SetColor(saved_text_color);
            d2d_->text_brush->SetOpacity(saved_text_opacity);
        }

        if (gameplay_field_drag_state_.visible && d2d_->card_brush &&
            d2d_->text_brush && d2d_->body_format) {
            const D2D1_RECT_F handle_rect = D2D1::RectF(
                gameplay_field_drag_state_.left,
                gameplay_field_drag_state_.top,
                gameplay_field_drag_state_.right,
                gameplay_field_drag_state_.bottom);
            const D2D1_ROUNDED_RECT handle = D2D1::RoundedRect(handle_rect, 11.0f, 11.0f);
            const bool highlighted = gameplay_field_drag_state_.hovered ||
                                     gameplay_field_drag_state_.active;
            const float saved_card_opacity = d2d_->card_brush->GetOpacity();
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const float saved_text_opacity = d2d_->text_brush->GetOpacity();

            d2d_->card_brush->SetOpacity(highlighted ? 0.98f : 0.84f);
            ctx->FillRoundedRectangle(handle, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved_card_opacity);
            if (d2d_->accent_brush) {
                const float saved_accent_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(highlighted ? 1.0f : 0.72f);
                ctx->DrawRoundedRectangle(handle,
                                          d2d_->accent_brush.Get(),
                                          highlighted ? 2.4f : 1.6f);
                d2d_->accent_brush->SetOpacity(saved_accent_opacity);
            }
            d2d_->text_brush->SetColor(D2D1::ColorF(0xFFFFFF, 1.0f));
            d2d_->text_brush->SetOpacity(1.0f);
            draw_text_clipped_aligned(L"\u2194",
                                      d2d_->body_format.Get(),
                                      handle_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->text_brush->SetColor(saved_text_color);
            d2d_->text_brush->SetOpacity(saved_text_opacity);
        }
