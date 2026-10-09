            // Keep card surfaces neutral; saturated colors identify the icons.
            const std::array<D2D1_COLOR_F, 10> colors{
                native_palette("options.key_mode", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.keymap", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.skin", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.graphics", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.audio", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.input", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.latency", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.profile", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.mode", D2D1::ColorF(0xEEEEEE)),
                native_palette("options.key_test", D2D1::ColorF(0xEEEEEE))};
            const std::array<D2D1_COLOR_F, 10> icon_colors{
                native_palette("options.icon.key_mode", D2D1::ColorF(0xFFD600)),
                native_palette("options.icon.keymap", D2D1::ColorF(0x00C8FF)),
                native_palette("options.icon.skin", D2D1::ColorF(0xA64DFF)),
                native_palette("options.icon.graphics", D2D1::ColorF(0x3C6FFF)),
                native_palette("options.icon.audio", D2D1::ColorF(0xFF3B30)),
                native_palette("options.icon.input", D2D1::ColorF(0x00D66F)),
                native_palette("options.icon.latency", D2D1::ColorF(0xFF7900)),
                native_palette("options.icon.profile", D2D1::ColorF(0xFF36B3)),
                native_palette("options.icon.mode", D2D1::ColorF(0xA9E600)),
                native_palette("options.icon.key_test", D2D1::ColorF(0x00D7BD))};
            const std::array<native_menu_assets::Asset, 10> icons = {
                native_menu_assets::kKeys, native_menu_assets::kKeymap, native_menu_assets::kSkin,
                native_menu_assets::kDisplay, native_menu_assets::kAudio, native_menu_assets::kInput,
                native_menu_assets::kLatency, native_menu_assets::kProfile, native_menu_assets::kSliders,
                native_menu_assets::kKeytest};
            const float gap = native_metric("options_grid.gap", 22.0f);
            const float width = (right - left - gap * 3.0f) / 4.0f;
            const float height = native_metric("options_grid.height", 270.0f);
            const float radius = std::clamp(native_metric("options_grid.radius", 12.0f), 0.0f, 32.0f);
            std::size_t selected = 0;
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                const auto& row = data.generic.rows[i];
                if (row.selected) selected = i;
                const float x = left + static_cast<float>(i % 4) * (width + gap);
                const float y = top + 40.0f + static_cast<float>(i / 4) * (height + gap);
                const D2D1_RECT_F rect = native_rect("options_grid.rect.001", D2D1::RectF(x, y, x + width, y + height));
                const auto rr = D2D1::RoundedRect(rect, radius, radius);
                const auto saved = d2d_->card_brush->GetColor();
                const auto accent = colors[i % colors.size()];
                const float tint = std::clamp(row.selected
                    ? native_metric("options_grid.selected_tint", 0.018f)
                    : native_metric("options_grid.tint", 0.0f), 0.0f, 0.65f);
                d2d_->card_brush->SetColor(D2D1::ColorF(
                    0.035f + accent.r * tint, 0.035f + accent.g * tint, 0.035f + accent.b * tint, 1.0f));
                ctx->FillRoundedRectangle(rr, d2d_->card_brush.Get());
                d2d_->card_brush->SetColor(row.selected ? icon_colors[i % icon_colors.size()]
                    : native_palette("options.border", D2D1::ColorF(0x282828)));
                const float border_width = std::clamp(row.selected
                    ? native_metric("options_grid.selected_border_width", 2.25f)
                    : native_metric("options_grid.border_width", 1.5f), 0.5f, 8.0f);
                ctx->DrawRoundedRectangle(rr, d2d_->card_brush.Get(), border_width);
                d2d_->card_brush->SetColor(row.selected ? accent : D2D1::ColorF(0x404040));
                ctx->FillRectangle(native_rect("options_grid.rect.002", D2D1::RectF(x + 24, y + 26, x + 68, y + 30)), d2d_->card_brush.Get());
                draw_text_clipped(to_wide(row.label), d2d_->option_format.Get(),
                    native_rect("options_grid.rect.003", D2D1::RectF(x + 24, y + 50, x + width - 24, y + 98)), row.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get());
                d2d_->card_brush->SetColor(native_palette("options.value", D2D1::ColorF(0xF5F5F5)));
                const auto alignment = d2d_->title_format->GetParagraphAlignment();
                d2d_->title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                draw_text_clipped_aligned(to_wide(row.value), d2d_->title_format.Get(),
                    native_rect("options_grid.rect.004", D2D1::RectF(x + 24, y + 114, x + width - 24, y + 222)),
                    d2d_->card_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
                d2d_->title_format->SetParagraphAlignment(alignment);
                d2d_->card_brush->SetColor(saved);
                const float icon_y = 8.0f * (1.0f - native_menu_motion_.entrance(static_cast<float>(i) * 0.035f));
                const auto icon_color = icon_colors[i % icon_colors.size()];
                draw_native_asset(icons[i % icons.size()], native_rect("options_grid.rect.005", D2D1::RectF(x + width - 70, y + 22 + icon_y,
                    x + width - 26, y + 66 + icon_y)), 1.0f, 0.0f, &icon_color);
                if (row.activatable) register_hit(rect, row.target_kind, row.row_index, MenuHitPart::Activate);
            }
            const D2D1_RECT_F description = native_rect("options_grid.rect.006", D2D1::RectF(left, top + 630, right, bottom - 12));
            draw_glass_panel(description, 14, 1, 0, false, 0);
            if (selected < data.generic.card_descriptions.size()) {
                const auto wrapping = d2d_->body_format->GetWordWrapping();
                d2d_->body_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                draw_text_clipped(to_wide(data.generic.card_descriptions[selected]), d2d_->body_format.Get(),
                    native_rect("options_grid.rect.007", D2D1::RectF(description.left + 28, description.top + 28, description.right - 28, description.bottom - 24)),
                    d2d_->text_brush.Get());
                d2d_->body_format->SetWordWrapping(wrapping);
            }
