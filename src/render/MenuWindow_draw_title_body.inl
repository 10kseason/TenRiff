        const double tick = static_cast<double>(GetTickCount64()) / 1000.0;
        const float logo_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 5.4, 0.18));
        const float button_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.2, 0.46));
        const float bar_base_y = 150.0f;
        const int bar_count = 18;
        const float bar_w = 18.0f;
        const float bar_gap = 12.0f;
        const float total_w = bar_count * bar_w + (bar_count - 1) * bar_gap;
        const float start_x = (kBaseWidth - total_w) * 0.5f;

        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            for (int i = 0; i < bar_count; ++i) {
                const double phase = tick * 2.2 + static_cast<double>(i) * 0.45;
                const float height = 18.0f + 66.0f * static_cast<float>(0.5 + 0.5 * std::sin(phase));
                const float x0 = start_x + static_cast<float>(i) * (bar_w + bar_gap);
                const D2D1_ROUNDED_RECT bar =
                    D2D1::RoundedRect(D2D1::RectF(x0, bar_base_y - height, x0 + bar_w, bar_base_y),
                                      4.0f, 4.0f);
                d2d_->accent_brush->SetOpacity(0.42f + 0.18f * static_cast<float>(std::sin(phase) * 0.5 + 0.5));
                ctx->FillRoundedRectangle(bar, d2d_->accent_brush.Get());
            }
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_horizon(344.0f, 186.0f, kBaseWidth - 186.0f, 820.0f, 0.10f, 0.28f + logo_pulse * 0.20f);
        draw_song_select_stardust(D2D1::RectF(114.0f, 96.0f, 1810.0f, 364.0f), 36, 0x491u, 0.09f);

        const D2D1_RECT_F logo_rect = D2D1::RectF(0.0f, 160.0f, kBaseWidth, 320.0f);
        const std::wstring logo_w = L"TENRIFF";
        if (d2d_->logo_format && d2d_->text_brush) {
            ID2D1Brush* brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                 : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), logo_rect);
            }
            if (brush) {
                ctx->DrawText(logo_w.c_str(), static_cast<UINT32>(logo_w.size()),
                              d2d_->logo_format.Get(), logo_rect, brush);
            }
        }

        const float button_w = 980.0f;
        const float button_h = 120.0f;
        const float button_gap = 26.0f;
        const float button_left = (kBaseWidth - button_w) * 0.5f;
        const float button_top = 380.0f;

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
                    ctx->DrawText(icon_w.c_str(), static_cast<UINT32>(icon_w.size()),
                                  d2d_->menu_icon_format.Get(), icon_rect, d2d_->text_brush.Get());
                }
            }

            if (d2d_->menu_button_format && d2d_->text_brush) {
                const std::wstring label_w = to_wide(button.label);
                ctx->DrawText(label_w.c_str(), static_cast<UINT32>(label_w.size()),
                              d2d_->menu_button_format.Get(), label_rect, d2d_->text_brush.Get());
            }
        }

        if (!data.title.guides.empty()) {
            const D2D1_RECT_F guide_rect = D2D1::RectF(1492.0f, 396.0f, 1834.0f, 820.0f);
            draw_glass_panel(guide_rect, 18.0f, 0.76f, 0.24f + logo_pulse * 0.10f, false, 8.0f);

            const D2D1_RECT_F guide_header_rect =
                D2D1::RectF(guide_rect.left + 26.0f, guide_rect.top + 18.0f, guide_rect.right - 26.0f, guide_rect.top + 64.0f);
            if (d2d_->body_format && d2d_->accent_brush) {
                const std::wstring guide_header_w = L"GUIDE";
                draw_text_clipped(guide_header_w, d2d_->body_format.Get(), guide_header_rect, d2d_->accent_brush.Get());
            }
            draw_song_select_horizon(guide_rect.top + 72.0f,
                                     guide_rect.left + 22.0f,
                                     guide_rect.right - 22.0f,
                                     170.0f,
                                     0.08f,
                                     0.18f + logo_pulse * 0.10f);

            float guide_y = guide_rect.top + 98.0f;
            for (std::size_t i = 0; i < data.title.guides.size(); ++i) {
                const D2D1_RECT_F line_rect =
                    D2D1::RectF(guide_rect.left + 26.0f, guide_y, guide_rect.right - 26.0f, guide_y + 56.0f);
                if (d2d_->body_format && d2d_->text_brush) {
                    const std::wstring line_w = to_wide(data.title.guides[i]);
                    draw_text_clipped(line_w, d2d_->body_format.Get(), line_rect, d2d_->text_brush.Get());
                }
                guide_y += 68.0f;
                if (i + 1 < data.title.guides.size() && d2d_->button_border_brush) {
                    const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                    d2d_->button_border_brush->SetOpacity(0.16f);
                    ctx->DrawLine(D2D1::Point2F(guide_rect.left + 24.0f, guide_y - 10.0f),
                                  D2D1::Point2F(guide_rect.right - 24.0f, guide_y - 10.0f),
                                  d2d_->button_border_brush.Get(),
                                  1.0f);
                    d2d_->button_border_brush->SetOpacity(saved_opacity);
                }
            }
        }

        draw_footer(data.title.profile, data.title.high_score, data.title.track);

