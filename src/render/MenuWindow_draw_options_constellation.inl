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
                    if (row.activatable) register_hit(rect, row.target_kind, row.row_index, MenuHitPart::Activate);
                    ++benchmark_options_constellation_cards_;
                }
