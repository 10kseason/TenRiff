        // Native review layout: the HTML reference supplies geometry only.
        // Every number, status and command comes from the existing menu model.
        const auto& review = data.song_select;
        const auto review_text = [&](const std::wstring& text, IDWriteTextFormat* format,
                                     const D2D1_RECT_F& rect, uint32_t rgb, auto... requested_alignment) {
            if (!format) return;
            const std::array<DWRITE_TEXT_ALIGNMENT, sizeof...(requested_alignment)> alignments{requested_alignment...};
            const auto alignment = alignments.empty() ? DWRITE_TEXT_ALIGNMENT_LEADING : alignments.front();
            const auto saved = format->GetTextAlignment();
            format->SetTextAlignment(alignment);
            draw_metadata_ellipsis(text, format, rect, semantic_color(rgb));
            format->SetTextAlignment(saved);
        };
        const auto review_wrap = [&](const std::wstring& text, IDWriteTextFormat* format,
                                     const D2D1_RECT_F& rect, uint32_t rgb) {
            if (!format || text.empty()) return;
            const auto wrapping = format->GetWordWrapping();
            const auto alignment = format->GetTextAlignment();
            format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            draw_text_exact(text.c_str(), static_cast<UINT32>(text.size()), format, rect, semantic_color(rgb),
                            D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
            format->SetWordWrapping(wrapping);
            format->SetTextAlignment(alignment);
        };
        const auto review_line = [&](float left, float top, float right) {
            ctx->DrawLine(D2D1::Point2F(left, top), D2D1::Point2F(right, top), semantic_color(0x222227), 1.0f);
        };
        const auto review_button = [&](const D2D1_RECT_F& rect, const std::wstring& label,
                                       MenuHitTargetKind kind, int index, auto... requested_states) {
            const std::array<bool, sizeof...(requested_states)> states{requested_states...};
            const bool enabled = states.empty() || states[0];
            const bool active = states.size() > 1 && states[1];
            const bool primary = states.size() > 2 && states[2];
            if (enabled) register_hit(rect, kind, index);
            if (primary || active) ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 6, 6),
                semantic_color(primary && enabled ? 0xFFFFFF : 0x202025));
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6, 6),
                semantic_color(active ? 0xECECEF : 0x36363D), 1.0f);
            const auto text_rect = D2D1::RectF(rect.left + 8, rect.top + 6, rect.right - 8, rect.bottom - 4);
            review_text(label, primary ? d2d_->header_format.Get() : d2d_->hud_format.Get(), text_rect,
                primary && enabled ? 0x000000 : enabled ? 0xECECEF : 0x62626B, DWRITE_TEXT_ALIGNMENT_CENTER);
        };
        const auto review_bitmap = [&](ID2D1Bitmap* bitmap, const D2D1_RECT_F& rect) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 4, 4), semantic_color(0x0D0D10));
            if (bitmap) {
                const auto source = centered_bitmap_source_rect(bitmap->GetSize(), rect);
                ctx->DrawBitmap(bitmap, rect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
            } else {
                const float margin = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.22f;
                draw_ten_mark(D2D1::RectF(rect.left + margin, rect.top + margin, rect.right - margin, rect.bottom - margin),
                              semantic_color(0x8F8F98), 1.0f);
            }
        };
        const auto review_lamp_brush = [&](const SongSelectScorePaint& paint, const D2D1_RECT_F& rect) -> ID2D1Brush* {
            if (paint.rainbow) {
                if (!d2d_->song_clear_gradient) {
                    std::array<D2D1_GRADIENT_STOP, kClearRainbowRgb.size()> stops;
                    for (std::size_t i = 0; i < stops.size(); ++i) stops[i] = {kClearRainbowStops[i], D2D1::ColorF(kClearRainbowRgb[i])};
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                    if (SUCCEEDED(ctx->CreateGradientStopCollection(stops.data(), static_cast<UINT>(stops.size()), &collection)))
                        ctx->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(1, 0)),
                                                       collection.Get(), &d2d_->song_clear_gradient);
                }
                if (d2d_->song_clear_gradient) {
                    d2d_->song_clear_gradient->SetStartPoint(D2D1::Point2F(rect.left, rect.top));
                    d2d_->song_clear_gradient->SetEndPoint(D2D1::Point2F(rect.left, rect.bottom));
                    return d2d_->song_clear_gradient.Get();
                }
            }
            return semantic_color(paint.rgb == 0xFFFFFF ? 0x3A3A40 : paint.rgb);
        };

        // 72px top bar / 56px footer, corresponding to the reference's 1920 frame.
        review_line(0, 72, 1920);
        review_text(L"TENRIFF", d2d_->song_logo_format.Get(), D2D1::RectF(48, 18, 252, 54), 0xFFFFFF);
        constexpr std::array<int, 5> review_nav{0, 1, 4, 5, 6};
        for (std::size_t i = 0; i < review_nav.size(); ++i) {
            const int route = review_nav[i];
            if (route >= static_cast<int>(review.left_nav.size())) continue;
            const auto& item = review.left_nav[route];
            const float left = 300 + static_cast<float>(i) * 170;
            const auto rect = D2D1::RectF(left, 0, left + 154, 72);
            register_hit(rect, MenuHitTargetKind::SongNavButton, route);
            const bool active = item.selected || (route == 0 && !review.showing_sources && !review.showing_records) ||
                (route == 1 && review.showing_sources) || (route == 4 && review.showing_records);
            review_text(to_wide(item.label), d2d_->song_nav_format.Get(), D2D1::RectF(left, 24, left + 154, 52),
                        active ? 0xFFFFFF : 0x8F8F98);
            if (active) ctx->FillRectangle(D2D1::RectF(left, 70, left + 154, 72), semantic_color(0xFFFFFF));
        }
        const auto profile = D2D1::RectF(1530, 12, 1872, 60);
        register_hit(profile, MenuHitTargetKind::SongProfilePanel, 0);
        const auto avatar_slot = D2D1::RectF(1834, 21, 1864, 51);
        if (auto* avatar = find_song_card_preview_bitmap(review.profile_avatar_path)) {
            review_bitmap(avatar, avatar_slot);
        } else {
            const auto circle = D2D1::Ellipse(D2D1::Point2F(1849, 36), 15, 15);
            ctx->FillEllipse(circle, semantic_color(0x0D0D10));
            ctx->DrawEllipse(circle, semantic_color(0x36363D), 1.0f);
            review_text(L"T", d2d_->hud_format.Get(), D2D1::RectF(1836, 24, 1862, 49),
                        0xBABAC2, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        review_text(to_wide(review.profile.empty() ? "PLAYER" : review.profile), d2d_->hud_format.Get(),
                    D2D1::RectF(1540, 25, 1820, 51), 0xBABAC2, DWRITE_TEXT_ALIGNMENT_TRAILING);

        // Folder context and the existing table/search/filter actions. The
        // intentionally omitted distribution strip has no replacement mock data.
        const std::string folder_title = review.showing_sources ? loc("LIBRARY", "라이브러리") :
            review.showing_records ? (review.online_records ? loc("ONLINE RECORDS", "온라인 기록") : loc("RECORDS", "기록")) :
            !review.difficulty_table_name.empty() && review.difficulty_table_active ? review.difficulty_table_name : review.current_source_name;
        review_text(to_wide(folder_title), d2d_->song_title_format.Get(), D2D1::RectF(48, 102, 612, 142), 0xFFFFFF);
        review_button(D2D1::RectF(630, 104, 800, 144), wloc("SORT / FILTER", "정렬 / 필터"), MenuHitTargetKind::SongNavButton, 3);
        std::wstring search = to_wide(review.search_query);
        if (search.empty()) search = wloc("SEARCH", "검색");
        if (review.search_active) search += L"|";
        review_button(D2D1::RectF(814, 104, 1048, 144), search, MenuHitTargetKind::SongNavButton, 2, true, review.search_active);
        review_button(D2D1::RectF(48, 149, 150, 176), wloc("TABLE", "난이도표"), MenuHitTargetKind::SongDifficultyTable,
                      static_cast<int>(SongDifficultyTableAction::EditUrl));
        review_button(D2D1::RectF(160, 149, 230, 176), wloc("FILE", "파일"), MenuHitTargetKind::SongDifficultyTable,
                      static_cast<int>(SongDifficultyTableAction::LocalFile));
        if (review.difficulty_table_active) review_button(D2D1::RectF(240, 149, 310, 176), wloc("RESET", "해제"),
            MenuHitTargetKind::SongDifficultyTable, static_cast<int>(SongDifficultyTableAction::Reset));
        std::string context = std::to_string(review.list_total_count) + " / " + review.group_summary +
            (review.browser_summary.empty() ? std::string{} : " / " + review.browser_summary);
        if (review.indexing) context = review.indexing_stage + "  " + std::to_string(review.indexing_processed) +
            (review.indexing_total >= 0 ? " / " + std::to_string(review.indexing_total) : std::string{}) + "  " + review.indexing_eta;
        review_text(to_wide(context), d2d_->hud_format.Get(), D2D1::RectF(328, 151, 1048, 175), 0x8F8F98);
        review_line(48, 182, 1048);

        constexpr float list_top = 190, list_bottom = 1006, row_gap = 4;
        constexpr int native_rows = app::kNativeSongSelectVisibleCardCount;
        constexpr float row_height = (list_bottom - list_top - row_gap * (native_rows - 1)) / native_rows;
        const auto row_count = std::min<std::size_t>(review.songs.size(), native_rows);
        for (std::size_t i = 0; i < row_count; ++i) {
            const auto& song = review.songs[i];
            const float top = list_top + static_cast<float>(i) * (row_height + row_gap);
            const auto row = D2D1::RectF(48, top, 1034, top + row_height);
            register_hit(row, MenuHitTargetKind::SongCard, song.song_index);
            if (song.selected) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(row, 5, 5), semantic_color(0x17171B));
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(row, 5, 5), semantic_color(0xFFFFFF), 1.0f);
            }
            const auto lamp = song_select_score_paint(song.lamp);
            const auto lamp_rect = D2D1::RectF(57, top + 10, 63, row.bottom - 10);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(lamp_rect, 3, 3), review_lamp_brush(lamp, lamp_rect));
            const float thumb_top = top + (row_height - 54) * 0.5f;
            review_bitmap(find_song_card_preview_bitmap(song.background_path), D2D1::RectF(79, thumb_top, 151, thumb_top + 54));
            review_text(to_wide(song.title), d2d_->song_title_format.Get(), D2D1::RectF(174, top + 10, 814, top + 44),
                        song.selected ? 0xFFFFFF : 0xECECEF);
            review_text(to_wide(song.artist), d2d_->hud_format.Get(), D2D1::RectF(174, top + 47, 814, top + 71), 0x8F8F98);
            const auto badge = D2D1::RectF(836, top + 24, 1016, top + 58);
            const auto clear = song_select_lamp_presentation(song.lamp);
            if (song.chart_count >= 0) {
                // A folder's inventory remains a count, never a play result.
                review_text(std::to_wstring(song.chart_count), d2d_->song_record_detail_format.Get(),
                            badge, 0xBABAC2, DWRITE_TEXT_ALIGNMENT_TRAILING);
            } else if (!clear.badge_label.empty()) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(badge, 14, 14), semantic_color(0x030503));
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(badge, 14, 14), clear_badge_brush(clear, badge), 1.8f);
                const float icon_size = 40.0f;
                const float icon_top = (badge.top + badge.bottom - icon_size) * 0.5f;
                const auto icon = D2D1::RectF(badge.left + 3, icon_top, badge.left + 3 + icon_size, icon_top + icon_size);
                const auto logo_color = D2D1::ColorF(clear.badge_rgb);
                draw_native_asset(native_menu_assets::kMark, icon, 1.0f, 0.0f, &logo_color,
                                  clear_badge_brush(clear, D2D1::RectF(8, 23, 94, 77)));
                draw_centered_text(to_wide(clear.badge_label), clear_badge_format,
                    D2D1::RectF(icon.right + 1, badge.top, badge.right - 7, badge.bottom),
                    clear_badge_brush(clear, badge), true);
            } else if (!clear.label.empty()) {
                review_text(to_wide(clear.label), d2d_->song_record_detail_format.Get(), badge,
                            clear.color.value_or(0x8F8F98), DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            // Unplayed songs have an empty result slot. Score stays in MY BEST.
        }
        if (review.songs.empty()) {
            review_text(to_wide(review.empty_title), d2d_->song_title_format.Get(), D2D1::RectF(80, 400, 1016, 448), 0xBABAC2);
            review_wrap(to_wide(review.empty_message), d2d_->body_format.Get(), D2D1::RectF(80, 464, 1016, 654), 0x8F8F98);
        }
        if (review.list_total_count > review.list_visible_count && review.list_visible_count > 0) {
            const auto track = D2D1::RectF(1042, list_top, 1048, list_bottom);
            const float ratio = std::clamp(static_cast<float>(review.list_visible_count) / review.list_total_count, 0.04f, 1.0f);
            const float thumb_height = std::max(36.0f, (list_bottom - list_top) * ratio);
            const float position = std::clamp(static_cast<float>(review.list_window_start) /
                std::max(1, review.list_total_count - review.list_visible_count), 0.0f, 1.0f);
            const float thumb_top = list_top + (list_bottom - list_top - thumb_height) * position;
            ctx->FillRoundedRectangle(D2D1::RoundedRect(track, 3, 3), semantic_color(0x17171B));
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(track.left, thumb_top, track.right, thumb_top + thumb_height), 3, 3),
                                      semantic_color(0x62626B));
            song_scrollbar_state_.visible = true;
            song_scrollbar_state_.left = track.left - 6; song_scrollbar_state_.right = track.right + 6;
            song_scrollbar_state_.top = track.top; song_scrollbar_state_.bottom = track.bottom;
            song_scrollbar_state_.thumb_top = thumb_top; song_scrollbar_state_.thumb_bottom = thumb_top + thumb_height;
            song_scrollbar_state_.total_count = review.list_total_count; song_scrollbar_state_.visible_count = review.list_visible_count;
            song_scrollbar_state_.window_start = review.list_window_start; song_scrollbar_state_.selected_index = review.list_selected_index;
        } else clear_song_scrollbar_state();

        // Selected cover and metadata, using actual cached chart artwork only.
        const SongCardData* selected_card = nullptr;
        for (const auto& card : review.songs) if (card.selected) { selected_card = &card; break; }
        const bool folder_view = review.showing_sources || review.showing_level_folders;
        const std::string title = review.showing_sources ? review.selected_source_name : !review.selected_song_title.empty()
            ? review.selected_song_title : selected_card ? selected_card->title : review.empty_title;
        review_bitmap(folder_view ? nullptr : find_song_card_preview_bitmap(review.selected_song_background_path), D2D1::RectF(1104, 98, 1456, 362));
        review_wrap(to_wide(title), d2d_->song_title_format.Get(), D2D1::RectF(1480, 98, 1872, 228), 0xFFFFFF);
        review_text(to_wide(folder_view ? (selected_card ? selected_card->artist : std::string{}) : review.selected_song_artist),
                    d2d_->body_format.Get(), D2D1::RectF(1480, 237, 1872, 268), 0xBABAC2);
        review_text(to_wide(review.selected_song_chart_author), d2d_->hud_format.Get(), D2D1::RectF(1480, 276, 1872, 302), 0x8F8F98);
        review_text(to_wide(review.selected_song_chart_name), d2d_->hud_format.Get(), D2D1::RectF(1480, 306, 1872, 332), 0x8F8F98);
        review_text(to_wide(folder_view ? (selected_card ? selected_card->detail : std::string{}) : review.selected_song_layout),
                    d2d_->hud_format.Get(), D2D1::RectF(1480, 337, 1872, 361), 0xBABAC2);

        review_line(1104, 382, 1872);
        const bool has_record = review.result_available || (review.online_records && review.record_count > 0);
        if (review.result_available) register_hit(D2D1::RectF(1104, 390, 1872, 604), MenuHitTargetKind::SongResultPanel, 0);
        review_text(folder_view ? wloc("CHARTS", "차트") : review.showing_records ? wloc("SELECTED PLAY", "선택한 플레이") : wloc("MY BEST", "내 최고 기록"),
                    d2d_->song_record_label_format.Get(), D2D1::RectF(1118, 392, 1560, 419), 0x8F8F98);
        if (folder_view) {
            const int count = selected_card ? selected_card->chart_count : 0;
            review_text(std::to_wstring(std::max(0, count)), d2d_->song_record_value_format.Get(),
                        D2D1::RectF(1118, 430, 1564, 500), 0xECECEF);
        } else if (has_record) {
            const auto paint = song_select_score_paint(review.score_clear_status);
            draw_grade(3, to_wide(std::to_string(review.best_score)), d2d_->song_record_value_format.Get(), D2D1::RectF(1118, 425, 1564, 500),
                       1.0f, DWRITE_TEXT_ALIGNMENT_LEADING, 1.0f, 0.0f, paint.rgb, paint.rainbow);
            draw_grade(1, to_wide(review.rank.empty() ? "--" : review.rank), d2d_->grade_format.Get(), D2D1::RectF(1594, 398, 1872, 504),
                       1.0f, DWRITE_TEXT_ALIGNMENT_TRAILING, 0.85f);
        } else review_text(L"--", d2d_->song_record_value_format.Get(), D2D1::RectF(1118, 430, 1564, 500), 0x62626B);
        const std::array<std::wstring, 3> stat_labels{wloc("ACCURACY", "정확도"), wloc("DETAIL SCORE", "상세 점수"),
            has_record && review.verified_full_combo ? L"FULL COMBO" : wloc("MAX COMBO", "최대 콤보")};
        const std::array<std::wstring, 3> stat_values{
            has_record ? to_wide(format_decimal(review.accuracy, 2) + "%") : L"--",
            has_record && (!review.online_records || review.online_detail_score_available) ? std::to_wstring(review.detail_score) : L"--",
            has_record ? std::to_wstring(review.max_combo) : L"--"};
        for (std::size_t i = 0; !folder_view && i < stat_labels.size(); ++i) {
            const float left = 1118 + static_cast<float>(i) * 252;
            const auto label_rect = D2D1::RectF(left, 520, left + 230, 548);
            if (i == 2 && has_record && review.verified_full_combo) draw_grade(4, stat_labels[i], d2d_->song_record_label_format.Get(),
                label_rect, 1.0f, DWRITE_TEXT_ALIGNMENT_LEADING, 1.0f, 0.25f, kFullComboTextRgb);
            else review_text(stat_labels[i], d2d_->song_record_label_format.Get(), label_rect, 0x8F8F98);
            review_text(stat_values[i], d2d_->song_metadata_value_format.Get(), D2D1::RectF(left, 552, left + 230, 592), 0xECECEF);
        }
        review_line(1104, 610, 1872);
        if (folder_view) {
            review_wrap(to_wide(review.showing_sources ? review.selected_source_path : review.browser_summary), d2d_->body_format.Get(),
                        D2D1::RectF(1118, 632, 1858, 740), 0xBABAC2);
            const std::string count = review.showing_sources && review.selected_source_song_count >= 0
                ? std::to_string(review.selected_source_song_count) + " " + loc("CHARTS", "차트") : review.group_summary;
            review_text(to_wide(count), d2d_->hud_format.Get(), D2D1::RectF(1118, 752, 1858, 782), 0x8F8F98);
        } else {
            const std::array<std::wstring, 5> labels{
                L"BPM", wloc("KEYS", "키 수"), L"Lv", wloc("NOTES", "노트 수"), wloc("JUDGEMENT", "판정 난이도")};
            const std::array<std::wstring, 5> values{
                to_wide((review.selected_song_bpm > 0 ? format_decimal(review.selected_song_bpm, 1) : "--") +
                    (review.selected_song_has_bpm_changes ? " *" : "")),
                review.selected_song_key_count > 0 ? std::to_wstring(review.selected_song_key_count) + L"K" : L"--",
                song_level_text(review.selected_song_difficulty.empty() ? "--" : review.selected_song_difficulty, selected_aery_badge),
                review.selected_song_note_count >= 0 ? std::to_wstring(review.selected_song_note_count) : L"--",
                to_wide(review.selected_song_bms_rank.empty() ? "--" : review.selected_song_bms_rank)};
            const auto metadata = native_rect("songselect_review.chart_metadata", D2D1::RectF(1118, 626, 1858, 690));
            const float metadata_width = (metadata.right - metadata.left) / 5;
            for (std::size_t i = 0; i < labels.size(); ++i) {
                const float left = metadata.left + static_cast<float>(i) * metadata_width;
                review_text(labels[i], d2d_->hud_format.Get(), D2D1::RectF(left, metadata.top, left + metadata_width - 8, metadata.top + 26), 0x8F8F98);
                review_text(values[i], d2d_->song_metadata_value_format.Get(),
                    D2D1::RectF(left, metadata.top + 29, left + metadata_width - 8, metadata.bottom),
                    i == 2 ? kDifficultyLevelColor : 0xECECEF);
            }
            const auto nps_panel = native_rect("songselect_review.nps_panel", D2D1::RectF(1118, 704, 1858, 792));
            ctx->FillRoundedRectangle(D2D1::RoundedRect(nps_panel, 5, 5), semantic_color(0x0D0D10));
            const auto nps = [&](double value) { return review.selected_song_nps_available ? format_decimal(value, 1) : std::string("--"); };
            const std::array<std::string, 3> nps_labels{
                "NPS MIN " + nps(review.selected_song_nps_min), "AVG " + nps(review.selected_song_nps_average),
                "MAX " + nps(review.selected_song_nps_max)};
            const float third = (nps_panel.right - nps_panel.left - 24) / 3;
            for (std::size_t i = 0; i < nps_labels.size(); ++i) {
                const float left = nps_panel.left + 12 + static_cast<float>(i) * third;
                review_text(to_wide(nps_labels[i]), d2d_->hud_format.Get(),
                    D2D1::RectF(left, nps_panel.top + 7, left + third - 6, nps_panel.top + 31), 0xBABAC2);
            }
            if (review.selected_song_nps_available && !review.selected_song_nps_bins.empty()) {
                const auto graph = D2D1::RectF(nps_panel.left + 12, nps_panel.top + 38, nps_panel.right - 12, nps_panel.bottom - 8);
                const float step = (graph.right - graph.left) / static_cast<float>(review.selected_song_nps_bins.size());
                const float peak = static_cast<float>(std::max(1.0, review.selected_song_nps_max));
                for (std::size_t i = 0; i < review.selected_song_nps_bins.size(); ++i) {
                    const float height = (graph.bottom - graph.top) * std::clamp(review.selected_song_nps_bins[i] / peak, 0.0f, 1.0f);
                    if (height <= 0) continue;
                    const float left = graph.left + step * static_cast<float>(i);
                    ctx->FillRectangle(D2D1::RectF(left, graph.bottom - height, left + std::max(1.0f, step - 1.0f), graph.bottom),
                                       semantic_color(0x8F8F98));
                }
            }

        }

        // The four existing quick settings retain their IDs and adjustment
        // semantics. They are read-only in folder/record views as before.
        const auto session = D2D1::RectF(1104, 804, 1872, 910);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(session, 6, 6), semantic_color(0x0D0D10));
        review_text(wloc("SESSION", "세션"), d2d_->hud_format.Get(), D2D1::RectF(1122, 814, 1330, 839), 0x8F8F98);
        if (review.left_nav.size() > 6) review_text(to_wide(review.left_nav[6].detail), d2d_->hud_format.Get(),
            D2D1::RectF(1340, 814, 1740, 839), 0x8F8F98, DWRITE_TEXT_ALIGNMENT_TRAILING);
        review_button(D2D1::RectF(1754, 812, 1858, 842), wloc("OPTIONS", "옵션"), MenuHitTargetKind::SongNavButton, 6);
        const std::array<std::wstring, 4> quick_labels{wloc("VISUAL LATENCY", "비주얼 레이턴시"), wloc("HI-SPEED", "하이스피드"),
            wloc("SHIFT START", "시프트 시작"), wloc("RANDOM", "랜덤")};
        const std::array<std::string, 4> quick_values{review.current_visual_latency, review.current_hi_speed, review.current_gauge, review.current_random};
        for (std::size_t i = 0; i < quick_labels.size(); ++i) {
            const float left = 1118 + static_cast<float>(i) * 187;
            const auto cell = D2D1::RectF(left, 848, left + 173, 902);
            const bool interactive = !folder_view && !review.showing_records;
            if (interactive) register_hit(cell, MenuHitTargetKind::SongQuickSetting, static_cast<int>(i), MenuHitPart::Increment);
            if (interactive && review.quick_settings_focused && review.quick_setting_cursor == static_cast<int>(i))
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(cell, 4, 4), semantic_color(0xECECEF), 1.0f);
            review_text(quick_labels[i], d2d_->hud_format.Get(), D2D1::RectF(left + 4, 849, left + 169, 870), 0x8F8F98);
            const uint32_t rgb = i == 2 ? song_select_gauge_text_color(review.current_gauge_tier) : 0xBABAC2;
            const auto value_rect = D2D1::RectF(left + 4, 875, left + 169, 901);
            if (i == 2) draw_grade(8, to_wide(quick_values[i]), d2d_->song_record_detail_format.Get(), value_rect,
                1.0f, DWRITE_TEXT_ALIGNMENT_LEADING, 1.0f, 0.0f, rgb, review.current_gauge_tier == "ex_hard");
            else review_text(to_wide(quick_values[i]), d2d_->song_record_detail_format.Get(), value_rect, rgb);
        }
        const std::string state = review.selected_song_collection_filter +
            (review.selected_song_favorite ? "  /  " + loc("FAVORITE", "즐겨찾기") : std::string{}) +
            (review.selected_song_ghost_status.empty() ? std::string{} : "  /  GHOST " + review.selected_song_ghost_status);
        review_text(to_wide(state), d2d_->hud_format.Get(), D2D1::RectF(1118, 915, 1858, 931), 0x8F8F98);
        const bool start_enabled = review.showing_sources ? !review.songs.empty() : review.showing_records
            ? review.result_available && !review.online_records : !review.songs.empty();
        const auto action = review.showing_sources ? wloc("OPEN LIBRARY", "라이브러리 열기") : review.showing_records
            ? review.online_records ? wloc("VIEW ONLY", "조회 전용") : wloc("OPEN RESULT", "결과 열기")
            : review.showing_level_folders ? wloc("OPEN LEVEL", "레벨 열기") : wloc("START", "시작");
        review_button(D2D1::RectF(1104, 936, 1872, 1006), action, MenuHitTargetKind::SongStartButton, 0, start_enabled, false, true);

        review_line(0, 1024, 1920);
        review_button(D2D1::RectF(48, 1034, 154, 1072), wloc("BACK", "뒤로"), MenuHitTargetKind::SongBackButton, 0);
        const uint32_t hint_rgb = review.audio_setup_required ? 0xFFD18A : 0x8F8F98;
        review_text(to_wide(review.primary_hint), d2d_->hud_format.Get(), D2D1::RectF(178, 1031, 1872, 1054), hint_rgb);
        review_text(to_wide(review.secondary_hint), d2d_->hud_format.Get(), D2D1::RectF(178, 1057, 1872, 1078), hint_rgb);
        if (review.difficulty_table_editing) {
#include "MenuWindow_draw_difficulty_table_editor.inl"
        }
