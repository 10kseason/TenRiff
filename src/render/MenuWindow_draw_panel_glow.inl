    // Profile borders are cached separately from grade glyphs. Gaussian bloom
    // is rebuilt only for a new size/color, never during steady menu frames.
    auto draw_panel_glow = [&](const D2D1_RECT_F& rect, float radius, uint32_t rgb, std::size_t slot) {
        const float width = std::ceil(rect.right - rect.left);
        const float height = std::ceil(rect.bottom - rect.top);
        if (width <= 0 || height <= 0 || slot >= d2d_->panel_glow_bitmaps.size()) return;
        auto& cache = d2d_->panel_glow_bitmaps[slot];
        constexpr float pad = 18.0f;
        if (!cache.attempted || cache.width != width || cache.height != height ||
            cache.radius != radius || cache.rgb != rgb) {
            cache = {};
            cache.attempted = true;
            cache.width = width; cache.height = height; cache.radius = radius; cache.rgb = rgb;
            Microsoft::WRL::ComPtr<ID2D1Device> device;
            Microsoft::WRL::ComPtr<ID2D1DeviceContext> off;
            Microsoft::WRL::ComPtr<ID2D1Bitmap1> border, bloom;
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
            Microsoft::WRL::ComPtr<ID2D1Effect> blur;
            ctx->GetDevice(device.GetAddressOf());
            const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
            const auto size = D2D1::SizeU(static_cast<UINT32>(std::min(width, 2048.0f) + pad * 2),
                                        static_cast<UINT32>(std::min(height, 512.0f) + pad * 2));
            if (width <= 2048 && height <= 512 && device &&
                SUCCEEDED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, off.GetAddressOf())) &&
                SUCCEEDED(off->CreateBitmap(size, nullptr, 0, props, border.GetAddressOf())) &&
                SUCCEEDED(off->CreateBitmap(size, nullptr, 0, props, bloom.GetAddressOf())) &&
                SUCCEEDED(off->CreateSolidColorBrush(D2D1::ColorF(rgb), brush.GetAddressOf())) &&
                SUCCEEDED(off->CreateEffect(CLSID_D2D1GaussianBlur, blur.GetAddressOf()))) {
                off->SetTarget(border.Get());
                off->BeginDraw();
                off->Clear(D2D1::ColorF(0, 0));
                off->DrawRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(pad, pad, pad + width, pad + height), radius, radius), brush.Get(), 2.2f);
                const HRESULT border_hr = off->EndDraw();
                off->SetTarget(bloom.Get());
                blur->SetInput(0, border.Get());
                blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 4.0f);
                off->BeginDraw();
                off->Clear(D2D1::ColorF(0, 0));
                off->DrawImage(blur.Get());
                const HRESULT bloom_hr = off->EndDraw();
                off->SetTarget(nullptr);
                if (SUCCEEDED(border_hr) && SUCCEEDED(bloom_hr)) cache.bitmap = std::move(bloom);
            }
        }
        if (cache.bitmap) ctx->DrawBitmap(cache.bitmap.Get(), D2D1::RectF(rect.left - pad, rect.top - pad,
            rect.left + width + pad, rect.top + height + pad), 0.65f, D2D1_INTERPOLATION_MODE_LINEAR);
        const auto color = d2d_->note_fill_brush->GetColor();
        const auto opacity = d2d_->note_fill_brush->GetOpacity();
        d2d_->note_fill_brush->SetColor(D2D1::ColorF(rgb));
        d2d_->note_fill_brush->SetOpacity(0.85f);
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), d2d_->note_fill_brush.Get(), 2.2f);
        d2d_->note_fill_brush->SetColor(color);
        d2d_->note_fill_brush->SetOpacity(opacity);
    };
