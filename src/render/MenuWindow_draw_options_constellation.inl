                // Same model order, row indices and hit targets as the options
                // controller. Only the built-in surface changes to the mockup.
                constexpr std::array<uint32_t, 8> icon_colors{
                    0xFFD600, 0x00D4EB, 0xD34CFF, 0x249CFF, 0xFF4039, 0x00E5B4, 0xFFCF00, 0xFF38DF};
                const std::array<native_menu_assets::Asset, 8> icons{
                    native_menu_assets::kKeys, native_menu_assets::kKeymap, native_menu_assets::kSkin,
                    native_menu_assets::kDisplay, native_menu_assets::kAudio, native_menu_assets::kInput,
                    native_menu_assets::kLatency, native_menu_assets::kProfile};
                constexpr float gap = 22, height = 270, radius = 22;
                const float width = (right - left - gap * 3) / 4;
                for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                    const auto& row = data.generic.rows[i];
                    const float x = left + static_cast<float>(i % 4) * (width + gap);
                    const float y = top + 40 + static_cast<float>(i / 4) * (height + gap);
                    const auto rect = D2D1::RectF(x, y, x + width, y + height);
                    // Scale the complete card around its existing center so text,
                    // icons and decoration retain their proportions in every language.
                    constexpr float card_scale = .85f;
                    D2D1_MATRIX_3X2_F saved_card_transform{};
                    ctx->GetTransform(&saved_card_transform);
                    ctx->SetTransform(D2D1::Matrix3x2F::Scale(card_scale, card_scale,
                        D2D1::Point2F(x + width * .5f, y + height * .5f)) * saved_card_transform);
                    const auto rr = D2D1::RoundedRect(rect, radius, radius);
                    const uint32_t edge = row.selected ? 0xFFE091 : 0xE0E9FF;
                    ctx->FillRoundedRectangle(rr, semantic_color(0x03081F, .62f));
                    draw_options_stars(rect, i, .85f);
                    // A few translucent border strokes give a local halo without
                    // offscreen blur passes or animation that competes with text.
                    ctx->DrawRoundedRectangle(rr, semantic_color(edge, row.selected ? .07f : .035f), 13);
                    ctx->DrawRoundedRectangle(rr, semantic_color(edge, row.selected ? .16f : .09f), 7);
                    ctx->DrawRoundedRectangle(rr, semantic_color(edge, row.selected ? .48f : .30f), 4);
                    ctx->DrawRoundedRectangle(rr, semantic_color(row.selected ? 0xFFF3C3 : 0xF0F3FF, .98f), 2.2f);
                    ctx->FillRectangle(D2D1::RectF(x + 24, y + 26, x + 68, y + 31), semantic_color(0xF5F6FF));
                    const auto icon_color = D2D1::ColorF(icon_colors[i]);
                    draw_native_asset(icons[i], D2D1::RectF(x + width - 69, y + 23, x + width - 26, y + 66),
                                      1, 0, &icon_color);
                    // Keep actual labels/current values. Existing text fitting
                    // handles long translations and the large-font preference.
                    const auto label_alignment = d2d_->title_format->GetParagraphAlignment();
                    d2d_->title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    draw_text_clipped_aligned(to_wide(row.label), d2d_->title_format.Get(),
                        D2D1::RectF(x + 24, y + 63, x + width - 24, y + 119), semantic_color(0xFFFFFF), DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->title_format->SetParagraphAlignment(label_alignment);
                    const auto value_alignment = d2d_->header_format->GetParagraphAlignment();
                    d2d_->header_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    draw_text_clipped_aligned(to_wide(row.value), d2d_->header_format.Get(),
                        D2D1::RectF(x + 24, y + 127, x + width - 24, y + 221), semantic_color(0xFFFFFF), DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->header_format->SetParagraphAlignment(value_alignment);
                    ctx->SetTransform(saved_card_transform);
                    // Hit regions use untransformed design coordinates, so shrink
                    // them explicitly to match the visible card, excluding the gaps.
                    const float inset_x = width * (1 - card_scale) * .5f;
                    const float inset_y = height * (1 - card_scale) * .5f;
                    if (row.activatable) register_hit(D2D1::RectF(x + inset_x, y + inset_y,
                        x + width - inset_x, y + height - inset_y),
                        row.target_kind, row.row_index, MenuHitPart::Activate);
                    ++benchmark_options_constellation_cards_;
                }
                // The model already supplies localized help; keep it outside the
                // scaled cards so the selected option has one readable hint line.
                for (std::size_t i = 0; i < data.generic.rows.size(); ++i) {
                    if (!data.generic.rows[i].selected || i >= data.generic.card_descriptions.size()) continue;
                    const float hint_top = top + 40 + height * 2 + gap + 32;
                    const auto hint_wrapping = d2d_->body_format->GetWordWrapping();
                    d2d_->body_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                    draw_text_clipped_aligned(to_wide(data.generic.card_descriptions[i]),
                        d2d_->body_format.Get(), D2D1::RectF(left + 24, hint_top, right - 24, hint_top + 48),
                        semantic_color(0xE0E9FF), DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->body_format->SetWordWrapping(hint_wrapping);
                    break;
                }
