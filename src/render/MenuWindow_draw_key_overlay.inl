        // The plugin uses the gameplay canvas (also used by the skin preview),
        // outside lane/ghost clips. Position 0..100% keeps the complete panel visible.
        const auto& overlay = data.gameplay.key_overlay;
        if (data.gameplay.key_overlay_enabled && overlay.visible &&
            app::key_overlay_supports(data.gameplay.lane_count) && d2d_->text_brush) {
            constexpr float width = 432.0f, height = 246.0f, margin = 16.0f;
            const auto position = [](double value, double fallback) {
                return static_cast<float>(std::clamp(std::isfinite(value) ? value : fallback, 0.0, 1.0));
            };
            const float x = margin + position(data.gameplay.key_overlay_x, 0.0) * (1920.0f - width - 2 * margin);
            const float y = margin + position(data.gameplay.key_overlay_y, 0.25) * (1080.0f - height - 2 * margin);
            const auto saved_color = d2d_->text_brush->GetColor();
            const auto saved_opacity = d2d_->text_brush->GetOpacity();
            auto brush = [&](uint32_t color, float alpha = 1.0f) {
                d2d_->text_brush->SetOpacity(1.0f);
                d2d_->text_brush->SetColor(D2D1::ColorF(color, alpha));
                return d2d_->text_brush.Get();
            };
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), 12, 12), brush(0x101925, 0.88f));
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), 12, 12), brush(0x6BCFDD, 0.6f), 1.0f);
            std::wostringstream kps;
            kps << L"KPS  ";
            if (overlay.preview || !overlay.live || overlay.limited) kps << L"--";
            else kps << std::fixed << std::setprecision(1) << overlay.kps;
            draw_readable_text(overlay.preview ? wloc("Key Viewer · Preview", "키뷰어 · 미리보기") : wloc("Key Viewer", "키뷰어"), scene_hud_format.Get(),
                D2D1::RectF(x + 16, y + 12, x + 270, y + 40), brush(0xA4B4C8));
            draw_readable_text_aligned(kps.str(), scene_body_format.Get(),
                D2D1::RectF(x + 276, y + 10, x + width - 16, y + 40), brush(0x8CEAF1), DWRITE_TEXT_ALIGNMENT_TRAILING);
            const float top = y + 48, bottom = y + height - 48;
            const float lane_width = (width - 32) / data.gameplay.lane_count;
            ctx->PushAxisAlignedClip(D2D1::RectF(x + 16, top, x + width - 16, bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            for (int lane = 0; lane < data.gameplay.lane_count; ++lane) {
                const float left = x + 16 + lane_width * lane;
                ctx->FillRectangle(D2D1::RectF(left + 2, top, left + lane_width - 2, bottom), brush(0x243446, 0.35f));
            }
            for (std::size_t i = 0; i < overlay.note_count; ++i) {
                const auto& note = overlay.notes[i];
                if (note.lane < 0 || note.lane >= data.gameplay.lane_count) continue;
                const float left = x + 16 + lane_width * note.lane + 4;
                const uint32_t color = static_cast<std::size_t>(note.lane) < data.gameplay.lane_color_count
                    ? data.gameplay.lane_colors[note.lane] : 0x7ADFEA;
                const auto note_y = [&](double time) {
                    return bottom - static_cast<float>((overlay.now_ms - time) / app::KeyOverlaySnapshot::TravelMs) * (bottom - top);
                };
                const float head = note_y(note.press_ms);
                const double end_ms = note.release_ms < 0 ? overlay.now_ms : note.release_ms;
                if (end_ms - note.press_ms >= 90.0) {
                    const float tail = note_y(end_ms);
                    ctx->FillRectangle(D2D1::RectF(left + 3, head, left + lane_width - 11, tail), brush(color, 0.45f));
                    if (note.release_ms >= 0) ctx->FillRectangle(D2D1::RectF(left, tail - 4, left + lane_width - 8, tail), brush(color));
                }
                ctx->FillRectangle(D2D1::RectF(left, head - 7, left + lane_width - 8, head), brush(color));
            }
            ctx->PopAxisAlignedClip();
            for (int lane = 0; lane < data.gameplay.lane_count; ++lane) {
                const float left = x + 16 + lane_width * lane;
                const auto key = D2D1::RectF(left + 2, bottom + 5, left + lane_width - 2, y + height - 12);
                const uint32_t color = static_cast<std::size_t>(lane) < data.gameplay.lane_color_count
                    ? data.gameplay.lane_colors[lane] : 0x7ADFEA;
                ctx->FillRoundedRectangle(D2D1::RoundedRect(key, 4, 4), brush(overlay.held[lane] ? color : 0x2B3B4E, overlay.held[lane] ? 0.95f : 0.8f));
                const std::wstring label = data.gameplay.key_labels[lane].empty() ? std::to_wstring(lane + 1) : to_wide(data.gameplay.key_labels[lane]);
                draw_readable_text_aligned(label, scene_hud_format.Get(), key, brush(0xFFFFFF), DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            d2d_->text_brush->SetColor(saved_color);
            d2d_->text_brush->SetOpacity(saved_opacity);
        }
