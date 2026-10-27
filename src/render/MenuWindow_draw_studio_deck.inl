        // Included inside the shared gameplay/preview draw path. Text layouts use
        // the existing slot cache; card geometry and all 64 bars live in the static layer.
        const auto& studio_fonts = d2d_->preview_fonts;
        auto studio_text = [&](const std::wstring& text, IDWriteTextFormat* font,
                               D2D1_RECT_F rect, D2D1_COLOR_F color,
                               DWRITE_TEXT_ALIGNMENT align = ::DWRITE_TEXT_ALIGNMENT_LEADING,
                               float spacing = 0.0f) {
            const auto saved = d2d_->text_brush->GetColor();
            d2d_->text_brush->SetColor(color);
            // The deck has opaque dark backing. Repeated outline passes obscure
            // small type and cost more than the glyphs; retain the same layout cache.
            draw_readable_text_aligned(text, font, rect, d2d_->text_brush.Get(), align, true, spacing);
            d2d_->text_brush->SetColor(saved);
        };
        auto update_studio_text = [&] {
            auto& c = scene_hud_cache;
            const auto score = make_gameplay_studio_score_counter(data.gameplay.score);
            c.studio_score_padded = score.padded; c.studio_score_significant = score.significant;
            c.studio_setting_labels = {L"RATE", L"HS", L"BPM", wloc("SCROLL", "스크롤"),
                wloc("LINE", "판정선"), wloc("LATENCY", "레이턴시")};
            c.studio_setting_values = {to_wide("x" + format_decimal(data.gameplay.rate, 2)),
                to_wide(format_decimal(data.gameplay.hispeed, 2)),
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.bpm)))),
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.scroll_speed)))),
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.judgement_line_position * 100))) + "%"),
                to_wide(format_signed_ms(data.gameplay.visual_offset_ms, 0))};
            c.studio_accuracy = to_wide(format_decimal(data.gameplay.accuracy, 2) + "%");
            c.studio_detail = to_wide("DETAIL " + format_decimal(data.gameplay.detailed_accuracy, 2));
            c.studio_max = L"MAX " + std::to_wstring(data.gameplay.max_combo);
            const std::array<int, 5> counts{data.gameplay.pg, data.gameplay.gr, data.gameplay.gd, data.gameplay.bd, data.gameplay.pr};
            for (std::size_t i = 0; i < counts.size(); ++i) c.studio_judge_counts[i] = std::to_wstring(counts[i]);
            const bool accuracy = data.gameplay.pacemaker_mode == "accuracy";
            c.studio_pace = to_wide("PACE " + std::string(data.gameplay.pacemaker_delta >= 0 ? "+" : "") +
                (accuracy ? format_decimal(data.gameplay.pacemaker_delta) : std::to_string(static_cast<int64_t>(std::llround(data.gameplay.pacemaker_delta))))) +
                (accuracy ? L"%p" : L" pts");
            c.studio_gauge_name = to_wide(data.gameplay.gauge_label);
            c.studio_timing = to_wide(format_signed_ms(data.gameplay.feedback_delta_ms, 0));
        };
        auto draw_studio_header = [&] {
            const auto& deck = studio_deck;
            const auto& c = scene_hud_cache;
            const auto muted = D2D1::ColorF(0x8290A2);
            D2D1_MATRIX_3X2_F saved_transform{};
            ctx->GetTransform(&saved_transform);
            if (deck.info_visible) {
                const auto saved_title_color = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(ng_color("title", D2D1::ColorF(0xF4F7FB)));
                draw_full_song_title(studio_fonts.studio_title.Get(), song_title_rect, d2d_->text_brush.Get(), true);
                d2d_->text_brush->SetColor(saved_title_color);
                ctx->SetTransform(D2D1::Matrix3x2F::Scale(deck.info_scale, deck.info_scale) *
                    D2D1::Matrix3x2F::Translation(deck.info_x, deck.info_y) * saved_transform);
                const float info_width = deck.info_width / deck.info_scale;
                draw_artist_and_level(D2D1::RectF(0, 42, info_width, 69), studio_fonts.studio_body.Get());
                const float cell_width = (info_width - 12.0f) / 3.0f;
                for (int i = 0; i < 6; ++i) {
                    const float x = (i % 3) * (cell_width + 6.0f) + 9.0f;
                    const float y = 83.0f + (i / 3) * 62.0f;
                    studio_text(c.studio_setting_labels[i], studio_fonts.studio_label.Get(),
                        D2D1::RectF(x, y, x + cell_width - 18.0f, y + 21), muted);
                    studio_text(c.studio_setting_values[i], studio_fonts.studio_numbers[4].Get(),
                        D2D1::RectF(x, y + 23, x + cell_width - 18.0f, y + 51), D2D1::ColorF(0xDDE3EB));
                }
                ctx->SetTransform(saved_transform);
            }
            if (deck.map_visible) {
                const float progress = data.gameplay.duration_samples > 0 ? static_cast<float>(std::clamp(
                    static_cast<double>(data.gameplay.current_sample) / data.gameplay.duration_samples, 0.0, 1.0)) : 0.0f;
                const float x = deck.map_x + deck.map_width * progress;
                const float bottom = deck.map_top + deck.map_height;
                auto* fill = d2d_->note_fill_brush.Get(); const auto saved = fill->GetColor();
                fill->SetColor(D2D1::ColorF(0x050608, 0.75f));
                ctx->FillRectangle(D2D1::RectF(deck.map_x, deck.map_top, x, bottom), fill);
                ctx->DrawLine(D2D1::Point2F(x, deck.map_top - 4), D2D1::Point2F(x, bottom + 3), d2d_->accent_brush.Get(), 2);
                fill->SetColor(saved);
                // Only the clock's displayed second invalidates these strings/layouts.
                const auto sample_rate = std::max(1, data.gameplay.sample_rate);
                const int64_t elapsed = std::max<int64_t>(0, data.gameplay.current_sample) / sample_rate;
                const int64_t total = std::max<int64_t>(0, data.gameplay.duration_samples) / sample_rate;
                if (scene_hud_cache.studio_elapsed_second != elapsed) {
                    scene_hud_cache.studio_elapsed_second = elapsed;
                    scene_hud_cache.studio_elapsed = to_wide(format_progress_clock(std::clamp<int64_t>(data.gameplay.current_sample, 0, std::max<int64_t>(0, data.gameplay.duration_samples))));
                }
                if (scene_hud_cache.studio_total_second != total) {
                    scene_hud_cache.studio_total_second = total;
                    scene_hud_cache.studio_total = to_wide(format_progress_clock(data.gameplay.duration_samples));
                }
                ctx->SetTransform(D2D1::Matrix3x2F::Scale(deck.map_scale, deck.map_scale) *
                    D2D1::Matrix3x2F::Translation(deck.map_x, deck.map_top) * saved_transform);
                const float map_width = deck.map_width / deck.map_scale;
                const float clock_x = std::clamp(map_width * progress - 38.0f, 0.0f, map_width - 76.0f);
                studio_text(c.studio_elapsed, studio_fonts.studio_numbers[2].Get(),
                    D2D1::RectF(clock_x, -28, clock_x + 76, -6), d2d_->accent_brush->GetColor(), DWRITE_TEXT_ALIGNMENT_CENTER);
                if (deck.map_times_visible) {
                    static const std::wstring zero = L"0:00";
                    studio_text(zero, studio_fonts.studio_numbers[1].Get(),
                        D2D1::RectF(0, 102, 76, 122), D2D1::ColorF(0x8A96A8));
                    studio_text(c.studio_total, studio_fonts.studio_numbers[1].Get(),
                        D2D1::RectF(map_width - 76, 102, map_width, 122), D2D1::ColorF(0x8A96A8), DWRITE_TEXT_ALIGNMENT_TRAILING);
                }
                ctx->SetTransform(saved_transform);
            }
            if (!deck.score_visible) return;
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(deck.score_scale, deck.score_scale) *
                D2D1::Matrix3x2F::Translation(deck.score_x, deck.score_y) * saved_transform);
            const float x = 0.0f, right = deck.score_width / deck.score_scale;
            static const std::wstring score_label = L"SCORE";
            studio_text(score_label, studio_fonts.studio_numbers[1].Get(), D2D1::RectF(x, 0, right, 20), muted, DWRITE_TEXT_ALIGNMENT_LEADING, 3);
            const auto score_rect = D2D1::RectF(x, 22, right, 92);
            studio_text(c.studio_score_padded, studio_fonts.studio_score.Get(), score_rect, D2D1::ColorF(0x232A35), DWRITE_TEXT_ALIGNMENT_TRAILING);
            studio_text(c.studio_score_significant, studio_fonts.studio_score.Get(), score_rect, ng_color("score", D2D1::ColorF(0xF4F7FB)), DWRITE_TEXT_ALIGNMENT_TRAILING);
            float y = 100;
            if (data.gameplay.pacemaker_mode != "off" && !data.gameplay.pacemaker_mode.empty()) {
                studio_text(c.studio_pace, studio_fonts.studio_numbers[3].Get(), D2D1::RectF(x, y, right, y + 27),
                    D2D1::ColorF(data.gameplay.pacemaker_delta < 0 ? 0xFF6B78 : 0x66E6B0));
                y += 28;
            }
            y += 14;
            studio_text(c.studio_accuracy, studio_fonts.studio_accuracy.Get(), D2D1::RectF(x, y, right, y + 46), d2d_->accent_brush->GetColor());
            studio_text(c.studio_detail, studio_fonts.studio_numbers[3].Get(), D2D1::RectF(x, y + 50, right, y + 78), muted);
            y += 94;
            auto* fill = d2d_->note_fill_brush.Get(); const auto saved = fill->GetColor();
            fill->SetColor(D2D1::ColorF(0x12161E));
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, right, y + 8), 4, 4), fill);
            const auto ratios = gameplay_studio_judgement_ratios({data.gameplay.pg, data.gameplay.gr, data.gameplay.gd, data.gameplay.bd, data.gameplay.pr});
            static const std::array<std::wstring, 5> labels{L"PG", L"GR", L"G", L"BAD", L"PR"};
            static const std::array<const char*, 5> tokens{"PG", "GR", "G", "BAD", "POOR"};
            float start = x;
            for (std::size_t i = 0; i < 5; ++i) {
                const float end = start + right * static_cast<float>(ratios[i]);
                fill->SetColor(ng_judge_color(tokens[i]));
                if (end > start) ctx->FillRectangle(D2D1::RectF(start, y, end, y + 8), fill);
                start = end;
            }
            fill->SetColor(saved);
            y += 18;
            // Fit complete name/count pairs, wrapping on pair boundaries.
            float cursor = x;
            for (std::size_t i = 0; i < 5; ++i) {
                const float name_width = static_cast<float>(labels[i].size()) * 12.0f;
                const float count_width = static_cast<float>(c.studio_judge_counts[i].size()) * 12.0f;
                const float width = name_width + count_width + 18.0f;
                if (cursor > x && cursor + width > right) { cursor = x; y += 29; }
                studio_text(labels[i], studio_fonts.studio_numbers[3].Get(), D2D1::RectF(cursor, y, cursor + name_width, y + 27), ng_judge_color(tokens[i]));
                studio_text(c.studio_judge_counts[i], studio_fonts.studio_numbers[3].Get(), D2D1::RectF(cursor + name_width + 5, y, cursor + width, y + 27), D2D1::ColorF(0x8A96A8));
                cursor += width;
            }
            studio_text(c.studio_max, studio_fonts.studio_numbers[2].Get(), D2D1::RectF(x, y + 36, right, y + 62), muted);
            ctx->SetTransform(saved_transform);
        };
        auto draw_studio_rails = [&] {
            const auto& field = surface_layout.player_field;
            const float hit = gameplay_field_y(field.top, field.height, judgement_line_position);
            const float ratio = static_cast<float>(std::clamp(data.gameplay.gauge / 100.0, 0.0, 1.0));
            const auto& token = data.gameplay.gauge_label;
            const char* slot = gameplay_gauge_is_ex(token) ? "gauge_ex_hard" : token == "HARD" ? "gauge_hard" : token == "EASY" ? "gauge_easy" : "gauge_normal";
            auto rgb = ng_rgb(slot, gameplay_gauge_color(token));
            const bool rainbow = gameplay_gauge_is_ex(token);
            const auto color = color_from_rgb(rgb, 1);
            auto* fill = d2d_->note_fill_brush.Get(); const auto saved = fill->GetColor(); fill->SetColor(color);
            if (ratio > 0) for (float x : {field.left - 12, field.right + 2})
                ctx->FillRectangle(D2D1::RectF(x, hit * (1 - ratio), x + 10, hit),
                    rainbow ? gauge_rainbow_brush(D2D1::RectF(x, 0, x + 10, hit), true, 1)
                            : static_cast<ID2D1Brush*>(fill));
            const float x = studio_deck.gauge_marker_x, y = gameplay_studio_gauge_marker_y(hit, ratio);
            const auto marker = D2D1::RectF(x, y, x + 96, y + 40);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(marker, 7, 7),
                rainbow ? gauge_rainbow_brush(marker, false, 1) : static_cast<ID2D1Brush*>(fill));
            fill->SetColor(saved);
            studio_text(scene_hud_cache.gauge_value_text, studio_fonts.studio_numbers[4].Get(), D2D1::RectF(x + 4, y + 4, x + 92, y + 36), D2D1::ColorF(0x0A0C10), DWRITE_TEXT_ALIGNMENT_CENTER);
            const auto label = D2D1::RectF(x, y + 43, x + 96, y + 65);
            if (rainbow) draw_readable_text_aligned(scene_hud_cache.studio_gauge_name,
                studio_fonts.studio_numbers[0].Get(), label, gauge_rainbow_brush(label, false, 1),
                DWRITE_TEXT_ALIGNMENT_LEADING, true, 2);
            else studio_text(scene_hud_cache.studio_gauge_name, studio_fonts.studio_numbers[0].Get(), label, color, DWRITE_TEXT_ALIGNMENT_LEADING, 2);
        };
        auto draw_studio_combo = [&](const GameplayFieldLayout& field, const std::wstring& value,
                                     const GameplayTextPopAnimation& animation, float top, float bottom, float offset, bool ghost) {
            auto rect = gameplay_combo_overlay_rect(field, combo_position, 47, top, bottom, offset, 8, 18);
            const auto center = D2D1::Point2F((rect.left + rect.right) * .5f, (rect.top + rect.bottom) * .5f);
            D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
            const float scale = animation.scale * hud_font_scale(data.gameplay.combo_font_scale);
            const float shift_y = animation.offset_y + avoid_field_counts(field, rect, scale, animation.offset_y, 20);
            record_combo_placement(rect, scale, shift_y, 20, ghost);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, center) *
                D2D1::Matrix3x2F::Translation(static_cast<float>(data.gameplay.combo_offset_x), shift_y) * saved);
            auto color = ng_color("combo", D2D1::ColorF(0xFFFFFF, .92f)); color.a *= animation.opacity;
            studio_text(value, studio_fonts.studio_combo.Get(), rect, color, DWRITE_TEXT_ALIGNMENT_CENTER);
            static const std::wstring label = L"COMBO";
            studio_text(label, studio_fonts.studio_numbers[1].Get(), D2D1::RectF(rect.left, rect.bottom, rect.right, rect.bottom + 20),
                D2D1::ColorF(0x6E7A8C, animation.opacity), DWRITE_TEXT_ALIGNMENT_CENTER, 5);
            ctx->SetTransform(saved);
        };
        auto draw_studio_feedback = [&](const GameplayFieldLayout& field, bool has_feedback, const std::string& feedback,
                                        double delta, bool has_non_pg_feedback, bool has_timing_feedback,
                                        const std::wstring& text, const std::wstring& timing_text,
                                        const GameplayTextPopAnimation& animation,
                                        const std::array<double, kGameplayTimingHistoryMaxEntries>& timing_history,
                                        std::size_t timing_history_count,
                                        const std::array<bool, kGameplayTimingHistoryMaxEntries>& maximums,
                                        double maximum_ms) {
            auto* fill = d2d_->note_fill_brush.Get();
            if (!fill) return;
            const auto saved_color = fill->GetColor();
            if (has_feedback && !text.empty()) {
                const float center_x = (field.left + field.right) * .5f + static_cast<float>(data.gameplay.judgement_offset_x);
                const float anchor = gameplay_combo_anchor_y(field, data.gameplay.judgement_position, 74, 82);
                const float y = anchor - 68;
                const float name_width = static_cast<float>(text.size()) * 21.0f;
                const float width = name_width + 36;
                const float x = center_x - width * .5f;
                D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
                const float scale = animation.scale * hud_font_scale(data.gameplay.judgement_font_scale);
                ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(center_x, y + 23)) *
                    D2D1::Matrix3x2F::Translation(0, animation.offset_y) * saved);
                auto color = ng_judge_color(feedback); color.a *= animation.opacity;
                // Judgement is text-only; the separate background preference
                // controls FAST/SLOW and timing, never a judgement capsule.
                studio_text(text, studio_fonts.studio_judgement.Get(), D2D1::RectF(x + 18, y + 2, x + 18 + name_width, y + 44), color, DWRITE_TEXT_ALIGNMENT_CENTER, 3);
                ctx->SetTransform(saved);
            }
            // Timing has a fixed field anchor and its own offsets and lifetime.
            // A PG pop, judgement XY or font animation cannot move or erase it.
            const float center_x = (field.left + field.right) * .5f;
            const float anchor = gameplay_combo_anchor_y(field, 0.24, 74, 82);
            const auto timing_color = delta < 0 ? ng_color("timing_fast", D2D1::ColorF(0x5DA9FF)) : ng_color("timing_slow", D2D1::ColorF(0xFF6B7A));
            if (has_timing_feedback && data.gameplay.show_timing_feedback && !timing_text.empty()) {
                auto text_rect = ng_rect("timing_label", D2D1::RectF(center_x - 92, anchor - 18, center_x + 92, anchor + 8));
                text_rect.left += static_cast<float>(data.gameplay.timing_text_offset_x);
                text_rect.right += static_cast<float>(data.gameplay.timing_text_offset_x);
                text_rect.top += static_cast<float>(data.gameplay.timing_text_offset_y);
                text_rect.bottom += static_cast<float>(data.gameplay.timing_text_offset_y);
                D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
                const float text_scale = hud_font_scale(data.gameplay.timing_text_scale);
                ctx->SetTransform(D2D1::Matrix3x2F::Scale(text_scale, text_scale,
                    D2D1::Point2F((text_rect.left + text_rect.right) * .5f,
                                 (text_rect.top + text_rect.bottom) * .5f)) * saved);
                studio_text(timing_text, ng_font("timing", studio_fonts.studio_timing.Get()), text_rect, timing_color, DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->SetTransform(saved);
            }
            if (data.gameplay.show_timing_bar && (data.gameplay.timing_bar_always_visible || timing_history_count > 0)) {
                auto timing_rect = ng_rect("timing", D2D1::RectF(center_x - 100, anchor + 16, center_x + 100, anchor + 18));
                const float cx = (timing_rect.left + timing_rect.right) * .5f + static_cast<float>(data.gameplay.timing_bar_offset_x);
                const float by = timing_rect.top + static_cast<float>(data.gameplay.timing_bar_offset_y);
                const float half_width = std::max(1.0f, (timing_rect.right - timing_rect.left) * .5f);
                D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
                const float bar_scale = hud_font_scale(data.gameplay.timing_bar_scale);
                ctx->SetTransform(D2D1::Matrix3x2F::Scale(bar_scale, bar_scale,
                    D2D1::Point2F(cx, by + 1)) * saved);
                const float maximum_half_width = half_width * timing_maximum_fraction(maximum_ms);
                const auto range_segment = [&](float left, float right, uint32_t color) {
                    fill->SetColor(D2D1::ColorF(color, 0.9f));
                    ctx->FillRectangle(D2D1::RectF(left, by - 0.5f, right, by + 2.5f), fill);
                };
                range_segment(cx - half_width, cx - maximum_half_width, kTimingFastColor);
                range_segment(cx - maximum_half_width, cx + maximum_half_width, kTimingMaximumColor);
                range_segment(cx + maximum_half_width, cx + half_width, kTimingLateColor);
                fill->SetColor(D2D1::ColorF(0xC9CED6)); ctx->FillRectangle(D2D1::RectF(cx - 1, by - 5, cx + 1, by + 7), fill);
                for (std::size_t i = 0; i < timing_history_count; ++i) {
                    const double marker_delta = timing_history[i];
                    auto color = D2D1::ColorF(timing_marker_color(marker_delta, maximum_ms, maximums[i]));
                    color.a *= 0.3f + 0.7f * static_cast<float>(i + 1) / static_cast<float>(timing_history_count);
                    fill->SetColor(color);
                    const float x = cx + static_cast<float>(std::clamp(marker_delta / 80.0, -1.0, 1.0)) * half_width;
                    if (data.gameplay.timing_marker_style == "barcode")
                        ctx->FillRectangle(D2D1::RectF(x - 1, by - 8, x + 1, by + 10), fill);
                    else
                        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, by + 1), 4, 4), fill);
                }
                ctx->SetTransform(saved);
            }
            fill->SetColor(saved_color);
        };
