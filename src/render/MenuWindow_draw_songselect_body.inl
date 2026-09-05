        const float ambient_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 6.4, 0.18));
        const float selection_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.2, 0.46));

        auto draw_rule = [&](float left, float top, float right, float opacity = 0.34f) {
            if (!d2d_->button_border_brush) {
                return;
            }
            const float saved = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(opacity);
            ctx->DrawLine(D2D1::Point2F(left, top), D2D1::Point2F(right, top),
                          d2d_->button_border_brush.Get(), 1.0f);
            d2d_->button_border_brush->SetOpacity(saved);
        };
        auto draw_centered_text = [&](const std::wstring& text,
                                      IDWriteTextFormat* format,
                                      const D2D1_RECT_F& rect,
                                      ID2D1Brush* brush,
                                      bool center_vertically = false) {
            if (!format || !brush) {
                return;
            }
            const auto saved_alignment = format->GetTextAlignment();
            const auto saved_paragraph_alignment = format->GetParagraphAlignment();
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            if (center_vertically) format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            draw_text_clipped(text, format, rect, brush);
            format->SetTextAlignment(saved_alignment);
            format->SetParagraphAlignment(saved_paragraph_alignment);
        };
        auto draw_trailing_text = [&](const std::wstring& text,
                                      IDWriteTextFormat* format,
                                      const D2D1_RECT_F& rect,
                                      ID2D1Brush* brush) {
            if (!format || !brush) {
                return;
            }
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            draw_text_clipped(text, format, rect, brush);
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        };
        auto draw_meta_pair = [&](const D2D1_RECT_F& rect,
                                  std::string_view label,
                                  std::string_view value,
                                  bool emphasized = false) {
            if (d2d_->hud_format && d2d_->muted_brush) {
                draw_text_clipped(to_wide(std::string(label)),
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(rect.left, rect.top, rect.right, rect.top + 23.0f),
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->title_format && d2d_->text_brush) {
                ID2D1Brush* value_brush =
                    emphasized && d2d_->accent_brush
                        ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                        : static_cast<ID2D1Brush*>(d2d_->text_brush.Get());
                draw_text_clipped(to_wide(std::string(value)),
                                  d2d_->title_format.Get(),
                                  D2D1::RectF(rect.left, rect.top + 22.0f, rect.right, rect.bottom),
                                  value_brush);
            }
        };

        if (d2d_->accent_brush && !modern_library_screen) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x19DDE8));
            d2d_->accent_brush->SetOpacity(0.018f + ambient_pulse * 0.010f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(850.0f, 420.0f), 620.0f, 370.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x9D62F2));
            d2d_->accent_brush->SetOpacity(0.012f + ambient_pulse * 0.008f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(1610.0f, 830.0f), 360.0f, 220.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        const D2D1_RECT_F top_bar =
            skin_layout_rect(data, "song_select.top_bar",
                             D2D1::RectF(0.0f, 0.0f, kBaseWidth, 126.0f));
        if (d2d_->panel_brush) {
            const float saved = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetOpacity(0.88f);
            ctx->FillRectangle(top_bar, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(saved);
        }
        draw_rule(top_bar.left, top_bar.bottom, top_bar.right, 0.56f);

        ID2D1Bitmap* lobby_logo_bitmap =
            data.lobby_skin.enabled
                ? find_song_card_preview_bitmap(data.lobby_skin.logo_path)
                : nullptr;
        const D2D1_RECT_F logo_slot =
            skin_layout_rect(data, "song_select.logo",
                             D2D1::RectF(48.0f, 18.0f, 388.0f, 108.0f));
        if (lobby_logo_bitmap) {
            const D2D1_SIZE_F logo_size = lobby_logo_bitmap->GetSize();
            const D2D1_RECT_F logo_rect = fit_rect_preserve_aspect(logo_slot, logo_size);
            ctx->DrawBitmap(lobby_logo_bitmap, logo_rect, 1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        if (!lobby_logo_bitmap && d2d_->song_logo_format && d2d_->text_brush) {
            const D2D1_RECT_F logo_text_rect =
                D2D1::RectF(logo_slot.left + 2.0f, logo_slot.top - 2.0f,
                            logo_slot.right - 4.0f, logo_slot.bottom - 10.0f);
            if (d2d_->accent_brush && !modern_library_screen) {
                const float saved = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.20f + ambient_pulse * 0.08f);
                draw_text_clipped(L"TENRIFF", d2d_->song_logo_format.Get(),
                                  D2D1::RectF(logo_text_rect.left + 3.0f,
                                              logo_text_rect.top + 2.0f,
                                              logo_text_rect.right + 3.0f,
                                              logo_text_rect.bottom + 2.0f),
                                  d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(0.72f);
                ctx->FillRectangle(D2D1::RectF(logo_slot.left + 8.0f,
                                               logo_slot.bottom - 13.0f,
                                               logo_slot.left + 168.0f,
                                               logo_slot.bottom - 10.0f),
                                   d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved);
            }
            draw_text_clipped(L"TENRIFF", modern_library_screen ? d2d_->header_format.Get() : d2d_->song_logo_format.Get(),
                              logo_text_rect, d2d_->text_brush.Get());
        }
        if (!lobby_logo_bitmap && d2d_->hud_format && d2d_->muted_brush) {
            draw_text_clipped(wloc("YOUR MUSIC, YOUR PACE", "나만의 리듬으로"),
                              d2d_->hud_format.Get(),
                              D2D1::RectF(logo_slot.left + (modern_library_screen ? 4.0f : 178.0f), logo_slot.top + 68.0f,
                                          logo_slot.right + 2.0f, logo_slot.bottom),
                              d2d_->muted_brush.Get());
        }
        if (d2d_->button_border_brush) {
            const float saved = d2d_->button_border_brush->GetOpacity();
            const float divider_x = logo_slot.right + 22.0f;
            d2d_->button_border_brush->SetOpacity(0.34f);
            ctx->DrawLine(D2D1::Point2F(divider_x, logo_slot.top + 14.0f),
                          D2D1::Point2F(divider_x, logo_slot.bottom - 12.0f),
                          d2d_->button_border_brush.Get(), 1.0f);
            d2d_->button_border_brush->SetOpacity(saved);
        }

        const std::array<int, 5> top_nav_indices = {0, 1, 4, 5, 6};
        const D2D1_RECT_F nav_bar =
            skin_layout_rect(data, "song_select.nav",
                             D2D1::RectF(492.0f, 12.0f, modern_library_screen ? 1400.0f : 1244.0f, 126.0f));
        const float nav_left = nav_bar.left;
        const float nav_width =
            (nav_bar.right - nav_bar.left) / static_cast<float>(top_nav_indices.size());
        for (std::size_t i = 0; i < top_nav_indices.size(); ++i) {
            const int source_index = top_nav_indices[i];
            if (source_index >= static_cast<int>(data.song_select.left_nav.size())) {
                continue;
            }
            const auto& item = data.song_select.left_nav[static_cast<std::size_t>(source_index)];
            const float x0 = nav_left + static_cast<float>(i) * nav_width;
            const D2D1_RECT_F tab =
                D2D1::RectF(x0, nav_bar.top, x0 + nav_width - 8.0f, nav_bar.bottom);
            const bool route_active =
                (source_index == 0 && !data.song_select.showing_sources &&
                 !data.song_select.showing_records) ||
                (source_index == 1 && data.song_select.showing_sources) ||
                (source_index == 4 && data.song_select.showing_records);
            register_hit(tab, MenuHitTargetKind::SongNavButton, source_index);
            if (route_active && d2d_->accent_brush) {
                const float saved = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.11f + selection_pulse * 0.05f);
                ctx->FillRectangle(tab, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(0.90f);
                ctx->FillRectangle(D2D1::RectF(tab.left, tab.bottom - 4.0f, tab.right, tab.bottom),
                                   d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved);
            }
            if (d2d_->song_nav_format && d2d_->text_brush) {
                draw_centered_text(to_wide(item.label),
                                   modern_library_screen ? d2d_->song_title_format.Get() : d2d_->song_nav_format.Get(),
                                   D2D1::RectF(tab.left + 6.0f, tab.top + 39.0f,
                                               tab.right - 8.0f, tab.bottom - 10.0f),
                                   route_active && d2d_->accent_brush
                                       ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                       : static_cast<ID2D1Brush*>(d2d_->text_brush.Get()));
            }
        }

        const D2D1_RECT_F profile_panel =
            skin_layout_rect(data, "song_select.profile",
                             D2D1::RectF(1518.0f, 16.0f, 1880.0f, 112.0f));
        register_hit(profile_panel, MenuHitTargetKind::SongProfilePanel, 0);
        draw_glass_panel(profile_panel, 14.0f, 0.78f, 0.24f, false, 3.0f);
        const D2D1_RECT_F avatar_rect = skin_layout_rect(
            data, "song_select.avatar",
            D2D1::RectF(profile_panel.left + 12.0f, profile_panel.top + 10.0f,
                        profile_panel.left + 88.0f, profile_panel.top + 86.0f));
        ctx->PushAxisAlignedClip(profile_panel, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        ID2D1Bitmap* avatar_bitmap =
            find_song_card_preview_bitmap(data.song_select.profile_avatar_path);
        if (avatar_bitmap) {
            const D2D1_RECT_F source =
                centered_bitmap_source_rect(avatar_bitmap->GetSize(), avatar_rect);
            ctx->DrawBitmap(avatar_bitmap, avatar_rect, 1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
        } else {
            draw_glass_panel(avatar_rect, 12.0f, 0.92f, 0.54f, true, 2.0f);
            if (d2d_->title_format && d2d_->accent_brush) {
                draw_centered_text(L"TR", d2d_->title_format.Get(),
                                   avatar_rect, d2d_->accent_brush.Get());
            }
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(avatar_rect, 12.0f, 12.0f),
                                      d2d_->button_border_brush.Get(), 1.2f);
        }
        ctx->PopAxisAlignedClip();
        if (d2d_->title_format && d2d_->text_brush) {
            draw_text_clipped(to_wide(data.song_select.profile.empty()
                                          ? std::string("PLAYER")
                                          : data.song_select.profile),
                              d2d_->title_format.Get(),
                              D2D1::RectF(avatar_rect.right + 18.0f, profile_panel.top + 18.0f,
                                          profile_panel.right - 16.0f, profile_panel.top + 56.0f),
                              d2d_->text_brush.Get());
        }
        if (d2d_->hud_format && d2d_->muted_brush) {
            draw_text_clipped(wloc("PROFILE  /  CLICK TO EDIT", "프로필  /  클릭하여 편집"),
                              d2d_->hud_format.Get(),
                              D2D1::RectF(avatar_rect.right + 20.0f, profile_panel.top + 58.0f,
                                          profile_panel.right - 16.0f, profile_panel.bottom - 10.0f),
                              d2d_->muted_brush.Get());
        }

        const D2D1_RECT_F left_panel =
            skin_layout_rect(data, "song_select.left_panel",
                             D2D1::RectF(38.0f, 152.0f, 720.0f, 922.0f));
        const D2D1_RECT_F center_panel =
            skin_layout_rect(data, "song_select.center_panel",
                             D2D1::RectF(744.0f, 152.0f, 1373.0f, 922.0f));
        const D2D1_RECT_F right_panel =
            skin_layout_rect(data, "song_select.right_panel",
                             D2D1::RectF(1397.0f, 152.0f, 1882.0f, 922.0f));

        draw_glass_panel(left_panel, 12.0f, 0.78f, 0.20f, false, 5.0f);
        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring library_title =
                data.song_select.showing_sources
                    ? wloc("SONG SOURCES", "곡 소스")
                    : (data.song_select.showing_records
                           ? (data.song_select.online_records
                                  ? wloc("ONLINE RECORDS", "온라인 기록")
                                  : wloc("LOCAL RECORDS", "로컬 기록"))
                           : wloc("SONG LIBRARY", "곡 라이브러리"));
            draw_text_clipped(library_title,
                              d2d_->title_format.Get(),
                              D2D1::RectF(left_panel.left + 18.0f, left_panel.top + 16.0f,
                                          left_panel.right - 130.0f, left_panel.top + 54.0f),
                              d2d_->text_brush.Get());
        }
        if (d2d_->hud_format && d2d_->muted_brush) {
            const int item_count = data.song_select.showing_sources
                                       ? data.song_select.source_count
                                       : (data.song_select.showing_records
                                              ? data.song_select.record_count
                                              : data.song_select.song_count);
            draw_trailing_text(to_wide(format_int_with_commas(item_count)),
                               d2d_->hud_format.Get(),
                               D2D1::RectF(left_panel.right - 130.0f, left_panel.top + 24.0f,
                                           left_panel.right - 18.0f, left_panel.top + 52.0f),
                               d2d_->muted_brush.Get());
        }
        draw_rule(left_panel.left + 18.0f, left_panel.top + 66.0f,
                  left_panel.right - 18.0f, 0.32f);
        if (data.song_select.indexing && d2d_->hud_format && d2d_->accent_brush) {
            std::string indexing_text =
                data.song_select.indexing_stage.empty() ? "INDEXING" : data.song_select.indexing_stage;
            if (data.song_select.indexing_percent >= 0) {
                indexing_text += " " + std::to_string(data.song_select.indexing_percent) + "%";
            }
            draw_text_clipped(to_wide(indexing_text), d2d_->hud_format.Get(),
                              D2D1::RectF(left_panel.left + 18.0f, left_panel.top + 45.0f,
                                          left_panel.right - 18.0f, left_panel.top + 68.0f),
                              d2d_->accent_brush.Get());
        }

        const float card_left = left_panel.left + 12.0f;
        const float card_right = left_panel.right - 18.0f;
        const float card_top = left_panel.top + 76.0f;
        const float card_height = 86.0f;
        const float card_gap = 8.0f;
        for (std::size_t i = 0; i < data.song_select.songs.size(); ++i) {
            const auto& song = data.song_select.songs[i];
            const float y0 = card_top + static_cast<float>(i) * (card_height + card_gap);
            const D2D1_RECT_F card = D2D1::RectF(card_left, y0, card_right, y0 + card_height);
            if (card.bottom > left_panel.bottom - 12.0f) {
                break;
            }
            register_hit(card, MenuHitTargetKind::SongCard, song.song_index);
            draw_glass_panel(card, 9.0f,
                             song.selected ? 0.90f : 0.56f,
                             song.selected ? 0.60f : 0.06f,
                             song.selected, song.selected ? 3.0f : 1.0f);
            if (modern_library_screen && song.selected && d2d_->button_selected_brush) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(card, 9.0f, 9.0f),
                                          d2d_->button_selected_brush.Get());
            }
            if (song.selected && d2d_->accent_brush) {
                const float saved = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.94f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(card.left, card.top + 9.0f,
                                                  card.left + 4.0f, card.bottom - 9.0f),
                                      2.0f, 2.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved);
            }

            const D2D1_RECT_F jacket =
                D2D1::RectF(card.left + 10.0f, card.top + 9.0f,
                            card.left + 80.0f, card.bottom - 9.0f);
            ID2D1Bitmap* jacket_bitmap = find_song_card_preview_bitmap(song.background_path);
            if (jacket_bitmap) {
                const D2D1_RECT_F source =
                    centered_bitmap_source_rect(jacket_bitmap->GetSize(), jacket);
                ctx->DrawBitmap(jacket_bitmap, jacket, 0.98f,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
            } else if (d2d_->card_brush) {
                const D2D1_COLOR_F fallback = jacket_color(song.title);
                const D2D1_COLOR_F saved_color = d2d_->card_brush->GetColor();
                const float saved_opacity = d2d_->card_brush->GetOpacity();
                d2d_->card_brush->SetColor(fallback);
                d2d_->card_brush->SetOpacity(0.70f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(jacket, 6.0f, 6.0f),
                                          d2d_->card_brush.Get());
                d2d_->card_brush->SetColor(saved_color);
                d2d_->card_brush->SetOpacity(saved_opacity);
            }

            const float text_left = jacket.right + 12.0f;
            const float value_left = card.right - 78.0f;
            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped(to_wide(song.title.empty() ? std::string("-") : song.title),
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(text_left, card.top + 10.0f,
                                              value_left - 8.0f, card.top + 38.0f),
                                  d2d_->text_brush.Get());
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                draw_text_clipped(to_wide(song.artist), d2d_->hud_format.Get(),
                                  D2D1::RectF(text_left, card.top + 38.0f,
                                              value_left - 8.0f, card.top + 61.0f),
                                  d2d_->muted_brush.Get());
                draw_text_clipped(to_wide(song.detail), d2d_->hud_format.Get(),
                                  D2D1::RectF(text_left, card.top + 60.0f,
                                              value_left - 8.0f, card.bottom - 5.0f),
                                  song.selected && d2d_->accent_brush
                                      ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                      : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()));
            }
            if (d2d_->title_format && d2d_->text_brush) {
                std::string level = !song.level_label.empty()
                                        ? song.level_label
                                        : (song.level > 0 ? std::to_string(song.level) : "");
                draw_trailing_text(to_wide(level), d2d_->title_format.Get(),
                                   D2D1::RectF(value_left, card.top + 13.0f,
                                               card.right - 12.0f, card.top + 48.0f),
                                   song.selected && d2d_->accent_brush
                                       ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                       : static_cast<ID2D1Brush*>(d2d_->text_brush.Get()));
            }
            if (song.favorite && d2d_->hud_format && d2d_->accent_brush) {
                draw_trailing_text(L"FAV", d2d_->hud_format.Get(),
                                   D2D1::RectF(value_left, card.top + 54.0f,
                                               card.right - 12.0f, card.bottom - 8.0f),
                                   d2d_->accent_brush.Get());
            } else if (!song.lamp.empty() && d2d_->hud_format && d2d_->muted_brush) {
                draw_trailing_text(to_wide(song.lamp), d2d_->hud_format.Get(),
                                   D2D1::RectF(value_left, card.top + 54.0f,
                                               card.right - 12.0f, card.bottom - 8.0f),
                                   d2d_->muted_brush.Get());
            }
        }

        if (data.song_select.songs.empty()) {
            if (d2d_->title_format && d2d_->text_brush) {
                draw_centered_text(to_wide(data.song_select.empty_title),
                                   d2d_->title_format.Get(),
                                   D2D1::RectF(left_panel.left + 34.0f, left_panel.top + 260.0f,
                                               left_panel.right - 34.0f, left_panel.top + 310.0f),
                                   d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const auto saved_wrapping = d2d_->body_format->GetWordWrapping();
                if (modern_library_screen) d2d_->body_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                draw_centered_text(to_wide(data.song_select.empty_message),
                                   d2d_->body_format.Get(),
                                   D2D1::RectF(left_panel.left + 34.0f, left_panel.top + 320.0f,
                                               left_panel.right - 34.0f, left_panel.top + 430.0f),
                                   d2d_->muted_brush.Get(), true);
                d2d_->body_format->SetWordWrapping(saved_wrapping);
            }
        }

        if (data.song_select.list_total_count > data.song_select.list_visible_count &&
            data.song_select.list_visible_count > 0 && d2d_->text_brush) {
            const D2D1_RECT_F track =
                D2D1::RectF(left_panel.right - 10.0f, card_top,
                            left_panel.right - 6.0f, left_panel.bottom - 14.0f);
            const float track_height = track.bottom - track.top;
            const float visible_ratio = std::clamp(
                static_cast<float>(data.song_select.list_visible_count) /
                    static_cast<float>(data.song_select.list_total_count), 0.04f, 1.0f);
            const float thumb_height = std::max(36.0f, track_height * visible_ratio);
            const int max_start =
                std::max(1, data.song_select.list_total_count - data.song_select.list_visible_count);
            const float start_ratio = std::clamp(
                static_cast<float>(data.song_select.list_window_start) /
                    static_cast<float>(max_start), 0.0f, 1.0f);
            const float thumb_top =
                track.top + (track_height - thumb_height) * start_ratio;
            const D2D1_RECT_F thumb =
                D2D1::RectF(track.left, thumb_top, track.right, thumb_top + thumb_height);
            const D2D1_COLOR_F saved_color = d2d_->text_brush->GetColor();
            const float saved_opacity = d2d_->text_brush->GetOpacity();
            d2d_->text_brush->SetColor(D2D1::ColorF(0xF4F1FF));
            d2d_->text_brush->SetOpacity(0.24f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(track, 2.0f, 2.0f),
                                      d2d_->text_brush.Get());
            d2d_->text_brush->SetOpacity(0.88f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(thumb, 2.0f, 2.0f),
                                      d2d_->text_brush.Get());
            d2d_->text_brush->SetOpacity(saved_opacity);
            d2d_->text_brush->SetColor(saved_color);

            song_scrollbar_state_.visible = true;
            song_scrollbar_state_.left = track.left - 8.0f;
            song_scrollbar_state_.top = track.top;
            song_scrollbar_state_.right = track.right + 8.0f;
            song_scrollbar_state_.bottom = track.bottom;
            song_scrollbar_state_.thumb_top = thumb.top;
            song_scrollbar_state_.thumb_bottom = thumb.bottom;
            song_scrollbar_state_.total_count = data.song_select.list_total_count;
            song_scrollbar_state_.visible_count = data.song_select.list_visible_count;
            song_scrollbar_state_.window_start = data.song_select.list_window_start;
            song_scrollbar_state_.selected_index = data.song_select.list_selected_index;
        } else {
            clear_song_scrollbar_state();
        }

        const float center_section_gap = 18.0f;
        const float center_action_height = 80.0f;
        const float center_paired_height =
            std::max(1.0f, (center_panel.bottom - center_panel.top - center_action_height -
                            center_section_gap * 2.0f) * 0.5f);
        const D2D1_RECT_F showcase =
            D2D1::RectF(center_panel.left, center_panel.top,
                        center_panel.right, center_panel.top + center_paired_height);
        draw_glass_panel(showcase, 12.0f, 0.86f, 0.30f, true, 5.0f);
        const bool has_selected_song =
            !data.song_select.showing_sources && !data.song_select.showing_records &&
            std::any_of(data.song_select.songs.begin(), data.song_select.songs.end(),
                        [](const SongCardData& song) { return song.selected; });
        const bool has_selected_preview_art = has_selected_song &&
            ensure_song_select_preview_bitmap(data.song_select) &&
            d2d_->song_select_preview_bitmap;
        const D2D1_RECT_F preview =
            D2D1::RectF(showcase.left + 8.0f, showcase.top + 8.0f,
                        showcase.right - 8.0f, showcase.bottom - 8.0f);
        if (has_selected_preview_art) {
            const D2D1_RECT_F source =
                centered_bitmap_source_rect(d2d_->song_select_preview_bitmap->GetSize(), preview);
            // Round the jacket to match the glass frame around it; a square bitmap
            // inside a rounded panel leaves the corners sticking out.
            Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> preview_clip;
            if (d2d_->d2d_factory) {
                static_cast<void>(d2d_->d2d_factory->CreateRoundedRectangleGeometry(
                    D2D1::RoundedRect(preview, 8.0f, 8.0f),
                    preview_clip.ReleaseAndGetAddressOf()));
            }
            if (preview_clip) {
                ctx->PushLayer(D2D1::LayerParameters(preview, preview_clip.Get()), nullptr);
            }
            ctx->DrawBitmap(d2d_->song_select_preview_bitmap.Get(), preview, 0.98f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
            if (preview_clip) {
                ctx->PopLayer();
            }
        } else if (has_selected_song && d2d_->card_brush) {
            const D2D1_COLOR_F fallback = jacket_color(data.song_select.selected_song_title);
            const D2D1_COLOR_F saved_color = d2d_->card_brush->GetColor();
            const float saved_opacity = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetColor(fallback);
            d2d_->card_brush->SetOpacity(0.72f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(preview, 8.0f, 8.0f),
                                      d2d_->card_brush.Get());
            d2d_->card_brush->SetColor(saved_color);
            d2d_->card_brush->SetOpacity(saved_opacity);
        }

        if (has_selected_song) {
            const D2D1_RECT_F chip =
                D2D1::RectF(preview.left + 14.0f, preview.top + 14.0f,
                            preview.left + 154.0f, preview.top + 48.0f);
            draw_glass_panel(chip, 7.0f, 0.88f, 0.42f, true, 1.0f);
            if (d2d_->hud_format && d2d_->accent_brush) {
                draw_centered_text(wloc("SELECTED", "선택됨"), d2d_->hud_format.Get(),
                                   chip, d2d_->accent_brush.Get());
            }
        } else if (!data.song_select.showing_sources && !data.song_select.showing_records) {
            draw_centered_text(wloc("SELECT A CHART", "곡을 선택하세요"), d2d_->song_title_format.Get(),
                               preview, d2d_->muted_brush.Get(), true);
        } else {
            const std::string headline =
                data.song_select.showing_sources
                    ? data.song_select.selected_source_name
                    : (data.song_select.rank.empty() ? "--" : data.song_select.rank);
            const std::string subline =
                data.song_select.showing_sources
                    ? data.song_select.selected_source_path
                    : data.song_select.selected_record_status;
            if (d2d_->rank_format && d2d_->accent_brush) {
                draw_centered_text(to_wide(headline), d2d_->rank_format.Get(),
                                   D2D1::RectF(showcase.left + 40.0f, showcase.top + 54.0f,
                                               showcase.right - 40.0f, showcase.top + 216.0f),
                                   d2d_->accent_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_centered_text(to_wide(subline), d2d_->body_format.Get(),
                                   D2D1::RectF(showcase.left + 52.0f, showcase.top + 224.0f,
                                               showcase.right - 52.0f, showcase.bottom - 22.0f),
                                   d2d_->muted_brush.Get());
            }
        }

        const D2D1_RECT_F best_panel =
            D2D1::RectF(center_panel.left, showcase.bottom + center_section_gap,
                        center_panel.right,
                        showcase.bottom + center_section_gap + center_paired_height);
        draw_glass_panel(best_panel, 12.0f, 0.82f,
                         data.song_select.result_available ? 0.44f : 0.12f,
                         data.song_select.result_available, 4.0f);
        if (data.song_select.result_available) {
            register_hit(best_panel, MenuHitTargetKind::SongResultPanel, 0);
        }
        if (d2d_->song_record_label_format && d2d_->muted_brush) {
            draw_text_clipped(data.song_select.showing_records
                                  ? wloc("SELECTED PLAY", "선택한 플레이")
                                  : wloc("MY BEST", "내 최고 기록"),
                              d2d_->song_record_label_format.Get(),
                              D2D1::RectF(best_panel.left + 22.0f, best_panel.top + 16.0f,
                                          best_panel.right - 22.0f, best_panel.top + 46.0f),
                              d2d_->muted_brush.Get());
        }
        if (d2d_->header_format && d2d_->accent_brush) {
            draw_centered_text(to_wide(data.song_select.rank.empty() ? "--" : data.song_select.rank),
                               d2d_->header_format.Get(),
                               D2D1::RectF(best_panel.left + 24.0f, best_panel.top + 48.0f,
                                           best_panel.right - 24.0f, best_panel.top + 122.0f),
                               d2d_->accent_brush.Get());
        }
        draw_rule(best_panel.left + 22.0f, best_panel.top + 136.0f,
                  best_panel.right - 22.0f, 0.28f);
        const float best_columns_left = best_panel.left + 22.0f;
        const float best_columns_right = best_panel.right - 22.0f;
        const float best_column_gap = 18.0f;
        const float best_column_width =
            (best_columns_right - best_columns_left - best_column_gap * 2.0f) / 3.0f;
        const std::array<std::array<std::string, 3>, 3> best_columns = {{
            {loc("SCORE", "점수"),
             data.song_select.result_available
                 ? format_int_with_commas(data.song_select.best_score) + " / " +
                       format_int_with_commas(data.song_select.max_score)
                 : std::string("--"),
             data.song_select.result_available
                 ? "DETAIL " + format_int_with_commas(data.song_select.detail_score) + " / " +
                       (data.song_select.max_detail_score > 0
                            ? format_int_with_commas(data.song_select.max_detail_score)
                            : std::string("--"))
                 : std::string()},
            {loc("ACCURACY", "정확도"),
             data.song_select.result_available
                 ? format_decimal(data.song_select.accuracy, 2) + "%"
                 : std::string("--"),
             data.song_select.result_available
                 ? "DETAIL " + format_decimal(data.song_select.detailed_accuracy, 2) + "%"
                 : std::string()},
            {loc("MAX COMBO", "최대 콤보"),
             data.song_select.result_available
                 ? format_int_with_commas(data.song_select.max_combo)
                 : std::string("--"),
             std::string()},
        }};
        if (d2d_->button_border_brush) {
            const float saved = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(0.20f);
            for (int divider = 1; divider < 3; ++divider) {
                const float x = best_columns_left +
                                static_cast<float>(divider) * best_column_width +
                                (static_cast<float>(divider) - 0.5f) * best_column_gap;
                ctx->DrawLine(D2D1::Point2F(x, best_panel.top + 154.0f),
                              D2D1::Point2F(x, best_panel.bottom - 22.0f),
                              d2d_->button_border_brush.Get(), 1.0f);
            }
            d2d_->button_border_brush->SetOpacity(saved);
        }
        for (std::size_t i = 0; i < best_columns.size(); ++i) {
            const float column_left =
                best_columns_left + static_cast<float>(i) * (best_column_width + best_column_gap);
            const float column_right = column_left + best_column_width;
            if (d2d_->song_record_label_format && d2d_->muted_brush) {
                draw_centered_text(to_wide(best_columns[i][0]),
                                   d2d_->song_record_label_format.Get(),
                                   D2D1::RectF(column_left, best_panel.top + 152.0f,
                                               column_right, best_panel.top + 184.0f),
                                   d2d_->muted_brush.Get());
            }
            if (d2d_->song_record_value_format && d2d_->text_brush) {
                draw_centered_text(to_wide(best_columns[i][1]),
                                   d2d_->song_record_value_format.Get(),
                                   D2D1::RectF(column_left, best_panel.top + 188.0f,
                                               column_right, best_panel.top + 228.0f),
                                   d2d_->text_brush.Get());
            }
            if (!best_columns[i][2].empty() && d2d_->song_record_detail_format &&
                d2d_->muted_brush) {
                draw_centered_text(to_wide(best_columns[i][2]),
                                   d2d_->song_record_detail_format.Get(),
                                   D2D1::RectF(column_left, best_panel.top + 234.0f,
                                               column_right, best_panel.bottom - 22.0f),
                                   d2d_->muted_brush.Get());
            }
        }

        const D2D1_RECT_F action_strip =
            D2D1::RectF(center_panel.left, best_panel.bottom + center_section_gap,
                        center_panel.right, center_panel.bottom);
        const D2D1_RECT_F search_button =
            D2D1::RectF(action_strip.left, action_strip.top,
                        action_strip.left + 246.0f, action_strip.bottom);
        const D2D1_RECT_F filter_button =
            D2D1::RectF(search_button.right + 12.0f, action_strip.top,
                        search_button.right + 286.0f, action_strip.bottom);
        const D2D1_RECT_F library_status =
            D2D1::RectF(filter_button.right + 12.0f, action_strip.top,
                        action_strip.right, action_strip.bottom);
        register_hit(search_button, MenuHitTargetKind::SongNavButton, 2);
        register_hit(filter_button, MenuHitTargetKind::SongNavButton, 3);
        // Accent border marks the two clickable controls; the status readout
        // beside them stays plain so the difference is visible at a glance.
        draw_glass_panel(search_button, 10.0f, 0.76f, 0.40f, true, 2.0f);
        draw_glass_panel(filter_button, 10.0f, 0.76f, 0.40f, true, 2.0f);
        draw_glass_panel(library_status, 10.0f, 0.58f, 0.06f, false, 2.0f);
        const bool searching =
            data.song_select.search_active || !data.song_select.search_query.empty();
        if (d2d_->body_format && d2d_->text_brush) {
            std::wstring search_label = wloc("SEARCH", "검색");
            if (searching) {
                search_label = to_wide(data.song_select.search_query);
                if (search_label.empty()) {
                    search_label = wloc("TYPE TO SEARCH", "검색어 입력");
                } else if (data.song_select.search_active) {
                    // A caret marks the field as still taking keystrokes.
                    search_label += L"|";
                }
            }
            draw_centered_text(search_label, d2d_->body_format.Get(),
                               search_button, d2d_->text_brush.Get(), true);
            draw_centered_text(wloc("SORT / FILTER", "정렬 / 필터"), d2d_->body_format.Get(),
                               filter_button, d2d_->text_brush.Get(), true);
        }
        if (d2d_->hud_format && d2d_->muted_brush) {
            const std::string status =
                modern_library_screen
                    ? std::to_string(data.song_select.list_total_count) + loc(" items", "개")
                    : searching ? (loc("MATCHES ", "검색 결과 ") +
                             std::to_string(data.song_select.list_total_count) +
                             loc("", "곡"))
                          : (data.song_select.sort_summary + "  /  " +
                             data.song_select.group_summary);
            draw_centered_text(to_wide(status), d2d_->hud_format.Get(), library_status,
                               d2d_->muted_brush.Get(), true);
        }

        draw_glass_panel(right_panel, 12.0f, 0.76f, 0.22f, false, 5.0f);
        const float right_left = right_panel.left + 24.0f;
        const float right_right = right_panel.right - 24.0f;
        if (d2d_->song_title_format && d2d_->text_brush) {
            const std::string title =
                data.song_select.showing_sources
                    ? data.song_select.selected_source_name
                    : data.song_select.selected_song_title;
            draw_text_clipped(to_wide(title.empty() ? "-" : title),
                              d2d_->song_title_format.Get(),
                              D2D1::RectF(right_left, right_panel.top + 24.0f,
                                          right_right, right_panel.top + 76.0f),
                              d2d_->text_brush.Get());
        }
        if (d2d_->song_artist_format && d2d_->accent_brush) {
            const std::string subtitle =
                data.song_select.showing_sources
                    ? loc("LOCAL SONG SOURCE", "로컬 곡 소스")
                    : (data.song_select.showing_records
                           ? data.song_select.selected_record_created_utc
                           : data.song_select.selected_song_artist);
            draw_text_clipped(to_wide(subtitle), d2d_->song_artist_format.Get(),
                              D2D1::RectF(right_left, right_panel.top + 78.0f,
                                          right_right, right_panel.top + 112.0f),
                              d2d_->accent_brush.Get());
        }
        draw_rule(right_left, right_panel.top + 132.0f, right_right, 0.36f);

        if (!data.song_select.showing_sources && !data.song_select.showing_records) {
            const D2D1_RECT_F difficulty_card =
                D2D1::RectF(right_left, right_panel.top + 150.0f,
                            right_right, right_panel.top + 332.0f);
            draw_glass_panel(difficulty_card, 12.0f, 0.84f, 0.54f, true, 4.0f);
            const float difficulty_split =
                difficulty_card.left + (difficulty_card.right - difficulty_card.left) * 0.44f;
            if (d2d_->button_border_brush) {
                const float saved = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.34f);
                ctx->DrawLine(D2D1::Point2F(difficulty_split, difficulty_card.top + 18.0f),
                              D2D1::Point2F(difficulty_split, difficulty_card.bottom - 18.0f),
                              d2d_->button_border_brush.Get(), 1.0f);
                d2d_->button_border_brush->SetOpacity(saved);
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                draw_text_clipped(L"KEYS / LEVEL",
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(difficulty_card.left + 18.0f,
                                              difficulty_card.top + 14.0f,
                                              difficulty_split - 12.0f,
                                              difficulty_card.top + 40.0f),
                                  d2d_->muted_brush.Get());
                draw_text_clipped(L"LAYOUT",
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(difficulty_split + 16.0f,
                                              difficulty_card.top + 14.0f,
                                              difficulty_card.right - 14.0f,
                                              difficulty_card.top + 40.0f),
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->header_format && d2d_->accent_brush) {
                const std::string key_label =
                    data.song_select.selected_song_key_count > 0
                        ? std::to_string(data.song_select.selected_song_key_count) + "K"
                        : "--";
                draw_centered_text(to_wide(key_label),
                                   d2d_->header_format.Get(),
                                   D2D1::RectF(difficulty_card.left + 18.0f,
                                               difficulty_card.top + 38.0f,
                                               difficulty_split - 12.0f,
                                               difficulty_card.top + 112.0f),
                                   d2d_->accent_brush.Get());
            }
            if (d2d_->title_format && d2d_->text_brush) {
                draw_centered_text(
                    to_wide(data.song_select.selected_song_difficulty.empty()
                                ? std::string("--")
                                : data.song_select.selected_song_difficulty),
                    d2d_->title_format.Get(),
                    D2D1::RectF(difficulty_card.left + 14.0f,
                                difficulty_card.top + 112.0f,
                                difficulty_split - 10.0f,
                                difficulty_card.bottom - 14.0f),
                    d2d_->text_brush.Get());
                draw_centered_text(to_wide(data.song_select.selected_song_layout),
                                   d2d_->title_format.Get(),
                                   D2D1::RectF(difficulty_split + 12.0f,
                                               difficulty_card.top + 48.0f,
                                               difficulty_card.right - 12.0f,
                                               difficulty_card.top + 112.0f),
                                   d2d_->accent_brush
                                       ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                       : static_cast<ID2D1Brush*>(d2d_->text_brush.Get()));
                draw_centered_text(
                    to_wide(data.song_select.selected_song_chart_name.empty()
                                ? std::string("--")
                                : data.song_select.selected_song_chart_name),
                    d2d_->body_format.Get(),
                    D2D1::RectF(difficulty_split + 12.0f,
                                difficulty_card.top + 118.0f,
                                difficulty_card.right - 12.0f,
                                difficulty_card.bottom - 14.0f),
                                   d2d_->text_brush.Get());
            }

            const float metadata_top = right_panel.top + 352.0f;
            const D2D1_RECT_F metadata_panel =
                D2D1::RectF(right_left, metadata_top, right_right, metadata_top + 78.0f);
            draw_glass_panel(metadata_panel, 9.0f, 0.62f, 0.12f, false, 1.0f);
            const float metadata_width = (right_right - right_left) / 4.0f;
            if (d2d_->button_border_brush) {
                const float saved = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.22f);
                for (int divider = 1; divider < 4; ++divider) {
                    const float x = right_left + metadata_width * static_cast<float>(divider);
                    ctx->DrawLine(D2D1::Point2F(x, metadata_top + 10.0f),
                                  D2D1::Point2F(x, metadata_top + 68.0f),
                                  d2d_->button_border_brush.Get(), 1.0f);
                }
                d2d_->button_border_brush->SetOpacity(saved);
            }
            draw_meta_pair(D2D1::RectF(right_left + 12.0f, metadata_top + 6.0f,
                                       right_left + metadata_width - 10.0f, metadata_top + 72.0f),
                           "BPM",
                           data.song_select.selected_song_bpm > 0.0
                               ? format_decimal(data.song_select.selected_song_bpm, 1) : "--");
            draw_meta_pair(D2D1::RectF(right_left + metadata_width + 12.0f, metadata_top + 6.0f,
                                       right_left + metadata_width * 2.0f - 10.0f,
                                       metadata_top + 72.0f),
                           loc("LEVEL", "레벨"),
                           data.song_select.selected_song_difficulty.empty()
                               ? "--" : data.song_select.selected_song_difficulty, true);
            draw_meta_pair(D2D1::RectF(right_left + metadata_width * 2.0f + 12.0f,
                                       metadata_top + 6.0f,
                                       right_left + metadata_width * 3.0f - 10.0f,
                                       metadata_top + 72.0f),
                           loc("NOTES", "노트 수"),
                           data.song_select.selected_song_note_count > 0
                               ? format_int_with_commas(data.song_select.selected_song_note_count)
                               : "--");
            draw_meta_pair(D2D1::RectF(right_left + metadata_width * 3.0f + 12.0f,
                                       metadata_top + 6.0f,
                                       right_right - 10.0f, metadata_top + 72.0f),
                           "NPS MED",
                           data.song_select.selected_song_nps_median > 0.0
                               ? format_decimal(data.song_select.selected_song_nps_median, 1)
                               : "--");

            const D2D1_RECT_F mode_area =
                D2D1::RectF(right_left, right_panel.top + 460.0f,
                            right_right, right_panel.top + 608.0f);
            const float mode_gap = 10.0f;
            const float mode_width = (mode_area.right - mode_area.left - mode_gap) * 0.5f;
            const float mode_height = (mode_area.bottom - mode_area.top - mode_gap) * 0.5f;
            const std::array<std::pair<std::string, std::string>, 4> mode_values = {{
                {loc("VISUAL LATENCY", "비주얼 레이턴시"), data.song_select.current_visual_latency},
                {loc("HI-SPEED", "하이스피드"), data.song_select.current_hi_speed},
                {loc("SHIFT START", "시프트 시작"), data.song_select.current_gauge},
                {loc("RANDOM", "랜덤"), data.song_select.current_random},
            }};
            for (std::size_t i = 0; i < mode_values.size(); ++i) {
                const float col = static_cast<float>(i % 2);
                const float row = static_cast<float>(i / 2);
                const D2D1_RECT_F cell =
                    D2D1::RectF(mode_area.left + col * (mode_width + mode_gap),
                                mode_area.top + row * (mode_height + mode_gap),
                                mode_area.left + col * (mode_width + mode_gap) + mode_width,
                                mode_area.top + row * (mode_height + mode_gap) + mode_height);
                register_hit(cell, MenuHitTargetKind::SongQuickSetting,
                             static_cast<int>(i), MenuHitPart::Increment);
                const bool keyboard_selected =
                    data.song_select.quick_settings_focused &&
                    data.song_select.quick_setting_cursor == static_cast<int>(i);
                // These cells take clicks, so they carry the accent border the
                // read-only readouts around them do not.
                draw_glass_panel(cell, 9.0f, 0.70f, 0.40f, true, 2.0f);
                if (keyboard_selected && d2d_->accent_brush) {
                    ctx->DrawRoundedRectangle(
                        D2D1::RoundedRect(cell, 9.0f, 9.0f),
                        d2d_->accent_brush.Get(),
                        4.0f);
                }
                if (d2d_->stats_value_format && d2d_->muted_brush) {
                    draw_text_clipped(to_wide(mode_values[i].first),
                                      d2d_->stats_value_format.Get(),
                                      D2D1::RectF(cell.left + 12.0f, cell.top + 5.0f,
                                                  cell.right - 12.0f, cell.top + 29.0f),
                                      d2d_->muted_brush.Get());
                }
                if (d2d_->song_title_format && d2d_->text_brush) {
                    const D2D1_RECT_F value_rect =
                        D2D1::RectF(cell.left + 12.0f, cell.top + 26.0f,
                                    cell.right - 12.0f, cell.bottom - 4.0f);
                    if (i == 2u) {
                        const D2D1_COLOR_F saved_color = d2d_->text_brush->GetColor();
                        const float saved_opacity = d2d_->text_brush->GetOpacity();
                        d2d_->text_brush->SetColor(D2D1::ColorF(0x05070A));
                        d2d_->text_brush->SetOpacity(0.88f);
                        draw_trailing_text(to_wide(mode_values[i].second),
                                           d2d_->song_title_format.Get(),
                                           D2D1::RectF(value_rect.left + 2.0f,
                                                       value_rect.top + 2.0f,
                                                       value_rect.right + 2.0f,
                                                       value_rect.bottom + 2.0f),
                                           d2d_->text_brush.Get());
                        d2d_->text_brush->SetColor(D2D1::ColorF(
                            song_select_gauge_text_color(
                                data.song_select.current_gauge_tier)));
                        d2d_->text_brush->SetOpacity(1.0f);
                        draw_trailing_text(to_wide(mode_values[i].second),
                                           d2d_->song_title_format.Get(), value_rect,
                                           d2d_->text_brush.Get());
                        d2d_->text_brush->SetColor(saved_color);
                        d2d_->text_brush->SetOpacity(saved_opacity);
                    } else {
                        draw_trailing_text(to_wide(mode_values[i].second),
                                           d2d_->song_title_format.Get(), value_rect,
                                           d2d_->text_brush.Get());
                    }
                }
            }

            if (d2d_->hud_format && d2d_->muted_brush) {
                std::string status =
                    loc("LAMP ", "램프 ") +
                    (data.song_select.selected_song_lamp.empty()
                         ? "--" : data.song_select.selected_song_lamp);
                status += data.song_select.selected_song_favorite
                              ? "  /  FAVORITE" : "  /  NOT FAVORITE";
                if (!data.song_select.selected_song_ghost_status.empty()) {
                    status += "  /  GHOST " + data.song_select.selected_song_ghost_status;
                }
                draw_text_clipped(to_wide(status), d2d_->hud_format.Get(),
                                  D2D1::RectF(right_left, right_panel.top + 626.0f,
                                              right_right, right_panel.top + 654.0f),
                                  d2d_->muted_brush.Get());
            }
        } else if (data.song_select.showing_sources) {
            draw_meta_pair(D2D1::RectF(right_left, right_panel.top + 162.0f,
                                       right_left + 190.0f, right_panel.top + 238.0f),
                           loc("CHARTS", "차트"),
                           data.song_select.selected_source_song_count >= 0
                               ? format_int_with_commas(data.song_select.selected_source_song_count)
                               : "--", true);
            draw_meta_pair(D2D1::RectF(right_left + 220.0f, right_panel.top + 162.0f,
                                       right_left + 390.0f, right_panel.top + 238.0f),
                           loc("ROOTS", "소스"),
                           format_int_with_commas(data.song_select.source_count));
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_text_clipped(to_wide(data.song_select.selected_source_path),
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(right_left, right_panel.top + 270.0f,
                                              right_right, right_panel.top + 410.0f),
                                  d2d_->muted_brush.Get());
            }
        } else {
            draw_meta_pair(D2D1::RectF(right_left, right_panel.top + 162.0f,
                                       right_left + 190.0f, right_panel.top + 238.0f),
                           loc("SCORE", "점수"),
                           format_int_with_commas(data.song_select.best_score) + " / " +
                               format_int_with_commas(data.song_select.max_score), true);
            draw_meta_pair(D2D1::RectF(right_left + 220.0f, right_panel.top + 162.0f,
                                       right_left + 380.0f, right_panel.top + 238.0f),
                           loc("ACCURACY", "정확도"),
                           format_decimal(data.song_select.accuracy, 2) + "% / D " +
                               format_decimal(data.song_select.detailed_accuracy, 2) + "%");
            draw_meta_pair(D2D1::RectF(right_left + 410.0f, right_panel.top + 162.0f,
                                       right_right, right_panel.top + 238.0f),
                           loc("COMBO", "콤보"),
                           format_int_with_commas(data.song_select.max_combo));
            if (d2d_->hud_format && d2d_->accent_brush) {
                const std::string detail_line = data.song_select.online_records
                    ? loc("READ-ONLY SERVER LEADERBOARD", "읽기 전용 서버 리더보드")
                    : loc("DETAIL SCORE ", "상세 점수 ") +
                          format_int_with_commas(data.song_select.detail_score) + " / " +
                          (data.song_select.max_detail_score > 0
                               ? format_int_with_commas(data.song_select.max_detail_score)
                               : std::string("--"));
                draw_text_clipped(to_wide(detail_line),
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(right_left, right_panel.top + 244.0f,
                                              right_right, right_panel.top + 270.0f),
                                  d2d_->accent_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::string replay =
                    data.song_select.online_records
                        ? loc("SERVER VERIFIED / CLIENT REPLAY NOT PROVIDED",
                              "서버 검증 / 클라이언트 리플레이 미제공")
                    : data.song_select.selected_record_replay_file.empty()
                        ? loc("RESULT ONLY", "결과만 저장됨")
                        : loc("REPLAY ", "리플레이 ") +
                              data.song_select.selected_record_replay_file;
                draw_text_clipped(to_wide(replay), d2d_->body_format.Get(),
                                  D2D1::RectF(right_left, right_panel.top + 272.0f,
                                              right_right, right_panel.top + 324.0f),
                                  d2d_->muted_brush.Get());
                draw_text_clipped(to_wide(data.song_select.selected_record_replay_detail),
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(right_left, right_panel.top + 326.0f,
                                              right_right, right_panel.top + 380.0f),
                                  d2d_->muted_brush.Get());
            }
        }

        const bool primary_enabled = data.song_select.list_total_count > 0 &&
                                     !data.song_select.online_records;
        const D2D1_RECT_F start_button =
            D2D1::RectF(right_left, right_panel.bottom - 112.0f,
                        right_right, right_panel.bottom - 24.0f);
        if (primary_enabled) {
            register_hit(start_button, MenuHitTargetKind::SongStartButton, 0);
        }
        if (primary_enabled && modern_library_screen && d2d_->accent_brush) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(start_button, 12.0f, 12.0f),
                                      d2d_->accent_brush.Get());
        } else if (primary_enabled && d2d_->play_brush) {
            set_brush_points(d2d_->play_brush.Get(), start_button);
            const float saved = d2d_->play_brush->GetOpacity();
            d2d_->play_brush->SetOpacity(0.92f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(start_button, 12.0f, 12.0f),
                                      d2d_->play_brush.Get());
            d2d_->play_brush->SetOpacity(saved);
        } else {
            draw_glass_panel(start_button, 12.0f, 0.56f, 0.04f, false, 2.0f);
        }
        if (d2d_->header_format && d2d_->text_brush && d2d_->muted_brush) {
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            if (modern_library_screen && primary_enabled) {
                d2d_->text_brush->SetColor(D2D1::ColorF(0x0B1620));
            }
            const std::wstring action_label =
                data.song_select.showing_sources
                    ? wloc("OPEN SOURCE", "소스 열기")
                    : (data.song_select.showing_records
                           ? (data.song_select.online_records
                                  ? wloc("VIEW ONLY", "조회 전용")
                                  : wloc("OPEN RESULT", "결과 열기"))
                           : wloc("START", "시작"));
            draw_centered_text(action_label, d2d_->header_format.Get(), start_button,
                               primary_enabled
                                   ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                   : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()), true);
            d2d_->text_brush->SetColor(saved_text_color);
        }

        const D2D1_RECT_F bottom_bar =
            skin_layout_rect(data, "song_select.bottom_bar",
                             D2D1::RectF(38.0f, 944.0f, 1882.0f, 1048.0f));
        draw_glass_panel(bottom_bar, 12.0f, 0.82f, 0.18f, false, 4.0f);
        const D2D1_RECT_F back_button =
            D2D1::RectF(bottom_bar.left + 12.0f, bottom_bar.top + 12.0f,
                        bottom_bar.left + 260.0f, bottom_bar.bottom - 12.0f);
        register_hit(back_button, MenuHitTargetKind::SongBackButton, 0);
        draw_glass_panel(back_button, 10.0f, 0.76f, 0.40f, true, 2.0f);
        if (d2d_->body_format && d2d_->text_brush) {
            draw_centered_text(wloc("BACK", "뒤로"), d2d_->body_format.Get(),
                               back_button, d2d_->text_brush.Get(), true);
        }
        if (d2d_->hud_format && d2d_->muted_brush) {
            draw_centered_text(to_wide(data.song_select.primary_hint), d2d_->hud_format.Get(),
                               D2D1::RectF(back_button.right + 28.0f, bottom_bar.top + 12.0f,
                                           bottom_bar.right - 28.0f, bottom_bar.top + 48.0f),
                               d2d_->muted_brush.Get());
            draw_centered_text(to_wide(data.song_select.secondary_hint), d2d_->hud_format.Get(),
                               D2D1::RectF(back_button.right + 28.0f, bottom_bar.top + 48.0f,
                                           bottom_bar.right - 28.0f, bottom_bar.bottom - 10.0f),
                               d2d_->muted_brush.Get());
        }
