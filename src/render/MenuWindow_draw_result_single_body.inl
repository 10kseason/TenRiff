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
            return loc(english, korean);
        };

        const auto semantic_brush = [&](uint32_t rgb) -> ID2D1Brush* {
            d2d_->note_fill_brush->SetColor(D2D1::ColorF(rgb));
            d2d_->note_fill_brush->SetOpacity(1.0f);
            return d2d_->note_fill_brush.Get();
        };
        const auto result_status_brush = [&](ID2D1Brush* fallback, const std::string& text,
                                              IDWriteTextFormat* format, const D2D1_RECT_F& bounds,
                                              DWRITE_TEXT_ALIGNMENT alignment) -> ID2D1Brush* {
            if (song_select_lamp_presentation(data.result.status).rainbow_badge) {
                auto paint_bounds = bounds;
                const float width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
                if (format && !text.empty() && width > 0.0f && height > 0.0f) {
                    const auto wide = to_wide(text);
                    const auto wrapping = format->GetWordWrapping();
                    const auto paragraph = format->GetParagraphAlignment();
                    DWRITE_TRIMMING trimming{};
                    Microsoft::WRL::ComPtr<IDWriteInlineObject> trimming_sign;
                    format->GetTrimming(&trimming, trimming_sign.GetAddressOf());
                    DWRITE_LINE_SPACING_METHOD line_method{};
                    float line_spacing = 0.0f, baseline = 0.0f;
                    format->GetLineSpacing(&line_method, &line_spacing, &baseline);
                    // Match draw_text_clipped's expanded layout and threshold.
                    // Its local scale transforms both glyphs and these brush
                    // coordinates, including narrow authored status rectangles.
                    const float estimated_scale = (data.lobby_skin.enabled || modern_menu_screen) &&
                        wrapping == DWRITE_WORD_WRAPPING_NO_WRAP
                        ? estimate_single_line_text_scale(wide, format->GetFontSize(), width, height) : 1.0f;
                    const float fit = estimated_scale < 0.995f ? estimated_scale : 1.0f;
                    auto& measure = d2d_->result_ex_status_measure;
                    if (!d2d_->result_ex_status_measure_attempted || measure.text != wide ||
                        measure.format.Get() != format || measure.width != width / fit || measure.height != height / fit ||
                        measure.alignment != alignment || measure.paragraph != paragraph || measure.wrapping != wrapping ||
                        measure.trimming.granularity != trimming.granularity || measure.trimming.delimiter != trimming.delimiter ||
                        measure.trimming.delimiterCount != trimming.delimiterCount || measure.trimming_sign.Get() != trimming_sign.Get() ||
                        measure.line_spacing_method != line_method || measure.line_spacing != line_spacing || measure.baseline != baseline) {
                        measure = {};
                        d2d_->result_ex_status_measure_attempted = true;
                        measure.text = wide; measure.format = format;
                        measure.width = width / fit; measure.height = height / fit; measure.alignment = alignment;
                        measure.paragraph = paragraph; measure.wrapping = wrapping; measure.trimming = trimming;
                        measure.trimming_sign = trimming_sign; measure.line_spacing_method = line_method;
                        measure.line_spacing = line_spacing; measure.baseline = baseline;
                        if (SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                            format, measure.width, measure.height, measure.layout.GetAddressOf())))
                            measure.layout->SetTextAlignment(alignment);
                    }
                    if (measure.layout) {
                        DWRITE_TEXT_METRICS metrics{};
                        if (SUCCEEDED(measure.layout->GetMetrics(&metrics))) {
                            paint_bounds.left = bounds.left + metrics.left;
                            paint_bounds.right = paint_bounds.left + std::max(1.0f, metrics.width);
                        }
                    }
                }
                if (!d2d_->result_ex_status_gradient_attempted) {
                    d2d_->result_ex_status_gradient_attempted = true;
                    std::array<D2D1_GRADIENT_STOP, kClearRainbowRgb.size()> stops;
                    for (std::size_t i = 0; i < stops.size(); ++i)
                        stops[i] = {kClearRainbowStops[i], D2D1::ColorF(kClearRainbowRgb[i])};
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                    if (SUCCEEDED(ctx->CreateGradientStopCollection(stops.data(), static_cast<UINT>(stops.size()), &collection)))
                        ctx->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                            D2D1::Point2F(paint_bounds.left, 0), D2D1::Point2F(paint_bounds.right, 0)),
                            collection.Get(), &d2d_->result_ex_status_gradient);
                }
                if (d2d_->result_ex_status_gradient) {
                    d2d_->result_ex_status_gradient->SetStartPoint(D2D1::Point2F(paint_bounds.left, 0));
                    d2d_->result_ex_status_gradient->SetEndPoint(D2D1::Point2F(paint_bounds.right, 0));
                    return d2d_->result_ex_status_gradient.Get();
                }
                return semantic_brush(kClearExRgb);
            }
            const auto rgb = result_gauge_shift_clear_color(data.result.status);
            return rgb ? semantic_brush(*rgb) : fallback;
        };
        const auto draw_result_outline = [&](const D2D1_RECT_F& rect, uint32_t rgb,
                                            float width, float alpha, float radius = 12.0f) {
            auto* brush = semantic_brush(rgb);
            brush->SetOpacity(std::clamp(alpha, 0.0f, 1.0f));
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush, width);
            brush->SetOpacity(1.0f);
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

        auto draw_result_control_text = [&](const std::string& text,
                                             IDWriteTextFormat* format,
                                             const D2D1_RECT_F& rect,
                                             ID2D1Brush* brush,
                                             float alpha) {
            if (!format) return;
            const auto saved_alignment = format->GetParagraphAlignment();
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            draw_result_text(text, format, rect, brush, alpha, DWRITE_TEXT_ALIGNMENT_CENTER);
            format->SetParagraphAlignment(saved_alignment);
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
                border->SetOpacity(saved_opacity * alpha * (modern_library_screen ? 0.24f : (accent ? 0.72f : 0.46f)));
                ctx->DrawRoundedRectangle(rounded, border, accent ? 1.4f : 1.0f);
                border->SetOpacity(saved_opacity);
            }
        };

        // Icons use geometry instead of a fallback font glyph. Measure and fit
        // the complete icon/label group, then center both on the same baseline.
        auto draw_result_action = [&](const std::string& text, const D2D1_RECT_F& rect,
                                      bool retry, float alpha) {
            auto& cache = d2d_->result_action_layouts[retry ? 1 : 0];
            auto* format = d2d_->result_action_format.Get();
            const auto wide = to_wide(text);
            if (!cache.layout || cache.text != wide || cache.format.Get() != format) {
                cache.layout.Reset(); cache.text = wide; cache.format = format;
                if (FAILED(d2d_->dwrite_factory->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                    format, 2048, 128, cache.layout.ReleaseAndGetAddressOf()))) return;
                cache.layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                cache.layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                cache.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
            DWRITE_TEXT_METRICS metrics{};
            cache.layout->GetMetrics(&metrics);
            const float icon = 24.0f, gap = 12.0f;
            const float width = icon + gap + metrics.widthIncludingTrailingWhitespace;
            const float height = std::max(icon, metrics.height);
            const float fit = std::min({1.0f, std::max(1.0f, rect.right - rect.left - 32.0f) / width,
                                      std::max(1.0f, rect.bottom - rect.top - 16.0f) / height});
            D2D1_MATRIX_3X2_F saved;
            ctx->GetTransform(&saved);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(fit, fit) * D2D1::Matrix3x2F::Translation(
                (rect.left + rect.right - width * fit) * 0.5f,
                (rect.top + rect.bottom - height * fit) * 0.5f) * saved);
            auto* brush = d2d_->text_brush.Get();
            const auto opacity = brush->GetOpacity(); brush->SetOpacity(alpha);
            const float cy = height * 0.5f;
            const std::size_t icon_slot = retry ? 1 : 0;
            auto& geometry = d2d_->result_action_icons[icon_slot];
            if (!d2d_->result_action_icons_attempted[icon_slot]) {
                d2d_->result_action_icons_attempted[icon_slot] = true;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                if (SUCCEEDED(d2d_->d2d_factory->CreatePathGeometry(geometry.GetAddressOf())) &&
                    SUCCEEDED(geometry->Open(sink.GetAddressOf()))) {
                    if (retry) {
                        sink->BeginFigure(D2D1::Point2F(20, -4), D2D1_FIGURE_BEGIN_HOLLOW);
                        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(12, -9), D2D1::SizeF(9, 9),
                            0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
                        sink->EndFigure(D2D1_FIGURE_END_OPEN);
                        sink->BeginFigure(D2D1::Point2F(17, -5), D2D1_FIGURE_BEGIN_HOLLOW);
                        sink->AddLine(D2D1::Point2F(21, -4));
                        sink->AddLine(D2D1::Point2F(21, -10));
                        sink->EndFigure(D2D1_FIGURE_END_OPEN);
                    } else {
                        sink->BeginFigure(D2D1::Point2F(3, -10), D2D1_FIGURE_BEGIN_FILLED);
                        sink->AddLine(D2D1::Point2F(21, 0));
                        sink->AddLine(D2D1::Point2F(3, 10));
                        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    }
                    if (FAILED(sink->Close())) geometry.Reset();
                }
            }
            if (geometry) {
                D2D1_MATRIX_3X2_F group_transform;
                ctx->GetTransform(&group_transform);
                ctx->SetTransform(D2D1::Matrix3x2F::Translation(0, cy) * group_transform);
                if (retry) ctx->DrawGeometry(geometry.Get(), brush, 2.6f);
                else ctx->FillGeometry(geometry.Get(), brush);
                ctx->SetTransform(group_transform);
            }
            draw_text_layout_readable(D2D1::Point2F(icon + gap, (height - metrics.height) * 0.5f),
                                      cache.layout.Get(), brush);
            brush->SetOpacity(opacity); ctx->SetTransform(saved);
        };

        // Secondary actions stay neutral in the native UI; the filled Continue
        // button owns the emphasis. Imported skins retain their accent wash.
        auto draw_action_button = [&](const D2D1_RECT_F& rect, float alpha, bool enabled) {
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 12.0f, 12.0f);
            if (d2d_->panel_brush) {
                const float saved_opacity = d2d_->panel_brush->GetOpacity();
                d2d_->panel_brush->SetOpacity(saved_opacity * alpha * 0.94f);
                ctx->FillRoundedRectangle(rounded, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(saved_opacity);
            }
            if (enabled && d2d_->button_selected_brush && !modern_library_screen) {
                const float saved_opacity = d2d_->button_selected_brush->GetOpacity();
                d2d_->button_selected_brush->SetOpacity(saved_opacity * alpha);
                ctx->FillRoundedRectangle(rounded, d2d_->button_selected_brush.Get());
                d2d_->button_selected_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->accent_brush && !modern_library_screen) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(saved_opacity * alpha * (enabled ? 0.90f : 0.32f));
                ctx->DrawRoundedRectangle(rounded, d2d_->accent_brush.Get(), 2.0f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (modern_library_screen && d2d_->button_border_brush) {
                const float saved = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(alpha * (enabled ? 0.85f : 0.35f));
                ctx->DrawRoundedRectangle(rounded, d2d_->button_border_brush.Get(), 1.0f);
                d2d_->button_border_brush->SetOpacity(saved);
            }
        };

        // These tabs use native geometry so old skin layouts cannot hide access
        // to analysis that was not part of their original result schema.
        const auto draw_analysis_tabs = [&]() {
            const std::array<std::string, 4> labels{{result_loc("SUMMARY", "요약"),
                result_loc("LANES", "레인"), result_loc("NOTE TYPES", "노트 종류"),
                result_loc("TIMELINE", "시간대")}};
            for (int i = 0; i < 4; ++i) {
                const auto rect = D2D1::RectF(674.0f + i * 176.0f, 44.0f,
                                             840.0f + i * 176.0f, 98.0f);
                const bool active = data.result.analysis_page == i;
                draw_result_panel(rect, 1.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 9, 9), semantic_brush(active ? 0x253E60 : 0x111B2A));
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 9, 9), semantic_brush(active ? 0x79D7FF : 0x526AA0), active ? 2.8f : 2.0f);
                draw_result_control_text(labels[static_cast<std::size_t>(i)], d2d_->result_action_format.Get(),
                    rect, semantic_brush(active ? 0xB5EDFF : 0xCDD8EF), 1.0f);
                if (presentation.interaction_ready) register_hit(rect, MenuHitTargetKind::SettingsRow, 3 + i);
            }
        };
        const auto draw_measured_histogram = [&](const gameplay::TimingDistribution& dist,
                                                 const D2D1_RECT_F& rect, float alpha) {
            if (!d2d_->accent_brush) return;
            const auto saved_color = d2d_->accent_brush->GetColor();
            const auto saved_opacity = d2d_->accent_brush->GetOpacity();
            const auto maximum = (std::max)(uint64_t{1}, *std::max_element(dist.histogram.begin(), dist.histogram.end()));
            const float width = (rect.right - rect.left) / static_cast<float>(dist.histogram.size());
            for (std::size_t i = 0; i < dist.histogram.size(); ++i) {
                const float height = static_cast<float>(static_cast<double>(dist.histogram[i]) /
                    static_cast<double>(maximum)) * (rect.bottom - rect.top);
                if (dist.histogram[i] == 0) continue;
                d2d_->accent_brush->SetColor(D2D1::ColorF(i < 21 ? 0x69A6FF : 0xFF7189));
                d2d_->accent_brush->SetOpacity(alpha);
                ctx->FillRectangle(D2D1::RectF(rect.left + width * static_cast<float>(i), rect.bottom - height,
                    rect.left + width * static_cast<float>(i + 1) - 1.0f, rect.bottom), d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        };
        if (data.result.analysis_page != 0) {
#include "MenuWindow_draw_result_analysis.inl"
            draw_web_upload_status();
            return;
        }

        ID2D1Bitmap* result_art =
            find_song_card_preview_bitmap(data.result.background_path);
        const D2D1_RECT_F full_result_rect = native_background_rect("result_single.rect.001");
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
            d2d_->panel_brush->SetColor(native_palette("palette.020914", D2D1::ColorF(0x020914)));
            d2d_->panel_brush->SetOpacity(0.56f + 0.22f * presentation.background);
            ctx->FillRectangle(full_result_rect, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetColor(saved_color);
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }

        const D2D1_POINT_2F prism_center = D2D1::Point2F(874.0f, 420.0f);
        if (d2d_->accent_brush && !modern_library_screen) {
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
                    (index % 3 == 0) ? native_palette("palette.a86cff", D2D1::ColorF(0xA86CFF)) : native_palette("palette.5cebff", D2D1::ColorF(0x5CEBFF)));
                d2d_->accent_brush->SetOpacity(
                    presentation.background * (0.06f + 0.10f * twinkle + all_perfect_boost));
                const float dot = 0.8f + static_cast<float>(index % 4) * 0.45f;
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), dot, dot),
                                 d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F header_line = native_rect("result_single.rect.002", D2D1::RectF(54.0f, 38.0f, 1866.0f, 122.0f));
        draw_native_asset(native_menu_assets::kMark, D2D1::RectF(54, 28, 118, 106), presentation.background);
        draw_native_wordmark(d2d_->header_format.Get(),
                             native_rect("result_single.rect.003", D2D1::RectF(136, 32, 398, 108)), presentation.background);
        draw_result_text(result_loc("RESULT", "결과"),
                         d2d_->title_format.Get(),
                         native_rect("result_single.rect.004", D2D1::RectF(410.0f, 44.0f, 600.0f, 84.0f)),
                         d2d_->muted_brush.Get(),
                         presentation.background);
        draw_result_text(result_loc("AFTER PERFORMANCE", "플레이 결과"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.005", D2D1::RectF(410.0f, 80.0f, 650.0f, 108.0f)),
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
            data, "result.profile", native_rect("result_single.rect.006", D2D1::RectF(1400.0f, 28.0f, 1848.0f, 108.0f)));
        draw_result_panel(profile_rect, presentation.background, false);
        if (presentation.background > 0.8f) draw_panel_glow(profile_rect, 12, 0x63DDEF, 1);
        const D2D1_RECT_F avatar_rect = skin_layout_rect(
            data, "result.avatar",
            native_rect("result_single.rect.007", D2D1::RectF(profile_rect.left + 15.0f, profile_rect.top + 9.0f,
                        profile_rect.left + 77.0f, profile_rect.top + 71.0f)));
        ctx->PushAxisAlignedClip(profile_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
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
        ctx->PopAxisAlignedClip();
        const float profile_text_left = std::max(profile_rect.left + 24.0f,
                                                 avatar_rect.right + 19.0f);
        draw_result_text(data.result.profile.empty() ? std::string("PLAYER") : data.result.profile,
                         d2d_->title_format.Get(),
                         native_rect("result_single.rect.008", D2D1::RectF(profile_text_left, profile_rect.top + 10.0f,
                                     profile_rect.right - 23.0f, profile_rect.top + 44.0f)),
                         d2d_->text_brush.Get(),
                         presentation.background);
        draw_result_text(result_loc("PERFORMANCE RESULT", "플레이 기록"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.009", D2D1::RectF(profile_text_left, profile_rect.top + 44.0f,
                                     profile_rect.right - 23.0f, profile_rect.bottom - 9.0f)),
                         d2d_->muted_brush.Get(),
                         presentation.background);

        const float information_slide = 34.0f * (1.0f - presentation.information);
        const D2D1_RECT_F song_panel = offset_rect(
            skin_layout_rect(data, "result.song_panel",
                             native_rect("result_single.rect.010", D2D1::RectF(70.0f, 154.0f, 590.0f, modern_library_screen ? 738.0f : 704.0f))),
            -information_slide, 0.0f);
        draw_result_panel(song_panel, presentation.information, true);
        if (modern_library_screen) draw_result_outline(song_panel, 0xA9BAC6, 1.6f, presentation.information);
        const float author_space = data.result.chart_author.empty() ? 0.0f : 26.0f;
        const D2D1_RECT_F art_rect =
            native_rect("result_single.rect.011", D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 42.0f,
                        song_panel.right - 44.0f, song_panel.top + 382.0f - author_space));
        if (result_art) {
            const D2D1_RECT_F source = centered_bitmap_source_rect(result_art->GetSize(), art_rect);
            ctx->DrawBitmap(result_art,
                            art_rect,
                            0.96f * presentation.information,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source);
        }
        draw_result_text(result_loc("TRACK", "플레이한 곡"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.012", D2D1::RectF(song_panel.left + 230.0f, song_panel.top + 14.0f,
                                     song_panel.right - 24.0f, song_panel.top + 38.0f)),
                         d2d_->accent_brush.Get(),
                         presentation.information,
                         DWRITE_TEXT_ALIGNMENT_TRAILING);
        draw_result_text(data.result.title.empty() ? std::string("Unknown Chart") : data.result.title,
                         d2d_->result_action_format.Get(),
                         native_rect("result_single.rect.013", D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 402.0f - author_space,
                                     song_panel.right - 44.0f, song_panel.top + 455.0f - author_space)),
                         d2d_->text_brush.Get(),
                         presentation.information);
        draw_result_text(data.result.artist.empty() ? std::string("Unknown Artist") : data.result.artist,
                         d2d_->result_body_format.Get(),
                         native_rect("result_single.rect.014", D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 455.0f - author_space,
                                     song_panel.right - 44.0f, song_panel.top + 488.0f - author_space)),
                         d2d_->muted_brush.Get(),
                         presentation.information);
        if (!data.result.chart_author.empty()) {
            draw_result_text("obj: " + data.result.chart_author, d2d_->body_format.Get(),
                             native_rect("result_single.chart_author", D2D1::RectF(song_panel.left + 44.0f, song_panel.top + 464.0f,
                                         song_panel.right - 44.0f, song_panel.top + 490.0f)),
                             d2d_->muted_brush.Get(), presentation.information);
        }

        const int display_level = data.result.level > 0 ? data.result.level : data.result.native_level;
        const std::string difficulty_label = !data.result.difficulty_label.empty()
                                                 ? data.result.difficulty_label
                                                 : (display_level > 0 ? "LV " + std::to_string(display_level) : "--");
        const std::array<std::pair<std::string, std::string>, 2> chart_metadata = {{
            {data.result.has_bpm_changes ? "BPM!" : "BPM",
             data.result.bpm > 0.0 ? format_decimal(data.result.bpm) : "--"},
            {"LV", difficulty_label},
        }};
        const float metadata_width = (song_panel.right - song_panel.left - 88.0f) / 2.0f;
        for (std::size_t index = 0; index < chart_metadata.size(); ++index) {
            const float left = song_panel.left + 44.0f + metadata_width * static_cast<float>(index);
            auto* value_brush = index == 1 ? semantic_brush(kDifficultyLevelColor) : d2d_->text_brush.Get();
            draw_result_text(chart_metadata[index].first,
                             d2d_->result_label_format.Get(),
                             native_rect("result_single.rect.015", D2D1::RectF(left, song_panel.top + 494.0f,
                                         left + metadata_width - 8.0f, song_panel.top + 522.0f)),
                             index == 1 ? value_brush : d2d_->muted_brush.Get(),
                             presentation.information, DWRITE_TEXT_ALIGNMENT_CENTER);
            const auto value_rect = native_rect("result_single.rect.016", D2D1::RectF(left, song_panel.top + 522.0f,
                                               left + metadata_width - 8.0f, song_panel.top + 568.0f));
            if (index == 1) {
                const float saved_opacity = value_brush->GetOpacity();
                value_brush->SetOpacity(saved_opacity * presentation.information);
                draw_difficulty_label(to_wide(difficulty_display_label(chart_metadata[index].second,
                                      data.result.difficulty_is_aery_five_key)), d2d_->song_metadata_value_format.Get(),
                                      value_rect, value_brush, data.result.difficulty_is_aery_five_key,
                                      DWRITE_TEXT_ALIGNMENT_CENTER, data.result.difficulty_prefix_key_count);
                value_brush->SetOpacity(saved_opacity);
            } else {
                draw_result_text(chart_metadata[index].second, d2d_->song_metadata_value_format.Get(),
                                 value_rect, value_brush, presentation.information,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }

        const float prism_bottom = native_metric("result_single.prism_bottom", 715.0f);
        if (modern_library_screen) {
#include "MenuWindow_draw_result_summary.inl"
        } else {
        const double score_maximum = data.result.max_score > 0
                                         ? static_cast<double>(data.result.max_score)
                                         : 1.0;
        const float score_quality = static_cast<float>(
            std::clamp(static_cast<double>(data.result.score) / score_maximum, 0.0, 1.0));
        const float failed_completion = result_success ? 1.0f : 0.76f;
        const float prism_completion = presentation.prism * failed_completion;
        const float prism_top = 116.0f + (1.0f - score_quality) * 20.0f;

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
                            : (result_success ? native_palette("palette.6be9ff", D2D1::ColorF(0x6BE9FF))
                                              : native_palette("palette.75698e", D2D1::ColorF(0x75698E)));
                    d2d_->accent_brush->SetColor(prism_color);
                    d2d_->accent_brush->SetOpacity(0.055f * presentation.prism);
                    ctx->FillGeometry(prism_geometry.Get(), d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetOpacity((0.48f + 0.35f * score_quality) * presentation.prism);
                    ctx->DrawGeometry(prism_geometry.Get(), d2d_->accent_brush.Get(), 2.3f);
                    d2d_->accent_brush->SetColor(native_palette("palette.b57cff", D2D1::ColorF(0xB57CFF)));
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
                                                             : native_palette("palette.9e65ff", D2D1::ColorF(0x9E65FF)));
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
            native_rect("result_single.rect.017", D2D1::RectF(prism_center.x - 260.0f,
                        158.0f - 5.0f * presentation.score_pulse,
                        prism_center.x + 260.0f,
                        230.0f + 5.0f * presentation.score_pulse));
        draw_result_text(std::to_string(counted_score) + " / " +
                             std::to_string(data.result.max_score),
                         d2d_->title_format.Get(),
                         score_rect,
                         modern_library_screen ? native_color(0xFFFFFF, 1) : d2d_->text_brush.Get(),
                         score_alpha * pulse_scale,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(result_loc("SCORE", "점수"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.018", D2D1::RectF(prism_center.x - 120.0f, 132.0f,
                                     prism_center.x + 120.0f, 160.0f)),
                         modern_library_screen ? native_color(0xFFFFFF, 1) : d2d_->muted_brush.Get(),
                         score_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(result_loc("DETAIL SCORE", "상세 점수") + "  " +
                             std::to_string(data.result.detail_score) + " / " +
                             std::to_string(data.result.max_detail_score),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.019", D2D1::RectF(prism_center.x - 180.0f, 238.0f,
                                     prism_center.x + 180.0f, 268.0f)),
                         modern_library_screen ? native_color(0xFFFFFF, 1) : d2d_->accent_brush.Get(),
                         score_alpha,
                         DWRITE_TEXT_ALIGNMENT_CENTER);

        const std::string rank_text = data.result.rank.empty() ? std::string("--") : data.result.rank;
        const D2D1_RECT_F rank_rect =
            native_rect("result_single.rect.020", D2D1::RectF(prism_center.x - 250.0f, 285.0f,
                        prism_center.x + 250.0f, 500.0f));
        if (presentation.chromatic > 0.0f && result_success) {
            draw_result_text(rank_text,
                             d2d_->rank_format.Get(),
                             native_rect("result_single.rect.021", D2D1::RectF(rank_rect.left - 4.0f, rank_rect.top,
                                         rank_rect.right - 4.0f, rank_rect.bottom)),
                             d2d_->accent_brush.Get(),
                             presentation.rank * presentation.chromatic * 0.55f,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_result_text(rank_text,
                             d2d_->rank_format.Get(),
                             native_rect("result_single.rect.022", D2D1::RectF(rank_rect.left + 4.0f, rank_rect.top,
                                         rank_rect.right + 4.0f, rank_rect.bottom)),
                             d2d_->muted_brush.Get(),
                             presentation.rank * presentation.chromatic * 0.45f,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        draw_grade(0, to_wide(rank_text), d2d_->rank_format.Get(), rank_rect,
                   presentation.rank, DWRITE_TEXT_ALIGNMENT_CENTER);

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
            native_rect("result_single.rect.023", D2D1::RectF(prism_center.x - 260.0f, 526.0f,
                        prism_center.x + 260.0f, 586.0f));
        const auto status_text = localized_result_status();
        draw_result_text(status_text,
                         d2d_->title_format.Get(),
                         status_rect,
                         result_status_brush(result_success ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                                           : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()),
                                             status_text, d2d_->title_format.Get(), status_rect, DWRITE_TEXT_ALIGNMENT_CENTER),
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
                             d2d_->result_label_format.Get(),
                             native_rect("result_single.rect.024", D2D1::RectF(prism_center.x - 210.0f, 590.0f,
                                         prism_center.x + 210.0f, 622.0f)),
                             d2d_->accent_brush.Get(),
                             presentation.all_perfect,
                             DWRITE_TEXT_ALIGNMENT_CENTER);
        } else if (data.result.full_combo) {
            draw_result_text(result_loc("FULL COMBO", "풀 콤보"),
                             d2d_->result_label_format.Get(),
                             native_rect("result_single.rect.025", D2D1::RectF(prism_center.x - 160.0f, 590.0f,
                                         prism_center.x + 160.0f, 622.0f)),
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
                d2d_->accent_brush->SetColor((index % 2 == 0) ? native_palette("palette.70ecff", D2D1::ColorF(0x70ECFF))
                                                              : native_palette("palette.b379ff", D2D1::ColorF(0xB379FF)));
                d2d_->accent_brush->SetOpacity((1.0f - presentation.rank) * 0.72f);
                ctx->DrawLine(start, end, d2d_->accent_brush.Get(), 1.6f);
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        }

        // Left edge shares the CONTINUE/REPLAY/RETRY column below it so the right
        // half of the screen reads as one column instead of two ragged ones.
        const D2D1_RECT_F analysis_panel = skin_layout_rect(
            data, "result.analysis_panel", native_rect("result_single.rect.026", D2D1::RectF(1320.0f, 154.0f, 1848.0f, modern_library_screen ? 738.0f : 704.0f)));
        draw_result_panel(analysis_panel, presentation.graph, false);
        draw_result_outline(analysis_panel, 0x28C76B, 3.0f, presentation.graph);
        draw_result_text(result_loc("PERFORMANCE", "플레이 분석"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.027", D2D1::RectF(analysis_panel.left + 24.0f, analysis_panel.top + 18.0f,
                                     analysis_panel.right - 24.0f, analysis_panel.top + 48.0f)),
                         d2d_->accent_brush.Get(),
                         presentation.graph);
        draw_result_text(result_loc("MEASURED TIMING", "실측 타이밍 분포") +
                         "  N=" + (data.result.analysis.available ? std::to_string(data.result.analysis.total.measured_count) : "--"),
                         d2d_->result_body_format.Get(),
                         native_rect("result_single.rect.028", D2D1::RectF(analysis_panel.left + 26.0f, analysis_panel.top + 66.0f,
                                     analysis_panel.right - 26.0f, analysis_panel.top + 98.0f)),
                         d2d_->text_brush.Get(),
                         presentation.graph);
        const D2D1_RECT_F timing_track =
            native_rect("result_single.rect.029", D2D1::RectF(analysis_panel.left + 30.0f, analysis_panel.top + 118.0f,
                        analysis_panel.right - 30.0f, analysis_panel.top + 226.0f));
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
            if (data.result.analysis.available) {
                draw_measured_histogram(data.result.analysis.total, D2D1::RectF(timing_track.left + 12,
                    timing_track.top + 52, timing_track.right - 12, timing_track.bottom - 12), presentation.graph);
            } else {
                draw_result_text(result_loc("Analysis unavailable for this record", "이 기록에는 분석 데이터가 없습니다"),
                    d2d_->result_label_format.Get(), timing_track, d2d_->muted_brush.Get(), presentation.graph,
                    DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            d2d_->accent_brush->SetColor(native_palette("palette.f7fafd", D2D1::ColorF(0xF7FAFD)));
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
                native_rect("result_single.rect.030", D2D1::RectF(timing_track.left + 12.0f, timing_track.top + 8.0f,
                            center_x - 12.0f, timing_track.top + 46.0f));
            const D2D1_RECT_F slow_rect =
                native_rect("result_single.rect.031", D2D1::RectF(center_x + 12.0f, timing_track.top + 8.0f,
                            timing_track.right - 12.0f, timing_track.top + 46.0f));
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x69A6FF));
            draw_result_text("FAST  " + std::to_string(data.result.fast_count),
                             d2d_->result_action_format.Get(),
                             fast_rect,
                             d2d_->accent_brush.Get(),
                             presentation.graph,
                             DWRITE_TEXT_ALIGNMENT_LEADING);
            d2d_->accent_brush->SetColor(D2D1::ColorF(0xFF7189));
            draw_result_text("SLOW  " + std::to_string(data.result.slow_count),
                             d2d_->result_action_format.Get(),
                             slow_rect,
                             d2d_->accent_brush.Get(),
                             presentation.graph,
                             DWRITE_TEXT_ALIGNMENT_TRAILING);
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }
        draw_result_text(result_loc("MEAN", "평균") + "  " +
                         (data.result.analysis.available && data.result.analysis.total.measured_count > 0
                            ? format_signed_ms(data.result.mean_delta_ms) : std::string("--")),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.032", D2D1::RectF(timing_track.left, timing_track.bottom + 8.0f,
                                     center_x, timing_track.bottom + 36.0f)),
                         d2d_->muted_brush.Get(),
                         presentation.graph);
        draw_result_text(result_loc("STD. DEV.", "표준편차") + "  " +
                             (data.result.analysis.available && data.result.analysis.total.measured_count > 1
                                ? format_decimal(data.result.stddev_delta_ms) + "ms" : std::string("--")),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.033", D2D1::RectF(center_x, timing_track.bottom + 8.0f,
                                     timing_track.right, timing_track.bottom + 36.0f)),
                         d2d_->muted_brush.Get(),
                         presentation.graph,
                         DWRITE_TEXT_ALIGNMENT_TRAILING);

        draw_result_text(result_loc("GAUGE CONSISTENCY", "게이지 변화"),
                         d2d_->result_body_format.Get(),
                         native_rect("result_single.rect.034", D2D1::RectF(analysis_panel.left + 26.0f, analysis_panel.top + 286.0f,
                                     analysis_panel.right - 26.0f, analysis_panel.top + 318.0f)),
                         d2d_->text_brush.Get(),
                         presentation.graph);
        const D2D1_RECT_F gauge_plot =
            native_rect("result_single.rect.035", D2D1::RectF(analysis_panel.left + 30.0f, analysis_panel.top + 332.0f,
                        analysis_panel.right - 30.0f, analysis_panel.bottom - 36.0f));
        // Gridlines alone do not say what height means, so each one carries its
        // percentage and the plot keeps a left gutter for those labels.
        const float gauge_axis_width = native_metric("result_single.gauge_axis_width", 62.0f);
        const D2D1_RECT_F gauge_track =
            native_rect("result_single.rect.036", D2D1::RectF(gauge_plot.left + gauge_axis_width, gauge_plot.top,
                        gauge_plot.right, gauge_plot.bottom));
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
                                 d2d_->result_label_format.Get(),
                                 native_rect("result_single.rect.037", D2D1::RectF(gauge_plot.left, y - 16.0f,
                                             gauge_track.left - 8.0f, y + 16.0f)),
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
            // One vertical gradient colors even steep drops by actual gauge
            // height, with no per-frame gradient or per-segment allocation.
            if (!d2d_->result_gauge_gradient) {
                const D2D1_GRADIENT_STOP stops[] = {{0, D2D1::ColorF(0x57E397)},
                    {0.5f, D2D1::ColorF(0xF3D26A)}, {1, D2D1::ColorF(0xFF566D)}};
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 3, collection.GetAddressOf())))
                    ctx->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(0, gauge_track.top), D2D1::Point2F(0, gauge_track.bottom)),
                        collection.Get(), d2d_->result_gauge_gradient.GetAddressOf());
            }
            if (d2d_->result_gauge_gradient) {
                d2d_->result_gauge_gradient->SetStartPoint(D2D1::Point2F(0, gauge_track.top));
                d2d_->result_gauge_gradient->SetEndPoint(D2D1::Point2F(0, gauge_track.bottom));
                d2d_->result_gauge_gradient->SetOpacity(presentation.graph);
            }
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
                ctx->DrawLine(p0, p1, d2d_->result_gauge_gradient
                    ? static_cast<ID2D1Brush*>(d2d_->result_gauge_gradient.Get())
                    : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get()), 3.0f);
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F stats_panel = skin_layout_rect(
            data, "result.stats_panel", native_rect("result_single.rect.038", D2D1::RectF(70.0f, 770.0f, 1270.0f, 994.0f)));
        draw_result_panel(stats_panel, presentation.statistics.front(), true);
        if (modern_library_screen) {
            draw_result_outline(stats_panel, 0xA9BAC6, 1.6f, presentation.statistics.front());
            // One outer frame joins the song, score and judgement areas. Resolve
            // all actual skin rectangles so custom layouts keep their geometry.
            const auto summary_bounds = native_rect("result_summary.rect.001", D2D1::RectF(620, 154, 1270, 738));
            const auto group = D2D1::RectF(std::min({song_panel.left, summary_bounds.left, stats_panel.left}) - 7,
                std::min({song_panel.top, summary_bounds.top, stats_panel.top}) - 7,
                std::max({song_panel.right, summary_bounds.right, stats_panel.right}) + 7,
                std::max({song_panel.bottom, summary_bounds.bottom, stats_panel.bottom}) + 7);
            draw_result_outline(group, 0xD4E0E8, 2.4f,
                std::min(presentation.information, presentation.statistics.front()), 16.0f);
        }
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
            {"P-GREAT", std::to_string(data.result.perfect), percent_for(data.result.perfect), D2D1::ColorF(0xFFE477)},
            {"GREAT", std::to_string(data.result.great), percent_for(data.result.great), D2D1::ColorF(0x90E5F5)},
            {"GOOD", std::to_string(data.result.good), percent_for(data.result.good), D2D1::ColorF(0xFFAA60)},
            {"POOR", std::to_string(data.result.poor), percent_for(data.result.poor), D2D1::ColorF(0xFF647A)},
            {"FAIL", std::to_string(data.result.bad), percent_for(data.result.bad), D2D1::ColorF(0xC399FF)},
            {result_loc("MAX COMBO", "최대 콤보"), std::to_string(data.result.max_combo),
             data.result.full_combo ? result_loc("FULL COMBO", "풀 콤보") : "COMBO / " + std::to_string(data.result.total_notes), D2D1::ColorF(0xCBEF6B)},
            {result_loc("ACCURACY", "정확도"), format_decimal(data.result.accuracy) + "%",
             result_loc("DETAIL", "상세") + " " + format_decimal(data.result.detailed_accuracy) + "%", D2D1::ColorF(0x63E9FF)},
        }};
        // The native summary owns both accuracies. Legacy image layouts still
        // need their original accuracy cell because they do not draw that summary.
        const std::size_t statistic_count = modern_library_screen ? 6 : result_statistics.size();
        constexpr std::array<float, 7> legacy_statistic_weights = {1, 1, 1, 1, 1, 1.15f, 1.35f};
        const float statistic_unit = (stats_panel.right - stats_panel.left) / (modern_library_screen ? 6.0f : 7.5f);
        float statistic_left = stats_panel.left;
        for (std::size_t index = 0; index < statistic_count; ++index) {
            const float alpha = presentation.statistics[index];
            const float rise = 18.0f * (1.0f - alpha);
            const float left = statistic_left;
            const float statistic_width = statistic_unit * (modern_library_screen ? 1.0f : legacy_statistic_weights[index]);
            statistic_left += statistic_width;
            const D2D1_RECT_F cell =
                native_rect("result_single.rect.039", D2D1::RectF(left, stats_panel.top, left + statistic_width, stats_panel.bottom));
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
                    (index < 5 && result_statistics[index].value == "0") ? 0.70f : 1.0f;
                d2d_->accent_brush->SetOpacity(alpha * zero_scale);
                draw_result_text(result_statistics[index].label,
                                 d2d_->result_body_format.Get(),
                                 native_rect("result_single.rect.040", D2D1::RectF(cell.left + 14.0f, cell.top + 28.0f + rise,
                                             cell.right - 14.0f, cell.top + 58.0f + rise)),
                                 d2d_->accent_brush.Get(),
                                 alpha * zero_scale,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_result_text(result_statistics[index].value,
                                 modern_library_screen ? (index < 5 ? d2d_->result_judgement_format.Get() : d2d_->result_metric_format.Get())
                                                       : d2d_->stats_value_format.Get(),
                                 native_rect("result_single.rect.041", D2D1::RectF(cell.left + 10.0f, cell.top + 70.0f + rise,
                                             cell.right - 10.0f, cell.top + 138.0f + rise)),
                                 d2d_->accent_brush.Get(),
                                 alpha * zero_scale,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
                const auto detail_rect = native_rect("result_single.rect.042", D2D1::RectF(cell.left + 10.0f, cell.top + 153.0f + rise,
                                             cell.right - 10.0f, cell.top + 190.0f + rise));
                if (!(index == 5 && data.result.all_combo_badge && draw_all_combo_image(detail_rect, alpha)))
                    draw_result_text(result_statistics[index].detail,
                                 d2d_->result_label_format.Get(),
                                 detail_rect,
                                 d2d_->accent_brush.Get(),
                                 alpha * zero_scale,
                                 DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
        }

        if (!modern_library_screen && data.result.all_perfect && d2d_->accent_brush && presentation.all_perfect > 0.0f) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            const float beam_x = stats_panel.left +
                                 (stats_panel.right - stats_panel.left) * presentation.all_perfect;
            d2d_->accent_brush->SetColor(native_palette("palette.83f5ff", D2D1::ColorF(0x83F5FF)));
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
            data, "result.continue", native_rect("result_single.rect.043", D2D1::RectF(1320.0f, 770.0f, 1848.0f, 856.0f)));
        const D2D1_RECT_F replay_rect = skin_layout_rect(
            data, "result.replay", native_rect("result_single.rect.044", D2D1::RectF(1320.0f, 872.0f, 1574.0f, 942.0f)));
        const D2D1_RECT_F retry_rect = skin_layout_rect(
            data, "result.retry", native_rect("result_single.rect.045", D2D1::RectF(1594.0f, 872.0f, 1848.0f, 942.0f)));
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
        if (modern_library_screen) {
            auto* fill = semantic_brush(0x79D7FF);
            fill->SetOpacity(controls_alpha);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(continue_rect, 12, 12), fill);
            fill->SetOpacity(1.0f);
        } else if (d2d_->play_brush) {
            const float saved = d2d_->play_brush->GetOpacity();
            set_brush_points(d2d_->play_brush.Get(), continue_rect);
            d2d_->play_brush->SetOpacity(controls_alpha);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(continue_rect, 12.0f, 12.0f),
                                      d2d_->play_brush.Get());
            d2d_->play_brush->SetOpacity(saved);
        } else {
            draw_result_panel(continue_rect, controls_alpha, true);
        }
        const D2D1_COLOR_F saved_continue_text = d2d_->text_brush->GetColor();
        if (presentation.interaction_ready) draw_native_focus(continue_rect, 12, 91, 0, true, true);
        if (modern_library_screen) d2d_->text_brush->SetColor(native_palette("palette.0b1620", D2D1::ColorF(0x0B1620)));
        draw_result_control_text(data.result.continue_label.empty()
                             ? result_loc("CONTINUE  \xE2\x9E\xA1", "계속  \xE2\x9E\xA1")
                             : data.result.continue_label,
                         d2d_->menu_button_format.Get(),
                         continue_rect,
                         d2d_->text_brush.Get(),
                         controls_alpha);
        d2d_->text_brush->SetColor(saved_continue_text);
        draw_action_button(replay_rect, controls_alpha, data.result.replay_available);
        draw_action_button(retry_rect, controls_alpha, true);
        if (data.result.replay_available) {
            draw_result_action(result_loc("WATCH REPLAY", "리플레이 보기"), replay_rect, false, controls_alpha);
        } else {
            draw_result_control_text(result_loc("REPLAY UNAVAILABLE", "리플레이 없음"),
                d2d_->result_action_format.Get(), replay_rect, d2d_->muted_brush.Get(), controls_alpha);
        }
        draw_result_action(result_loc("RETRY", "재도전"), retry_rect, true, controls_alpha);
        draw_result_text(presentation.interaction_ready
                             ? result_loc("ENTER / Continue    R / Retry    F1 / Replay",
                                          "ENTER / 계속    R / 재도전    F1 / 리플레이")
                             : result_loc("SPACE / Skip result reveal", "SPACE / 결과 연출 건너뛰기"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_single.rect.046", D2D1::RectF(1320.0f, 952.0f, 1848.0f, 982.0f)),
                         d2d_->muted_brush.Get(),
                         presentation.interaction_ready ? controls_alpha : presentation.background,
                         DWRITE_TEXT_ALIGNMENT_CENTER);

        draw_result_text(result_loc("F9 / Screenshot (PNG)  >  screenshots/",
                                    "F9 / 스크린샷 (PNG)  >  screenshots/"),
                         d2d_->result_body_format.Get(),
                         native_rect("result_single.rect.047", D2D1::RectF(620.0f, 1022.0f, 1300.0f, 1050.0f)),
                         d2d_->muted_brush.Get(),
                         0.82f * presentation.background,
                         DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_analysis_tabs();
