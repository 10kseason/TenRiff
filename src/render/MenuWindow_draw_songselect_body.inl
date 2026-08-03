        const float ambient_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 7.2, 0.12));
        const float header_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.8, 0.03));
        const float nav_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 3.9, 0.28));
        const float card_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 4.5, 0.51));
        const float preview_pulse = static_cast<float>(pulse_wave_01(render_now_ns, 5.6, 0.74));

        auto draw_section_divider = [&](float y, float left, float right, float alpha) {
            if (!d2d_->button_border_brush) {
                return;
            }
            const float saved_opacity = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(alpha);
            ctx->DrawLine(D2D1::Point2F(left, y), D2D1::Point2F(right, y), d2d_->button_border_brush.Get(), 1.0f);
            d2d_->button_border_brush->SetOpacity(saved_opacity);
        };

        auto draw_stat_section = [&](float y, std::string_view label, float left, float right) {
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring section_w = to_wide(std::string(label));
                draw_text_clipped(section_w,
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(left, y, right, y + 22.0f),
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.22f);
                ctx->DrawLine(D2D1::Point2F(left, y + 24.0f), D2D1::Point2F(right, y + 24.0f),
                              d2d_->accent_brush.Get(), 1.1f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            return y + 30.0f;
        };

        auto draw_chip = [&](const D2D1_RECT_F& rect, std::string_view text, bool selected) {
            draw_glass_panel(rect,
                             11.0f,
                             selected ? 0.84f : 0.70f,
                             selected ? (0.40f + card_pulse * 0.18f) : 0.12f,
                             selected,
                             3.0f);
            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring chip_w = to_wide(std::string(text));
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(chip_w, d2d_->body_format.Get(), rect, d2d_->text_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        };

        if (d2d_->accent_brush) {
            const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
            d2d_->accent_brush->SetOpacity(0.020f + header_pulse * 0.012f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kBaseWidth * 0.5f, 114.0f), 220.0f, 38.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetOpacity(0.016f + ambient_pulse * 0.010f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(250.0f, 950.0f), 220.0f, 70.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetOpacity(0.012f + preview_pulse * 0.008f);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(1638.0f, 914.0f), 200.0f, 62.0f),
                             d2d_->accent_brush.Get());
            d2d_->accent_brush->SetColor(saved_color);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_horizon(154.0f, 86.0f, kBaseWidth - 86.0f, 840.0f, 0.12f, 0.44f + header_pulse * 0.18f);
        draw_song_select_horizon(kBaseHeight - 228.0f, 18.0f, kBaseWidth - 18.0f, 720.0f, 0.10f,
                                 0.24f + ambient_pulse * 0.10f);
        draw_song_select_stardust(D2D1::RectF(66.0f, 882.0f, 904.0f, 1042.0f), 38, 0x51u, 0.12f);
        draw_song_select_stardust(D2D1::RectF(960.0f, 874.0f, 1770.0f, 1038.0f), 24, 0x251u, 0.06f);

        const D2D1_RECT_F header_rect = D2D1::RectF(0.0f, 70.0f, kBaseWidth, 170.0f);
        const std::wstring header_w = L"TENRIFF SELECT";
        ID2D1Brush* header_brush = d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                                    : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
        if (d2d_->logo_brush) {
            set_brush_points(d2d_->logo_brush.Get(), header_rect);
        }
        if (d2d_->header_format && header_brush) {
            d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(header_w, d2d_->header_format.Get(), header_rect, header_brush);
            d2d_->header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (data.song_select.indexing && d2d_->hud_format && d2d_->muted_brush) {
            std::string status = data.song_select.indexing_stage.empty() ? std::string("INDEXING")
                                                                         : data.song_select.indexing_stage;
            if (data.song_select.indexing_percent >= 0) {
                status += " " + std::to_string(data.song_select.indexing_percent) + "%";
            }
            if (data.song_select.indexing_total > 0) {
                status += " (" + format_int_with_commas(data.song_select.indexing_processed) + "/" +
                          format_int_with_commas(data.song_select.indexing_total) + ")";
            } else if (data.song_select.indexing_processed > 0) {
                status += " " + format_int_with_commas(data.song_select.indexing_processed);
            }
            if (!data.song_select.indexing_eta.empty()) {
                status += " ETA " + data.song_select.indexing_eta;
            }
            status += " (" + std::to_string(data.song_select.song_count) + " songs)";
            const std::wstring status_w = to_wide(status);
            const D2D1_RECT_F status_rect = D2D1::RectF(560.0f, 190.0f, 1360.0f, 222.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(status_w, d2d_->hud_format.Get(), status_rect, d2d_->muted_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

            const D2D1_RECT_F progress_track = D2D1::RectF(560.0f, 228.0f, 1360.0f, 246.0f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.72f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_track, 8.0f, 8.0f), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(progress_track, 8.0f, 8.0f),
                                          d2d_->button_border_brush.Get(), 1.0f);
            }
            if (d2d_->accent_brush && data.song_select.indexing_percent >= 0) {
                const float fill_ratio =
                    std::clamp(static_cast<float>(data.song_select.indexing_percent) / 100.0f, 0.0f, 1.0f);
                const D2D1_RECT_F progress_fill =
                    D2D1::RectF(progress_track.left + 3.0f,
                                progress_track.top + 3.0f,
                                progress_track.left + 3.0f +
                                    (progress_track.right - progress_track.left - 6.0f) * fill_ratio,
                                progress_track.bottom - 3.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(progress_fill, 6.0f, 6.0f), d2d_->accent_brush.Get());
            }
        }

        const ScreenContentBands bands =
            make_screen_content_bands(70.0f, 100.0f, true, 50.0f, 70.0f);
        const float content_top = bands.body_top;
        const float content_bottom = bands.body_bottom;
        const bool has_selected_preview_art =
            !data.song_select.showing_sources && !data.song_select.showing_records &&
            ensure_song_select_preview_bitmap(data.song_select) && d2d_->song_select_preview_bitmap;
        const float nav_left = 120.0f;
        const float nav_width = 290.0f;
        float nav_top = content_top + 12.0f;
        float nav_height = 74.0f;
        float nav_gap = 14.0f;
        const std::size_t nav_count = data.song_select.left_nav.size();
        if (nav_count > 0) {
            const bool dense_nav = nav_count >= 10;
            const float min_nav_height = dense_nav ? 50.0f : 64.0f;
            const float min_nav_gap = dense_nav ? 4.0f : 8.0f;
            const float max_nav_gap = dense_nav ? 10.0f : 14.0f;
            if (dense_nav) {
                nav_height = 64.0f;
                nav_gap = 8.0f;
            }
            const float nav_available_height = std::max(0.0f, content_bottom - nav_top);
            if (nav_count > 1) {
                const float natural_gap =
                    (nav_available_height - nav_height * static_cast<float>(nav_count)) /
                    static_cast<float>(nav_count - 1);
                nav_gap = std::clamp(std::floor(natural_gap), min_nav_gap, max_nav_gap);
            } else {
                nav_gap = 0.0f;
            }

            const float nav_used_height =
                nav_height * static_cast<float>(nav_count) +
                nav_gap * static_cast<float>(nav_count > 0 ? nav_count - 1 : 0);
            if (nav_used_height > nav_available_height) {
                const float adjusted_height =
                    (nav_available_height -
                     nav_gap * static_cast<float>(nav_count > 0 ? nav_count - 1 : 0)) /
                    static_cast<float>(nav_count);
                nav_height = std::max(min_nav_height, std::floor(adjusted_height));
            }

            const float final_nav_used_height =
                nav_height * static_cast<float>(nav_count) +
                nav_gap * static_cast<float>(nav_count > 0 ? nav_count - 1 : 0);
            const float overflow = final_nav_used_height - nav_available_height;
            if (overflow > 0.0f) {
                nav_top = std::max(content_top, nav_top - overflow);
            }
        }
        const bool compact_nav = nav_height < 66.0f;
        for (std::size_t i = 0; i < data.song_select.left_nav.size(); ++i) {
            const auto& item = data.song_select.left_nav[i];
            const float y0 = nav_top + static_cast<float>(i) * (nav_height + nav_gap);
            const D2D1_RECT_F rect = D2D1::RectF(nav_left, y0, nav_left + nav_width, y0 + nav_height);
            register_hit(rect, MenuHitTargetKind::SongNavButton, static_cast<int>(i));
            if (item.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.10f + nav_pulse * 0.06f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(offset_rect(rect, 0.0f, 1.0f), 16.0f, 16.0f),
                                          d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            draw_glass_panel(rect,
                             compact_nav ? 12.0f : 14.0f,
                             item.selected ? 0.84f : 0.62f,
                             item.selected ? (0.54f + nav_pulse * 0.16f) : 0.08f,
                             item.selected,
                             3.5f);
            if (item.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.84f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(rect.left + 8.0f,
                                                  rect.top + (compact_nav ? 10.0f : 12.0f),
                                                  rect.left + 12.0f,
                                                  rect.bottom - (compact_nav ? 10.0f : 12.0f)),
                                      2.0f, 2.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F icon_rect =
                D2D1::RectF(rect.left + 16.0f, rect.top, rect.left + (compact_nav ? 66.0f : 78.0f), rect.bottom);
            const bool has_detail = !item.detail.empty();
            const float text_left = rect.left + (compact_nav ? 72.0f : 82.0f);
            const D2D1_RECT_F label_rect = has_detail
                                               ? D2D1::RectF(text_left,
                                                             rect.top + (compact_nav ? 4.0f : 6.0f),
                                                             rect.right - 18.0f,
                                                             rect.top + (compact_nav ? 28.0f : 40.0f))
                                               : D2D1::RectF(text_left, rect.top, rect.right - 18.0f, rect.bottom);
            const D2D1_RECT_F detail_rect =
                D2D1::RectF(text_left,
                            rect.top + (compact_nav ? 24.0f : 34.0f),
                            rect.right - 18.0f,
                            rect.bottom - (compact_nav ? 4.0f : 6.0f));

            if (d2d_->menu_icon_format && d2d_->text_brush) {
                const std::wstring icon_w = to_wide(item.icon);
                if (!icon_w.empty()) {
                    draw_text_clipped(icon_w, d2d_->menu_icon_format.Get(), icon_rect, d2d_->text_brush.Get());
                }
            }
            if (d2d_->text_brush) {
                const std::wstring label_w = to_wide(item.label);
                IDWriteTextFormat* label_format =
                    compact_nav && d2d_->option_format ? d2d_->option_format.Get() : d2d_->title_format.Get();
                if (label_format) {
                    draw_text_clipped(label_w, label_format, label_rect, d2d_->text_brush.Get());
                }
            }
            if (has_detail) {
                ID2D1SolidColorBrush* detail_brush = item.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
                IDWriteTextFormat* detail_format =
                    compact_nav && d2d_->hud_format ? d2d_->hud_format.Get() : d2d_->body_format.Get();
                if (detail_brush && detail_format) {
                    const std::wstring detail_w = to_wide(item.detail);
                    draw_text_clipped(detail_w, detail_format, detail_rect, detail_brush);
                }
            }
        }

        const D2D1_RECT_F list_rect = D2D1::RectF(450.0f, content_top, 1270.0f, content_bottom);
        draw_glass_panel(list_rect, 18.0f, 0.84f, 0.54f + ambient_pulse * 0.14f, true, 8.0f);
        if (has_selected_preview_art) {
            // Reuse the already-cached selected preview as a quiet list backdrop. One low-alpha
            // bitmap draw keeps the cards readable without adding another decode or cache path.
            const D2D1_RECT_F backdrop_rect =
                D2D1::RectF(list_rect.left + 8.0f, list_rect.top + 8.0f,
                            list_rect.right - 8.0f, list_rect.bottom - 8.0f);
            const D2D1_RECT_F backdrop_source =
                centered_bitmap_source_rect(d2d_->song_select_preview_bitmap->GetSize(), backdrop_rect);
            ctx->PushAxisAlignedClip(backdrop_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            ctx->DrawBitmap(d2d_->song_select_preview_bitmap.Get(),
                            backdrop_rect,
                            0.075f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &backdrop_source);
            ctx->PopAxisAlignedClip();
        }
        if (d2d_->title_format && d2d_->text_brush) {
            const std::wstring list_header_w = data.song_select.showing_sources
                                                   ? wloc("Sources", "소스")
                                                   : (data.song_select.showing_records
                                                          ? wloc("Records", "기록")
                                                          : wloc("Songs", "곡"));
            const D2D1_RECT_F list_header_rect =
                D2D1::RectF(list_rect.left + 28.0f, list_rect.top + 18.0f, list_rect.right - 220.0f, list_rect.top + 58.0f);
            draw_text_clipped(list_header_w, d2d_->title_format.Get(), list_header_rect, d2d_->text_brush.Get());
        }
        if (d2d_->body_format && d2d_->muted_brush) {
            const std::wstring list_detail_w = data.song_select.showing_sources
                                                   ? to_wide(loc("RECENT ROOTS", "최근 소스"))
                                                   : (data.song_select.showing_records
                                                          ? to_wide(std::to_string(data.song_select.record_count) + " " +
                                                                    loc("PLAYS", "플레이"))
                                                          : to_wide_with_placeholder(data.song_select.current_source_name,
                                                                                    "<invalid source>",
                                                                                    "song-select-header"));
            const D2D1_RECT_F list_detail_rect =
                D2D1::RectF(list_rect.left + 30.0f, list_rect.top + 52.0f, list_rect.right - 220.0f, list_rect.top + 76.0f);
            draw_text_clipped(list_detail_w, d2d_->body_format.Get(), list_detail_rect, d2d_->muted_brush.Get());
        }
        if (d2d_->hud_format && d2d_->text_brush) {
            const std::string count_text =
                data.song_select.showing_sources
                    ? (std::to_string(data.song_select.source_count) + " " + loc("ROOTS", "소스"))
                    : (data.song_select.showing_records
                           ? (std::to_string(data.song_select.record_count) + " " + loc("ENTRIES", "항목"))
                           : (std::to_string(data.song_select.song_count) + " " + loc("CHARTS", "차트")));
            const D2D1_RECT_F count_rect =
                D2D1::RectF(list_rect.right - 220.0f, list_rect.top + 24.0f, list_rect.right - 24.0f, list_rect.top + 54.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            draw_text_clipped(to_wide(count_text), d2d_->hud_format.Get(), count_rect, d2d_->text_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        draw_section_divider(list_rect.top + 72.0f, list_rect.left + 24.0f, list_rect.right - 24.0f, 0.30f);

        const float card_left = list_rect.left + 28.0f;
        const float card_right = list_rect.right - 28.0f;
        const float card_top = list_rect.top + 70.0f;
        const float card_h = 110.0f;
        const float card_gap = 18.0f;

        for (std::size_t i = 0; i < data.song_select.songs.size(); ++i) {
            const auto& song = data.song_select.songs[i];
            const float y0 = card_top + static_cast<float>(i) * (card_h + card_gap);
            const D2D1_RECT_F card = D2D1::RectF(card_left, y0, card_right, y0 + card_h);
            register_hit(card, MenuHitTargetKind::SongCard, song.song_index);
            if (song.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.08f + card_pulse * 0.05f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(offset_rect(card, 0.0f, 1.0f), 16.0f, 16.0f),
                                          d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            draw_glass_panel(card,
                             14.0f,
                             song.selected ? 0.86f : 0.66f,
                             song.selected ? (0.56f + card_pulse * 0.16f) : 0.10f,
                             song.selected,
                             4.0f);
            if (song.selected && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.88f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(card.left + 8.0f, card.top + 14.0f,
                                                  card.left + 13.0f, card.bottom - 14.0f),
                                      2.5f, 2.5f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(0.14f + card_pulse * 0.08f);
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(card.left + 22.0f, card.top + 7.0f,
                                                  card.right - 22.0f, card.top + 9.0f),
                                      1.0f, 1.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F jacket =
                D2D1::RectF(card.left + 18.0f, card.top + 12.0f, card.left + 158.0f, card.bottom - 12.0f);
            const D2D1_ROUNDED_RECT jacket_rr = D2D1::RoundedRect(jacket, 10.0f, 10.0f);
            if (ID2D1Bitmap* jacket_bitmap = find_song_card_preview_bitmap(song.background_path)) {
                const D2D1_RECT_F source_rect =
                    centered_bitmap_source_rect(jacket_bitmap->GetSize(), jacket);
                ctx->PushAxisAlignedClip(jacket, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                ctx->DrawBitmap(jacket_bitmap,
                                jacket,
                                song.selected ? 0.98f : 0.92f,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                &source_rect);
                ctx->PopAxisAlignedClip();
                if (d2d_->accent_brush) {
                    const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                    const float saved_opacity = d2d_->accent_brush->GetOpacity();
                    d2d_->accent_brush->SetColor(D2D1::ColorF(0x081018));
                    d2d_->accent_brush->SetOpacity(song.selected ? 0.08f : 0.16f);
                    ctx->FillRoundedRectangle(jacket_rr, d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(D2D1::ColorF(blend_rgb(0xD9E8F5, 0xFFFFFF, 0.35f), 0.20f));
                    d2d_->accent_brush->SetOpacity(0.20f);
                    ctx->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(jacket.left + 8.0f, jacket.top + 8.0f, jacket.right - 8.0f, jacket.top + 26.0f), 8.0f, 8.0f),
                        d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(saved_color);
                    d2d_->accent_brush->SetOpacity(saved_opacity);
                }
            } else {
                if (d2d_->accent_brush) {
                    const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                    const float saved_opacity = d2d_->accent_brush->GetOpacity();
                    const D2D1_COLOR_F color = jacket_color(song.title);
                    d2d_->accent_brush->SetColor(color);
                    d2d_->accent_brush->SetOpacity(0.58f);
                    ctx->FillRoundedRectangle(jacket_rr, d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(D2D1::ColorF(blend_rgb(0xD9E8F5, 0xFFFFFF, 0.35f), 0.20f));
                    d2d_->accent_brush->SetOpacity(0.22f);
                    ctx->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(jacket.left + 8.0f, jacket.top + 8.0f, jacket.right - 8.0f, jacket.top + 26.0f), 8.0f, 8.0f),
                        d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetColor(saved_color);
                    d2d_->accent_brush->SetOpacity(saved_opacity);
                }
                if (d2d_->text_brush) {
                    const float saved_opacity = d2d_->text_brush->GetOpacity();
                    d2d_->text_brush->SetOpacity(0.10f);
                    ctx->DrawLine(D2D1::Point2F(jacket.left + 12.0f, jacket.top + 20.0f),
                                  D2D1::Point2F(jacket.right - 12.0f, jacket.top + 20.0f),
                                  d2d_->text_brush.Get(),
                                  1.0f);
                    ctx->DrawLine(D2D1::Point2F(jacket.left + 12.0f, jacket.bottom - 18.0f),
                                  D2D1::Point2F(jacket.right - 18.0f, jacket.top + 24.0f),
                                  d2d_->text_brush.Get(),
                                  1.2f);
                    d2d_->text_brush->SetOpacity(saved_opacity);
                }
            }
            if (d2d_->button_border_brush) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(song.selected ? 0.75f : 0.48f);
                ctx->DrawRoundedRectangle(jacket_rr, d2d_->button_border_brush.Get(), 1.2f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }

            const bool show_group_label = !song.group_label.empty() && !data.song_select.showing_sources &&
                                          !data.song_select.showing_records;
            if (show_group_label && d2d_->body_format && d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(song.selected ? 0.92f : 0.70f);
                const D2D1_RECT_F group_rect =
                    D2D1::RectF(jacket.right + 18.0f, card.top + 14.0f, card.right - 180.0f, card.top + 34.0f);
                draw_text_clipped(to_wide_with_placeholder(song.group_label,
                                                           "<group>",
                                                           "song-card-group:" + std::to_string(song.song_index)),
                                  d2d_->body_format.Get(),
                                  group_rect,
                                  d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            const float title_top = show_group_label ? (card.top + 30.0f) : (card.top + 18.0f);
            const D2D1_RECT_F title_rect =
                D2D1::RectF(jacket.right + 18.0f, title_top, card.right - 180.0f, title_top + 40.0f);
            const D2D1_RECT_F artist_rect = data.song_select.showing_sources
                                                ? D2D1::RectF(jacket.right + 18.0f, card.top + 54.0f,
                                                              card.right - 180.0f, card.top + 88.0f)
                                                : D2D1::RectF(jacket.right + 18.0f,
                                                              show_group_label ? (card.top + 70.0f) : (card.top + 62.0f),
                                                              card.right - 180.0f,
                                                              card.bottom - 18.0f);
            const D2D1_RECT_F detail_rect =
                D2D1::RectF(jacket.right + 18.0f, card.top + 82.0f, card.right - 180.0f, card.bottom - 12.0f);

            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring title_w =
                    to_wide_with_placeholder(song.title, "<invalid title>",
                                             "song-card-title:" + std::to_string(song.song_index));
                draw_text_clipped(title_w, d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->song_artist_format && d2d_->muted_brush) {
                const std::wstring artist_w =
                    to_wide_with_placeholder(song.artist, "<invalid artist>",
                                             "song-card-artist:" + std::to_string(song.song_index));
                draw_text_clipped(artist_w, d2d_->song_artist_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }

            if (data.song_select.showing_sources || data.song_select.showing_records) {
                if (!song.detail.empty() && d2d_->body_format) {
                    ID2D1SolidColorBrush* detail_brush = song.selected ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
                    if (detail_brush) {
                        const std::wstring detail_w =
                            to_wide_with_placeholder(song.detail, "<invalid detail>",
                                                     "song-card-detail:" + std::to_string(song.song_index));
                        draw_text_clipped(detail_w, d2d_->body_format.Get(), detail_rect, detail_brush);
                    }
                }
                if (data.song_select.showing_sources && d2d_->body_format && d2d_->text_brush) {
                    const std::string count_label =
                        (song.level > 0) ? (std::to_string(song.level) + " SONGS") : std::string("OPEN");
                    draw_chip(D2D1::RectF(card.right - 170.0f, card.top + 18.0f, card.right - 18.0f, card.top + 52.0f),
                              count_label,
                              song.selected);
                }
            } else if ((!song.level_label.empty() || song.level > 0 || song.rating > 0.0) &&
                       d2d_->body_format && d2d_->text_brush) {
                std::ostringstream level_stream;
                if (!song.level_label.empty()) {
                    level_stream << song.level_label;
                } else if (song.level > 0) {
                    level_stream << "LV " << song.level;
                }
                if (song.rating > 0.0) {
                    if (!level_stream.str().empty()) {
                        level_stream << "  ";
                    }
                    level_stream << std::fixed << std::setprecision(2) << "CR " << song.rating;
                }
                const std::string level_text = level_stream.str();
                draw_chip(D2D1::RectF(card.right - 170.0f, card.top + 18.0f, card.right - 18.0f, card.top + 52.0f),
                          level_text,
                          song.selected);
            }
            if ((!song.lamp.empty() || song.favorite) && d2d_->body_format && d2d_->text_brush) {
                std::string state_text;
                if (song.favorite) {
                    state_text += "\xE2\x98\x85 ";
                }
                if (!song.lamp.empty()) {
                    state_text += song.lamp;
                } else {
                    state_text += loc("FAVORITE", "페이보릿");
                }
                draw_chip(D2D1::RectF(card.right - 210.0f, card.top + 58.0f, card.right - 18.0f, card.top + 90.0f),
                          state_text,
                          song.selected);
            }
        }

        if (data.song_select.songs.empty() && d2d_->title_format && d2d_->body_format) {
            const D2D1_RECT_F empty_rect =
                D2D1::RectF(list_rect.left + 34.0f, list_rect.top + 146.0f, list_rect.right - 34.0f, list_rect.bottom - 80.0f);
            if (d2d_->text_brush) {
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(
                    to_wide(data.song_select.empty_title.empty() ? std::string("NO ITEMS") : data.song_select.empty_title),
                    d2d_->title_format.Get(),
                    D2D1::RectF(empty_rect.left, empty_rect.top, empty_rect.right, empty_rect.top + 48.0f),
                    d2d_->text_brush.Get());
                d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->muted_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(
                    to_wide(data.song_select.empty_message.empty() ? std::string("Try another source or filter.")
                                                                   : data.song_select.empty_message),
                    d2d_->body_format.Get(),
                    D2D1::RectF(empty_rect.left + 40.0f, empty_rect.top + 62.0f, empty_rect.right - 40.0f,
                                empty_rect.top + 132.0f),
                    d2d_->muted_brush.Get());
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        if (data.song_select.list_total_count > data.song_select.list_visible_count &&
            data.song_select.list_visible_count > 0) {
            const D2D1_RECT_F track_rect =
                D2D1::RectF(list_rect.right - 14.0f, card_top + 4.0f, list_rect.right - 9.0f, list_rect.bottom - 26.0f);
            const float track_height = track_rect.bottom - track_rect.top;
            const float thumb_height = std::max(
                58.0f,
                track_height * (static_cast<float>(data.song_select.list_visible_count) /
                                static_cast<float>(data.song_select.list_total_count)));
            const int scrollable_count =
                std::max(0, data.song_select.list_total_count - data.song_select.list_visible_count);
            const float thumb_travel = std::max(0.0f, track_height - thumb_height);
            const float thumb_top =
                track_rect.top +
                ((scrollable_count > 0)
                     ? (thumb_travel * (static_cast<float>(data.song_select.list_window_start) /
                                        static_cast<float>(scrollable_count)))
                     : 0.0f);
            const D2D1_RECT_F thumb_rect =
                D2D1::RectF(track_rect.left, thumb_top, track_rect.right, thumb_top + thumb_height);

            if (d2d_->button_border_brush) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.36f);
                ctx->DrawLine(D2D1::Point2F((track_rect.left + track_rect.right) * 0.5f, track_rect.top),
                              D2D1::Point2F((track_rect.left + track_rect.right) * 0.5f, track_rect.bottom),
                              d2d_->button_border_brush.Get(),
                              1.2f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(song_scroll_drag_active_ ? 0.98f : 0.82f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(thumb_rect, 6.0f, 6.0f), d2d_->accent_brush.Get());
                d2d_->accent_brush->SetOpacity(0.22f);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(thumb_rect, 6.0f, 6.0f), d2d_->accent_brush.Get(), 4.0f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            song_scrollbar_state_.visible = true;
            song_scrollbar_state_.left = track_rect.left - 8.0f;
            song_scrollbar_state_.top = track_rect.top;
            song_scrollbar_state_.right = track_rect.right + 8.0f;
            song_scrollbar_state_.bottom = track_rect.bottom;
            song_scrollbar_state_.thumb_top = thumb_rect.top;
            song_scrollbar_state_.thumb_bottom = thumb_rect.bottom;
            song_scrollbar_state_.total_count = data.song_select.list_total_count;
            song_scrollbar_state_.visible_count = data.song_select.list_visible_count;
            song_scrollbar_state_.window_start = data.song_select.list_window_start;
            song_scrollbar_state_.selected_index = data.song_select.list_selected_index;
        } else {
            clear_song_scrollbar_state();
        }

        if (data.song_select.songs.empty() && d2d_->title_format && d2d_->muted_brush) {
            const std::wstring empty_w = data.song_select.showing_sources
                                             ? wloc("No song folders loaded yet. Use F2 or drag and drop a folder.",
                                                    "아직 불러온 곡 폴더가 없습니다. F2를 누르거나 폴더를 드래그 앤 드롭하세요.")
                                             : (data.song_select.showing_records
                                                    ? wloc("No local records saved for this chart yet.",
                                                           "이 차트에 저장된 로컬 기록이 아직 없습니다.")
                                                    : wloc("No charts matched the current search/filter.",
                                                           "현재 검색/필터와 일치하는 차트가 없습니다."));
            const D2D1_RECT_F empty_rect =
                D2D1::RectF(list_rect.left + 40.0f, list_rect.top + 180.0f, list_rect.right - 40.0f, list_rect.top + 260.0f);
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(empty_w, d2d_->title_format.Get(), empty_rect, d2d_->muted_brush.Get());
            d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        const D2D1_RECT_F right_rect = D2D1::RectF(1320.0f, content_top, 1800.0f, content_bottom);
        draw_glass_panel(right_rect, 18.0f, 0.84f, 0.58f + preview_pulse * 0.16f, true, 8.0f);

        const float stats_left = right_rect.left + 24.0f;
        const float stats_right = right_rect.right - 24.0f;
        float stats_y = right_rect.top + 160.0f;
        float row_h = 30.0f;
        const auto compute_row_height = [&](float top, int row_count, float bottom) {
            if (row_count <= 0) {
                return 30.0f;
            }
            const float available = std::max(0.0f, bottom - top);
            if (available <= 0.0f) {
                return 24.0f;
            }
            return std::clamp(std::floor(available / static_cast<float>(row_count)), 24.0f, 30.0f);
        };

        auto draw_stat_row = [&](std::string_view label, int64_t value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(format_int_with_commas(value));
            const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 120.0f, stats_y + row_h);
            const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 120.0f, stats_y, stats_right, stats_y + row_h);
            draw_text_clipped(label_w,
                              d2d_->stats_label_format.Get(),
                              label_rect,
                              d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
            draw_text_clipped(value_w, d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            stats_y += row_h;
        };

        auto draw_stat_text_row = [&](std::string_view label, std::string_view value) {
            if (!d2d_->stats_label_format || !d2d_->stats_value_format || !d2d_->text_brush) {
                return;
            }
            const std::wstring label_w = to_wide(std::string(label));
            const std::wstring value_w = to_wide(std::string(value));
            const bool compact_value = value_w.size() > 18u && d2d_->hud_format;
            const float value_width = compact_value ? 280.0f : 180.0f;
            const D2D1_RECT_F label_rect =
                D2D1::RectF(stats_left, stats_y, stats_right - value_width, stats_y + row_h);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(stats_right - value_width, stats_y, stats_right, stats_y + row_h);
            draw_text_clipped(label_w,
                              d2d_->stats_label_format.Get(),
                              label_rect,
                              d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
            draw_text_clipped(value_w,
                              compact_value ? d2d_->hud_format.Get() : d2d_->stats_value_format.Get(),
                              value_rect,
                              d2d_->text_brush.Get());
            stats_y += row_h;
        };

        const D2D1_RECT_F right_clip_rect =
            D2D1::RectF(right_rect.left + 8.0f, right_rect.top + 8.0f, right_rect.right - 8.0f, right_rect.bottom - 8.0f);
        ctx->PushAxisAlignedClip(right_clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        const D2D1_RECT_F showcase_rect =
            D2D1::RectF(right_rect.left + 20.0f, right_rect.top + 20.0f, right_rect.right - 20.0f, right_rect.top + 236.0f);
        draw_glass_panel(showcase_rect, 18.0f, 0.80f, 0.66f + preview_pulse * 0.12f, true, 6.0f);

        if (data.song_select.showing_sources) {
            row_h = compute_row_height(stats_y, 4, right_rect.bottom - 28.0f);
            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring source_title_w =
                    data.song_select.selected_source_name.empty()
                        ? to_wide(loc("No Source Selected", "선택된 소스 없음"))
                        : to_wide_with_placeholder(data.song_select.selected_source_name,
                                                   "<invalid source>",
                                                   "selected-source-name");
                const D2D1_RECT_F source_title_rect =
                    D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.top + 44.0f, showcase_rect.right - 18.0f, showcase_rect.top + 94.0f);
                draw_text_clipped(source_title_w, d2d_->song_title_format.Get(), source_title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                draw_text_clipped(wloc("SOURCE STATUS", "소스 상태"),
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.top + 12.0f, showcase_rect.right - 18.0f, showcase_rect.top + 34.0f),
                                  d2d_->muted_brush.Get());
                const std::wstring source_path_w =
                    to_wide_with_placeholder(data.song_select.selected_source_path.empty()
                                                 ? data.song_select.current_source_path
                                                 : data.song_select.selected_source_path,
                                             "<invalid path>",
                                             "selected-source-path");
                const D2D1_RECT_F source_path_rect =
                    D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.top + 98.0f, showcase_rect.right - 18.0f, showcase_rect.top + 154.0f);
                draw_text_clipped(source_path_w, d2d_->body_format.Get(), source_path_rect, d2d_->muted_brush.Get());
            }
            draw_chip(D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.bottom - 48.0f,
                                  showcase_rect.left + 146.0f, showcase_rect.bottom - 16.0f),
                      data.song_select.selected_source_active ? loc("ACTIVE", "활성") : loc("READY", "대기"),
                      data.song_select.selected_source_active);

            stats_y = draw_stat_section(showcase_rect.bottom + 20.0f, loc("SOURCE DATA", "소스 정보"), stats_left, stats_right);
            draw_stat_row(loc("ROOTS", "소스"), data.song_select.source_count);
            draw_stat_row(loc("CURRENT", "현재"), data.song_select.song_count);
            draw_stat_row(loc("SELECTED", "선택"),
                          data.song_select.selected_source_song_count >= 0 ? data.song_select.selected_source_song_count : 0);
            if (d2d_->stats_label_format && d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring label_w = wloc("STATUS", "상태");
                const std::wstring value_w = to_wide(data.song_select.selected_source_active ? loc("ACTIVE", "활성")
                                                                                              : loc("READY", "대기"));
                const D2D1_RECT_F label_rect = D2D1::RectF(stats_left, stats_y, stats_right - 120.0f, stats_y + row_h);
                const D2D1_RECT_F value_rect = D2D1::RectF(stats_right - 160.0f, stats_y, stats_right, stats_y + row_h);
                draw_text_clipped(label_w, d2d_->stats_label_format.Get(), label_rect,
                                  d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
                draw_text_clipped(value_w, d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            }
        } else if (data.song_select.showing_records) {
            const std::wstring rank_w = to_wide(data.song_select.rank.empty() ? std::string("--") : data.song_select.rank);
            if (d2d_->rank_format && header_brush) {
                const D2D1_RECT_F rank_rect = D2D1::RectF(showcase_rect.left, showcase_rect.top + 4.0f,
                                                         showcase_rect.right, showcase_rect.top + 132.0f);
                draw_text_clipped(rank_w, d2d_->rank_format.Get(), rank_rect, header_brush);
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring status_w = to_wide(data.song_select.selected_record_status);
                const std::wstring time_w = to_wide(data.song_select.selected_record_created_utc);
                const D2D1_RECT_F status_rect =
                    D2D1::RectF(showcase_rect.left + 20.0f, showcase_rect.top + 116.0f, showcase_rect.right - 20.0f, showcase_rect.top + 144.0f);
                const D2D1_RECT_F time_rect =
                    D2D1::RectF(showcase_rect.left + 20.0f, showcase_rect.top + 142.0f, showcase_rect.right - 20.0f, showcase_rect.top + 170.0f);
                draw_text_clipped(status_w, d2d_->body_format.Get(), status_rect, d2d_->muted_brush.Get());
                draw_text_clipped(time_w, d2d_->body_format.Get(), time_rect, d2d_->muted_brush.Get());
            }
            if (data.song_select.result_available) {
                const D2D1_RECT_F open_result_rect =
                    D2D1::RectF(showcase_rect.left + 18.0f, showcase_rect.bottom - 42.0f,
                                showcase_rect.right - 18.0f, showcase_rect.bottom - 10.0f);
                register_hit(open_result_rect, MenuHitTargetKind::SongResultPanel, 0);
                draw_glass_panel(open_result_rect, 10.0f, 0.86f, 0.62f, true, 3.0f);
                if (d2d_->body_format && d2d_->text_brush) {
                    d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    draw_text_clipped(wloc("OPEN RESULT", "결과 열기"),
                                      d2d_->body_format.Get(), open_result_rect, d2d_->text_brush.Get());
                    d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            }

            stats_y = draw_stat_section(showcase_rect.bottom + 20.0f, loc("SESSION", "세션"), stats_left, stats_right);
            row_h = compute_row_height(stats_y, 8, right_rect.bottom - 160.0f);
            draw_stat_row(loc("SCORE", "점수"), data.song_select.best_score);
            draw_stat_text_row(loc("ACCURACY", "정확도"), format_decimal(data.song_select.accuracy, 2) + "%");
            draw_stat_row(loc("MAX COMBO", "최대 콤보"), data.song_select.max_combo);
            draw_stat_row(loc("PERFECT", "퍼펙트"), data.song_select.perfect);
            draw_stat_row(loc("GREAT", "그레이트"), data.song_select.great);
            draw_stat_row(loc("GOOD", "굿"), data.song_select.good);
            draw_stat_row("BAD", data.song_select.bad);
            draw_stat_row("POOR", data.song_select.poor);

            stats_y += 8.0f;
            stats_y = draw_stat_section(stats_y, loc("REPLAY", "리플레이"), stats_left, stats_right);
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring replay_file_w =
                    to_wide(data.song_select.selected_record_replay_file.empty()
                                ? loc("No replay file", "리플레이 파일 없음")
                                : data.song_select.selected_record_replay_file);
                const std::wstring replay_detail_w = to_wide(data.song_select.selected_record_replay_detail);
                const std::wstring replay_meta_w =
                    to_wide(loc("LANES ", "레인 ") + std::to_string(data.song_select.selected_record_replay_lane_count) +
                            " / " + loc("EVENTS ", "이벤트 ") + std::to_string(data.song_select.selected_record_replay_event_count));
                const D2D1_RECT_F replay_file_rect =
                    D2D1::RectF(stats_left, stats_y, stats_right, stats_y + 24.0f);
                const D2D1_RECT_F replay_detail_rect =
                    D2D1::RectF(stats_left, stats_y + 26.0f, stats_right, stats_y + 50.0f);
                const D2D1_RECT_F replay_meta_rect =
                    D2D1::RectF(stats_left, stats_y + 52.0f, stats_right, stats_y + 76.0f);
                draw_text_clipped(replay_file_w, d2d_->body_format.Get(), replay_file_rect, d2d_->muted_brush.Get());
                draw_text_clipped(replay_detail_w, d2d_->body_format.Get(), replay_detail_rect, d2d_->muted_brush.Get());
                draw_text_clipped(replay_meta_w, d2d_->body_format.Get(), replay_meta_rect, d2d_->muted_brush.Get());
            }
        } else {
            const D2D1_RECT_F preview_rect =
                D2D1::RectF(showcase_rect.left + 8.0f, showcase_rect.top + 8.0f, showcase_rect.right - 8.0f, showcase_rect.bottom - 8.0f);
            const D2D1_ROUNDED_RECT preview_rr = D2D1::RoundedRect(preview_rect, 14.0f, 14.0f);
            if (has_selected_preview_art) {
                const D2D1_RECT_F source_rect =
                    centered_bitmap_source_rect(d2d_->song_select_preview_bitmap->GetSize(), preview_rect);
                ctx->DrawBitmap(d2d_->song_select_preview_bitmap.Get(),
                                preview_rect,
                                0.96f,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                &source_rect);
            } else if (d2d_->accent_brush) {
                const D2D1_COLOR_F color = jacket_color(data.song_select.selected_song_title);
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                d2d_->accent_brush->SetColor(color);
                d2d_->accent_brush->SetOpacity(0.80f);
                ctx->FillRoundedRectangle(preview_rr, d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
                d2d_->accent_brush->SetOpacity(0.12f + preview_pulse * 0.06f);
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F((preview_rect.left + preview_rect.right) * 0.5f,
                                                             preview_rect.bottom - 18.0f),
                                               180.0f, 54.0f),
                                 d2d_->accent_brush.Get());
                draw_song_select_stardust(preview_rect, 14, 0x901u, 0.10f);
                d2d_->accent_brush->SetOpacity(1.0f);
                d2d_->accent_brush->SetColor(saved_color);
                if (d2d_->title_format && d2d_->text_brush) {
                    const std::wstring empty_preview_w = wloc("NO BG PREVIEW", "배경 미리보기 없음");
                    d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    draw_text_clipped(empty_preview_w, d2d_->title_format.Get(), preview_rect, d2d_->text_brush.Get());
                    d2d_->title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            }
            const D2D1_RECT_F preview_hud_band =
                D2D1::RectF(preview_rect.left + 8.0f,
                            preview_rect.top + 8.0f,
                            preview_rect.right - 8.0f,
                            preview_rect.top + 46.0f);
            if (d2d_->card_brush) {
                const float saved_opacity = d2d_->card_brush->GetOpacity();
                d2d_->card_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(preview_hud_band, 9.0f, 9.0f),
                                          d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x6EE7F2));
                d2d_->accent_brush->SetOpacity(0.82f);
                const D2D1_RECT_F preview_hud_rule =
                    D2D1::RectF(preview_hud_band.left + 10.0f,
                                preview_hud_band.bottom - 4.0f,
                                preview_hud_band.right - 10.0f,
                                preview_hud_band.bottom - 2.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(preview_hud_rule, 1.0f, 1.0f),
                                          d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->button_border_brush) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.72f);
                ctx->DrawRoundedRectangle(preview_rr, d2d_->button_border_brush.Get(), 1.2f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }

            if (d2d_->body_format && d2d_->text_brush) {
                const std::wstring preview_label_w = wloc("CHART PREVIEW", "차트 미리보기");
                const D2D1_RECT_F preview_label_rect =
                    D2D1::RectF(preview_hud_band.left + 14.0f,
                                preview_hud_band.top + 2.0f,
                                preview_hud_band.right - 14.0f,
                                preview_hud_band.bottom - 6.0f);
                const D2D1_COLOR_F saved_color = d2d_->text_brush->GetColor();
                const float saved_opacity = d2d_->text_brush->GetOpacity();
                d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD));
                d2d_->text_brush->SetOpacity(0.94f);
                draw_text_clipped(preview_label_w, d2d_->body_format.Get(), preview_label_rect, d2d_->text_brush.Get());
                d2d_->text_brush->SetColor(saved_color);
                d2d_->text_brush->SetOpacity(saved_opacity);
            }

            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::wstring title_w =
                    to_wide_with_placeholder(data.song_select.selected_song_title, "<invalid title>", "selected-song-title");
                const D2D1_RECT_F title_rect =
                    D2D1::RectF(right_rect.left + 24.0f, preview_rect.bottom + 20.0f, right_rect.right - 24.0f, preview_rect.bottom + 70.0f);
                draw_text_clipped(title_w, d2d_->song_title_format.Get(), title_rect, d2d_->text_brush.Get());
            }
            if (d2d_->song_artist_format && d2d_->muted_brush) {
                const std::wstring artist_w =
                    to_wide_with_placeholder(data.song_select.selected_song_artist, "<invalid artist>", "selected-song-artist");
                const D2D1_RECT_F artist_rect =
                    D2D1::RectF(right_rect.left + 24.0f, preview_rect.bottom + 66.0f, right_rect.right - 24.0f, preview_rect.bottom + 100.0f);
                draw_text_clipped(artist_w, d2d_->song_artist_format.Get(), artist_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::wstring detail_w = to_wide(data.song_select.selected_song_detail);
                const D2D1_RECT_F detail_rect =
                    D2D1::RectF(right_rect.left + 24.0f, preview_rect.bottom + 96.0f, right_rect.right - 24.0f, preview_rect.bottom + 126.0f);
                draw_text_clipped(detail_w, d2d_->body_format.Get(), detail_rect, d2d_->muted_brush.Get());
            }

            stats_y = draw_stat_section(preview_rect.bottom + 140.0f, loc("OVERVIEW", "개요"), stats_left, stats_right);
            row_h = 24.0f;
            draw_stat_text_row(loc("RANK", "랭크"), data.song_select.rank.empty() ? std::string("--") : data.song_select.rank);
            draw_stat_text_row(loc("LAMP", "클리어"), data.song_select.selected_song_lamp.empty() ? std::string("--") : data.song_select.selected_song_lamp);
            draw_stat_text_row(loc("FAVORITE", "페이보릿"), data.song_select.selected_song_favorite ? loc("YES", "예") : loc("NO", "아니오"));
            draw_stat_text_row(loc("GHOST", "고스트"),
                               data.song_select.selected_song_ghost_status.empty()
                                   ? loc("NONE", "없음")
                                   : data.song_select.selected_song_ghost_status);
            draw_stat_text_row(loc("COLLECTION", "컬렉션"), data.song_select.selected_song_collection_filter.empty() ? loc("ALL", "전체") : data.song_select.selected_song_collection_filter);

            stats_y += 12.0f;
            const D2D1_RECT_F best_result_rect =
                D2D1::RectF(stats_left - 8.0f, stats_y, stats_right + 8.0f,
                            std::min(stats_y + 112.0f, right_rect.bottom - 16.0f));
            draw_glass_panel(best_result_rect,
                             14.0f,
                             data.song_select.result_available ? 0.88f : 0.66f,
                             data.song_select.result_available ? 0.58f : 0.12f,
                             data.song_select.result_available,
                             4.0f);
            if (data.song_select.result_available) {
                register_hit(best_result_rect, MenuHitTargetKind::SongResultPanel, 0);
            }

            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped(wloc("BEST SCORE", "최고 점수"),
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(best_result_rect.left + 14.0f, best_result_rect.top + 8.0f,
                                              best_result_rect.left + 170.0f, best_result_rect.top + 36.0f),
                                  d2d_->muted_brush ? d2d_->muted_brush.Get() : d2d_->text_brush.Get());
            }
            if (d2d_->song_title_format && d2d_->text_brush) {
                d2d_->song_title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                draw_text_clipped(to_wide(data.song_select.result_available
                                              ? format_int_with_commas(data.song_select.best_score)
                                              : std::string("--")),
                                  d2d_->song_title_format.Get(),
                                  D2D1::RectF(best_result_rect.left + 170.0f, best_result_rect.top + 6.0f,
                                              best_result_rect.right - 14.0f, best_result_rect.top + 40.0f),
                                  d2d_->text_brush.Get());
                d2d_->song_title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            if (d2d_->hud_format && d2d_->muted_brush) {
                const std::string summary =
                    data.song_select.result_available
                        ? ((data.song_select.rank.empty() ? std::string("--") : data.song_select.rank) +
                           " / " + loc("MAX COMBO ", "최대 콤보 ") +
                           format_int_with_commas(data.song_select.max_combo))
                        : loc("Play once to create a local result.", "플레이하면 로컬 결과가 생성됩니다.");
                draw_text_clipped(to_wide(summary),
                                  d2d_->hud_format.Get(),
                                  D2D1::RectF(best_result_rect.left + 14.0f, best_result_rect.top + 42.0f,
                                              best_result_rect.right - 14.0f, best_result_rect.top + 70.0f),
                                  d2d_->muted_brush.Get());
            }
            if (d2d_->body_format && d2d_->text_brush) {
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_clipped(data.song_select.result_available
                                      ? wloc("OPEN RESULT", "결과 열기")
                                      : wloc("NO SAVED RESULT", "저장된 결과 없음"),
                                  d2d_->body_format.Get(),
                                  D2D1::RectF(best_result_rect.left + 12.0f, best_result_rect.top + 74.0f,
                                              best_result_rect.right - 12.0f, best_result_rect.bottom - 6.0f),
                                  data.song_select.result_available
                                      ? static_cast<ID2D1Brush*>(d2d_->text_brush.Get())
                                      : static_cast<ID2D1Brush*>(d2d_->muted_brush.Get()));
                d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        ctx->PopAxisAlignedClip();

        if (d2d_->hud_format && d2d_->muted_brush) {
            const D2D1_RECT_F primary_rect =
                D2D1::RectF(list_rect.left, right_rect.bottom + 6.0f, right_rect.right, right_rect.bottom + 34.0f);
            const D2D1_RECT_F secondary_rect =
                D2D1::RectF(list_rect.left, right_rect.bottom + 30.0f, right_rect.right, right_rect.bottom + 58.0f);
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            draw_text_clipped(to_wide(data.song_select.primary_hint),
                              d2d_->hud_format.Get(),
                              primary_rect,
                              d2d_->muted_brush.Get());
            draw_text_clipped(to_wide(data.song_select.secondary_hint),
                              d2d_->hud_format.Get(),
                              secondary_rect,
                              d2d_->muted_brush.Get());
            d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        draw_footer(data.song_select.profile, data.song_select.high_score, data.song_select.track, true);

