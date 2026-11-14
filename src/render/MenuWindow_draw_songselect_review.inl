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
        // Shared constellation surface for this screen only. Fixed points avoid
        // allocations/flicker and are clipped to each control's own rectangle.
        const auto review_box = [&](const D2D1_RECT_F& rect, bool emphasized = false, bool enabled = true) {
            const auto rounded = D2D1::RoundedRect(rect, 7, 7);
            ctx->FillRoundedRectangle(rounded, native_color(emphasized ? 0x151C32 : 0x080E19, .88f));
            ctx->PushAxisAlignedClip(D2D1::RectF(rect.left + 2, rect.top + 2, rect.right - 2, rect.bottom - 2),
                                      D2D1_ANTIALIAS_MODE_ALIASED);
            constexpr std::array<D2D1_POINT_2F, 7> stars{{{.025f,.21f},{.08f,.68f},{.18f,.39f},
                {.37f,.81f},{.64f,.21f},{.82f,.61f},{.97f,.28f}}};
            const float width = rect.right - rect.left, height = rect.bottom - rect.top;
            const auto point = [&](std::size_t index) { return D2D1::Point2F(
                rect.left + stars[index].x * width, rect.top + stars[index].y * height); };
            for (std::size_t i = 1; i < stars.size(); ++i)
                ctx->DrawLine(point(i - 1), point(i), native_color(0x7BAFE8, enabled ? .20f : .08f), .8f);
            for (std::size_t i = 0; i < stars.size(); ++i) {
                const auto at = point(i);
                ctx->FillEllipse(D2D1::Ellipse(at, 3.8f, 3.8f), native_color(0x78B8FF, enabled ? .09f : .025f));
                ctx->FillEllipse(D2D1::Ellipse(at, i % 2 ? 1.0f : 1.4f, i % 2 ? 1.0f : 1.4f),
                                 native_color(0xD9EEFF, enabled ? .82f : .22f));
            }
            ctx->PopAxisAlignedClip();
            if (emphasized) {
                ctx->DrawRoundedRectangle(rounded, native_color(0x8CAFFF, .10f), 8.0f);
                ctx->DrawRoundedRectangle(rounded, native_color(0xABC6FF, .22f), 4.0f);
            }
            ctx->DrawRoundedRectangle(rounded, native_color(emphasized ? 0xDCE6FF : 0x8292B0,
                enabled ? (emphasized ? .95f : .60f) : .25f), emphasized ? 1.8f : 1.0f);
        };
        const auto review_center = [&](const std::wstring& text, IDWriteTextFormat* format,
                                        const D2D1_RECT_F& rect, uint32_t rgb) {
            if (!format) return;
            const auto saved = format->GetParagraphAlignment();
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            review_text(text, format, rect, rgb, DWRITE_TEXT_ALIGNMENT_CENTER);
            format->SetParagraphAlignment(saved);
        };
        const auto review_marquee = [&](std::size_t slot, const std::wstring& text, IDWriteTextFormat* format,
                                        const D2D1_RECT_F& rect, uint32_t rgb, bool centered) {
            if (!format || text.empty() || slot >= d2d_->song_review_marquee.size()) return;
            const bool reduced = native_system_reduced_motion_ || config_.reduce_menu_motion ||
                native_motion_value("enabled", 1, 0, 1) < .5f;
            auto& cache = d2d_->song_review_marquee[slot];
            if (!cache.attempted || cache.text != text || cache.format != format || cache.font_size != format->GetFontSize()) {
                cache = {}; cache.attempted = true; cache.text = text; cache.format = format;
                cache.font_size = format->GetFontSize(); cache.start_ns = render_now_ns;
                if (SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                    format, 100000, 256, cache.layout.GetAddressOf()))) {
                    cache.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                    cache.layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    cache.layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                    cache.layout->GetMetrics(&cache.metrics);
                }
            }
            if (!cache.layout) { review_text(text, format, rect, rgb); return; }
            const float width = rect.right - rect.left;
            const float scale = std::min(1.0f, (rect.bottom - rect.top) / std::max(1.0f, cache.metrics.height));
            const float overflow = std::max(0.0f, cache.metrics.widthIncludingTrailingWhitespace * scale - width);
            double seconds = (render_now_ns - cache.start_ns) / 1e9;
            if (benchmark_timings_enabled_ && benchmark_song_marquee_phase_ >= 0) {
                const double travel = overflow / kSongReviewMarqueePixelsPerSecond;
                seconds = benchmark_song_marquee_phase_ == 0 ? 1.0 : benchmark_song_marquee_phase_ == 1
                    ? kSongReviewMarqueeDwell + travel * .5 : benchmark_song_marquee_phase_ == 2
                    ? kSongReviewMarqueeDwell + travel + .5 : 2 * kSongReviewMarqueeDwell + travel * 1.5;
            }
            const float offset = song_review_marquee_offset(seconds, overflow, reduced);
            if (benchmark_timings_enabled_ && benchmark_song_marquee_count_ < benchmark_song_marquee_.size())
                benchmark_song_marquee_[benchmark_song_marquee_count_++] = {static_cast<int>(slot), offset, overflow,
                    rect.left, rect.top, rect.right, rect.bottom, centered, reduced};
            if (reduced) { review_text(text, format, rect, rgb,
                centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING); return; }
            const float x = rect.left + (centered && overflow == 0 ? (width - cache.metrics.width * scale) * .5f : -offset);
            const float y = rect.top + std::max(0.0f, (rect.bottom - rect.top - cache.metrics.height * scale) * .5f);
            D2D1_MATRIX_3X2_F saved{};
            ctx->GetTransform(&saved);
            ctx->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(x, y) * saved);
            ctx->DrawTextLayout(D2D1::Point2F(0, 0), cache.layout.Get(), semantic_color(rgb));
            ctx->SetTransform(saved);
            ctx->PopAxisAlignedClip();
        };
        const auto review_button = [&](const D2D1_RECT_F& rect, const std::wstring& label,
                                       MenuHitTargetKind kind, int index, auto... requested_states) {
            const std::array<bool, sizeof...(requested_states)> states{requested_states...};
            const bool enabled = states.empty() || states[0];
            const bool active = states.size() > 1 && states[1];
            const bool primary = states.size() > 2 && states[2];
            if (enabled) register_hit(rect, kind, index);
            review_box(rect, primary || active, enabled);
            auto* format = primary ? d2d_->header_format.Get() : d2d_->hud_format.Get();
            if (kind == MenuHitTargetKind::SongSourceAdd) format = d2d_->song_record_detail_format.Get();
            review_center(label, format, D2D1::RectF(rect.left + 8, rect.top + 2, rect.right - 8, rect.bottom - 2),
                          enabled ? 0xF4F7FF : 0x62626B);
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
        draw_ten_mark(D2D1::RectF(40, 8, 104, 64), semantic_color(0xFFFFFF), 1.0f);
        draw_native_wordmark(d2d_->header_format.Get(), D2D1::RectF(118, 5, 418, 67));
        constexpr std::array<int, 5> review_nav{0, 1, 4, 5, 6};
        const float folder_title_right = review.showing_sources ? 404.0f : 612.0f;
        for (std::size_t i = 0; i < review_nav.size(); ++i) {
            const int route = review_nav[i];
            if (route >= static_cast<int>(review.left_nav.size())) continue;
            const auto& item = review.left_nav[route];
            const float left = kSongReviewNavLeft + static_cast<float>(i) * kSongReviewNavStep;
            const auto rect = D2D1::RectF(left, 0, left + 154, 72);
            register_hit(rect, MenuHitTargetKind::SongNavButton, route);
            const bool active = item.selected || (route == 0 && !review.showing_sources && !review.showing_records) ||
                (route == 1 && review.showing_sources) || (route == 4 && review.showing_records);
            review_text(to_wide(item.label), d2d_->song_nav_format.Get(), D2D1::RectF(left, 24, left + 154, 52),
                        active ? 0xFFFFFF : 0x8F8F98, DWRITE_TEXT_ALIGNMENT_CENTER);
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
        review_text(to_wide(folder_title), d2d_->song_title_format.Get(), D2D1::RectF(48, 102, folder_title_right, 142), 0xFFFFFF);
        // Source management belongs to Library. Align it with the content
        // header controls, not the navigation bar or the folder title baseline.
        if (review.showing_sources) review_button(D2D1::RectF(416, 104, 612, 144),
            wloc("ADD FOLDER", "곡 폴더 추가"), MenuHitTargetKind::SongSourceAdd, 0);
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
            const auto clear = song_select_lamp_presentation(song.lamp);
            const bool has_clear = song.chart_count < 0 && !clear.badge_label.empty();
            register_hit(row, MenuHitTargetKind::SongCard, song.song_index);
            if (song.selected) {
                review_box(row, true);
            }
            if (has_clear) {
                const auto art_index = song_select_clear_art_index(song.lamp, song.verified_full_combo);
                if (auto* art = art_index ? d2d_->song_clear_nebula[*art_index].Get() : nullptr) {
                    // Keep the whole authored galaxy: compress its full height
                    // into the short row instead of cropping away its vertical pattern.
                    const auto size = art->GetSize();
                    const auto nebula = D2D1::RectF(kSongSelectClearArtLeft, top, row.right, row.bottom);
                    const auto source = D2D1::RectF(0, 0, size.width, size.height);
                    auto* saturated = d2d_->song_clear_nebula_saturated[*art_index].Get();
                    if (saturated && size.width > 0 && size.height > 0) {
                        D2D1_MATRIX_3X2_F saved{};
                        ctx->GetTransform(&saved);
                        ctx->PushAxisAlignedClip(nebula, D2D1_ANTIALIAS_MODE_ALIASED);
                        ctx->SetTransform(D2D1::Matrix3x2F::Scale((nebula.right - nebula.left) / size.width,
                            (nebula.bottom - nebula.top) / size.height) *
                            D2D1::Matrix3x2F::Translation(nebula.left, nebula.top) * saved);
                        ctx->DrawImage(saturated, D2D1::Point2F(0, 0), source, D2D1_INTERPOLATION_MODE_LINEAR);
                        ctx->SetTransform(saved);
                        ctx->PopAxisAlignedClip();
                    } else {
                        ctx->DrawBitmap(art, nebula, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
                    }
                    if (benchmark_timings_enabled_ && benchmark_song_clear_art_count_ < benchmark_song_clear_art_.size()) {
                        benchmark_song_clear_art_[benchmark_song_clear_art_count_++] = {
                            song.song_index, static_cast<int>(*art_index), nebula.left, nebula.right,
                            source.right - source.left, size.width, size.height, size.height,
                            saturated ? kSongSelectClearArtSaturation : 1.0f};
                    }
                }
            }
            const auto lamp = song_select_score_paint(song.lamp);
            const auto lamp_rect = D2D1::RectF(57, top + 10, 63, row.bottom - 10);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(lamp_rect, 3, 3), review_lamp_brush(lamp, lamp_rect));
            const float thumb_top = top + (row_height - 54) * 0.5f;
            auto* thumbnail = find_song_card_preview_bitmap(song.background_path);
            review_bitmap(thumbnail, D2D1::RectF(79, thumb_top, 151, thumb_top + 54));
            if (benchmark_timings_enabled_) {
                if (thumbnail) ++benchmark_song_thumbnail_images_;
                else ++benchmark_song_thumbnail_fallbacks_;
            }
            review_marquee(i * 2, to_wide(song.title), d2d_->song_title_format.Get(),
                D2D1::RectF(174, top + 6,
                    !has_clear && (song.chart_count >= 0 || !clear.label.empty()) ? 814.0f : 1016.0f,
                    top + 43), song.selected ? 0xFFFFFF : 0xECECEF, song.selected);
            review_marquee(i * 2 + 1, to_wide(song.artist), d2d_->hud_format.Get(),
                D2D1::RectF(174, top + 44,
                    !has_clear && (song.chart_count >= 0 || !clear.label.empty()) ? 814.0f : 1016.0f,
                    top + 72), 0xB6BDC9, song.selected);
            const auto badge = D2D1::RectF(836, top + 24, 1016, top + 58);
            if (song.chart_count >= 0) {
                // A folder's inventory remains a count, never a play result.
                review_text(std::to_wstring(song.chart_count), d2d_->song_record_detail_format.Get(),
                            badge, 0xBABAC2, DWRITE_TEXT_ALIGNMENT_TRAILING);
            } else if (!has_clear && !clear.label.empty()) {
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
        const auto selected_clear = song_select_lamp_presentation(review.showing_records
            ? review.score_clear_status : !review.selected_song_lamp.empty()
                ? review.selected_song_lamp : selected_card ? selected_card->lamp : std::string{});
        const bool show_selected_badge = !folder_view && !selected_clear.badge_label.empty();
        const bool selected_all_combo = song_review_all_combo_badge(show_selected_badge, review.showing_records,
            review.verified_full_combo, selected_card && selected_card->verified_full_combo);
        review_marquee(20, to_wide(title), d2d_->song_title_format.Get(),
                       D2D1::RectF(1480, 98, 1872, 174), 0xFFFFFF, false);
        if (show_selected_badge) {
            // The clear emblem belongs under the selected song's large title,
            // not under every list-row title. Reserve space above artist metadata.
            const auto badge = D2D1::RectF(1480, 184, 1760, 230);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(badge, 18, 18), semantic_color(0x030503));
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(badge, 18, 18),
                                      clear_badge_brush(selected_clear, badge), 1.8f);
            const auto icon = D2D1::RectF(1485, 179, 1541, 235);
            const auto logo_color = D2D1::ColorF(selected_clear.badge_rgb);
            draw_native_asset(native_menu_assets::kMark, icon, 1.0f, 0.0f, &logo_color,
                              clear_badge_brush(selected_clear, D2D1::RectF(8, 23, 94, 77)));
            const auto text_box = D2D1::RectF(1547, 184, 1748, 230);
            const auto center = D2D1::Point2F(1647.5f, 207);
            // Keep the original lamp outline/logo. The achievement changes only
            // the text and adds inexpensive light inside its own badge area.
            static const std::wstring all_combo_text = L"ALL COMBO";
            const auto normal_badge_text = selected_all_combo ? std::wstring{} : to_wide(selected_clear.badge_label);
            const auto& badge_text = selected_all_combo ? all_combo_text : normal_badge_text;
            bool combo_light = false;
            bool combo_reduced = false;
            float shimmer = 0, flare_x = 0;
            if (selected_all_combo) {
                if (!d2d_->song_review_combo_light_attempted) {
                    d2d_->song_review_combo_light_attempted = true;
                    const D2D1_GRADIENT_STOP stops[] = {{0, D2D1::ColorF(0xFFF3C9, .35f)},
                        {.3f, D2D1::ColorF(0xBFEAFF, .18f)}, {1, D2D1::ColorF(0xA6D9FF, 0)}};
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> colors;
                    if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 3, &colors))) {
                        ctx->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center,
                            D2D1::Point2F(0, 0), 96, 17), colors.Get(), &d2d_->song_review_combo_bloom);
                        ctx->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center,
                            D2D1::Point2F(0, 0), 14, 10), colors.Get(), &d2d_->song_review_combo_flare);
                    }
                }
                combo_reduced = native_system_reduced_motion_ || config_.reduce_menu_motion ||
                    native_motion_value("enabled", 1, 0, 1) < .5f;
                const double phase = combo_reduced ? 0 : native_menu_motion_.seconds() * (6.283185307179586 / 6.0);
                shimmer = .85f + .15f * static_cast<float>(std::sin(phase));
                flare_x = 1647.5f + (combo_reduced ? 0 : 73.0f * static_cast<float>(std::sin(phase)));
                combo_light = d2d_->song_review_combo_bloom && d2d_->song_review_combo_flare;
                if (combo_light) {
                    ctx->PushAxisAlignedClip(D2D1::RectF(1542, 186, 1756, 228), D2D1_ANTIALIAS_MODE_ALIASED);
                    auto* bloom = d2d_->song_review_combo_bloom.Get();
                    bloom->SetOpacity(.48f * shimmer);
                    ctx->FillEllipse(D2D1::Ellipse(center, 96, 17), bloom);
                    ctx->PopAxisAlignedClip();
                }
            }
            benchmark_song_review_all_combo_ = selected_all_combo;
            benchmark_song_review_combo_light_ = combo_light;
            benchmark_song_review_combo_reduced_ = combo_reduced;
            benchmark_song_review_combo_shimmer_ = shimmer;
            benchmark_song_review_combo_flare_x_ = flare_x;
            D2D1_MATRIX_3X2_F previous;
            ctx->GetTransform(&previous);
            const auto word_transform = D2D1::Matrix3x2F::Scale(1.3f, 1.3f, center) * previous;
            if (combo_light) {
                // Reuse the same cached text layout for a subpixel glow edge.
                // No intermediate bitmap, blur effect or glyph-outline allocation.
                for (const float dx : {-.65f, .65f}) {
                    ctx->SetTransform(D2D1::Matrix3x2F::Translation(dx, 0) * word_transform);
                    draw_centered_text(badge_text, clear_badge_format, text_box,
                        semantic_color(0xECF8FF, .09f * shimmer), true);
                }
            }
            ctx->SetTransform(word_transform);
            draw_centered_text(badge_text, clear_badge_format, text_box,
                               clear_badge_brush(selected_clear, badge), true);
            ctx->SetTransform(previous);
            if (combo_light) {
                // A slow, short streak rather than a full-screen/offscreen blur.
                ctx->PushAxisAlignedClip(D2D1::RectF(1542, 186, 1756, 228), D2D1_ANTIALIAS_MODE_ALIASED);
                const auto at = D2D1::Point2F(flare_x, 195);
                auto* flare = d2d_->song_review_combo_flare.Get();
                flare->SetCenter(at); flare->SetOpacity(.75f * shimmer);
                ctx->FillEllipse(D2D1::Ellipse(at, 14, 10), flare);
                ctx->DrawLine(D2D1::Point2F(at.x - 23, at.y), D2D1::Point2F(at.x + 23, at.y),
                    semantic_color(0xFFF6DD, .24f * shimmer), .7f);
                ctx->DrawLine(D2D1::Point2F(at.x, at.y - 5), D2D1::Point2F(at.x, at.y + 5),
                    semantic_color(0xECF8FF, .3f * shimmer), .7f);
                ctx->PopAxisAlignedClip();
            }
        }
        if (!folder_view) {
            const auto difficulty = song_level_text(review.selected_song_difficulty.empty() ? "--" :
                review.selected_song_difficulty, selected_aery_badge);
            review_text(difficulty, d2d_->song_record_detail_format.Get(), D2D1::RectF(1480, 237, 1872, 265),
                        kDifficultyLevelColor);
        }
        review_marquee(21, to_wide(folder_view ? (selected_card ? selected_card->artist : std::string{}) : review.selected_song_artist),
                    d2d_->body_format.Get(), D2D1::RectF(1480, 270, 1872, 298), 0xBABAC2, false);
        review_text(to_wide(review.selected_song_chart_author), d2d_->hud_format.Get(), D2D1::RectF(1480, 300, 1872, 322), 0x8F8F98);
        review_text(to_wide(review.selected_song_chart_name), d2d_->hud_format.Get(), D2D1::RectF(1480, 324, 1872, 345), 0x8F8F98);
        review_text(to_wide(folder_view ? (selected_card ? selected_card->detail : std::string{}) : review.selected_song_layout),
                    d2d_->hud_format.Get(), D2D1::RectF(1480, 347, 1872, 369), 0xBABAC2);

        review_line(1104, 382, 1872);
        const bool has_record = review.result_available || (review.online_records && review.record_count > 0);
        if (review.result_available) register_hit(D2D1::RectF(1104, 390, 1872, 696), MenuHitTargetKind::SongResultPanel, 0);
        const auto record_label = folder_view ? wloc("CHARTS", "차트") :
            ((review.showing_records ? wloc("SELECTED PLAY", "선택한 플레이") : wloc("MY BEST", "내 최고 기록")) + L" / EX Score");
        review_text(record_label,
            d2d_->song_record_label_format.Get(), D2D1::RectF(1118, 392, 1872, 418), 0xADB8CA);
        const auto score_rect = D2D1::RectF(1118, 420, 1872, 460);
        if (folder_view) {
            review_text(std::to_wstring(std::max(0, selected_card ? selected_card->chart_count : 0)),
                d2d_->song_record_value_format.Get(), score_rect, 0xECECEF);
        } else if (has_record) {
            const auto paint = song_select_score_paint(review.score_clear_status);
            draw_grade(3, to_wide(ex_score_text(review.ex_score)), d2d_->song_record_value_format.Get(), score_rect,
                1.0f, DWRITE_TEXT_ALIGNMENT_LEADING, 1.0f, 0.0f, paint.rgb, paint.rainbow);
        } else review_text(L"--", d2d_->song_record_value_format.Get(), score_rect, 0x62626B);
        if (!folder_view) {
            const auto grade_panel = D2D1::RectF(1286, 472, 1690, 618);
            review_box(grade_panel);
            draw_grade(1, to_wide(has_record && !review.rank.empty() ? review.rank : "--"),
                d2d_->result_grade_format.Get(), D2D1::RectF(1298, 478, 1678, 612),
                1.0f, DWRITE_TEXT_ALIGNMENT_CENTER, 1.0f);
            const std::array<std::wstring, 4> labels{L"BPM", wloc("KEYS", "키 수"), wloc("NOTES", "노트 수"), wloc("JUDGEMENT", "판정 난이도")};
            const std::array<std::wstring, 4> values{
                to_wide((review.selected_song_bpm > 0 ? format_decimal(review.selected_song_bpm, 1) : "--") +
                    (review.selected_song_has_bpm_changes ? " *" : "")),
                review.selected_song_key_count > 0 ? std::to_wstring(review.selected_song_key_count) + L"K" : L"--",
                review.selected_song_note_count >= 0 ? std::to_wstring(review.selected_song_note_count) : L"--",
                to_wide(review.selected_song_bms_rank.empty() ? "--" : review.selected_song_bms_rank)};
            const std::array<D2D1_RECT_F, 4> cells{{{1104,472,1274,540},{1104,548,1274,618},
                {1702,472,1872,540},{1702,548,1872,618}}};
            for (std::size_t i = 0; i < cells.size(); ++i) {
                const auto& cell = cells[i]; review_box(cell);
                review_center(labels[i], d2d_->hud_format.Get(), D2D1::RectF(cell.left + 4, cell.top + 4, cell.right - 4, cell.top + 28), 0xADB8CA);
                review_center(values[i], d2d_->song_metadata_value_format.Get(), D2D1::RectF(cell.left + 4, cell.top + 28, cell.right - 4, cell.bottom - 4), 0xF0F5FF);
            }
            const std::array<std::wstring, 2> stat_labels{wloc("ACCURACY", "정확도"), wloc("MAX COMBO", "최대 콤보")};
            const std::array<std::wstring, 2> stat_values{
                has_record ? to_wide(format_decimal(review.accuracy, 2) + "%") : L"--",
                has_record ? std::to_wstring(review.max_combo) : L"--"};
            review_line(1104, 629, 1872);
            for (std::size_t i = 0; i < 2; ++i) {
                const float left = 1104 + static_cast<float>(i) * 384;
                review_center(stat_labels[i], d2d_->song_record_label_format.Get(), D2D1::RectF(left, 636, left + 384, 659), 0xADB8CA);
                review_center(stat_values[i], d2d_->song_metadata_value_format.Get(), D2D1::RectF(left, 659, left + 384, 695), 0xF0F5FF);
            }
        }
        if (folder_view) {
            review_wrap(to_wide(review.showing_sources ? review.selected_source_path : review.browser_summary), d2d_->body_format.Get(),
                        D2D1::RectF(1118, 632, 1858, 740), 0xBABAC2);
            const std::string count = review.showing_sources && review.selected_source_song_count >= 0
                ? std::to_string(review.selected_source_song_count) + " " + loc("CHARTS", "차트") : review.group_summary;
            review_text(to_wide(count), d2d_->hud_format.Get(), D2D1::RectF(1118, 752, 1858, 782), 0x8F8F98);
        } else {
            const auto nps_panel = native_rect("songselect_review.nps_panel", D2D1::RectF(1118, 704, 1858, 792));
            review_box(nps_panel);
            const auto nps = [&](double value) { return review.selected_song_nps_available ? format_decimal(value, 1) : std::string("--"); };
            const std::array<std::string, 3> nps_labels{
                "NPS MIN " + nps(review.selected_song_nps_min), "AVG " + nps(review.selected_song_nps_average),
                "MAX " + nps(review.selected_song_nps_max)};
            const float third = (nps_panel.right - nps_panel.left - 24) / 3;
            for (std::size_t i = 0; i < nps_labels.size(); ++i) {
                const float left = nps_panel.left + 12 + static_cast<float>(i) * third;
                review_center(to_wide(nps_labels[i]), d2d_->hud_format.Get(),
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
        review_box(session);
        review_text(wloc("SESSION", "세션"), d2d_->hud_format.Get(), D2D1::RectF(1122, 814, 1330, 839), 0x8F8F98);
        if (review.left_nav.size() > 6) review_text(to_wide(review.left_nav[6].detail), d2d_->hud_format.Get(),
            D2D1::RectF(1340, 814, 1740, 839), 0x8F8F98, DWRITE_TEXT_ALIGNMENT_TRAILING);
        review_button(D2D1::RectF(1754, 812, 1858, 842), wloc("OPTIONS", "옵션"), MenuHitTargetKind::SongNavButton, 6);
        const std::array<std::wstring, 4> quick_labels{wloc("VISUAL LATENCY", "비주얼 레이턴시"), wloc("NOTE SPEED", "노트 속도"),
            wloc("GAUGE SHIFT", "게이지 시프트"), wloc("RANDOM", "랜덤")};
        const std::array<std::string, 4> quick_values{review.current_visual_latency, review.current_hi_speed, review.current_gauge, review.current_random};
        for (std::size_t i = 0; i < quick_labels.size(); ++i) {
            const float left = 1118 + static_cast<float>(i) * 187;
            const auto cell = D2D1::RectF(left, 848, left + 173, 902);
            const bool interactive = !folder_view && !review.showing_records;
            if (interactive) register_hit(cell, MenuHitTargetKind::SongQuickSetting, static_cast<int>(i), MenuHitPart::Increment);
            review_box(cell, interactive && review.quick_settings_focused && review.quick_setting_cursor == static_cast<int>(i));
            review_center(quick_labels[i], d2d_->hud_format.Get(), D2D1::RectF(left + 4, 849, left + 169, 873), 0xADB8CA);
            const uint32_t rgb = i == 2 ? song_select_gauge_text_color(review.current_gauge_tier) : 0xBABAC2;
            const auto value_rect = D2D1::RectF(left + 4, 875, left + 169, 901);
            if (i == 2) draw_grade(8, to_wide(quick_values[i]), d2d_->song_record_detail_format.Get(), value_rect,
                1.0f, DWRITE_TEXT_ALIGNMENT_CENTER, 1.0f, 0.0f, rgb, review.current_gauge_tier == "ex_hard");
            else review_center(to_wide(quick_values[i]), d2d_->song_record_detail_format.Get(), value_rect, rgb);
        }
        const std::string state = review.selected_song_collection_filter +
            (review.selected_song_favorite ? "  /  " + loc("FAVORITE", "즐겨찾기") : std::string{}) +
            (review.selected_song_ghost_status.empty() ? std::string{} : "  /  GHOST " + review.selected_song_ghost_status);
        review_text(to_wide(state), d2d_->hud_format.Get(), D2D1::RectF(1118, 915, 1858, 931), 0x8F8F98);
        const bool show_imported_score = !folder_view && !review.showing_records &&
            !review.selected_song_lr2oraja_label.empty();
        if (show_imported_score) {
            // Keep the imported source beside its own score and below session
            // state, with enough space for the existing start action.
            review_text(to_wide(review.selected_song_lr2oraja_label), d2d_->hud_format.Get(),
                D2D1::RectF(1118, 934, 1476, 960), 0xBABAC2);
            review_text(to_wide(review.selected_song_lr2oraja_score), d2d_->hud_format.Get(),
                D2D1::RectF(1488, 934, 1858, 960), 0xBABAC2, DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
        const bool start_enabled = review.showing_sources ? !review.songs.empty() : review.showing_records
            ? review.result_available && !review.online_records : !review.songs.empty();
        const auto action = review.showing_sources ? wloc("OPEN LIBRARY", "라이브러리 열기") : review.showing_records
            ? review.online_records ? wloc("VIEW ONLY", "조회 전용") : wloc("OPEN RESULT", "결과 열기")
            : review.showing_level_folders ? wloc("OPEN LEVEL", "레벨 열기") : wloc("START", "시작");
        review_button(D2D1::RectF(1104, show_imported_score ? 966.0f : 936.0f, 1872, 1006),
            action, MenuHitTargetKind::SongStartButton, 0, start_enabled, false, true);

        review_line(0, 1024, 1920);
        review_button(D2D1::RectF(48, 1034, 154, 1072), wloc("BACK", "뒤로"), MenuHitTargetKind::SongBackButton, 0);
        const uint32_t hint_rgb = review.audio_setup_required ? 0xFFD18A : 0x8F8F98;
        review_text(to_wide(review.primary_hint), d2d_->hud_format.Get(), D2D1::RectF(178, 1031, 1872, 1054), hint_rgb);
        review_text(to_wide(review.secondary_hint), d2d_->hud_format.Get(), D2D1::RectF(178, 1057, 1872, 1078), hint_rgb);
        if (review.difficulty_table_editing) {
#include "MenuWindow_draw_difficulty_table_editor.inl"
        }
