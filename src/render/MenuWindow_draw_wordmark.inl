    // A single cached glyph layout is shared by home, library, settings and
    // result headers. The TI symbol is drawn separately and remains still.
    auto draw_native_wordmark = [&](IDWriteTextFormat* format, const D2D1_RECT_F& rect, float opacity = 1.0f) {
        if (!format || !d2d_->text_brush || rect.right <= rect.left || rect.bottom <= rect.top) return;
        opacity = std::clamp(opacity, 0.0f, 1.0f);
        if (!native_motion_screen || !d2d_->native_menu_brush || !d2d_->dwrite_factory) {
            const float saved_opacity = d2d_->text_brush->GetOpacity();
            d2d_->text_brush->SetOpacity(saved_opacity * opacity);
            draw_text_clipped(L"TENRIFF", format, rect, d2d_->text_brush.Get());
            d2d_->text_brush->SetOpacity(saved_opacity);
            return;
        }
        if (!d2d_->native_wordmark_layout || d2d_->native_wordmark_format != format) {
            d2d_->native_wordmark_layout.Reset();
            if (FAILED(d2d_->dwrite_factory->CreateTextLayout(L"TENRIFF", 7, format, 2048, 256,
                d2d_->native_wordmark_layout.ReleaseAndGetAddressOf()))) return;
            d2d_->native_wordmark_layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            d2d_->native_wordmark_layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            d2d_->native_wordmark_layout->GetMetrics(&d2d_->native_wordmark_metrics);
            d2d_->native_wordmark_format = format;
        }
        const auto& metrics = d2d_->native_wordmark_metrics;
        const float scale = std::min({1.0f, (rect.right - rect.left - 4.0f) / std::max(1.0f, metrics.width),
            (rect.bottom - rect.top - 4.0f) / std::max(1.0f, metrics.height)});
        if (scale <= 0.0f) return;
        const auto motion = native_wordmark_motion(native_seconds, native_menu_motion_.moving());
        D2D1_MATRIX_3X2_F saved;
        ctx->GetTransform(&saved);
        ctx->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
            D2D1::Matrix3x2F::Translation(rect.left + 2.0f, rect.top + 2.0f) * saved);
        auto glyphs = [&](float dx, float dy, uint32_t color, float alpha, bool contour = false) {
            draw_text_layout_readable(D2D1::Point2F(dx, dy), d2d_->native_wordmark_layout.Get(),
                               native_color(color, alpha * opacity), D2D1_DRAW_TEXT_OPTIONS_CLIP, contour);
        };
        glyphs(-1.6f, 0.8f, 0x63E9F2, motion.bloom * native_intensity);
        glyphs(1.7f, 0.0f, 0xB7A7FF, motion.bloom * native_intensity);
        glyphs(0, 0, 0xF1FAFF, 1.0f, true);
        const float scan_y = metrics.height * motion.scan;
        ctx->PushAxisAlignedClip(D2D1::RectF(0, scan_y, metrics.width + 2, scan_y + 10), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        glyphs(0, 0, 0x79E9F5, 0.42f * native_intensity);
        ctx->PopAxisAlignedClip();
        if (motion.glitch != 0.0f && native_intensity > 0.0f) {
            for (int band = 0; band < 3; ++band) {
                const float top = metrics.height * (0.23f + 0.22f * band);
                ctx->PushAxisAlignedClip(D2D1::RectF(0, top, metrics.width + 2, top + metrics.height * 0.12f),
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                const float shift = motion.glitch * (band == 1 ? -1.0f : 1.0f);
                glyphs(shift, 0, 0x65ECFF, 0.92f * native_intensity);
                glyphs(-shift * 0.6f, 0, 0xCFB0FF, 0.65f * native_intensity);
                ctx->PopAxisAlignedClip();
            }
        }
        if (motion.sparkle > 0.01f && native_intensity > 0.0f) {
            const float x = metrics.width * motion.sparkle_x;
            const float y = metrics.height * 0.30f;
            ctx->DrawLine(D2D1::Point2F(x - 3, y), D2D1::Point2F(x + 3, y),
                          native_color(0xE0FAFF, motion.sparkle * native_intensity * opacity), 0.8f);
            ctx->DrawLine(D2D1::Point2F(x, y - 3), D2D1::Point2F(x, y + 3),
                          native_color(0xE0FAFF, motion.sparkle * native_intensity * opacity), 0.8f);
        }
        ctx->SetTransform(saved);
        ctx->PopAxisAlignedClip();
    };
