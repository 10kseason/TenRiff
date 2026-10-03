        // Native home shares the library palette and guide pagination. MenuApp
        // still owns button order, selection and the empty-library recovery action.
#include "MenuWindow_draw_generic_help.inl"

        if (d2d_->panel_brush) {
            ctx->FillRectangle(native_rect("title_native.rect.001", D2D1::RectF(0, 0, kBaseWidth, 126)), d2d_->panel_brush.Get());
        }
        const auto logo = skin_layout_rect(data, "title.logo", D2D1::RectF(56, 24, 418, 94));
        draw_native_asset(native_menu_assets::kMark, native_rect("title_native.rect.002", D2D1::RectF(logo.left, logo.top + 2, logo.left + 64, logo.bottom)));
        draw_native_wordmark(d2d_->header_format.Get(),
                             native_rect("title_native.rect.003", D2D1::RectF(logo.left + 82, logo.top, logo.right, logo.bottom)));
        draw_text_clipped(wloc("HOME", "홈"), d2d_->song_title_format.Get(),
                          native_rect("title_native.rect.004", D2D1::RectF(466, 44, 850, 88)), d2d_->muted_brush.Get());
        const D2D1_RECT_F profile = data.performance.visible
            ? native_rect("title_native.rect.005", D2D1::RectF(1030, 20, 1430, 110)) : native_rect("title_native.rect.006", D2D1::RectF(1424, 20, 1824, 110));
        draw_glass_panel(profile, 12, 0.94f, 0, false, 0);
        const auto avatar = D2D1::RectF(profile.left + 12, profile.top + 12, profile.left + 78, profile.top + 78);
        if (auto* bitmap = find_song_card_preview_bitmap(data.title.profile_avatar_path)) {
            const auto source = centered_bitmap_source_rect(bitmap->GetSize(), avatar);
            ctx->DrawBitmap(bitmap, avatar, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
        } else {
            draw_native_asset(native_menu_assets::kMark, avatar, 0.90f);
        }
        draw_native_panel_edge(avatar, 5, 0x96D5E4, 0.7f);
        draw_text_clipped(wloc("PROFILE", "프로필"), d2d_->hud_format.Get(),
                          native_rect("title_native.rect.007", D2D1::RectF(profile.left + 94, profile.top + 12, profile.right - 24, profile.top + 38)),
                          d2d_->muted_brush.Get());
        draw_text_clipped(to_wide(data.title.profile.empty() ? "PLAYER" : data.title.profile),
                          d2d_->song_title_format.Get(),
                          native_rect("title_native.rect.008", D2D1::RectF(profile.left + 94, profile.top + 42, profile.right - 24, profile.bottom - 12)),
                          d2d_->text_brush.Get());

        draw_text_clipped(L"BMS RHYTHM GAME", d2d_->hud_format.Get(),
                          native_rect("title_native.rect.009", D2D1::RectF(96, 198, 932, 226)), d2d_->accent_brush.Get());
        const float hero_reveal = native_menu_motion_.entrance(0.06f, 0.65f);
        const float hero_float = static_cast<float>(std::sin(native_seconds * 1.15) * 7.0);
        if (!has_title_art && data.lobby_skin.background_path.empty()) {
        draw_native_orbit(800, 358 + hero_float, 120, 0.56f * hero_reveal);
        draw_native_asset(native_menu_assets::kPrism,
                          native_rect("title_native.rect.010", D2D1::RectF(708, 250 + hero_float, 892, 458 + hero_float)),
                          0.85f * hero_reveal, static_cast<float>(std::sin(native_seconds * 0.35) * 5));
        draw_native_asset(native_menu_assets::kSpark, native_rect("title_native.rect.011", D2D1::RectF(916, 255, 934, 273)), hero_reveal * 0.8f);
        }
        draw_text_clipped(wloc("Your music.", "나만의 음악,"), d2d_->header_format.Get(),
                          native_rect("title_native.rect.012", D2D1::RectF(96, 254, 664, 334)), d2d_->text_brush.Get());
        draw_text_clipped(wloc("Your rhythm.", "나만의 리듬."), d2d_->header_format.Get(),
                          native_rect("title_native.rect.013", D2D1::RectF(96, 334, 664, 414)), d2d_->accent_brush.Get());
        draw_text_clipped(wloc("Pick a track. Set your pace. Enjoy the next play.",
                               "좋아하는 곡을 고르고, 나의 리듬으로."),
                          d2d_->song_artist_format.Get(), native_rect("title_native.rect.014", D2D1::RectF(98, 438, 684, 476)), d2d_->muted_brush.Get());

        draw_generic_help(native_rect("title_native.rect.015", skin_layout_rect(data, "title.guide", D2D1::RectF(96, 516, 932, 924))), "title", data.title.guides, {}, false);

        const auto buttons = skin_layout_rect(data, "title.buttons", D2D1::RectF(1016, 260, 1824, 824));
        // Keep the whole group editable while preserving relative row spacing.
        const float button_scale_y = std::max(0.1f, (buttons.bottom - buttons.top) / 564.0f);
        draw_text_clipped(wloc("LET'S PLAY", "시작하기"), d2d_->hud_format.Get(),
                          native_rect("title_native.rect.016", D2D1::RectF(buttons.left, buttons.top - 54, buttons.right, buttons.top - 24)), d2d_->muted_brush.Get());
        float button_top = buttons.top;
        for (std::size_t index = 0; index < data.title.buttons.size(); ++index) {
            const auto& button = data.title.buttons[index];
            const bool primary = index == 0;
            const float height = (primary ? 168.0f : 114.0f) * button_scale_y;
            const float entrance_x = 24.0f * (1.0f - native_menu_motion_.entrance(static_cast<float>(index) * 0.055f));
            const D2D1_RECT_F rect = native_rect("title_native.rect.017", D2D1::RectF(buttons.left + entrance_x, button_top, buttons.right + entrance_x, button_top + height));
            const auto rounded = D2D1::RoundedRect(rect, 14, 14);
            register_hit(rect, MenuHitTargetKind::TitleButton, static_cast<int>(index));
            draw_glass_panel(rect, 14, 0.95f, 0, button.selected, 0);
            if (primary && d2d_->accent_brush) {
                const auto saved = d2d_->card_brush->GetColor();
                d2d_->card_brush->SetColor(native_palette("title.primary", D2D1::ColorF(0x111111)));
                ctx->FillRoundedRectangle(rounded, d2d_->card_brush.Get());
                d2d_->card_brush->SetColor(saved);
            } else if (button.selected && d2d_->button_selected_brush) {
                ctx->FillRoundedRectangle(rounded, d2d_->button_selected_brush.Get());
            }
            if (button.selected) {
                ID2D1Brush* focus = primary ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                           : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
                if (focus) ctx->DrawRoundedRectangle(rounded, focus, 2.0f);
            }
            draw_native_focus(rect, 14, index, static_cast<int>(index), button.selected, primary);
            const auto saved_text = d2d_->text_brush->GetColor();
            const auto saved_muted = d2d_->muted_brush->GetColor();
            if (primary) {
                d2d_->text_brush->SetColor(native_palette("title.primary_text", D2D1::ColorF(0xF5F5F5)));
                d2d_->muted_brush->SetColor(native_palette("title.primary_detail", D2D1::ColorF(0xC2CCD3)));
            }
            const float label_top = rect.top + (primary ? 32.0f : 18.0f);
            draw_text_clipped(to_wide(button.label), d2d_->menu_button_format.Get(),
                              native_rect("title_native.rect.018", D2D1::RectF(rect.left + 28, label_top, rect.right - 96, label_top + 50)),
                              d2d_->text_brush.Get());
            draw_text_clipped(to_wide(button.detail), d2d_->body_format.Get(),
                              native_rect("title_native.rect.019", D2D1::RectF(rect.left + 30, label_top + 56, rect.right - 96, rect.bottom - 14)),
                              d2d_->muted_brush.Get());
            const auto saved_paragraph = d2d_->menu_icon_format->GetParagraphAlignment();
            d2d_->menu_icon_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            const std::array<native_menu_assets::Asset, 4> button_assets = {
                native_menu_assets::kChevron, native_menu_assets::kNetwork,
                native_menu_assets::kSliders, native_menu_assets::kExit};
            if (primary) {
                // Keep the action arrow in the same high-contrast text color.
                draw_text_clipped_aligned(L"\u2192", d2d_->menu_icon_format.Get(),
                    native_rect("title_native.rect.020", D2D1::RectF(rect.right - 84, rect.top, rect.right - 20, rect.bottom)),
                    d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            } else {
                const float cy = (rect.top + rect.bottom) * 0.5f;
                draw_native_asset(button_assets[std::min<std::size_t>(index, 3)],
                    native_rect("title_native.rect.021", D2D1::RectF(rect.right - 78, cy - 24, rect.right - 30, cy + 24)), button.selected ? 1.0f : 0.65f);
            }
            d2d_->menu_icon_format->SetParagraphAlignment(saved_paragraph);
            d2d_->text_brush->SetColor(saved_text);
            d2d_->muted_brush->SetColor(saved_muted);
            button_top = rect.bottom + 18.0f * button_scale_y;
        }
        draw_text_clipped(wloc("UP / DOWN  Select     ENTER or double-click  Open",
                               "위 / 아래  선택     ENTER 또는 더블클릭  열기"),
                          d2d_->body_format.Get(), native_rect("title_native.rect.022", D2D1::RectF(buttons.left + 2, buttons.bottom + 42, buttons.right, buttons.bottom + 78)), d2d_->muted_brush.Get());

        const D2D1_RECT_F footer = native_rect("title_native.rect.023", skin_layout_rect(data, "title.footer", D2D1::RectF(64, 972, 1856, 1048)));
        draw_glass_panel(footer, 12, 0.9f, 0, false, 0);
        draw_text_clipped(wloc("SELECTED TRACK", "선택한 곡"), d2d_->hud_format.Get(),
                          native_rect("title_native.rect.024", D2D1::RectF(footer.left + 24, footer.top + 18, footer.left + 266, footer.top + 56)), d2d_->muted_brush.Get());
        const std::string track = data.title.track.empty() || data.title.track == "-"
            ? loc("No track selected", "선택한 곡 없음") : data.title.track;
        draw_text_clipped(to_wide(track), d2d_->song_title_format.Get(),
                          native_rect("title_native.rect.025", D2D1::RectF(footer.left + 276, footer.top + 18, footer.right - 466, footer.top + 58)), d2d_->text_brush.Get());
        draw_native_spectrum(native_rect("title_native.rect.026", skin_layout_rect(data, "title.spectrum", D2D1::RectF(footer.right - 426, footer.top + 18, footer.right - 282, footer.top + 56))), 0.75f, 18);
        draw_text_clipped_aligned(wloc("F1  HELP", "F1  도움말"), d2d_->hud_format.Get(),
                                  native_rect("title_native.rect.027", D2D1::RectF(footer.right - 246, footer.top + 18, footer.right - 24, footer.top + 56)), d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
