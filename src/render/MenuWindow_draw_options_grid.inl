            // Category accents remain visible before selection and are editable
            // independently of the shared black/white menu palette.
            const std::array<D2D1_COLOR_F, 10> colors{
                native_palette("options.key_mode", D2D1::ColorF(0x8EDBFF)),
                native_palette("options.keymap", D2D1::ColorF(0xA5E8BA)),
                native_palette("options.skin", D2D1::ColorF(0xFFBEA0)),
                native_palette("options.graphics", D2D1::ColorF(0xF6DC8F)),
                native_palette("options.audio", D2D1::ColorF(0xFFB5C2)),
                native_palette("options.input", D2D1::ColorF(0xA7EBE5)),
                native_palette("options.latency", D2D1::ColorF(0xF7A795)),
                native_palette("options.profile", D2D1::ColorF(0xF1DFC0)),
                native_palette("options.mode", D2D1::ColorF(0xCEE79A)),
                native_palette("options.key_test", D2D1::ColorF(0x91D9C1))};
            const float gap = native_metric("options_grid.gap", 22.0f);
            const float width = (right - left - gap * 4.0f) / 5.0f;
            const float height = native_metric("options_grid.height", 270.0f);
            std::size_t selected = 0;
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                const auto& row = data.generic.rows[i];
                if (row.selected) selected = i;
                const float x = left + static_cast<float>(i % 5) * (width + gap);
                const float y = top + 40.0f + static_cast<float>(i / 5) * (height + gap);
                const D2D1_RECT_F rect = native_rect("options_grid.rect.001", D2D1::RectF(x, y, x + width, y + height));
                const auto rr = D2D1::RoundedRect(rect, 18, 18);
                const auto saved = d2d_->card_brush->GetColor();
                const auto pastel = colors[i % colors.size()];
                const float tint = row.selected ? 0.36f : 0.22f;
                d2d_->card_brush->SetColor(D2D1::ColorF(
                    0.055f + pastel.r * tint, 0.055f + pastel.g * tint, 0.055f + pastel.b * tint, 1.0f));
                ctx->FillRoundedRectangle(rr, d2d_->card_brush.Get());
                d2d_->card_brush->SetColor(D2D1::ColorF(pastel.r, pastel.g, pastel.b, pastel.a * (row.selected ? 1.0f : 0.70f)));
                ctx->DrawRoundedRectangle(rr, d2d_->card_brush.Get(), row.selected ? 2.4f : 1.2f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(native_rect("options_grid.rect.002", D2D1::RectF(x + 24, y + 24, x + 88, y + 31)), 3, 3), d2d_->card_brush.Get());
                draw_text_clipped(to_wide(row.label), d2d_->option_format.Get(),
                    native_rect("options_grid.rect.003", D2D1::RectF(x + 24, y + 50, x + width - 24, y + 98)), d2d_->text_brush.Get());
                d2d_->card_brush->SetColor(pastel);
                const auto alignment = d2d_->title_format->GetParagraphAlignment();
                d2d_->title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                draw_text_clipped_aligned(to_wide(row.value), d2d_->title_format.Get(),
                    native_rect("options_grid.rect.004", D2D1::RectF(x + 24, y + 114, x + width - 24, y + 222)),
                    d2d_->card_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
                d2d_->title_format->SetParagraphAlignment(alignment);
                // Selection also has an inset outline, so hue is never the only cue.
                if (row.selected) {
                    d2d_->card_brush->SetColor(D2D1::ColorF(pastel.r, pastel.g, pastel.b, pastel.a * 0.65f));
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(inset_rect(rect, 5, 5), 14, 14),
                        d2d_->card_brush.Get(), 1.0f);
                }
                d2d_->card_brush->SetColor(saved);
                const std::array<native_menu_assets::Asset, 10> icons = {
                    native_menu_assets::kSliders, native_menu_assets::kInput, native_menu_assets::kPrism,
                    native_menu_assets::kDisplay, native_menu_assets::kAudio, native_menu_assets::kInput,
                    native_menu_assets::kWave, native_menu_assets::kMark, native_menu_assets::kSliders,
                    native_menu_assets::kInput};
                const float icon_y = 8.0f * (1.0f - native_menu_motion_.entrance(static_cast<float>(i) * 0.035f));
                draw_native_asset(icons[i % icons.size()], native_rect("options_grid.rect.005", D2D1::RectF(x + width - 68, y + 18 + icon_y,
                    x + width - 24, y + 62 + icon_y)), row.selected ? 1.0f : 0.55f);
                if (row.activatable) register_hit(rect, row.target_kind, row.row_index, MenuHitPart::Activate);
            }
            const D2D1_RECT_F description = native_rect("options_grid.rect.006", D2D1::RectF(left, top + 630, right, bottom - 12));
            draw_glass_panel(description, 14, 1, 0, false, 0);
            if (selected < data.generic.card_descriptions.size()) {
                const auto wrapping = d2d_->body_format->GetWordWrapping();
                d2d_->body_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                draw_text_clipped(to_wide(data.generic.card_descriptions[selected]), d2d_->body_format.Get(),
                    native_rect("options_grid.rect.007", D2D1::RectF(description.left + 28, description.top + 28, description.right - 28, description.top + 76)),
                    d2d_->text_brush.Get());
                d2d_->body_format->SetWordWrapping(wrapping);
            }

            if (!data.generic.notes.empty()) {
                std::string tips;
                for (const auto& note : data.generic.notes) { if (!tips.empty()) tips += "\n"; tips += note; }
                const auto wrapping = d2d_->hud_format->GetWordWrapping();
                d2d_->hud_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                draw_text_clipped(to_wide(tips), d2d_->hud_format.Get(),
                    D2D1::RectF(description.left + 28, description.top + 78, description.right - 28, description.bottom - 12),
                    d2d_->muted_brush.Get());
                d2d_->hud_format->SetWordWrapping(wrapping);
            }
