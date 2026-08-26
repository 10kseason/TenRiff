        const double tick = static_cast<double>(GetTickCount64()) / 1000.0;
        const float logo_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 5.4, 0.18));
        const float button_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.2, 0.46));
        constexpr int bar_count = 18;
        constexpr float kSpectrumBarWidth = 18.0f;
        constexpr float kSpectrumBarGap = 12.0f;
        constexpr float kSpectrumMaxHeight = 84.0f;
        constexpr float kSpectrumNaturalWidth =
            bar_count * kSpectrumBarWidth + (bar_count - 1) * kSpectrumBarGap;
        const D2D1_RECT_F spectrum = skin_layout_rect(
            data, "title.spectrum",
            D2D1::RectF((kBaseWidth - kSpectrumNaturalWidth) * 0.5f, 150.0f - kSpectrumMaxHeight,
                        (kBaseWidth + kSpectrumNaturalWidth) * 0.5f, 150.0f));
        const float spectrum_scale_x = (spectrum.right - spectrum.left) / kSpectrumNaturalWidth;
        const float spectrum_scale_y = (spectrum.bottom - spectrum.top) / kSpectrumMaxHeight;
        const float bar_w = kSpectrumBarWidth * spectrum_scale_x;
        const float bar_gap = kSpectrumBarGap * spectrum_scale_x;

        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            for (int i = 0; i < bar_count; ++i) {
                const double phase = tick * 2.2 + static_cast<double>(i) * 0.45;
                const float height =
                    (18.0f + 66.0f * static_cast<float>(0.5 + 0.5 * std::sin(phase))) *
                    spectrum_scale_y;
                const float x0 = spectrum.left + static_cast<float>(i) * (bar_w + bar_gap);
                const D2D1_ROUNDED_RECT bar = D2D1::RoundedRect(
                    D2D1::RectF(x0, spectrum.bottom - height, x0 + bar_w, spectrum.bottom),
                    4.0f, 4.0f);
                d2d_->accent_brush->SetOpacity(0.42f + 0.18f * static_cast<float>(std::sin(phase) * 0.5 + 0.5));
                ctx->FillRoundedRectangle(bar, d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_horizon(344.0f, 186.0f, kBaseWidth - 186.0f, 820.0f, 0.10f, 0.28f + logo_pulse * 0.20f);
        draw_song_select_stardust(D2D1::RectF(114.0f, 96.0f, 1810.0f, 364.0f), 36, 0x491u, 0.09f);
        const ScreenContentBands bands =
            make_screen_content_bands(160.0f, 170.0f, false, 24.0f, 24.0f);

        const std::wstring logo_w = L"TenRiff";
        float logo_left = 590.0f;
        constexpr float kLogoTop = 184.0f;
        float logo_width = 740.0f;
        float logo_height = 104.0f;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> logo_layout;
        if (d2d_->dwrite_factory && d2d_->logo_format) {
            if (SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(logo_w.c_str(),
                                                                 static_cast<UINT32>(logo_w.size()),
                                                                 d2d_->logo_format.Get(),
                                                                 1024.0f,
                                                                 160.0f,
                                                                 &logo_layout)) &&
                logo_layout) {
                DWRITE_TEXT_METRICS logo_metrics{};
                if (SUCCEEDED(logo_layout->GetMetrics(&logo_metrics))) {
                    logo_width = std::ceil(std::max(logo_metrics.width, logo_metrics.widthIncludingTrailingWhitespace));
                    logo_height = std::ceil(logo_metrics.height);
                    logo_left = std::floor((kBaseWidth - logo_width) * 0.5f);
                } else {
                    logo_layout.Reset();
                }
            }
        }
        const D2D1_RECT_F logo_rect = skin_layout_rect(
            data, "title.logo",
            D2D1::RectF(logo_left, kLogoTop, logo_left + logo_width,
                        kLogoTop + std::max(96.0f, logo_height)));
        // The layout path draws from a point, so centre the measured wordmark in
        // whatever box it ends up in.
        const float logo_draw_left =
            logo_rect.left +
            std::max(0.0f, ((logo_rect.right - logo_rect.left) - logo_width) * 0.5f);
        const D2D1_RECT_F logo_shadow_rect = offset_rect(logo_rect, 0.0f, 6.0f);
        ID2D1Bitmap* title_logo_bitmap = data.lobby_skin.enabled
                                             ? find_song_card_preview_bitmap(data.lobby_skin.logo_path)
                                             : nullptr;
        const float logo_rule_y = logo_rect.bottom + 18.0f;
        const float logo_rule_gap = 42.0f;
        const float logo_rule_width = 280.0f;
        const D2D1_ROUNDED_RECT logo_rule_left =
            D2D1::RoundedRect(D2D1::RectF(std::max(220.0f, logo_rect.left - logo_rule_gap - logo_rule_width),
                                          logo_rule_y,
                                          std::max(220.0f, logo_rect.left - logo_rule_gap),
                                          logo_rule_y + 4.0f),
                              2.0f,
                              2.0f);
        const D2D1_ROUNDED_RECT logo_rule_right =
            D2D1::RoundedRect(D2D1::RectF(std::min(kBaseWidth - 220.0f, logo_rect.right + logo_rule_gap),
                                          logo_rule_y,
                                          std::min(kBaseWidth - 220.0f,
                                                   logo_rect.right + logo_rule_gap + logo_rule_width),
                                          logo_rule_y + 4.0f),
                              2.0f,
                              2.0f);
        if (d2d_->accent_brush) {
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(0.18f + logo_pulse * 0.10f);
            ctx->FillRoundedRectangle(logo_rule_left, d2d_->accent_brush.Get());
            ctx->FillRoundedRectangle(logo_rule_right, d2d_->accent_brush.Get());
            d2d_->accent_brush->SetOpacity(0.14f + logo_pulse * 0.06f);
            if (!title_logo_bitmap && d2d_->logo_format) {
                if (logo_layout) {
                    ctx->DrawTextLayout(D2D1::Point2F(logo_draw_left, logo_shadow_rect.top),
                                        logo_layout.Get(),
                                        d2d_->accent_brush.Get(),
                                        D2D1_DRAW_TEXT_OPTIONS_CLIP);
                } else {
                    draw_text_clipped_aligned(logo_w,
                                              d2d_->logo_format.Get(),
                                              logo_shadow_rect,
                                              d2d_->accent_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }
        if (title_logo_bitmap) {
            const D2D1_RECT_F fitted_logo = fit_rect_preserve_aspect(logo_rect,
                                                                     title_logo_bitmap->GetSize());
            ctx->DrawBitmap(title_logo_bitmap,
                            fitted_logo,
                            1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else if (d2d_->logo_format && d2d_->text_brush) {
            ID2D1Brush* brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                 : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), logo_rect);
            }
            if (brush) {
                if (logo_layout) {
                    ctx->DrawTextLayout(D2D1::Point2F(logo_draw_left, logo_rect.top),
                                        logo_layout.Get(),
                                        brush,
                                        D2D1_DRAW_TEXT_OPTIONS_CLIP);
                } else {
                    draw_text_clipped_aligned(logo_w,
                                              d2d_->logo_format.Get(),
                                              logo_rect,
                                              brush,
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }
        }

        const D2D1_RECT_F buttons_area = skin_layout_rect(
            data, "title.buttons",
            D2D1::RectF((kBaseWidth - 980.0f) * 0.5f, std::max(360.0f, bands.body_top - 6.0f),
                        (kBaseWidth + 980.0f) * 0.5f, bands.body_bottom - 8.0f));
        const float button_w = buttons_area.right - buttons_area.left;
        float button_h = 120.0f;
        float button_gap = 26.0f;
        const float button_left = buttons_area.left;
        const float button_stack_top = buttons_area.top;
        const float button_stack_bottom = buttons_area.bottom;
        const float natural_stack_height =
            button_h * static_cast<float>(data.title.buttons.size()) +
            button_gap * static_cast<float>(data.title.buttons.empty() ? 0 : data.title.buttons.size() - 1);
        const float button_available_height = std::max(0.0f, button_stack_bottom - button_stack_top);
        if (!data.title.buttons.empty() && natural_stack_height > button_available_height) {
            if (data.title.buttons.size() > 1) {
                button_gap = std::clamp(
                    std::floor((button_available_height - button_h * static_cast<float>(data.title.buttons.size())) /
                               static_cast<float>(data.title.buttons.size() - 1)),
                    12.0f,
                    26.0f);
            } else {
                button_gap = 0.0f;
            }
            const float fitted_button_h =
                (button_available_height -
                 button_gap * static_cast<float>(data.title.buttons.empty() ? 0 : data.title.buttons.size() - 1)) /
                static_cast<float>(std::max<std::size_t>(1, data.title.buttons.size()));
            button_h = std::clamp(std::floor(fitted_button_h), 104.0f, 120.0f);
        }
        const float button_stack_height =
            button_h * static_cast<float>(data.title.buttons.size()) +
            button_gap * static_cast<float>(data.title.buttons.empty() ? 0 : data.title.buttons.size() - 1);
        const float button_top = std::max(button_stack_top, button_stack_bottom - button_stack_height);

        for (std::size_t i = 0; i < data.title.buttons.size(); ++i) {
            const auto& button = data.title.buttons[i];
            const float y0 = button_top + static_cast<float>(i) * (button_h + button_gap);
            const D2D1_RECT_F rect = D2D1::RectF(button_left, y0, button_left + button_w, y0 + button_h);
            register_hit(rect, MenuHitTargetKind::TitleButton, static_cast<int>(i));

            ID2D1LinearGradientBrush* fill = nullptr;
            if (i == 0) {
                fill = d2d_->play_brush.Get();
            } else if (i == 1) {
                fill = d2d_->edit_brush.Get();
            } else if (i == 2) {
                fill = d2d_->options_brush.Get();
            } else if (i == 3) {
                fill = d2d_->exit_brush.Get();
            }

            draw_glass_panel(rect,
                             18.0f,
                             button.selected ? 0.84f : 0.74f,
                             button.selected ? (0.58f + button_pulse * 0.20f) : 0.22f,
                             button.selected,
                             8.0f);

            if (fill) {
                const D2D1_RECT_F accent_rect =
                    D2D1::RectF(rect.left + 18.0f, rect.top + 16.0f, rect.left + 34.0f, rect.bottom - 16.0f);
                const D2D1_ROUNDED_RECT accent_rr = D2D1::RoundedRect(accent_rect, 7.0f, 7.0f);
                const D2D1_RECT_F top_line =
                    D2D1::RectF(rect.left + 52.0f, rect.top + 14.0f, rect.right - 28.0f, rect.top + 18.0f);
                const D2D1_ROUNDED_RECT top_line_rr = D2D1::RoundedRect(top_line, 2.0f, 2.0f);
                set_brush_points(fill, rect);
                const float saved_opacity = fill->GetOpacity();
                fill->SetOpacity(button.selected ? 0.92f : 0.68f);
                ctx->FillRoundedRectangle(accent_rr, fill);
                fill->SetOpacity(button.selected ? 0.44f : 0.24f);
                ctx->FillRoundedRectangle(top_line_rr, fill);
                fill->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F icon_rect = D2D1::RectF(rect.left + 52.0f, rect.top, rect.left + 162.0f, rect.bottom);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(rect.left + 172.0f, rect.top, rect.right - 20.0f, rect.bottom);

            if (d2d_->menu_icon_format && d2d_->text_brush) {
                const std::wstring icon_w = to_wide(button.icon);
                if (!icon_w.empty()) {
                    draw_text_clipped(icon_w, d2d_->menu_icon_format.Get(), icon_rect, d2d_->text_brush.Get());
                }
            }

            if (d2d_->menu_button_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(button.label);
                draw_text_clipped(label_w, d2d_->menu_button_format.Get(), label_rect, d2d_->text_brush.Get());
            }
        }

        if (!data.title.guides.empty()) {
            const D2D1_RECT_F overlay_rect = performance_overlay_panel_rect();
            const float guide_top =
                data.performance.visible ? std::max(button_stack_top + 26.0f, overlay_rect.bottom + 26.0f)
                                         : (button_stack_top + 26.0f);
            const float guide_bottom = std::max(guide_top + 220.0f, bands.body_bottom - 24.0f);
            const D2D1_RECT_F guide_rect =
                skin_layout_rect(data, "title.guide",
                                 D2D1::RectF(1492.0f, guide_top, 1834.0f, guide_bottom));
            draw_glass_panel(guide_rect, 18.0f, 0.76f, 0.24f + logo_pulse * 0.10f, false, 8.0f);

            const D2D1_RECT_F guide_header_rect =
                D2D1::RectF(guide_rect.left + 26.0f, guide_rect.top + 18.0f, guide_rect.right - 26.0f, guide_rect.top + 64.0f);
            if (d2d_->body_format && d2d_->accent_brush) {
                const std::wstring guide_header_w = wloc("GUIDE", "가이드");
                draw_text_clipped(guide_header_w, d2d_->body_format.Get(), guide_header_rect, d2d_->accent_brush.Get());
            }
            draw_song_select_horizon(guide_rect.top + 72.0f,
                                     guide_rect.left + 22.0f,
                                     guide_rect.right - 22.0f,
                                     170.0f,
                                     0.08f,
                                     0.18f + logo_pulse * 0.10f);

            const float guide_lines_top = guide_rect.top + 98.0f;
            const float guide_lines_bottom = guide_rect.bottom - 20.0f;
            const float guide_available_height = std::max(0.0f, guide_lines_bottom - guide_lines_top);
            const float guide_row_pitch =
                std::clamp(std::floor(guide_available_height / static_cast<float>(data.title.guides.size())),
                           22.0f,
                           62.0f);
            const float guide_line_height = std::max(16.0f, guide_row_pitch - 6.0f);
            float guide_y = guide_lines_top;
            const D2D1_RECT_F guide_clip_rect =
                D2D1::RectF(guide_rect.left + 18.0f, guide_lines_top - 6.0f, guide_rect.right - 18.0f, guide_rect.bottom - 18.0f);
            ctx->PushAxisAlignedClip(guide_clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            for (std::size_t i = 0; i < data.title.guides.size(); ++i) {
                const D2D1_RECT_F line_rect =
                    D2D1::RectF(guide_rect.left + 26.0f, guide_y, guide_rect.right - 26.0f, guide_y + guide_line_height);
                if (d2d_->body_format && d2d_->text_brush) {
                    const std::wstring line_w = to_wide(data.title.guides[i]);
                    draw_text_clipped(line_w, d2d_->body_format.Get(), line_rect, d2d_->text_brush.Get());
                }
                guide_y += guide_row_pitch;
                if (i + 1 < data.title.guides.size() && d2d_->button_border_brush) {
                    const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                    d2d_->button_border_brush->SetOpacity(0.16f);
                    ctx->DrawLine(D2D1::Point2F(guide_rect.left + 24.0f, guide_y - 8.0f),
                                  D2D1::Point2F(guide_rect.right - 24.0f, guide_y - 8.0f),
                                  d2d_->button_border_brush.Get(),
                                  1.0f);
                    d2d_->button_border_brush->SetOpacity(saved_opacity);
                }
            }
            ctx->PopAxisAlignedClip();
        }

        draw_footer(data.title.profile, data.title.high_score, data.title.track);

