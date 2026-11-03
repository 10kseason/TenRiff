        // Result titles need measured whole-text fitting: a wrapped CJK title
        // bypasses the ordinary single-line estimate, and user fonts/large UI
        // text can make two lines taller than their fixed strip. Reuse the
        // bounded text-layout slots and fit only when text/font/bounds change.
        const auto draw_result_fitted_text = [&](const std::string& text, IDWriteTextFormat* format,
                                                 const D2D1_RECT_F& bounds, ID2D1Brush* ink,
                                                 float opacity, bool wrap, int diagnostic_slot) {
            if (!format || !ink || text.empty() || opacity <= 0.001f) return;
            constexpr float inset = 2.0f; // Preserve the existing readable outline.
            const float width = bounds.right - bounds.left - inset * 2;
            const float height = bounds.bottom - bounds.top - inset * 2;
            if (width <= 0 || height <= 0) return;
            const auto wide = to_wide(text);
            const auto wrapping = wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP;
            D2DResources::ReadableTextLayout transient;
            auto& cache = menu_text_slot < d2d_->menu_readable_text.size()
                ? d2d_->menu_readable_text[menu_text_slot++] : transient;
            DWRITE_LINE_SPACING_METHOD line_method{};
            float line_spacing = 0, baseline = 0;
            format->GetLineSpacing(&line_method, &line_spacing, &baseline);
            auto* diagnostic = diagnostic_slot == 0 ? &benchmark_result_title_ :
                &benchmark_result_details_[static_cast<std::size_t>(diagnostic_slot - 1)];
            if (!cache.layout || cache.text != wide || cache.format.Get() != format ||
                cache.width != width || cache.height != height || cache.wrapping != wrapping ||
                cache.alignment != DWRITE_TEXT_ALIGNMENT_LEADING || cache.paragraph != DWRITE_PARAGRAPH_ALIGNMENT_NEAR ||
                cache.line_spacing_method != line_method || cache.line_spacing != line_spacing || cache.baseline != baseline) {
                cache = {};
                if (FAILED(d2d_->dwrite_factory->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                    format, width, 1'000'000.0f, cache.layout.GetAddressOf()))) return;
                cache.text = wide; cache.format = format; cache.width = width; cache.height = height;
                cache.wrapping = wrapping; cache.alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
                cache.paragraph = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
                cache.line_spacing_method = line_method; cache.line_spacing = line_spacing; cache.baseline = baseline;
                cache.layout->SetWordWrapping(wrapping);
                cache.layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                cache.layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                const DWRITE_TRIMMING no_trim{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
                cache.layout->SetTrimming(&no_trim, nullptr);
                const auto fits = [&](float scale) {
                    cache.layout->SetMaxWidth(width / scale);
                    DWRITE_TEXT_METRICS measured{};
                    return SUCCEEDED(cache.layout->GetMetrics(&measured)) &&
                        measured.widthIncludingTrailingWhitespace * scale <= width && measured.height * scale <= height;
                };
                float scale = 1.0f;
                if (!fits(scale)) {
                    // Width expansion permits natural reflow as the glyphs
                    // shrink. Measuring only the first wrapped layout would
                    // miss line-count changes at Japanese word boundaries.
                    float low = 0, high = 1;
                    for (int pass = 0; pass < 16; ++pass) {
                        const float candidate = (low + high) * 0.5f;
                        if (fits(candidate)) low = candidate; else high = candidate;
                    }
                    scale = std::max(0.00001f, low * 0.999f);
                }
                cache.layout->SetMaxWidth(width / scale);
                cache.layout->SetMaxHeight(height / scale);
                if (benchmark_timings_enabled_) ++diagnostic->layout_builds;
            }
            const float scale = width / cache.layout->GetMaxWidth();
            if (benchmark_timings_enabled_) {
                DWRITE_TEXT_METRICS measured{};
                cache.layout->GetMetrics(&measured);
                diagnostic->left = bounds.left + inset; diagnostic->top = bounds.top + inset;
                diagnostic->right = bounds.right - inset; diagnostic->bottom = bounds.bottom - inset;
                diagnostic->field_left = bounds.left; diagnostic->field_right = bounds.right;
                diagnostic->font_size = format->GetFontSize() * scale;
                diagnostic->text_width = measured.widthIncludingTrailingWhitespace * scale;
                diagnostic->text_height = measured.height * scale;
                diagnostic->characters = static_cast<uint32_t>(wide.size()); diagnostic->line_count = measured.lineCount;
            }
            D2D1_MATRIX_3X2_F saved{};
            ctx->GetTransform(&saved);
            const float old_opacity = ink->GetOpacity();
            ink->SetOpacity(old_opacity * std::clamp(opacity, 0.0f, 1.0f));
            ctx->PushAxisAlignedClip(bounds, D2D1_ANTIALIAS_MODE_ALIASED);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                D2D1::Matrix3x2F::Translation(bounds.left + inset, bounds.top + inset) * saved);
            draw_text_layout_readable(D2D1::Point2F(0, 0), cache.layout.Get(), ink);
            ctx->SetTransform(saved);
            ctx->PopAxisAlignedClip();
            ink->SetOpacity(old_opacity);
        };
