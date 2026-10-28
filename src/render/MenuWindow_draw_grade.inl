    // Grades are semantic colors, independent of the monochrome menu palette.
    // Rasterize two Gaussian bloom layers and the crisp glyph once per slot;
    // steady frames (including gameplay) never run a blur or rebuild a layout.
    auto draw_grade = [&](std::size_t slot, const std::wstring& text, IDWriteTextFormat* format,
                          const D2D1_RECT_F& rect, float alpha,
                          DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
                          float text_scale = 1.0f) {
        const float width = std::ceil(rect.right - rect.left), height = std::ceil(rect.bottom - rect.top);
        if (text.empty() || !format || alpha <= 0 || width <= 0 || height <= 0) return;
        auto& cache = d2d_->grade_bitmaps[slot];
        const auto color = D2D1::ColorF(grade_display_color(text));
        const bool rainbow = grade_uses_clear_rainbow(text);
        const bool prism = text == L"S+" || text == L"S++" || text == L"S+++";
        constexpr float pad = 28.0f;
        if (!cache.attempted || cache.text != text || cache.format.Get() != format ||
            cache.width != width || cache.height != height || cache.alignment != alignment || cache.text_scale != text_scale) {
            cache = {};
            // Cache failure too: an unavailable effect must not allocate again
            // every frame. Device/font recreation invalidates this entry.
            cache.attempted = true; cache.text = text; cache.format = format;
            cache.text_scale = text_scale;
            cache.width = width; cache.height = height; cache.alignment = alignment;
            // Measure once for both bloom and plain-glyph fallback. A wider
            // grade such as S++ must fit even when an effect/device fails.
            if (SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                    format, width, height, cache.layout.GetAddressOf()))) {
                cache.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                cache.layout->SetTextAlignment(alignment);
                cache.layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                if (text_scale != 1.0f) cache.layout->SetFontSize(format->GetFontSize() * text_scale,
                    DWRITE_TEXT_RANGE{0, static_cast<UINT32>(text.size())});
                DWRITE_TEXT_METRICS metrics{};
                cache.layout->GetMetrics(&metrics);
                cache.fit = std::min({1.0f, width / std::max(1.0f, metrics.widthIncludingTrailingWhitespace),
                                          height / std::max(1.0f, metrics.height)});
                cache.layout->SetMaxWidth(width / cache.fit);
                cache.layout->SetMaxHeight(height / cache.fit);
            }
            // Anchor the clear palette to the actual glyph bounds, not the
            // often much wider rank slot. Build once with the existing glyph cache.
            auto make_rainbow = [&](ID2D1RenderTarget* target) {
                Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> gradient;
                if (!rainbow || !cache.layout) return gradient;
                DWRITE_TEXT_METRICS metrics{};
                cache.layout->GetMetrics(&metrics);
                std::array<D2D1_GRADIENT_STOP, kClearRainbowRgb.size()> stops;
                for (std::size_t s = 0; s < stops.size(); ++s)
                    stops[s] = {kClearRainbowStops[s], D2D1::ColorF(kClearRainbowRgb[s])};
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                if (SUCCEEDED(target->CreateGradientStopCollection(stops.data(), static_cast<UINT>(stops.size()), &collection)))
                    target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(metrics.left, 0),
                        D2D1::Point2F(metrics.left + std::max(1.0f, metrics.width), 0)),
                        collection.Get(), &gradient);
                return gradient;
            };
            cache.fallback_gradient = make_rainbow(ctx);
            // A separate device context avoids interrupting the window's BeginDraw.
            Microsoft::WRL::ComPtr<ID2D1Device> device;
            Microsoft::WRL::ComPtr<ID2D1DeviceContext> off;
            Microsoft::WRL::ComPtr<ID2D1Bitmap1> glyph, bloom;
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
            Microsoft::WRL::ComPtr<ID2D1Effect> blur;
            ctx->GetDevice(device.GetAddressOf());
            const auto size = D2D1::SizeU(static_cast<UINT32>(std::min(width, 2048.0f) + pad * 2),
                                        static_cast<UINT32>(std::min(height, 512.0f) + pad * 2));
            const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
            if (width <= 2048 && height <= 512 && device && cache.layout &&
                SUCCEEDED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, off.GetAddressOf())) &&
                SUCCEEDED(off->CreateBitmap(size, nullptr, 0, props, glyph.GetAddressOf())) &&
                SUCCEEDED(off->CreateBitmap(size, nullptr, 0, props, bloom.GetAddressOf())) &&
                SUCCEEDED(off->CreateSolidColorBrush(color, brush.GetAddressOf())) &&
                SUCCEEDED(off->CreateEffect(CLSID_D2D1GaussianBlur, blur.GetAddressOf()))) {
                off->SetTarget(glyph.Get());
                off->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                off->BeginDraw();
                off->Clear(D2D1::ColorF(0, 0));
                off->SetTransform(D2D1::Matrix3x2F::Scale(cache.fit, cache.fit) * D2D1::Matrix3x2F::Translation(pad, pad));
                const auto gradient = make_rainbow(off.Get());
                if (prism) {
                    // Bake the bevel/depth with the glyph, so steady frames
                    // have the same single-bitmap cost as the existing grades.
                    // Bound physical edge width for both small records and
                    // large result slots; these effects never affect layout.
                    const float edge = std::clamp(format->GetFontSize() * text_scale * cache.fit * 0.018f,
                                                  0.75f, 2.2f) / cache.fit;
                    brush->SetColor(D2D1::ColorF(0x09244A));
                    for (int layer = 3; layer >= 1; --layer)
                        off->DrawTextLayout(D2D1::Point2F(edge * layer * 0.55f, edge * layer),
                                            cache.layout.Get(), brush.Get());
                    brush->SetColor(D2D1::ColorF(0x73BFFF));
                    constexpr std::array<D2D1_POINT_2F, 8> rim{{
                        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
                        {-0.7f, -0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {0.7f, 0.7f}}};
                    for (auto offset : rim)
                        off->DrawTextLayout(D2D1::Point2F(offset.x * edge, offset.y * edge),
                                            cache.layout.Get(), brush.Get());
                    brush->SetColor(D2D1::ColorF(0xE8FFFF));
                    off->DrawTextLayout(D2D1::Point2F(-edge * 0.45f, -edge * 0.7f),
                                        cache.layout.Get(), brush.Get());
                    brush->SetColor(color);
                }
                off->DrawTextLayout(D2D1::Point2F(0, 0), cache.layout.Get(),
                    gradient ? static_cast<ID2D1Brush*>(gradient.Get()) : brush.Get());
                if (prism) {
                    DWRITE_TEXT_METRICS metrics{};
                    cache.layout->GetMetrics(&metrics);
                    // A diagonal reflection band suggests a prism face without
                    // animated shine, extra per-frame work or external artwork.
                    const D2D1_GRADIENT_STOP shine_stops[] = {
                        {0.0f, D2D1::ColorF(0xFFFFFF, 0.38f)},
                        {0.24f, D2D1::ColorF(0xFFFFFF, 0.02f)},
                        {0.44f, D2D1::ColorF(0xFFFFFF, 0.04f)},
                        {0.49f, D2D1::ColorF(0xFFFFFF, 0.52f)},
                        {0.53f, D2D1::ColorF(0xFFFFFF, 0.05f)},
                        {0.76f, D2D1::ColorF(0xFFFFFF, 0.0f)},
                        {1.0f, D2D1::ColorF(0xC9F9FF, 0.30f)}};
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> shine_collection;
                    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> shine;
                    if (SUCCEEDED(off->CreateGradientStopCollection(shine_stops,
                            static_cast<UINT>(std::size(shine_stops)), &shine_collection)) &&
                        SUCCEEDED(off->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                            D2D1::Point2F(metrics.left, metrics.top),
                            D2D1::Point2F(metrics.left + std::max(1.0f, metrics.width), metrics.top + metrics.height)),
                            shine_collection.Get(), &shine)))
                        off->DrawTextLayout(D2D1::Point2F(0, 0), cache.layout.Get(), shine.Get());
                }
                const HRESULT glyph_hr = off->EndDraw();
                off->SetTransform(D2D1::Matrix3x2F::Identity());
                off->SetTarget(bloom.Get());
                blur->SetInput(0, glyph.Get());
                blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 7.0f);
                off->BeginDraw();
                off->Clear(D2D1::ColorF(0, 0));
                off->DrawImage(blur.Get());
                blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 2.2f);
                off->DrawImage(blur.Get());
                off->DrawBitmap(glyph.Get());
                const HRESULT bloom_hr = off->EndDraw();
                off->SetTarget(nullptr);
                if (SUCCEEDED(glyph_hr) && SUCCEEDED(bloom_hr)) {
                    cache.text = text; cache.format = format; cache.width = width; cache.height = height;
                    cache.alignment = alignment; cache.color = color; cache.bitmap = std::move(bloom);
                }
            }
        }
        if (cache.bitmap) {
            ctx->DrawBitmap(cache.bitmap.Get(), D2D1::RectF(rect.left - pad, rect.top - pad,
                rect.left + width + pad, rect.top + height + pad), std::clamp(alpha, 0.0f, 1.0f),
                D2D1_INTERPOLATION_MODE_LINEAR);
        } else {
            const auto saved = d2d_->accent_brush->GetColor();
            const auto opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(color); d2d_->accent_brush->SetOpacity(alpha);
            D2D1_MATRIX_3X2_F saved_transform{};
            ctx->GetTransform(&saved_transform);
            if (cache.layout) {
                ctx->SetTransform(D2D1::Matrix3x2F::Scale(cache.fit, cache.fit) *
                    D2D1::Matrix3x2F::Translation(rect.left, rect.top) * saved_transform);
                if (cache.fallback_gradient) cache.fallback_gradient->SetOpacity(std::clamp(alpha, 0.0f, 1.0f));
                ctx->DrawTextLayout(D2D1::Point2F(0, 0), cache.layout.Get(),
                                    cache.fallback_gradient ? static_cast<ID2D1Brush*>(cache.fallback_gradient.Get())
                                                            : d2d_->accent_brush.Get(),
                                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
            } else {
                const auto fallback_rect = text_scale == 1.0f ? rect : D2D1::RectF(rect.left, rect.top,
                    rect.left + width / text_scale, rect.top + height / text_scale);
                if (text_scale != 1.0f) ctx->SetTransform(D2D1::Matrix3x2F::Scale(text_scale, text_scale,
                    D2D1::Point2F(rect.left, rect.top)) * saved_transform);
                draw_text_clipped_aligned(text, format, fallback_rect, d2d_->accent_brush.Get(), alignment);
            }
            ctx->SetTransform(saved_transform);
            d2d_->accent_brush->SetColor(saved); d2d_->accent_brush->SetOpacity(opacity);
        }
    };
