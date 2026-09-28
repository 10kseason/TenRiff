    // This entire layer is gated to the built-in non-gameplay screens. Geometry
    // is uploaded once per device; all per-frame work has a fixed upper bound.
    const bool native_motion_screen = modern_menu_screen && data.kind != MenuScreenKind::BmsEditor;
    const int native_screen_key = static_cast<int>(data.kind) * 1009 +
        (data.kind == MenuScreenKind::GenericList
             ? static_cast<int>(std::hash<std::string>{}(data.generic.heading) & 0x3fffffff) : 0);
    if (native_motion_screen && (!native_animation_preference_checked_ ||
        render_now_ns - native_animation_preference_checked_ns_ > 1'000'000'000LL)) {
        BOOL animations = TRUE;
        if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0)) {
            native_system_reduced_motion_ = !animations;
        }
        native_animation_preference_checked_ = true;
        native_animation_preference_checked_ns_ = render_now_ns;
    }
    native_menu_motion_.begin(native_screen_key, render_now_ns, native_motion_screen,
                              native_system_reduced_motion_ || config_.reduce_menu_motion ||
                                  native_motion_value("enabled", 1, 0, 1) < 0.5f,
                              native_motion_value("speed", 1, 0, 4),
                              native_motion_value("entry_seconds", 0.42f, 0.05f, 2));
    const double native_seconds = native_menu_motion_.seconds();
    const float native_intensity = native_motion_value("intensity", 1, 0, 2);
    if (native_motion_screen && !d2d_->native_menu_brush) {
        ctx->CreateSolidColorBrush(native_palette("palette.63e9f2", D2D1::ColorF(0x63E9F2)), &d2d_->native_menu_brush);
        const D2D1_GRADIENT_STOP stops[] = {
            {0.0f, native_palette("palette.101d32", D2D1::ColorF(0x101D32))}, {0.56f, native_palette("palette.101421", D2D1::ColorF(0x101421))},
            {1.0f, native_palette("palette.211c35", D2D1::ColorF(0x211C35))}};
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 3, &collection))) {
            ctx->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(1920, 1080)),
                collection.Get(), &d2d_->native_menu_background);
        }
        for (std::size_t i = 0; i < native_menu_assets::paths.size(); ++i) {
            const auto& path = native_menu_assets::paths[i];
            auto& geometry = d2d_->native_menu_geometry[i];
            if (FAILED(d2d_->d2d_factory->CreatePathGeometry(&geometry))) continue;
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            if (FAILED(geometry->Open(&sink))) { geometry.Reset(); continue; }
            const auto& first = native_menu_assets::points[path.first];
            sink->BeginFigure(D2D1::Point2F(first.x, first.y),
                              path.filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
            for (unsigned j = 1; j < path.count; ++j) {
                const auto& point = native_menu_assets::points[path.first + j];
                sink->AddLine(D2D1::Point2F(point.x, point.y));
            }
            sink->EndFigure(path.filled ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
            if (FAILED(sink->Close())) geometry.Reset();
        }
    }
    auto native_color = [&](uint32_t rgb, float alpha) -> ID2D1SolidColorBrush* {
        auto* brush = d2d_->native_menu_brush.Get();
        if (brush) {
            D2D1_COLOR_F color = D2D1::ColorF(rgb);
            const auto it = d2d_->native_menu_palette.find(rgb);
            if (it != d2d_->native_menu_palette.end()) color = it->second;
            brush->SetColor(color);
            brush->SetOpacity(std::clamp(alpha, 0.0f, 1.0f));
        }
        return brush;
    };
    // Stable mode accents shared by the library and result metadata. The
    // palette slots remain editable through the native skin catalog.
    auto key_count_brush = [&](int keys) -> ID2D1SolidColorBrush* {
        switch (keys) {
            case 4: return native_color(0x7EE2A8, 1);
            case 5: return native_color(0xFFD479, 1);
            case 6: return native_color(0x8FB8FF, 1);
            case 7: return native_color(0xC2A0FF, 1);
            case 8: return native_color(0xFF9BC5, 1);
            case 9: return native_color(0xA9DB72, 1);
            case 10: return native_color(0x63E9F2, 1);
            case 11: return native_color(0xDAC89A, 1);
            case 12: return native_color(0xFFAB75, 1);
            case 13: return native_color(0x9EA7F5, 1);
            case 14: return native_color(0x81D5CF, 1);
            case 15: return native_color(0xE4B4EF, 1);
            case 16: return native_color(0xFF7F8C, 1);
            default: return native_color(0xECF6FF, 1);
        }
    };
    auto draw_native_asset = [&](native_menu_assets::Asset asset, const D2D1_RECT_F& rect,
                                 float alpha = 1.0f, float rotation = 0.0f) {
        if (!native_motion_screen || !d2d_->native_menu_brush || alpha <= 0) return;
        if (native_overrides && !native_style.assets.empty()) {
            const std::array<std::pair<native_menu_assets::Asset, const char*>, 12> names = {{
                {native_menu_assets::kMark, "mark"}, {native_menu_assets::kPrism, "prism"},
                {native_menu_assets::kChevron, "chevron"}, {native_menu_assets::kSpark, "spark"},
                {native_menu_assets::kWave, "wave"}, {native_menu_assets::kAudio, "audio"},
                {native_menu_assets::kDisplay, "display"}, {native_menu_assets::kInput, "input"},
                {native_menu_assets::kNetwork, "network"}, {native_menu_assets::kSliders, "sliders"},
                {native_menu_assets::kFolder, "folder"}, {native_menu_assets::kExit, "exit"}}};
            for (const auto& named : names) {
                if (named.first.first != asset.first) continue;
                const auto it = native_style.assets.find(named.second);
                if (it != native_style.assets.end()) {
                    if (ID2D1Bitmap* bitmap = find_song_card_preview_bitmap(it->second)) {
                        D2D1_MATRIX_3X2_F saved;
                        ctx->GetTransform(&saved);
                        ctx->SetTransform(D2D1::Matrix3x2F::Rotation(rotation,
                            D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f)) * saved);
                        ctx->DrawBitmap(bitmap, fit_rect_preserve_aspect(rect, bitmap->GetSize()),
                                        alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                        ctx->SetTransform(saved);
                        return;
                    }
                }
                break;
            }
        }
        D2D1_MATRIX_3X2_F saved;
        ctx->GetTransform(&saved);
        const float size = std::min(rect.right - rect.left, rect.bottom - rect.top);
        const float x = (rect.left + rect.right - size) * 0.5f;
        const float y = (rect.top + rect.bottom - size) * 0.5f;
        ctx->SetTransform(D2D1::Matrix3x2F::Rotation(rotation, D2D1::Point2F(50, 50)) *
                          D2D1::Matrix3x2F::Scale(size / 100, size / 100) *
                          D2D1::Matrix3x2F::Translation(x, y) * saved);
        for (unsigned i = asset.first; i < asset.first + asset.count; ++i) {
            auto* geometry = d2d_->native_menu_geometry[i].Get();
            if (!geometry) continue;
            auto* brush = native_color(native_menu_assets::paths[i].color, alpha);
            if (native_menu_assets::paths[i].filled) ctx->FillGeometry(geometry, brush);
            else ctx->DrawGeometry(geometry, brush, 1.6f);
        }
        ctx->SetTransform(saved);
    };
    auto draw_native_spectrum = [&](const D2D1_RECT_F& rect, float alpha, int bars = 24) {
        if (!native_motion_screen || !d2d_->native_menu_brush) return;
        // Decorative cadence, deliberately independent of the playback clock.
        const float step = (rect.right - rect.left) / static_cast<float>(bars);
        for (int i = 0; i < bars; ++i) {
            const float wave = 0.22f + 0.78f * static_cast<float>(std::pow(
                0.5 + 0.5 * std::sin(native_seconds * 2.4 + i * 0.71) *
                    std::cos(native_seconds * 0.7 - i * 0.23), 2.0));
            const float height = (rect.bottom - rect.top) * wave;
            ctx->FillRoundedRectangle(D2D1::RoundedRect(native_rect("native_motion.rect.001", D2D1::RectF(
                rect.left + i * step, rect.bottom - height,
                rect.left + i * step + step * 0.48f, rect.bottom)), 1.5f, 1.5f),
                native_color(i % 5 == 0 ? 0xA499FF : 0x63E9F2, alpha));
        }
    };
    auto draw_native_focus = [&](const D2D1_RECT_F& rect, float radius, std::size_t slot,
                                 int identity, bool selected, bool primary = false) {
        if (!native_motion_screen || !d2d_->native_menu_brush) return;
        const float focus = native_menu_motion_.focus(slot, identity, selected);
        const auto rr = D2D1::RoundedRect(rect, radius, radius);
        if (focus > 0.005f) {
            ctx->DrawRoundedRectangle(rr, native_color(0x63E9F2, 0.10f * focus), 7.0f);
            ctx->DrawRoundedRectangle(rr, native_color(0xA9F5FF, 0.66f * focus), 1.5f);
            const float length = (rect.right - rect.left - radius * 2) *
                (0.28f + 0.15f * static_cast<float>(0.5 + 0.5 * std::sin(native_seconds * 1.6)));
            ctx->DrawLine(D2D1::Point2F(rect.left + radius, rect.bottom - 1),
                          D2D1::Point2F(rect.left + radius + length, rect.bottom - 1),
                          native_color(0x63E9F2, 0.8f * focus), 2.0f);
        }
        if (primary && native_menu_motion_.moving()) {
            const float phase = static_cast<float>(std::fmod(native_seconds + 0.7, 5.4) / 1.8);
            if (phase < 1.0f) {
                const float x = rect.left - 80 + (rect.right - rect.left + 160) * phase;
                ctx->PushAxisAlignedClip(native_rect("native_motion.rect.002", D2D1::RectF(rect.left + radius, rect.top + 2,
                                                   rect.right - radius, rect.bottom - 2)), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                ctx->DrawLine(D2D1::Point2F(x, rect.bottom), D2D1::Point2F(x + 70, rect.top),
                              native_color(0xFFFFFF, 0.12f), 26.0f);
                ctx->PopAxisAlignedClip();
            }
        }
    };
    auto draw_native_orbit = [&](float x, float y, float radius, float alpha) {
        if (!native_motion_screen || !d2d_->native_menu_brush) return;
        alpha *= native_intensity;
        for (int ring = 0; ring < 3; ++ring) {
            const float rx = radius + ring * 14;
            const float ry = rx * (0.52f + ring * 0.12f);
            ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), rx, ry),
                             native_color(ring == 1 ? 0xA499FF : 0x63E9F2, alpha * 0.26f), 1.0f);
            const double angle = native_seconds * (0.22 + ring * 0.09) + ring * 2.1;
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + rx * static_cast<float>(std::cos(angle)),
                y + ry * static_cast<float>(std::sin(angle))), 3, 3), native_color(0xC5F7FF, alpha));
        }
    };
    auto draw_native_background = [&]() {
        if (!native_motion_screen || !d2d_->native_menu_brush) return;
        if (d2d_->native_menu_background) ctx->FillRectangle(
            native_rect("native_motion.rect.003", D2D1::RectF(0, 0, kBaseWidth, kBaseHeight)), d2d_->native_menu_background.Get());
        const float drift = static_cast<float>(std::fmod(native_seconds * 8, 120));
        for (int i = -10; i < 20; ++i) {
            const float x = i * 120.0f + drift;
            ctx->DrawLine(D2D1::Point2F(x, 1080), D2D1::Point2F(x + 580, 0),
                          native_color(0x89AAE0, i % 4 == 0 ? 0.055f : 0.018f), 1.0f);
        }
        for (int i = 0; i < 18; ++i) {
            const float x = static_cast<float>(std::fmod(i * 137.0 + native_seconds * (3 + i % 4), 1920));
            const float y = static_cast<float>(std::fmod(i * 251.0 + 160, 1080));
            const float alpha = 0.13f + 0.08f * static_cast<float>(std::sin(native_seconds + i));
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 1.6f, 1.6f), native_color(0xBBD9FF, alpha));
        }
        const float reveal = native_menu_motion_.entrance(0, 0.65f);
        ctx->DrawLine(D2D1::Point2F(0, 127), D2D1::Point2F(1920 * reveal, 127),
                      native_color(0x63E9F2, 0.65f), 2.0f);
    };
