        auto draw_panel = [&](const D2D1_RECT_F& rect, bool accent_border = false) {
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
            if (d2d_->panel_brush) {
                ctx->FillRoundedRectangle(rr, d2d_->panel_brush.Get());
            }
            if (accent_border && d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->accent_brush.Get(), 1.8f);
            } else if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), 1.4f);
            }
        };

        const auto localized_result_status = [&]() {
            if (data.result.status == "GAME OVER") {
                return loc("GAME OVER", "게임 오버");
            }
            if (data.result.status == "FAILED") {
                return loc("FAILED", "실패");
            }
            if (data.result.status == "ABORTED") {
                return loc("ABORTED", "중도 종료");
            }
            if (data.result.status == "CLEAR") {
                return loc("CLEAR", "클리어");
            }
            if (data.result.status == "HARD CLEAR") {
                return loc("HARD CLEAR", "하드 클리어");
            }
            if (data.result.status == "EASY CLEAR") {
                return loc("EASY CLEAR", "이지 클리어");
            }
            return data.result.status;
        };
        const auto localized_result_gauge = [&]() {
            if (!ui_korean) {
                return data.result.gauge_label;
            }
            if (data.result.gauge_label == "HARD") {
                return std::string("하드");
            }
            if (data.result.gauge_label == "EASY") {
                return std::string("이지");
            }
            return std::string("노말");
        };

        const std::wstring header_w = wloc("RESULT", "결과");
        const D2D1_RECT_F header_rect = D2D1::RectF(100.0f, 60.0f, 700.0f, 150.0f);
        ID2D1Brush* header_brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                    : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
        if (d2d_->logo_brush) {
            set_brush_points(d2d_->logo_brush.Get(), header_rect);
        }
        if (d2d_->header_format && header_brush) {
            draw_text_clipped(header_w, d2d_->header_format.Get(), header_rect, header_brush);
        }

        const D2D1_RECT_F summary_rect = D2D1::RectF(100.0f, 180.0f, 730.0f, 930.0f);
        const D2D1_RECT_F gauge_rect =
            D2D1::RectF(770.0f, 180.0f, std::min(1820.0f, performance_overlay_safe_left(24.0f)), 500.0f);
        const D2D1_RECT_F breakdown_rect = D2D1::RectF(770.0f, 530.0f, 1285.0f, 930.0f);
        const D2D1_RECT_F detail_rect = D2D1::RectF(1315.0f, 530.0f, 1820.0f, 930.0f);
        draw_panel(summary_rect, true);
        draw_panel(gauge_rect, true);
        draw_panel(breakdown_rect);
        draw_panel(detail_rect);

        const std::wstring title_w = to_wide(data.result.title.empty() ? loc("Unknown Chart", "알 수 없는 차트") : data.result.title);
        const std::wstring artist_w = to_wide(data.result.artist.empty() ? loc("Unknown Artist", "알 수 없는 아티스트") : data.result.artist);
        if (d2d_->song_title_format && d2d_->text_brush) {
            const D2D1_RECT_F title_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 28.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 90.0f);
            draw_text_clipped(title_w, d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
        }
        if (d2d_->body_format && d2d_->muted_brush) {
            const D2D1_RECT_F artist_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 92.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 124.0f);
            draw_text_clipped(artist_w, d2d_->body_format.Get(), artist_rect, d2d_->muted_brush.Get());
        }

        if (d2d_->rank_format && d2d_->accent_brush) {
            const std::wstring rank_w = to_wide(data.result.rank.empty() ? std::string("--") : data.result.rank);
            const D2D1_RECT_F rank_rect =
                D2D1::RectF(summary_rect.left + 20.0f, summary_rect.top + 120.0f, summary_rect.right - 20.0f,
                            summary_rect.top + 280.0f);
            draw_text_clipped(rank_w, d2d_->rank_format.Get(), rank_rect, d2d_->accent_brush.Get());
        }

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring score_w = to_wide(format_int_with_commas(data.result.score));
            const D2D1_RECT_F score_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 290.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 350.0f);
            draw_text_clipped_aligned(score_w,
                                      d2d_->title_format.Get(),
                                      score_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        if (d2d_->hud_format && d2d_->muted_brush) {
            const std::wstring status_w = to_wide(localized_result_status() + "  /  " + loc("SCORE", "점수"));
            const D2D1_RECT_F status_rect =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.top + 352.0f, summary_rect.right - 28.0f,
                            summary_rect.top + 388.0f);
            draw_text_clipped_aligned(status_w,
                                      d2d_->hud_format.Get(),
                                      status_rect,
                                      d2d_->muted_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        const float meter_left = summary_rect.left + 42.0f;
        const float meter_right = summary_rect.right - 42.0f;
        auto draw_meter = [&](float top, const std::string& label, double value, double max_value,
                              const D2D1_COLOR_F& color) {
            const float height = 18.0f;
            const D2D1_RECT_F frame = D2D1::RectF(meter_left, top, meter_right, top + height);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.60f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(frame, 9.0f, 9.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(frame, 9.0f, 9.0f), d2d_->button_border_brush.Get(), 1.0f);
            }

            const float ratio = static_cast<float>(std::clamp(max_value > 0.0 ? value / max_value : 0.0, 0.0, 1.0));
            if (d2d_->accent_brush && ratio > 0.0f) {
                const auto saved = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(color);
                const D2D1_RECT_F fill =
                    D2D1::RectF(frame.left + 3.0f, frame.top + 3.0f,
                                frame.left + 3.0f + (frame.right - frame.left - 6.0f) * ratio, frame.bottom - 3.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 7.0f, 7.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved);
            }

            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(label);
                const std::wstring value_w = to_wide(format_decimal(value) + (max_value <= 1.0 ? std::string("") : "%"));
                const D2D1_RECT_F label_rect = D2D1::RectF(frame.left, frame.top - 34.0f, frame.left + 220.0f, frame.top - 4.0f);
                const D2D1_RECT_F value_rect = D2D1::RectF(frame.right - 180.0f, frame.top - 34.0f, frame.right, frame.top - 4.0f);
                draw_text_clipped(label_w, d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
                draw_text_clipped_aligned(value_w,
                                          d2d_->body_format.Get(),
                                          value_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
        };

        draw_meter(summary_rect.top + 445.0f, loc("Accuracy", "정확도"), data.result.accuracy, 100.0, D2D1::ColorF(0x6EE7F2));
        const bool result_cleared = data.result.status.find("CLEAR") != std::string::npos;
        draw_meter(summary_rect.top + 530.0f, loc("Gauge ", "게이지 ") + localized_result_gauge(), data.result.gauge_value, 100.0,
                   result_cleared ? D2D1::ColorF(0xFABB4B) : D2D1::ColorF(0xFF6B6B));

        auto draw_panel_row = [&](const D2D1_RECT_F& panel_rect, float y, std::string_view label, std::string_view value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(std::string(value));
            const D2D1_RECT_F label_rect =
                D2D1::RectF(panel_rect.left + 42.0f, y, panel_rect.left + 290.0f, y + 32.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(panel_rect.right - 240.0f, y, panel_rect.right - 42.0f, y + 32.0f);
            ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                          d2d_->stats_label_format.Get(), label_rect, d2d_->text_brush.Get());
            ctx->DrawText(value_w.c_str(), static_cast<UINT32>(value_w.size()),
                          d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
        };

        const float summary_stats_top = summary_rect.top + 544.0f;
        const float summary_stats_step = 36.0f;
        draw_panel_row(summary_rect, summary_stats_top + summary_stats_step * 0.0f, loc("Max Combo", "최대 콤보"),
                       std::to_string(data.result.max_combo));
        draw_panel_row(summary_rect, summary_stats_top + summary_stats_step * 1.0f, loc("Judged", "판정 수"),
                       std::to_string(data.result.judged_notes));
        draw_panel_row(summary_rect, summary_stats_top + summary_stats_step * 2.0f, loc("Total Notes", "전체 노트"),
                       std::to_string(data.result.total_notes));
        draw_panel_row(summary_rect, summary_stats_top + summary_stats_step * 3.0f, loc("Gauge Shifts", "게이지 전환"),
                       std::to_string(data.result.shift_count));

        if (d2d_->body_format && d2d_->muted_brush) {
            const std::wstring detail_w =
                to_wide(data.result.replay_available
                            ? loc("LEFT / Restart   F1 / Replay   ENTER / ESC / Return",
                                  "LEFT / 재시작   F1 / 리플레이   ENTER / ESC / 돌아가기")
                            : loc("LEFT / Restart   ENTER / ESC / Return",
                                  "LEFT / 재시작   ENTER / ESC / 돌아가기"));
            const D2D1_RECT_F detail_rect_hint =
                D2D1::RectF(summary_rect.left + 28.0f, summary_rect.bottom - 48.0f, summary_rect.right - 28.0f,
                            summary_rect.bottom - 18.0f);
            draw_text_clipped_aligned(detail_w,
                                      d2d_->body_format.Get(),
                                      detail_rect_hint,
                                      d2d_->muted_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring gauge_title_w = wloc("Gauge Trace", "게이지 추적");
            const D2D1_RECT_F gauge_title_rect =
                D2D1::RectF(gauge_rect.left + 28.0f, gauge_rect.top + 20.0f, gauge_rect.right - 28.0f, gauge_rect.top + 60.0f);
            draw_text_clipped(gauge_title_w, d2d_->title_format.Get(), gauge_title_rect, d2d_->text_brush.Get());
        }

        const D2D1_RECT_F plot_rect = D2D1::RectF(gauge_rect.left + 34.0f, gauge_rect.top + 86.0f,
                                                  gauge_rect.right - 34.0f, gauge_rect.bottom - 46.0f);
        if (d2d_->card_brush) {
            d2d_->card_brush->SetOpacity(0.42f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(plot_rect, 14.0f, 14.0f), d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(plot_rect, 14.0f, 14.0f), d2d_->button_border_brush.Get(), 1.0f);
        }

        const auto gauge_plot_y = [&](float value) {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return plot_rect.top + clamped * (plot_rect.bottom - plot_rect.top);
        };

        if (d2d_->muted_brush && d2d_->hud_format) {
            for (int line = 0; line <= 4; ++line) {
                const float t = static_cast<float>(line) / 4.0f;
                const float y = plot_rect.top + t * (plot_rect.bottom - plot_rect.top);
                d2d_->muted_brush->SetOpacity(line == 0 ? 0.50f : 0.22f);
                ctx->DrawLine(D2D1::Point2F(plot_rect.left + 6.0f, y), D2D1::Point2F(plot_rect.right - 6.0f, y),
                              d2d_->muted_brush.Get(), line == 0 ? 1.2f : 0.8f);
                d2d_->muted_brush->SetOpacity(1.0f);
                const std::wstring label_w = to_wide(std::to_string(line * 25) + "%");
                const D2D1_RECT_F label_rect = D2D1::RectF(plot_rect.left + 10.0f, y - 16.0f, plot_rect.left + 80.0f, y + 12.0f);
                draw_text_clipped(label_w, d2d_->hud_format.Get(), label_rect, d2d_->muted_brush.Get());
            }
        }

        if (!data.result.gauge_shifts.empty() && d2d_->accent_brush && d2d_->hud_format) {
            const auto saved = d2d_->accent_brush->GetColor();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0xFAE36E));
            for (const auto& shift : data.result.gauge_shifts) {
                const float x = plot_rect.left + shift.position * (plot_rect.right - plot_rect.left);
                d2d_->accent_brush->SetOpacity(0.50f);
                ctx->DrawLine(D2D1::Point2F(x, plot_rect.top + 8.0f), D2D1::Point2F(x, plot_rect.bottom - 8.0f),
                              d2d_->accent_brush.Get(), 1.2f);
                d2d_->accent_brush->SetOpacity(1.0f);
                const std::wstring shift_w = to_wide(shift.label);
                const D2D1_RECT_F shift_rect = D2D1::RectF(x - 36.0f, plot_rect.top + 8.0f, x + 36.0f, plot_rect.top + 30.0f);
                draw_text_clipped_aligned(shift_w,
                                          d2d_->hud_format.Get(),
                                          shift_rect,
                                          d2d_->accent_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            d2d_->accent_brush->SetColor(saved);
        }

        if (data.result.gauge_points.size() >= 2 && d2d_->accent_brush) {
            const auto saved = d2d_->accent_brush->GetColor();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            for (std::size_t i = 1; i < data.result.gauge_points.size(); ++i) {
                const auto& prev = data.result.gauge_points[i - 1];
                const auto& next = data.result.gauge_points[i];
                const D2D1_POINT_2F p0 = D2D1::Point2F(plot_rect.left + prev.position * (plot_rect.right - plot_rect.left),
                                                       gauge_plot_y(prev.value));
                const D2D1_POINT_2F p1 = D2D1::Point2F(plot_rect.left + next.position * (plot_rect.right - plot_rect.left),
                                                       gauge_plot_y(next.value));
                ctx->DrawLine(p0, p1, d2d_->accent_brush.Get(), 3.2f);
            }
            const auto& tail = data.result.gauge_points.back();
            const D2D1_ELLIPSE tail_dot = D2D1::Ellipse(
                D2D1::Point2F(plot_rect.left + tail.position * (plot_rect.right - plot_rect.left),
                              gauge_plot_y(tail.value)),
                5.5f, 5.5f);
            ctx->FillEllipse(tail_dot, d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(saved);
        } else if (d2d_->body_format && d2d_->muted_brush) {
            const std::wstring empty_w = wloc("No gauge history captured.", "기록된 게이지 히스토리가 없습니다.");
            const D2D1_RECT_F empty_rect = D2D1::RectF(plot_rect.left, plot_rect.top + 80.0f, plot_rect.right, plot_rect.top + 120.0f);
            draw_text_clipped_aligned(empty_w,
                                      d2d_->body_format.Get(),
                                      empty_rect,
                                      d2d_->muted_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        if (d2d_->body_format && d2d_->text_brush) {
            const std::wstring gauge_summary_w = to_wide(
                localized_result_gauge() + "  " + format_decimal(data.result.gauge_value) + "%");
            const D2D1_RECT_F gauge_summary_rect =
                D2D1::RectF(gauge_rect.right - 280.0f, gauge_rect.top + 22.0f, gauge_rect.right - 24.0f, gauge_rect.top + 58.0f);
            draw_text_clipped_aligned(gauge_summary_w,
                                      d2d_->body_format.Get(),
                                      gauge_summary_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_TRAILING);
        }

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring mix_title_w = wloc("Judgement Mix", "판정 분포");
            const D2D1_RECT_F mix_title_rect =
                D2D1::RectF(breakdown_rect.left + 24.0f, breakdown_rect.top + 18.0f, breakdown_rect.right - 24.0f,
                            breakdown_rect.top + 56.0f);
            draw_text_clipped(mix_title_w, d2d_->title_format.Get(), mix_title_rect, d2d_->text_brush.Get());
        }

        struct JudgeBar {
            const char* label;
            int count;
            D2D1_COLOR_F color;
        };
        const JudgeBar bars[] = {
            {"PG", data.result.perfect, D2D1::ColorF(0x5EE5A7)},
            {"GR", data.result.great, D2D1::ColorF(0x6EE7F2)},
            {"G", data.result.good, D2D1::ColorF(0xFAE36E)},
            {"BAD", data.result.bad, D2D1::ColorF(0xFF9F43)},
            {"POOR", data.result.poor, D2D1::ColorF(0xFF6B6B)},
        };

        const int max_bar_count = std::max({1, data.result.perfect, data.result.great, data.result.good,
                                            data.result.bad, data.result.poor});
        float bar_y = breakdown_rect.top + 82.0f;
        for (const auto& bar : bars) {
            const D2D1_RECT_F label_rect =
                D2D1::RectF(breakdown_rect.left + 26.0f, bar_y, breakdown_rect.left + 86.0f, bar_y + 28.0f);
            const D2D1_RECT_F track_rect =
                D2D1::RectF(breakdown_rect.left + 96.0f, bar_y + 3.0f, breakdown_rect.right - 132.0f, bar_y + 21.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(breakdown_rect.right - 122.0f, bar_y - 2.0f, breakdown_rect.right - 24.0f, bar_y + 26.0f);

            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(bar.label);
                const std::wstring value_w = to_wide(std::to_string(bar.count));
                draw_text_clipped(label_w, d2d_->body_format.Get(), label_rect, d2d_->text_brush.Get());
                draw_text_clipped_aligned(value_w,
                                          d2d_->body_format.Get(),
                                          value_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.60f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(track_rect, 8.0f, 8.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush && bar.count > 0) {
                const auto saved = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(bar.color);
                const float width = (track_rect.right - track_rect.left) * static_cast<float>(bar.count) /
                                    static_cast<float>(max_bar_count);
                const D2D1_RECT_F fill = D2D1::RectF(track_rect.left, track_rect.top,
                                                     track_rect.left + width, track_rect.bottom);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved);
            }
            bar_y += 52.0f;
        }

        draw_panel_row(breakdown_rect, breakdown_rect.top + 360.0f, loc("Timing Mean", "평균 타이밍"), format_signed_ms(data.result.mean_delta_ms));
        draw_panel_row(breakdown_rect, breakdown_rect.top + 398.0f, loc("Timing StdDev", "타이밍 표준편차"),
                       format_decimal(data.result.stddev_delta_ms) + "ms");
        draw_panel_row(breakdown_rect, breakdown_rect.top + 436.0f, loc("Warnings", "경고"),
                       std::to_string(data.result.export_warning_count));

        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring session_w = wloc("Session Detail", "세션 정보");
            const D2D1_RECT_F session_rect =
                D2D1::RectF(detail_rect.left + 24.0f, detail_rect.top + 18.0f, detail_rect.right - 24.0f,
                            detail_rect.top + 56.0f);
            draw_text_clipped(session_w, d2d_->title_format.Get(), session_rect, d2d_->text_brush.Get());
        }

        auto draw_detail_line = [&](float y, std::string_view text, bool muted = false) {
            if (!d2d_->body_format) {
                return;
            }
            ID2D1Brush* brush = muted ? static_cast<ID2D1Brush*>(d2d_->muted_brush.Get())
                                      : static_cast<ID2D1Brush*>(d2d_->text_brush.Get());
            if (!brush) {
                return;
            }
            const std::wstring line_w = to_wide(std::string(text));
            const D2D1_RECT_F line_rect =
                D2D1::RectF(detail_rect.left + 24.0f, y, detail_rect.right - 24.0f, y + 28.0f);
            draw_text_clipped(line_w, d2d_->body_format.Get(), line_rect, brush);
        };

        float detail_y = detail_rect.top + 58.0f;
        draw_detail_line(detail_y, loc("Profile: ", "프로필: ") + data.result.profile, true);
        detail_y += 32.0f;
        draw_detail_line(detail_y, loc("Track: ", "트랙: ") + data.result.track, true);
        detail_y += 40.0f;
        if (!data.result.replay_file.empty()) {
            draw_detail_line(detail_y, loc("Replay", "리플레이"));
            detail_y += 26.0f;
            draw_detail_line(detail_y, data.result.replay_file, true);
            detail_y += 34.0f;
        }
        if (!data.result.result_file.empty()) {
            draw_detail_line(detail_y, loc("Result File", "결과 파일"));
            detail_y += 26.0f;
            draw_detail_line(detail_y, data.result.result_file, true);
            detail_y += 34.0f;
        }

        if (data.result.timing_guidance_visible) {
            const D2D1_RECT_F guidance_rect =
                D2D1::RectF(detail_rect.left + 20.0f, detail_y, detail_rect.right - 20.0f, detail_y + 108.0f);
            const D2D1_ROUNDED_RECT guidance_rr = D2D1::RoundedRect(guidance_rect, 14.0f, 14.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.92f);
                ctx->FillRoundedRectangle(guidance_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                const auto saved = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(
                    data.result.timing_guidance_direction > 0 ? D2D1::ColorF(0xFF8A5B) : D2D1::ColorF(0x5DA9FF));
                ctx->DrawRoundedRectangle(guidance_rr, d2d_->accent_brush.Get(), 1.8f);
                d2d_->accent_brush->SetColor(saved);
            }

            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F title_rect =
                    D2D1::RectF(guidance_rect.left + 16.0f, guidance_rect.top + 12.0f,
                                guidance_rect.right - 16.0f, guidance_rect.top + 36.0f);
                draw_text_clipped(to_wide(data.result.timing_guidance_title),
                                  d2d_->body_format.Get(),
                                  title_rect,
                                  d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const D2D1_RECT_F message_rect =
                    D2D1::RectF(guidance_rect.left + 16.0f, guidance_rect.top + 40.0f,
                                guidance_rect.right - 16.0f, guidance_rect.top + 66.0f);
                const D2D1_RECT_F detail_rect_message =
                    D2D1::RectF(guidance_rect.left + 16.0f, guidance_rect.top + 68.0f,
                                guidance_rect.right - 16.0f, guidance_rect.top + 94.0f);
                draw_text_clipped(to_wide(data.result.timing_guidance_message),
                                  d2d_->body_format.Get(),
                                  message_rect,
                                  d2d_->muted_brush.Get());
                draw_text_clipped(to_wide(data.result.timing_guidance_detail),
                                  d2d_->body_format.Get(),
                                  detail_rect_message,
                                  d2d_->muted_brush.Get());
            }
            detail_y = guidance_rect.bottom + 18.0f;
        }

        const bool show_replay_button = !data.result.replay_file.empty();
        const bool replay_available = data.result.replay_available;
        const D2D1_RECT_F back_rect =
            D2D1::RectF(detail_rect.left + 24.0f, detail_rect.bottom - 78.0f, detail_rect.right - 24.0f,
                        detail_rect.bottom - 24.0f);
        const D2D1_RECT_F replay_rect =
            D2D1::RectF(detail_rect.left + 24.0f, back_rect.top - 66.0f, detail_rect.right - 24.0f,
                        back_rect.top - 12.0f);

        float note_y = detail_y + 18.0f;
        const float notes_bottom = (show_replay_button ? replay_rect.top : back_rect.top) - 18.0f;
        if (d2d_->body_format && d2d_->text_brush && note_y + 24.0f < notes_bottom) {
            const std::wstring notes_w = wloc("Notes", "메모");
            const D2D1_RECT_F notes_rect =
                D2D1::RectF(detail_rect.left + 24.0f, note_y, detail_rect.right - 24.0f, note_y + 28.0f);
            draw_text_clipped(notes_w, d2d_->body_format.Get(), notes_rect, d2d_->text_brush.Get());
        }
        note_y += 34.0f;
        if (note_y < notes_bottom && d2d_->body_format) {
            const D2D1_RECT_F notes_clip_rect =
                D2D1::RectF(detail_rect.left + 18.0f, note_y - 4.0f, detail_rect.right - 18.0f, notes_bottom);
            const int max_lines = std::max(
                0,
                static_cast<int>(std::floor((notes_clip_rect.bottom - note_y) / 30.0f)));
            if (max_lines > 0) {
                ctx->PushAxisAlignedClip(notes_clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                std::size_t visible_lines = std::min<std::size_t>(data.result.notes.size(), static_cast<std::size_t>(max_lines));
                const bool truncated = data.result.notes.size() > visible_lines;
                if (truncated && visible_lines > 0) {
                    --visible_lines;
                }
                for (std::size_t i = 0; i < visible_lines; ++i) {
                    draw_detail_line(note_y, data.result.notes[i], true);
                    note_y += 30.0f;
                }
                if (truncated) {
                    const std::string remaining = "+" + std::to_string(data.result.notes.size() - visible_lines) + " " +
                                                  loc("more", "더");
                    draw_detail_line(note_y, remaining, true);
                }
                ctx->PopAxisAlignedClip();
            }
        }

        if (show_replay_button) {
            if (replay_available) {
                register_hit(replay_rect, MenuHitTargetKind::SettingsRow, 1);
            }
            if (d2d_->button_selected_brush) {
                d2d_->button_selected_brush->SetOpacity(replay_available ? 0.88f : 0.32f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(replay_rect, 14.0f, 14.0f), d2d_->button_selected_brush.Get());
                d2d_->button_selected_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                d2d_->accent_brush->SetOpacity(replay_available ? 1.0f : 0.35f);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(replay_rect, 14.0f, 14.0f), d2d_->accent_brush.Get(), 1.5f);
                d2d_->accent_brush->SetOpacity(1.0f);
            }
            if (d2d_->title_format && d2d_->text_brush) {
                const std::wstring replay_w = replay_available ? wloc("WATCH REPLAY", "리플레이 보기")
                                                               : wloc("REPLAY UNAVAILABLE", "리플레이 사용 불가");
                ID2D1Brush* replay_brush =
                    replay_available ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                     : static_cast<ID2D1Brush*>(d2d_->muted_brush ? d2d_->muted_brush.Get()
                                                                                   : d2d_->text_brush.Get());
                draw_text_clipped_aligned(replay_w,
                                          d2d_->title_format.Get(),
                                          replay_rect,
                                          replay_brush,
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }

        register_hit(back_rect, MenuHitTargetKind::SettingsRow, 0);
        if (d2d_->button_selected_brush) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(back_rect, 14.0f, 14.0f), d2d_->button_selected_brush.Get());
        }
        if (d2d_->accent_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(back_rect, 14.0f, 14.0f), d2d_->accent_brush.Get(), 1.5f);
        }
        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring back_w = wloc("BACK TO SONG SELECT", "곡 선택으로 돌아가기");
            draw_text_clipped_aligned(back_w,
                                      d2d_->title_format.Get(),
                                      back_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
        }

