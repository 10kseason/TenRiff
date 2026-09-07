            constexpr std::array<uint32_t, 10> colors{
                0xEFA5A5, 0xF2BB99, 0xC1ACE8, 0x9CBFEB, 0x9ED7BC,
                0x92CDD3, 0xE9D296, 0xE5ACCB, 0xB9CAA1, 0xB8BCE8};
            constexpr float gap = 22.0f;
            const float width = (right - left - gap * 4.0f) / 5.0f;
            constexpr float height = 270.0f;
            std::size_t selected = 0;
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                const auto& row = data.generic.rows[i];
                if (row.selected) selected = i;
                const float x = left + static_cast<float>(i % 5) * (width + gap);
                const float y = top + 40.0f + static_cast<float>(i / 5) * (height + gap);
                const D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + height);
                const auto rr = D2D1::RoundedRect(rect, 18, 18);
                const auto saved = d2d_->card_brush->GetColor();
                const auto pastel = D2D1::ColorF(colors[i]);
                const float tint = row.selected ? 0.26f : 0.11f;
                d2d_->card_brush->SetColor(D2D1::ColorF(
                    0.06f + pastel.r * tint, 0.08f + pastel.g * tint, 0.11f + pastel.b * tint, 1.0f));
                ctx->FillRoundedRectangle(rr, d2d_->card_brush.Get());
                d2d_->card_brush->SetColor(D2D1::ColorF(colors[i], row.selected ? 1.0f : 0.48f));
                ctx->DrawRoundedRectangle(rr, d2d_->card_brush.Get(), row.selected ? 3.0f : 1.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 24, y + 24, x + 64, y + 30), 3, 3), d2d_->card_brush.Get());
                draw_text_clipped(to_wide(row.label), d2d_->option_format.Get(),
                    D2D1::RectF(x + 24, y + 50, x + width - 24, y + 98), d2d_->text_brush.Get());
                d2d_->card_brush->SetColor(D2D1::ColorF(colors[i], 1.0f));
                const auto alignment = d2d_->title_format->GetParagraphAlignment();
                d2d_->title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                draw_text_clipped_aligned(to_wide(row.value), d2d_->title_format.Get(),
                    D2D1::RectF(x + 24, y + 114, x + width - 24, y + 222),
                    d2d_->card_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
                d2d_->title_format->SetParagraphAlignment(alignment);
                d2d_->card_brush->SetColor(saved);
                if (row.activatable) register_hit(rect, row.target_kind, row.row_index, MenuHitPart::Activate);
            }
            const D2D1_RECT_F description = D2D1::RectF(left, top + 630, right, bottom - 12);
            draw_glass_panel(description, 14, 1, 0, false, 0);
            if (selected < data.generic.card_descriptions.size()) {
                const auto wrapping = d2d_->body_format->GetWordWrapping();
                d2d_->body_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                draw_text_clipped(to_wide(data.generic.card_descriptions[selected]), d2d_->body_format.Get(),
                    D2D1::RectF(description.left + 28, description.top + 28, description.right - 28, description.bottom - 20),
                    d2d_->text_brush.Get());
                d2d_->body_format->SetWordWrapping(wrapping);
            }
