        // Native home shares the library palette and guide pagination. MenuApp
        // still owns button order, selection and the empty-library recovery action.
#include "MenuWindow_draw_generic_help.inl"

        if (d2d_->panel_brush) {
            ctx->FillRectangle(D2D1::RectF(0, 0, kBaseWidth, 126), d2d_->panel_brush.Get());
        }
        draw_text_clipped(L"TENRIFF", d2d_->header_format.Get(),
                          D2D1::RectF(64, 24, 370, 94), d2d_->text_brush.Get());
        draw_text_clipped(wloc("HOME", "홈"), d2d_->song_title_format.Get(),
                          D2D1::RectF(420, 44, 850, 88), d2d_->muted_brush.Get());
        const D2D1_RECT_F profile = data.performance.visible
            ? D2D1::RectF(1030, 20, 1430, 110) : D2D1::RectF(1424, 20, 1824, 110);
        draw_glass_panel(profile, 12, 0.94f, 0, false, 0);
        draw_text_clipped(wloc("PROFILE", "프로필"), d2d_->hud_format.Get(),
                          D2D1::RectF(profile.left + 24, profile.top + 12, profile.right - 24, profile.top + 38),
                          d2d_->muted_brush.Get());
        draw_text_clipped(to_wide(data.title.profile.empty() ? "PLAYER" : data.title.profile),
                          d2d_->song_title_format.Get(),
                          D2D1::RectF(profile.left + 24, profile.top + 42, profile.right - 24, profile.bottom - 12),
                          d2d_->text_brush.Get());

        draw_text_clipped(L"BMS RHYTHM GAME", d2d_->hud_format.Get(),
                          D2D1::RectF(96, 198, 932, 226), d2d_->accent_brush.Get());
        draw_text_clipped(wloc("Your music.", "나만의 음악,"), d2d_->header_format.Get(),
                          D2D1::RectF(96, 254, 932, 334), d2d_->text_brush.Get());
        draw_text_clipped(wloc("Your rhythm.", "나만의 리듬."), d2d_->header_format.Get(),
                          D2D1::RectF(96, 334, 932, 414), d2d_->accent_brush.Get());
        draw_text_clipped(wloc("Pick a track. Set your pace. Enjoy the next play.",
                               "좋아하는 곡을 고르고, 나에게 맞는 속도로 즐겨보세요."),
                          d2d_->song_artist_format.Get(), D2D1::RectF(98, 432, 932, 470), d2d_->muted_brush.Get());

        draw_generic_help(D2D1::RectF(96, 516, 932, 924), "title", data.title.guides, {}, false);

        draw_text_clipped(wloc("LET'S PLAY", "시작하기"), d2d_->hud_format.Get(),
                          D2D1::RectF(1016, 206, 1824, 236), d2d_->muted_brush.Get());
        float button_top = 260.0f;
        for (std::size_t index = 0; index < data.title.buttons.size(); ++index) {
            const auto& button = data.title.buttons[index];
            const bool primary = index == 0;
            const float height = primary ? 168.0f : 114.0f;
            const D2D1_RECT_F rect = D2D1::RectF(1016, button_top, 1824, button_top + height);
            const auto rounded = D2D1::RoundedRect(rect, 14, 14);
            register_hit(rect, MenuHitTargetKind::TitleButton, static_cast<int>(index));
            draw_glass_panel(rect, 14, 0.95f, 0, button.selected, 0);
            if (primary && d2d_->accent_brush) {
                ctx->FillRoundedRectangle(rounded, d2d_->accent_brush.Get());
            } else if (button.selected && d2d_->button_selected_brush) {
                ctx->FillRoundedRectangle(rounded, d2d_->button_selected_brush.Get());
            }
            if (button.selected) {
                ID2D1Brush* focus = primary ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                           : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
                if (focus) ctx->DrawRoundedRectangle(rounded, focus, 2.0f);
            }
            const auto saved_text = d2d_->text_brush->GetColor();
            const auto saved_muted = d2d_->muted_brush->GetColor();
            if (primary) {
                d2d_->text_brush->SetColor(D2D1::ColorF(0x0B1620));
                d2d_->muted_brush->SetColor(D2D1::ColorF(0x234652));
            }
            const float label_top = rect.top + (primary ? 32.0f : 18.0f);
            draw_text_clipped(to_wide(button.label), d2d_->menu_button_format.Get(),
                              D2D1::RectF(rect.left + 28, label_top, rect.right - 96, label_top + 50),
                              d2d_->text_brush.Get());
            draw_text_clipped(to_wide(button.detail), d2d_->body_format.Get(),
                              D2D1::RectF(rect.left + 30, label_top + 56, rect.right - 96, rect.bottom - 14),
                              d2d_->muted_brush.Get());
            const auto saved_paragraph = d2d_->menu_icon_format->GetParagraphAlignment();
            d2d_->menu_icon_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            draw_text_clipped_aligned(primary ? L"\u2192" : to_wide(button.icon), d2d_->menu_icon_format.Get(),
                                      D2D1::RectF(rect.right - 84, rect.top, rect.right - 20, rect.bottom),
                                      d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->menu_icon_format->SetParagraphAlignment(saved_paragraph);
            d2d_->text_brush->SetColor(saved_text);
            d2d_->muted_brush->SetColor(saved_muted);
            button_top = rect.bottom + 18.0f;
        }
        draw_text_clipped(wloc("UP / DOWN  Select     ENTER or double-click  Open",
                               "위 / 아래  선택     ENTER 또는 더블클릭  열기"),
                          d2d_->body_format.Get(), D2D1::RectF(1018, 866, 1824, 902), d2d_->muted_brush.Get());

        const D2D1_RECT_F footer = D2D1::RectF(64, 972, 1856, 1048);
        draw_glass_panel(footer, 12, 0.9f, 0, false, 0);
        draw_text_clipped(wloc("SELECTED TRACK", "선택한 곡"), d2d_->hud_format.Get(),
                          D2D1::RectF(88, 990, 330, 1028), d2d_->muted_brush.Get());
        const std::string track = data.title.track.empty() || data.title.track == "-"
            ? loc("No track selected", "선택한 곡 없음") : data.title.track;
        draw_text_clipped(to_wide(track), d2d_->song_title_format.Get(),
                          D2D1::RectF(340, 990, 1580, 1030), d2d_->text_brush.Get());
        draw_text_clipped_aligned(wloc("F1  HELP", "F1  도움말"), d2d_->hud_format.Get(),
                                  D2D1::RectF(1610, 990, 1832, 1028), d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
