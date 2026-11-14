        if (data.gameplay.score_comparison_enabled && d2d_->text_brush && scene_hud_format) {
            constexpr float width = 352.0f, height = 246.0f, margin = 16.0f;
            const auto coordinate = [](double value, double fallback) {
                return static_cast<float>(std::clamp(std::isfinite(value) ? value : fallback, 0.0, 1.0));
            };
            const float x = margin + coordinate(data.gameplay.score_comparison_x, 0.0) * (1920 - width - margin * 2);
            const float y = margin + coordinate(data.gameplay.score_comparison_y, 0.28) * (1080 - height - margin * 2);
            const auto saved_color = d2d_->text_brush->GetColor();
            const float saved_opacity = d2d_->text_brush->GetOpacity();
            const auto brush = [&](uint32_t color, float opacity = 1.0f) {
                d2d_->text_brush->SetColor(D2D1::ColorF(color));
                d2d_->text_brush->SetOpacity(opacity);
                return d2d_->text_brush.Get();
            };
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), 12, 12),
                                      brush(0x102032, 0.88f));
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), 12, 12),
                                      brush(0x80DCE8, 0.42f), 1.0f);
            const auto& comparison = data.gameplay.score_comparison;
            draw_readable_text(wloc("EX Score / RANK", "EX Score / 랭크"), scene_hud_format.Get(),
                D2D1::RectF(x + 16, y + 10, x + width - 16, y + 37), brush(0xA4B4C8));
            const auto signed_integer = [](int64_t value) {
                return (value > 0 ? L"+" : L"") + std::to_wstring(value);
            };
            const std::wstring best = comparison.live && comparison.best_available
                ? signed_integer(comparison.score_difference) : L"--";
            const uint32_t best_color = comparison.score_difference < 0 ? 0xFF8C98 : 0x8CEAD1;
            draw_readable_text(L"BEST", scene_hud_format.Get(),
                D2D1::RectF(x + 16, y + 42, x + 150, y + 75), brush(0xC4D4E4));
            draw_readable_text_aligned(best, scene_body_format.Get(),
                D2D1::RectF(x + 156, y + 39, x + width - 16, y + 75), brush(best_color), DWRITE_TEXT_ALIGNMENT_TRAILING);
            for (std::size_t i = 0; i < app::kLiveRankNames.size(); ++i) {
                const float top = y + 82 + static_cast<float>(i) * 32;
                const double value = comparison.rank_difference[i];
                // A displayed tie is exactly 0; do not show a negative zero.
                const double quantized = std::round(value * 100.0) / 100.0;
                const double rounded = quantized == 0.0 ? 0.0 : quantized;
                const std::wstring delta = comparison.live && comparison.rank_available
                    ? to_wide((rounded > 0 ? "+" : "") + format_decimal(rounded, 2)) + L"%p" : L"--";
                const std::wstring label = to_wide(std::string(app::kLiveRankNames[i])) + L"  " +
                    std::to_wstring(static_cast<int>(app::kLiveRankAccuracyTargets[i])) + L"%";
                draw_readable_text(label, scene_hud_format.Get(),
                    D2D1::RectF(x + 16, top, x + 150, top + 30), brush(0xC4D4E4));
                draw_readable_text_aligned(delta, scene_hud_format.Get(),
                    D2D1::RectF(x + 152, top, x + width - 16, top + 30),
                    brush(value < 0 ? 0xFF8C98 : 0x8CEAD1), DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            draw_fitted_readable_text(wloc("BEST: same song time / ranks: accuracy", "BEST: 같은 곡 시점 / 랭크: 정확도"),
                scene_hud_format.Get(), D2D1::RectF(x + 16, y + 215, x + width - 16, y + 241), brush(0x8297AF),
                DWRITE_TEXT_ALIGNMENT_LEADING);
            d2d_->text_brush->SetColor(saved_color);
            d2d_->text_brush->SetOpacity(saved_opacity);
        }
