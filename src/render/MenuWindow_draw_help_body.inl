        if (!data.help_overlay.visible) {
            return;
        }

        if (d2d_->panel_brush) {
            const float saved_opacity = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetOpacity(0.88f);
            ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight), d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F panel_rect = D2D1::RectF(260.0f, 132.0f, kBaseWidth - 260.0f, kBaseHeight - 128.0f);
        draw_glass_panel(panel_rect, 24.0f, 0.92f, 0.56f, true, 8.0f);

        if (d2d_->header_format && d2d_->text_brush) {
            const D2D1_RECT_F title_rect =
                D2D1::RectF(panel_rect.left + 34.0f, panel_rect.top + 28.0f, panel_rect.right - 34.0f, panel_rect.top + 92.0f);
            draw_text_clipped(to_wide(data.help_overlay.title), d2d_->header_format.Get(), title_rect, d2d_->text_brush.Get());
        }
        if (d2d_->accent_brush) {
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(0.24f);
            ctx->DrawLine(D2D1::Point2F(panel_rect.left + 32.0f, panel_rect.top + 104.0f),
                          D2D1::Point2F(panel_rect.right - 32.0f, panel_rect.top + 104.0f),
                          d2d_->accent_brush.Get(),
                          1.4f);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        float line_y = panel_rect.top + 130.0f;
        for (const auto& line : data.help_overlay.lines) {
            if (line_y > panel_rect.bottom - 120.0f) {
                break;
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(panel_rect.left + 34.0f, line_y + 10.0f, panel_rect.left + 42.0f, line_y + 18.0f),
                                      3.0f,
                                      3.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                const D2D1_RECT_F line_rect =
                    D2D1::RectF(panel_rect.left + 58.0f, line_y, panel_rect.right - 42.0f, line_y + 44.0f);
                draw_text_clipped(to_wide(line), d2d_->body_format.Get(), line_rect, d2d_->text_brush.Get());
            }
            line_y += 56.0f;
        }

        if (!data.help_overlay.footer.empty() && d2d_->body_format && d2d_->muted_brush) {
            const D2D1_RECT_F footer_rect =
                D2D1::RectF(panel_rect.left + 40.0f, panel_rect.bottom - 88.0f, panel_rect.right - 40.0f, panel_rect.bottom - 34.0f);
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(to_wide(data.help_overlay.footer), d2d_->body_format.Get(), footer_rect, d2d_->muted_brush.Get());
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

