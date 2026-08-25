        const int64_t presentation_elapsed_ns =
            data.result.presentation_start_ns > 0
                ? std::max<int64_t>(0, render_now_ns - data.result.presentation_start_ns)
                : kResultPresentationDurationNs;
        const ResultPresentationFrame presentation =
            result_presentation_frame(presentation_elapsed_ns,
                                      data.result.presentation_skipped ||
                                          data.result.presentation_start_ns <= 0);
        const double presentation_seconds =
            static_cast<double>(presentation_elapsed_ns) / 1'000'000'000.0;
        const bool result_success = data.result.cleared;
        const auto result_loc = [&](std::string_view english, std::string_view korean) {
            return std::string(data.ui_korean ? korean : english);
        };

        auto draw_result_text = [&](const std::string& text,
                                    IDWriteTextFormat* format,
                                    const D2D1_RECT_F& rect,
                                    ID2D1Brush* brush,
                                    float alpha,
                                    auto... alignment_args) {
            DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
            ((alignment = alignment_args), ...);
            if (!format || !brush || alpha <= 0.001f) {
                return;
            }
            const float saved_opacity = brush->GetOpacity();
            brush->SetOpacity(saved_opacity * std::clamp(alpha, 0.0f, 1.0f));
            draw_text_clipped_aligned(to_wide(text), format, rect, brush, alignment);
            brush->SetOpacity(saved_opacity);
        };

        auto draw_result_panel = [&](const D2D1_RECT_F& rect, float alpha, bool accent = false) {
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 12.0f, 12.0f);
            if (d2d_->panel_brush) {
                const float saved_opacity = d2d_->panel_brush->GetOpacity();
                d2d_->panel_brush->SetOpacity(saved_opacity * alpha * 0.88f);
                ctx->FillRoundedRectangle(rounded, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(saved_opacity);
            }
            ID2D1Brush* border = accent && d2d_->accent_brush
                                     ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                     : static_cast<ID2D1Brush*>(d2d_->button_border_brush.Get());
            if (border) {
                const float saved_opacity = border->GetOpacity();
                border->SetOpacity(saved_opacity * alpha * (accent ? 0.72f : 0.46f));
                ctx->DrawRoundedRectangle(rounded, border, accent ? 1.4f : 1.0f);
                border->SetOpacity(saved_opacity);
            }
        };

        // Clickable controls get an accent wash and a thicker accent border so they
        // read as buttons instead of blending into the static analysis panels.
        auto draw_action_button = [&](const D2D1_RECT_F& rect, float alpha, bool enabled) {
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 12.0f, 12.0f);
            if (d2d_->panel_brush) {
                const float saved_opacity = d2d_->panel_brush->GetOpacity();
                d2d_->panel_brush->SetOpacity(saved_opacity * alpha * 0.94f);
                ctx->FillRoundedRectangle(rounded, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(saved_opacity);
            }
            if (enabled && d2d_->button_selected_brush) {
                const float saved_opacity = d2d_->button_selected_brush->GetOpacity();
                d2d_->button_selected_brush->SetOpacity(saved_opacity * alpha);
                ctx->FillRoundedRectangle(rounded, d2d_->button_selected_brush.Get());
                d2d_->button_selected_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(saved_opacity * alpha * (enabled ? 0.90f : 0.32f));
                ctx->DrawRoundedRectangle(rounded, d2d_->accent_brush.Get(), 2.0f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
        };

        ID2D1Bitmap* result_art =
            find_song_card_preview_bitmap(data.result.background_path);
        const D2D1_RECT_F full_result_rect = D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight);
        if (result_art && presentation.background > 0.0f) {
            const D2D1_RECT_F source =
                centered_bitmap_source_rect(result_art->GetSize(), full_result_rect);
            ctx->DrawBitmap(result_art,
                            full_result_rect,
                            0.14f * presentation.background,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source);
        }
        if (d2d_->panel_brush) {
            const D2D1_COLOR_F saved_color = d2d_->panel_brush->GetColor();
            const float saved_opacity = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetColor(D2D1::ColorF(0x020914));
            d2d_->panel_brush->SetOpacity(0.56f + 0.22f * presentation.background);
            ctx->FillRectangle(full_result_rect, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetColor(saved_color);
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }

        const D2D1_POINT_2F prism_center = D2D1::Point2F(874.0f, 420.0f);
        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            for (int index = 0; index < 52; ++index) {
                const float seed = static_cast<float>(index) * 1.6180339f;
                const float angle = seed * 2.37f + static_cast<float>(presentation_seconds * 0.16);
                const float radius = 120.0f + static_cast<float>((index * 47) % 430);
                const float x = prism_center.x + std::cos(angle) * radius;
                const float y = prism_center.y + std::sin(angle * 0.83f) * radius * 0.54f;
                const float twinkle = 0.45f + 0.55f * std::sin(angle * 1.7f + seed);
                const float all_perfect_boost =
                    data.result.all_perfect ? presentation.all_perfect * 0.22f : 0.0f;
                d2d_->accent_brush->SetColor(
                    (index % 3 == 0) ? D2D1::ColorF(0xA86CFF) : D2D1::ColorF(0x5CEBFF));
                d2d_->accent_brush->SetOpacity(
                    presentation.background * (0.06f + 0.10f * twinkle + all_perfect_boost));
                const float dot = 0.8f + static_cast<float>(index % 4) * 0.45f;
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), dot, dot),
                                 d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F header_line = D2D1::RectF(54.0f, 38.0f, 1866.0f, 122.0f);
        draw_result_text("TENRIFF",
                         d2d_->header_format.Get(),
                         D2D1::RectF(70.0f, 36.0f, 360.0f, 100.0f),
                         d2d_->text_brush.Get(),
                         presentation.background);
        draw_result_text(result_loc("RESULT", "결과"),
                         d2d_->title_format.Get(),
                         D2D1::RectF(410.0f, 44.0f, 600.0f, 84.0f),
                         d2d_->muted_brush.Get(),
                         presentation.background);
        draw_result_text(result_loc("AFTER PERFORMANCE", "플레이 결과"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(410.0f, 80.0f, 650.0f, 108.0f),
                         d2d_->muted_brush.Get(),
                         presentation.background);
        if (d2d_->button_border_brush) {
            const float saved = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(0.32f * presentation.background);
            ctx->DrawLine(D2D1::Point2F(header_line.left, header_line.bottom),
                          D2D1::Point2F(header_line.right, header_line.bottom),
                          d2d_->button_border_brush.Get(),
                          1.0f);
            d2d_->button_border_brush->SetOpacity(saved);
        }

        const D2D1_RECT_F profile_rect = skin_layout_rect(
            data, "result.profile", D2D1::RectF(1400.0f, 28.0f, 1848.0f, 108.0f));
        draw_result_panel(profile_rect, presentation.background, false);
        const D2D1_RECT_F avatar_rect = D2D1::RectF(
            profile_rect.left + 15.0f, profile_rect.top + 9.0f,
            profile_rect.left + 77.0f, profile_rect.bottom - 9.0f);
        if (ID2D1Bitmap* avatar =
                find_song_card_preview_bitmap(data.result.profile_avatar_path)) {
            const D2D1_RECT_F source = centered_bitmap_source_rect(avatar->GetSize(), avatar_rect);
            ctx->DrawBitmap(avatar,
                            avatar_rect,
                            0.96f * presentation.background,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source);
        } else if (d2d_->card_brush) {
            const float saved = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetOpacity(0.72f * presentation.background);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(avatar_rect, 10.0f, 10.0f),
                                      d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved);
        }
        draw_result_text(data.result.profile.empty() ? std::string("PLAYER") : data.result.profile,
                         d2d_->title_format.Get(),
                         D2D1::RectF(profile_rect.left + 96.0f, profile_rect.top + 10.0f,
                                     profile_rect.right - 23.0f, profile_rect.top + 44.0f),
                         d2d_->text_brush.Get(),
                         presentation.background);
        draw_result_text(result_loc("PERFORMANCE RESULT", "플레이 기록"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(profile_rect.left + 96.0f, profile_rect.top + 44.0f,
                                     profile_rect.right - 23.0f, profile_rect.bottom - 9.0f),
                         d2d_->muted_brush.Get(),
                         presentation.background);

        const float information_slide = 34.0f * (1.0f - presentation.information);
        const D2D1_RECT_F song_panel = offset_rect(
            skin_layout_rect(data, "result.song_panel",
                             D2D1::RectF(70.0f, 154.0f, 590.0f, 704.0f)),
            -information_slide, 0.0f);
        draw_result_panel(song_panel, presentation.information, true);
        const D2D1_RECT_F art_rect =
            D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 42.0f,
                        song_panel.right - 44.0f, song_panel.top + 382.0f);
        if (result_art) {
            const D2D1_RECT_F source = centered_bitmap_source_rect(result_art->GetSize(), art_rect);
            ctx->DrawBitmap(result_art,
                            art_rect,
                            0.96f * presentation.information,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source);
        }
        draw_result_text(result_loc("// TRACK COMPLETE", "// 플레이 완료"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(song_panel.left + 230.0f, song_panel.top + 14.0f,
                                     song_panel.right - 24.0f, song_panel.top + 38.0f),
                         d2d_->accent_brush.Get(),
                         presentation.information,
                         DWRITE_TEXT_ALIGNMENT_TRAILING);
        draw_result_text(data.result.title.empty() ? std::string("Unknown Chart") : data.result.title,
                         d2d_->song_title_format.Get(),
                         D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 402.0f,
                                     song_panel.right - 44.0f, song_panel.top + 455.0f),
                         d2d_->text_brush.Get(),
                         presentation.information);
        draw_result_text(data.result.artist.empty() ? std::string("Unknown Artist") : data.result.artist,
                         d2d_->body_format.Get(),
                         D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 455.0f,
                                     song_panel.right - 44.0f, song_panel.top + 488.0f),
                         d2d_->muted_brush.Get(),
                         presentation.information);

        const int display_level = data.result.level > 0 ? data.result.level : data.result.native_level;
        const std::string mode_label =
            data.result.key_count > 0 ? std::to_string(data.result.key_count) + "KEY" : "--";
        const std::array<std::pair<std::string, std::string>, 4> chart_metadata = {{
            {"BPM", data.result.bpm > 0.0 ? format_decimal(data.result.bpm) : "--"},
            {result_loc("MODE", "키 모드"), mode_label},
            {result_loc("LEVEL", "레벨"), display_level > 0 ? std::to_string(display_level) : "--"},
            {result_loc("GAUGE", "게이지"), data.result.gauge_label},
        }};
        const float metadata_width = (song_panel.right - song_panel.left - 88.0f) / 4.0f;
        for (std::size_t index = 0; index < chart_metadata.size(); ++index) {
            const float left = song_panel.left + 44.0f + metadata_width * static_cast<float>(index);
            draw_result_text(chart_metadata[index].first,
                             d2d_->hud_format.Get(),
                             D2D1::RectF(left, song_panel.top + 505.0f,
                                         left + metadata_width - 8.0f, song_panel.top + 530.0f),
                             d2d_->muted_brush.Get(),
                             presentation.information);
            draw_result_text(chart_metadata[index].second,
                             d2d_->body_format.Get(),
                             D2D1::RectF(left, song_panel.top + 530.0f,
                                         left + metadata_width - 8.0f, song_panel.top + 558.0f),
                             d2d_->text_brush.Get(),
                             presentation.information);
        }

        const double score_maximum = data.result.max_score > 0
                                         ? static_cast<double>(data.result.max_score)
                                         : 1.0;
        const float score_quality = static_cast<float>(
            std::clamp(static_cast<double>(data.result.score) / score_maximum, 0.0, 1.0));
        const float failed_completion = result_success ? 1.0f : 0.76f;
        const float prism_completion = presentation.prism * failed_completion;
        const float prism_top = 116.0f + (1.0f - score_quality) * 20.0f;
        const float prism_bottom = 715.0f;
        const float prism_half_width = 222.0f + score_quality * 18.0f;
        auto assemble_point = [&](float x, float y) {
            return D2D1::Point2F(
                prism_center.x + (x - prism_center.x) * (0.90f + 0.10f * prism_completion),
                prism_bottom - (prism_bottom - y) * prism_completion);
        };
        const D2D1_POINT_2F prism_points[] = {
            assemble_point(prism_center.x, prism_top),
            assemble_point(prism_center.x + prism_half_width, prism_top + 74.0f),
            assemble_point(prism_center.x + prism_half_width - 8.0f, prism_bottom - 40.0f),
            assemble_point(prism_center.x + 158.0f, prism_bottom),
            assemble_point(prism_center.x - 158.0f, prism_bottom),
            assemble_point(prism_center.x - prism_half_width + 8.0f, prism_bottom - 40.0f),
            assemble_point(prism_center.x - prism_half_width, prism_top + 74.0f),
        };

        if (d2d_->glow_brush && presentation.prism > 0.0f) {
            const float saved = d2d_->glow_brush->GetOpacity();
            d2d_->glow_brush->SetCenter(D2D1::Point2F(prism_center.x, prism_bottom - 25.0f));
            d2d_->glow_brush->SetGradientOriginOffset(D2D1::Point2F(0.0f, -120.0f));
            d2d_->glow_brush->SetRadiusX(330.0f);
            d2d_->glow_brush->SetRadiusY(170.0f);
            d2d_->glow_brush->SetOpacity((0.25f + score_quality * 0.22f) * presentation.prism);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(prism_center.x, prism_bottom - 20.0f),
                                           340.0f, 150.0f),
                             d2d_->glow_brush.Get());
            d2d_->glow_brush->SetOpacity(saved);
        }

        Microsoft::WRL::ComPtr<ID2D1PathGeometry> prism_geometry;
        if (d2d_->d2d_factory &&
            SUCCEEDED(d2d_->d2d_factory->CreatePathGeometry(&prism_geometry)) &&
            prism_geometry) {
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            if (SUCCEEDED(prism_geometry->Open(&sink)) && sink) {
                sink->BeginFigure(prism_points[0], D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLines(prism_points + 1, static_cast<UINT32>(std::size(prism_points) - 1));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                if (d2d_->accent_brush) {
                    const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                    const float saved_opacity = d2d_->accent_brush->GetOpacity();
                    const float hue_cycle = 0.5f + 0.5f * std::sin(static_cast<float>(presentation_seconds * 4.0));
                    const D2D1_COLOR_F prism_color =
                        data.result.all_perfect && presentation.all_perfect > 0.0f
                            ? D2D1::ColorF(0.36f + 0.30f * hue_cycle,
                                           0.72f + 0.22f * (1.0f - hue_cycle),
                                           1.0f,
                                           1.0f)
                            : (result_success ? D2D1::ColorF(0x6BE9FF)
                                              : D2D1::ColorF(0x75698E));
                    d2d_->accent_brush->SetColor(prism_color);
                    d2d_->accent_brush->SetOpacity(0.055f * presentation.prism);
                    ctx->FillGeometry(prism_geometry.Get(), d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetOpacity((0.48f + 0.35f * score_quality) * presentation.prism);
                    ctx->DrawGeometry(prism_geometry.Get(), d2d_->accent_brush.Get(), 2.3f);
                    d2d_->accent_brush->SetColor(D2D1::ColorF(0xB57CFF));
                    d2d_->accent_brush->SetOpacity(0.34f * presentation.prism);
                    ctx->DrawLine(prism_points[0], prism_points[4], d2d_->accent_brush.Get(), 1.0f);
                    ctx->DrawLine(prism_points[0], prism_points[3], d2d_->accent_brush.Get(), 1.0f);
                    ctx->DrawLine(prism_points[6], prism_points[2], d2d_->accent_brush.Get(), 0.9f);
                    ctx->DrawLine(assemble_point(prism_center.x - 150.0f, prism_top + 230.0f),
                                  assemble_point(prism_center.x + 150.0f, prism_top + 280.0f),
                                  d2d_->accent_brush.Get(), 0.8f);
                    d2d_->accent_brush->SetColor(saved_color);
                    d2d_->accent_brush->SetOpacity(saved_opacity);
                }
            }
        }

        if (d2d_->accent_brush && presentation.prism > 0.0f) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            for (int ring = 0; ring < 4; ++ring) {
                const float phase = static_cast<float>(presentation_seconds * (0.20 + ring * 0.07));
                const float rx = 225.0f + ring * 38.0f;
                const float ry = 34.0f + ring * 12.0f;
                d2d_->accent_brush->SetColor((ring % 2 == 0) ? D2D1::ColorF(0x63E9FF)
                                                             : D2D1::ColorF(0x9E65FF));
                d2d_->accent_brush->SetOpacity((0.16f - ring * 0.018f) * presentation.prism);
                ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(prism_center.x, prism_bottom - 15.0f), rx, ry),
                                 d2d_->accent_brush.Get(), 1.1f);
                const D2D1_POINT_2F orbit = D2D1::Point2F(
                    prism_center.x + std::cos(phase + ring) * rx,
                    prism_bottom - 15.0f + std::sin(phase + ring) * ry);
                d2d_->accent_brush->SetOpacity(0.50f * presentation.prism);
                ctx->FillEllipse(D2D1::Ellipse(orbit, 2.5f, 2.5f), d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const int64_t counted_score = result_counted_score(data.result.score, presentation.score);
        const float score_alpha = presentation.score;
        const float pulse_scale = 1.0f + presentation.score_pulse * 0.06f;
        const D2D1_RECT_F score_rect =
            D2D1::RectF(prism_center.x - 260.0f,
                        158.0f - 5.0f * presentation.score_pulse,
                        prism_center.x + 260.0f,
                        230.0f + 5.0f * presentation.score_pulse);
        draw_result_text(format_int_with_commas(counted_score) + " / " +
                             format_int_with_commas(data.result.max_score),
                         d2d_->title_format.Get(),
                         score_rect,
                         d2d_->text_brush.Get(),
                         score_alpha * pulse_scale,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(result_loc("SCORE", "점수"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(prism_center.x - 120.0f, 132.0f,
                                     prism_center.x + 120.0f, 160.0f),
                         d2d_->muted_brush.Get(),
                         score_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(result_loc("DETAIL SCORE", "상세 점수") + "  " +
                             format_int_with_commas(data.result.detail_score) + " / " +
                             format_int_with_commas(data.result.max_detail_score),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(prism_center.x - 180.0f, 238.0f,
                                     prism_center.x + 180.0f, 268.0f),
                         d2d_->accent_brush.Get(),
                         score_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);

        const std::string rank_text = data.result.rank.empty() ? std::string("--") : data.result.rank;
        const D2D1_RECT_F rank_rect =
            D2D1::RectF(prism_center.x - 250.0f, 285.0f,
                        prism_center.x + 250.0f, 500.0f);
        if (presentation.chromatic > 0.0f && result_success) {
            draw_result_text(rank_text,
                             d2d_->rank_format.Get(),
                             D2D1::RectF(rank_rect.left - 4.0f, rank_rect.top,
                                         rank_rect.right - 4.0f, rank_rect.bottom),
                             d2d_->accent_brush.Get(),
                             presentation.rank * presentation.chromatic * 0.55f,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_result_text(rank_text,
                             d2d_->rank_format.Get(),
                             D2D1::RectF(rank_rect.left + 4.0f, rank_rect.top,
                                         rank_rect.right + 4.0f, rank_rect.bottom),
                             d2d_->muted_brush.Get(),
                             presentation.rank * presentation.chromatic * 0.45f,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        draw_result_text(rank_text,
                         d2d_->rank_format.Get(),
                         rank_rect,
                         result_success ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                        : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()),
                         presentation.rank,
                         DWRITE_TEXT_ALIGNMENT_CENTER);

        if (result_success && presentation.shockwave > 0.0f && presentation.shockwave < 1.0f &&
            d2d_->accent_brush) {
            const float saved = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity((1.0f - presentation.shockwave) * 0.62f);
            const float radius = 42.0f + presentation.shockwave * 250.0f;
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(prism_center.x, 400.0f), radius, radius * 0.44f),
                             d2d_->accent_brush.Get(), 2.2f);
            d2d_->accent_brush->SetOpacity(saved);
        }

        const D2D1_RECT_F status_rect =
            D2D1::RectF(prism_center.x - 260.0f, 526.0f,
                        prism_center.x + 260.0f, 586.0f);
        draw_result_text(localized_result_status(),
                         d2d_->title_format.Get(),
                         status_rect,
                         result_success ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                        : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()),
                         presentation.status,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        if (presentation.status_scan > 0.0f && presentation.status_scan < 1.0f &&
            d2d_->accent_brush) {
            const float x = status_rect.left +
                            (status_rect.right - status_rect.left) * presentation.status_scan;
            const float saved = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(0.75f * presentation.status);
            ctx->DrawLine(D2D1::Point2F(x, status_rect.top + 7.0f),
                          D2D1::Point2F(x, status_rect.bottom - 7.0f),
                          d2d_->accent_brush.Get(), 3.0f);
            d2d_->accent_brush->SetOpacity(saved);
        }
        if (data.result.all_perfect) {
            draw_result_text(result_loc("ALL NOTES PERFECTED", "모든 노트 퍼펙트"),
                             d2d_->hud_format.Get(),
                             D2D1::RectF(prism_center.x - 210.0f, 590.0f,
                                         prism_center.x + 210.0f, 622.0f),
                             d2d_->accent_brush.Get(),
                             presentation.all_perfect,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
        } else if (data.result.full_combo) {
            draw_result_text(result_loc("FULL COMBO", "풀 콤보"),
                             d2d_->hud_format.Get(),
                             D2D1::RectF(prism_center.x - 160.0f, 590.0f,
                                         prism_center.x + 160.0f, 622.0f),
                             d2d_->accent_brush.Get(),
                             presentation.status,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        if (result_success && presentation.rank > 0.0f && presentation.rank < 1.0f &&
            d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            for (int index = 0; index < 14; ++index) {
                const float angle = static_cast<float>(index) * 0.4488f;
                const float radius = 70.0f + presentation.rank * (95.0f + (index % 4) * 23.0f);
                const D2D1_POINT_2F start =
                    D2D1::Point2F(prism_center.x + std::cos(angle) * radius,
                                  405.0f + std::sin(angle) * radius * 0.54f);
                const D2D1_POINT_2F end =
                    D2D1::Point2F(start.x + std::cos(angle) * 12.0f,
                                  start.y + std::sin(angle) * 7.0f);
                d2d_->accent_brush->SetColor((index % 2 == 0) ? D2D1::ColorF(0x70ECFF)
                                                              : D2D1::ColorF(0xB379FF));
                d2d_->accent_brush->SetOpacity((1.0f - presentation.rank) * 0.72f);
                ctx->DrawLine(start, end, d2d_->accent_brush.Get(), 1.6f);
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        // Left edge shares the CONTINUE/REPLAY/RETRY column below it so the right
        // half of the screen reads as one column instead of two ragged ones.
        const D2D1_RECT_F analysis_panel = skin_layout_rect(
            data, "result.analysis_panel", D2D1::RectF(1320.0f, 154.0f, 1848.0f, 704.0f));
        draw_result_panel(analysis_panel, presentation.graph, false);
        draw_result_text(result_loc("// PERFORMANCE ANALYSIS", "// 플레이 분석"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(analysis_panel.left + 24.0f, analysis_panel.top + 18.0f,
                                     analysis_panel.right - 24.0f, analysis_panel.top + 48.0f),
                         d2d_->accent_brush.Get(),
                         presentation.graph);
        draw_result_text(result_loc("TIMING SUMMARY", "타이밍 요약"),
                         d2d_->body_format.Get(),
                         D2D1::RectF(analysis_panel.left + 26.0f, analysis_panel.top + 66.0f,
                                     analysis_panel.right - 26.0f, analysis_panel.top + 98.0f),
                         d2d_->text_brush.Get(),
                         presentation.graph);
        const D2D1_RECT_F timing_track =
            D2D1::RectF(analysis_panel.left + 30.0f, analysis_panel.top + 118.0f,
                        analysis_panel.right - 30.0f, analysis_panel.top + 226.0f);
        if (d2d_->card_brush) {
            const float saved = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetOpacity(0.34f * presentation.graph);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(timing_track, 8.0f, 8.0f),
                                      d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved);
        }
        const float center_x = (timing_track.left + timing_track.right) * 0.5f;
        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            const float spread = static_cast<float>(std::clamp(data.result.stddev_delta_ms, 1.0, 65.0));
            const float mean = static_cast<float>(std::clamp(data.result.mean_delta_ms, -80.0, 80.0));
            // Derived from the track so the 57 bars always land inside it.
            const float bar_spacing = (timing_track.right - timing_track.left - 24.0f) / 56.0f;
            for (int bar = -28; bar <= 28; ++bar) {
                const float delta = static_cast<float>(bar) * 2.8f;
                const float gaussian = std::exp(-0.5f * ((delta - mean) / spread) * ((delta - mean) / spread));
                const float x = center_x + static_cast<float>(bar) * bar_spacing;
                const float height = gaussian * 76.0f * presentation.graph;
                d2d_->accent_brush->SetColor(bar < 0 ? D2D1::ColorF(0x54E9FF)
                                                      : D2D1::ColorF(0xA56BFF));
                d2d_->accent_brush->SetOpacity((data.result.judged_notes > 0 ? 0.72f : 0.20f) * presentation.graph);
                ctx->DrawLine(D2D1::Point2F(x, timing_track.bottom - 12.0f),
                              D2D1::Point2F(x, timing_track.bottom - 12.0f - height),
                              d2d_->accent_brush.Get(), 5.0f);
            }
            d2d_->accent_brush->SetColor(D2D1::ColorF(0xF7FAFD));
            d2d_->accent_brush->SetOpacity(0.52f * presentation.graph);
            ctx->DrawLine(D2D1::Point2F(center_x, timing_track.top + 8.0f),
                          D2D1::Point2F(center_x, timing_track.bottom - 8.0f),
                          d2d_->accent_brush.Get(), 1.0f);
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }
        // The histogram halves already encode direction by colour; naming them and
        // printing the hit counts is what makes the split readable at a glance.
        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            const D2D1_RECT_F fast_rect =
                D2D1::RectF(timing_track.left + 12.0f, timing_track.top + 8.0f,
                            center_x - 12.0f, timing_track.top + 38.0f);
            const D2D1_RECT_F slow_rect =
                D2D1::RectF(center_x + 12.0f, timing_track.top + 8.0f,
                            timing_track.right - 12.0f, timing_track.top + 38.0f);
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x54E9FF));
            draw_result_text("FAST  " + std::to_string(data.result.fast_count),
                             d2d_->hud_format.Get(),
                             fast_rect,
                             d2d_->accent_brush.Get(),
                             presentation.graph,
                             DWRITE_TEXT_ALIGNMENT_LEADING);
            d2d_->accent_brush->SetColor(D2D1::ColorF(0xA56BFF));
            draw_result_text("SLOW  " + std::to_string(data.result.slow_count),
                             d2d_->hud_format.Get(),
                             slow_rect,
                             d2d_->accent_brush.Get(),
                             presentation.graph,
                             DWRITE_TEXT_ALIGNMENT_TRAILING);
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }
        draw_result_text(result_loc("MEAN", "평균") + "  " + format_signed_ms(data.result.mean_delta_ms),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(timing_track.left, timing_track.bottom + 8.0f,
                                     center_x, timing_track.bottom + 36.0f),
                         d2d_->muted_brush.Get(),
                         presentation.graph);
        draw_result_text(result_loc("SPREAD", "분산") + "  " +
                             format_decimal(data.result.stddev_delta_ms) + "ms",
                         d2d_->hud_format.Get(),
                         D2D1::RectF(center_x, timing_track.bottom + 8.0f,
                                     timing_track.right, timing_track.bottom + 36.0f),
                         d2d_->muted_brush.Get(),
                         presentation.graph,
                         DWRITE_TEXT_ALIGNMENT_TRAILING);

        draw_result_text(result_loc("GAUGE CONSISTENCY", "게이지 변화"),
                         d2d_->body_format.Get(),
                         D2D1::RectF(analysis_panel.left + 26.0f, analysis_panel.top + 286.0f,
                                     analysis_panel.right - 26.0f, analysis_panel.top + 318.0f),
                         d2d_->text_brush.Get(),
                         presentation.graph);
        const D2D1_RECT_F gauge_plot =
            D2D1::RectF(analysis_panel.left + 30.0f, analysis_panel.top + 332.0f,
                        analysis_panel.right - 30.0f, analysis_panel.bottom - 36.0f);
        // Gridlines alone do not say what height means, so each one carries its
        // percentage and the plot keeps a left gutter for those labels.
        const float gauge_axis_width = 46.0f;
        const D2D1_RECT_F gauge_track =
            D2D1::RectF(gauge_plot.left + gauge_axis_width, gauge_plot.top,
                        gauge_plot.right, gauge_plot.bottom);
        if (d2d_->button_border_brush) {
            const float saved = d2d_->button_border_brush->GetOpacity();
            for (int line = 0; line <= 4; ++line) {
                const float y = gauge_track.top + (gauge_track.bottom - gauge_track.top) *
                                                      static_cast<float>(line) / 4.0f;
                const bool edge = line == 0 || line == 4;
                d2d_->button_border_brush->SetOpacity((edge ? 0.46f : 0.24f) * presentation.graph);
                ctx->DrawLine(D2D1::Point2F(gauge_track.left, y),
                              D2D1::Point2F(gauge_track.right, y),
                              d2d_->button_border_brush.Get(), edge ? 1.1f : 0.8f);
                draw_result_text(std::to_string(100 - line * 25) + "%",
                                 d2d_->hud_format.Get(),
                                 D2D1::RectF(gauge_plot.left, y - 13.0f,
                                             gauge_track.left - 8.0f, y + 13.0f),
                                 d2d_->muted_brush.Get(),
                                 presentation.graph,
                                 DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            d2d_->button_border_brush->SetOpacity(0.34f * presentation.graph);
            ctx->DrawLine(D2D1::Point2F(gauge_track.left, gauge_track.top),
                          D2D1::Point2F(gauge_track.left, gauge_track.bottom),
                          d2d_->button_border_brush.Get(), 1.1f);
            d2d_->button_border_brush->SetOpacity(saved);
        }
        if (data.result.gauge_points.size() >= 2 && d2d_->accent_brush) {
            const float reveal_half = presentation.graph * 0.5f;
            const float reveal_left = 0.5f - reveal_half;
            const float reveal_right = 0.5f + reveal_half;
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(result_success ? D2D1::ColorF(0x68EDF3)
                                                        : D2D1::ColorF(0x827A98));
            d2d_->accent_brush->SetOpacity(0.88f * presentation.graph);
            for (std::size_t index = 1; index < data.result.gauge_points.size(); ++index) {
                const auto& previous = data.result.gauge_points[index - 1];
                const auto& current = data.result.gauge_points[index];
                if (current.position < reveal_left || previous.position > reveal_right) {
                    continue;
                }
                const D2D1_POINT_2F p0 = D2D1::Point2F(
                    gauge_track.left + previous.position * (gauge_track.right - gauge_track.left),
                    gauge_track.bottom - previous.value * (gauge_track.bottom - gauge_track.top));
                const D2D1_POINT_2F p1 = D2D1::Point2F(
                    gauge_track.left + current.position * (gauge_track.right - gauge_track.left),
                    gauge_track.bottom - current.value * (gauge_track.bottom - gauge_track.top));
                ctx->DrawLine(p0, p1, d2d_->accent_brush.Get(), 2.2f);
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F stats_panel = skin_layout_rect(
            data, "result.stats_panel", D2D1::RectF(70.0f, 770.0f, 1270.0f, 994.0f));
        draw_result_panel(stats_panel, presentation.statistics.front(), true);
        struct ResultStatistic {
            std::string label;
            std::string value;
            std::string detail;
            D2D1_COLOR_F color;
        };
        const int judged_total = std::max(1, data.result.judged_notes);
        const auto percent_for = [&](int count) {
            return format_decimal(static_cast<double>(count) * 100.0 /
                                  static_cast<double>(judged_total)) + "%";
        };
        const std::array<ResultStatistic, 7> result_statistics = {{
            {"P-GREAT", std::to_string(data.result.perfect), percent_for(data.result.perfect), D2D1::ColorF(0x63E9FF)},
            {"GREAT", std::to_string(data.result.great), percent_for(data.result.great), D2D1::ColorF(0x73DDF5)},
            {"GOOD", std::to_string(data.result.good), percent_for(data.result.good), D2D1::ColorF(0xA8EA58)},
            {"POOR", std::to_string(data.result.poor), percent_for(data.result.poor), D2D1::ColorF(0xFF6B7D)},
            {"FAIL", std::to_string(data.result.bad), percent_for(data.result.bad), D2D1::ColorF(0xF2B84B)},
            {result_loc("MAX COMBO", "최대 콤보"), std::to_string(data.result.max_combo),
             data.result.full_combo ? result_loc("FULL COMBO", "풀 콤보") : "/ " + std::to_string(data.result.total_notes), D2D1::ColorF(0xF7FAFD)},
            {result_loc("ACCURACY", "정확도"), format_decimal(data.result.accuracy) + "%",
             result_loc("DETAIL", "상세") + " " + format_decimal(data.result.detailed_accuracy) + "%", D2D1::ColorF(0x63E9FF)},
        }};
        const float statistic_width = (stats_panel.right - stats_panel.left) /
                                      static_cast<float>(result_statistics.size());
        for (std::size_t index = 0; index < result_statistics.size(); ++index) {
            const float alpha = presentation.statistics[index];
            const float rise = 18.0f * (1.0f - alpha);
            const float left = stats_panel.left + statistic_width * static_cast<float>(index);
            const D2D1_RECT_F cell =
                D2D1::RectF(left, stats_panel.top, left + statistic_width, stats_panel.bottom);
            if (index > 0 && d2d_->button_border_brush) {
                const float saved = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.34f * alpha);
                ctx->DrawLine(D2D1::Point2F(cell.left, cell.top + 18.0f),
                              D2D1::Point2F(cell.left, cell.bottom - 18.0f),
                              d2d_->button_border_brush.Get(), 1.0f);
                d2d_->button_border_brush->SetOpacity(saved);
            }
            if (d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetColor(result_statistics[index].color);
                const float zero_scale =
                    (index < 5 && result_statistics[index].value == "0") ? 0.38f : 1.0f;
                d2d_->accent_brush->SetOpacity(alpha * zero_scale);
                draw_result_text(result_statistics[index].label,
                                 d2d_->hud_format.Get(),
                                 D2D1::RectF(cell.left + 14.0f, cell.top + 28.0f + rise,
                                             cell.right - 14.0f, cell.top + 58.0f + rise),
                                 d2d_->accent_brush.Get(),
                                 alpha * zero_scale,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_result_text(result_statistics[index].value,
                                 d2d_->stats_value_format.Get(),
                                 D2D1::RectF(cell.left + 10.0f, cell.top + 76.0f + rise,
                                             cell.right - 10.0f, cell.top + 126.0f + rise),
                                 d2d_->text_brush.Get(),
                                 alpha * zero_scale,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_result_text(result_statistics[index].detail,
                                 d2d_->hud_format.Get(),
                                 D2D1::RectF(cell.left + 10.0f, cell.top + 142.0f + rise,
                                             cell.right - 10.0f, cell.top + 174.0f + rise),
                                 (index == 5 && data.result.full_combo)
                                     ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                     : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()),
                                 alpha * zero_scale,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
        }

        if (data.result.all_perfect && d2d_->accent_brush && presentation.all_perfect > 0.0f) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            const float beam_x = stats_panel.left +
                                 (stats_panel.right - stats_panel.left) * presentation.all_perfect;
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x83F5FF));
            d2d_->accent_brush->SetOpacity(0.62f * (1.0f - 0.45f * presentation.all_perfect));
            ctx->DrawLine(D2D1::Point2F(beam_x - 70.0f, stats_panel.top + 8.0f),
                          D2D1::Point2F(beam_x, stats_panel.top + 8.0f),
                          d2d_->accent_brush.Get(), 2.0f);
            for (int index = 0; index < 18; ++index) {
                const float x = prism_center.x + (static_cast<float>((index * 83) % 360) - 180.0f);
                const float rise = presentation.all_perfect * (80.0f + (index % 5) * 26.0f);
                const float y = prism_bottom - 20.0f - rise;
                d2d_->accent_brush->SetOpacity((1.0f - presentation.all_perfect * 0.45f) * 0.48f);
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 2.0f, 4.0f),
                                 d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F continue_rect = skin_layout_rect(
            data, "result.continue", D2D1::RectF(1320.0f, 770.0f, 1848.0f, 856.0f));
        const D2D1_RECT_F replay_rect = skin_layout_rect(
            data, "result.replay", D2D1::RectF(1320.0f, 872.0f, 1574.0f, 942.0f));
        const D2D1_RECT_F retry_rect = skin_layout_rect(
            data, "result.retry", D2D1::RectF(1594.0f, 872.0f, 1848.0f, 942.0f));
        const float controls_alpha = presentation.interaction_ready
                                         ? std::max(0.35f, presentation.controls)
                                         : 0.16f;
        if (presentation.interaction_ready) {
            register_hit(continue_rect, MenuHitTargetKind::SettingsRow, 0);
            register_hit(retry_rect, MenuHitTargetKind::SettingsRow, 2);
            if (data.result.replay_available) {
                register_hit(replay_rect, MenuHitTargetKind::SettingsRow, 1);
            }
        }
        if (d2d_->play_brush) {
            const float saved = d2d_->play_brush->GetOpacity();
            set_brush_points(d2d_->play_brush.Get(), continue_rect);
            d2d_->play_brush->SetOpacity(controls_alpha);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(continue_rect, 12.0f, 12.0f),
                                      d2d_->play_brush.Get());
            d2d_->play_brush->SetOpacity(saved);
        } else {
            draw_result_panel(continue_rect, controls_alpha, true);
        }
        draw_result_text(data.result.continue_label.empty()
                             ? result_loc("CONTINUE  \xE2\x9E\xA1", "계속  \xE2\x9E\xA1")
                             : data.result.continue_label,
                         d2d_->menu_button_format.Get(),
                         continue_rect,
                         d2d_->text_brush.Get(),
                         controls_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_action_button(replay_rect, controls_alpha, data.result.replay_available);
        draw_action_button(retry_rect, controls_alpha, true);
        draw_result_text(data.result.replay_available
                             ? result_loc("\xE2\x96\xB6  WATCH REPLAY", "\xE2\x96\xB6  리플레이 보기")
                             : result_loc("REPLAY UNAVAILABLE", "리플레이 없음"),
                         d2d_->stats_value_format.Get(),
                         replay_rect,
                         data.result.replay_available
                             ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                             : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()),
                         controls_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(result_loc("\xE2\x86\xBB  RETRY", "\xE2\x86\xBB  재도전"),
                         d2d_->stats_value_format.Get(),
                         retry_rect,
                         d2d_->text_brush.Get(),
                         controls_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(presentation.interaction_ready
                             ? result_loc("ENTER / Continue    R / Retry    F1 / Replay",
                                          "ENTER / 계속    R / 재도전    F1 / 리플레이")
                             : result_loc("SPACE / Skip result reveal", "SPACE / 결과 연출 건너뛰기"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(1320.0f, 952.0f, 1848.0f, 982.0f),
                         d2d_->muted_brush.Get(),
                         presentation.interaction_ready ? controls_alpha : presentation.background,
                         DWRITE_TEXT_ALIGNMENT_CENTER);

        draw_result_text("TENRIFF PERFORMANCE ANALYSIS SYSTEM",
                         d2d_->hud_format.Get(),
                         D2D1::RectF(620.0f, 1022.0f, 1300.0f, 1050.0f),
                         d2d_->muted_brush.Get(),
                         0.46f * presentation.background,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
