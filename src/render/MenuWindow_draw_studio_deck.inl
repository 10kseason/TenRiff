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
            const auto muted = D2D1::ColorF(0x6E7A8C);
            studio_text(c.title_text, studio_fonts.studio_title.Get(),
                D2D1::RectF(deck.left_x, 52, deck.left_x + deck.left_width, 86), ng_color("title", D2D1::ColorF(0xF4F7FB)));
            studio_text(c.artist_text, studio_fonts.studio_body.Get(),
                D2D1::RectF(deck.left_x, 90, deck.left_x + deck.left_width, 111), ng_color("body", D2D1::ColorF(0x7C889B)));
            const float cell_width = (deck.left_width - 12.0f) / 3.0f;
            for (int i = 0; i < 6; ++i) {
                const float x = deck.left_x + (i % 3) * (cell_width + 6.0f) + 9.0f;
                const float y = 135.0f + (i / 3) * 62.0f;
                studio_text(c.studio_setting_labels[i], studio_fonts.studio_label.Get(),
                    D2D1::RectF(x, y, x + cell_width - 18.0f, y + 16), muted);
                studio_text(c.studio_setting_values[i], studio_fonts.studio_numbers[4].Get(),
                    D2D1::RectF(x, y + 19, x + cell_width - 18.0f, y + 43), D2D1::ColorF(0xDDE3EB));
            }
            if (deck.map_visible) {
                const float progress = data.gameplay.duration_samples > 0 ? static_cast<float>(std::clamp(
                    static_cast<double>(data.gameplay.current_sample) / data.gameplay.duration_samples, 0.0, 1.0)) : 0.0f;
                const float y = 60.0f + 960.0f * progress;
                auto* fill = d2d_->note_fill_brush.Get(); const auto saved = fill->GetColor();
                fill->SetColor(D2D1::ColorF(0x050608, 0.75f));
                ctx->FillRectangle(D2D1::RectF(deck.map_x, 60, deck.map_x + 56, y), fill);
                ctx->DrawLine(D2D1::Point2F(deck.map_x - 8, y), D2D1::Point2F(deck.map_x + 64, y), d2d_->accent_brush.Get(), 2);
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
                studio_text(c.studio_elapsed, studio_fonts.studio_numbers[2].Get(),
                    D2D1::RectF(deck.map_x - 8, y - 22, deck.map_x + 56, y - 2), d2d_->accent_brush->GetColor(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                if (deck.map_times_visible) {
                    static const std::wstring zero = L"0:00";
                    studio_text(zero, studio_fonts.studio_numbers[1].Get(),
                        D2D1::RectF(deck.map_x - 58, 56, deck.map_x - 10, 75), D2D1::ColorF(0x4B5563), DWRITE_TEXT_ALIGNMENT_TRAILING);
                    studio_text(c.studio_total, studio_fonts.studio_numbers[1].Get(),
                        D2D1::RectF(deck.map_x - 58, 1006, deck.map_x - 10, 1025), D2D1::ColorF(0x4B5563), DWRITE_TEXT_ALIGNMENT_TRAILING);
                }
            }
            if (!deck.right_hud_visible) return;
            const float x = deck.right_x, right = x + deck.right_width;
            static const std::wstring score_label = L"SCORE";
            studio_text(score_label, studio_fonts.studio_numbers[1].Get(), D2D1::RectF(x, 48, right, 68), muted, DWRITE_TEXT_ALIGNMENT_LEADING, 3);
            const auto score_rect = D2D1::RectF(x, 70, right, 124);
            studio_text(c.studio_score_padded, studio_fonts.studio_score.Get(), score_rect, D2D1::ColorF(0x232A35), DWRITE_TEXT_ALIGNMENT_TRAILING);
            studio_text(c.studio_score_significant, studio_fonts.studio_score.Get(), score_rect, ng_color("score", D2D1::ColorF(0xF4F7FB)), DWRITE_TEXT_ALIGNMENT_TRAILING);
            float y = 130;
            if (data.gameplay.pacemaker_mode != "off" && !data.gameplay.pacemaker_mode.empty()) {
                studio_text(c.studio_pace, studio_fonts.studio_numbers[3].Get(), D2D1::RectF(x, y, right, y + 22),
                    D2D1::ColorF(data.gameplay.pacemaker_delta < 0 ? 0xFF6B78 : 0x66E6B0));
                y += 28;
            }
            y += 14;
            studio_text(c.studio_accuracy, studio_fonts.studio_accuracy.Get(), D2D1::RectF(x, y, right, y + 36), d2d_->accent_brush->GetColor());
            studio_text(c.studio_detail, studio_fonts.studio_numbers[3].Get(), D2D1::RectF(x, y + 40, right, y + 62), muted);
            y += 80;
            auto* fill = d2d_->note_fill_brush.Get(); const auto saved = fill->GetColor();
            fill->SetColor(D2D1::ColorF(0x12161E));
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, right, y + 8), 4, 4), fill);
            const auto ratios = gameplay_studio_judgement_ratios({data.gameplay.pg, data.gameplay.gr, data.gameplay.gd, data.gameplay.bd, data.gameplay.pr});
            static const std::array<std::wstring, 5> labels{L"PG", L"GR", L"G", L"BAD", L"PR"};
            static const std::array<const char*, 5> tokens{"PG", "GR", "G", "BAD", "POOR"};
            float start = x;
            for (std::size_t i = 0; i < 5; ++i) {
                const float end = start + deck.right_width * static_cast<float>(ratios[i]);
                fill->SetColor(ng_judge_color(tokens[i]));
                if (end > start) ctx->FillRectangle(D2D1::RectF(start, y, end, y + 8), fill);
                start = end;
            }
            fill->SetColor(saved);
            y += 18;
            // Fit complete name/count pairs, wrapping on pair boundaries.
            float cursor = x;
            for (std::size_t i = 0; i < 5; ++i) {
                const float name_width = static_cast<float>(labels[i].size()) * 9.0f;
                const float count_width = static_cast<float>(c.studio_judge_counts[i].size()) * 9.0f;
                const float width = name_width + count_width + 18.0f;
                if (cursor > x && cursor + width > right) { cursor = x; y += 24; }
                studio_text(labels[i], studio_fonts.studio_numbers[3].Get(), D2D1::RectF(cursor, y, cursor + name_width, y + 22), ng_judge_color(tokens[i]));
                studio_text(c.studio_judge_counts[i], studio_fonts.studio_numbers[3].Get(), D2D1::RectF(cursor + name_width + 5, y, cursor + width, y + 22), D2D1::ColorF(0x8A96A8));
                cursor += width;
            }
            studio_text(c.studio_max, studio_fonts.studio_numbers[2].Get(), D2D1::RectF(x, y + 34, right, y + 56), muted);
        };
        auto draw_studio_rails = [&] {
            const auto& field = surface_layout.player_field;
            const float hit = gameplay_field_y(field.top, field.height, judgement_line_position);
            const float ratio = static_cast<float>(std::clamp(data.gameplay.gauge / 100.0, 0.0, 1.0));
            const auto& token = data.gameplay.gauge_label;
            const char* slot = token == "EX-HARD" ? "gauge_ex_hard" : token == "HARD" ? "gauge_hard" : token == "EASY" ? "gauge_easy" : "gauge_normal";
            auto rgb = ng_rgb(slot, gameplay_gauge_color(token));
            // Only the shipped dark EX-HARD default is lifted; authored colors win.
            if (token == "EX-HARD" && rgb == 0x292C31) rgb = 0xC9CED6;
            const auto color = color_from_rgb(rgb, 1);
            auto* fill = d2d_->note_fill_brush.Get(); const auto saved = fill->GetColor(); fill->SetColor(color);
            if (ratio > 0) for (float x : {field.left - 6, field.right + 2})
                ctx->FillRectangle(D2D1::RectF(x, hit * (1 - ratio), x + 4, hit), fill);
            const float x = field.right + 14, y = gameplay_studio_gauge_marker_y(hit, ratio);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + 62, y + 27), 6, 6), fill);
            fill->SetColor(saved);
            studio_text(scene_hud_cache.gauge_value_text, studio_fonts.studio_numbers[4].Get(), D2D1::RectF(x + 8, y + 2, x + 54, y + 26), D2D1::ColorF(0x0A0C10), DWRITE_TEXT_ALIGNMENT_CENTER);
            studio_text(scene_hud_cache.studio_gauge_name, studio_fonts.studio_numbers[0].Get(), D2D1::RectF(x, y + 30, x + 86, y + 48), color, DWRITE_TEXT_ALIGNMENT_LEADING, 2);
        };
        auto draw_studio_combo = [&](const GameplayFieldLayout& field, const std::wstring& value,
                                     const GameplayTextPopAnimation& animation, float top, float bottom, float offset) {
            auto rect = gameplay_combo_overlay_rect(field, combo_position, 47, top, bottom, offset, 8, 18);
            const auto center = D2D1::Point2F((rect.left + rect.right) * .5f, (rect.top + rect.bottom) * .5f);
            D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
            const float scale = animation.scale * hud_font_scale(data.gameplay.combo_font_scale);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, center) *
                D2D1::Matrix3x2F::Translation(static_cast<float>(data.gameplay.combo_offset_x), animation.offset_y) * saved);
            auto color = ng_color("combo", D2D1::ColorF(0xFFFFFF, .92f)); color.a *= animation.opacity;
            studio_text(value, studio_fonts.studio_combo.Get(), rect, color, DWRITE_TEXT_ALIGNMENT_CENTER);
            static const std::wstring label = L"COMBO";
            studio_text(label, studio_fonts.studio_numbers[1].Get(), D2D1::RectF(rect.left, rect.bottom, rect.right, rect.bottom + 20),
                D2D1::ColorF(0x6E7A8C, animation.opacity), DWRITE_TEXT_ALIGNMENT_CENTER, 5);
            ctx->SetTransform(saved);
        };
        auto draw_studio_feedback = [&](const GameplayFieldLayout& field, bool has_feedback, const std::string& feedback,
                                        double delta, const std::wstring& text, const GameplayTextPopAnimation& animation) {
            if (!has_feedback || text.empty()) return;
            const bool timing = feedback != "PG" && !scene_hud_cache.feedback_timing_text.empty();
            const bool timing_text = timing && data.gameplay.show_timing_feedback;
            const float center_x = (field.left + field.right) * .5f + static_cast<float>(data.gameplay.judgement_offset_x);
            const float anchor = gameplay_combo_anchor_y(field, data.gameplay.judgement_position, 74, 82);
            const float y = anchor - 68;
            const float name_width = static_cast<float>(text.size()) * 21.0f;
            const float width = name_width + 36 + (timing_text ? 100 : 0);
            const float x = center_x - width * .5f;
            D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
            const float scale = animation.scale * hud_font_scale(data.gameplay.judgement_font_scale);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(center_x, y + 23)) *
                D2D1::Matrix3x2F::Translation(0, animation.offset_y) * saved);
            auto color = ng_judge_color(feedback); color.a *= animation.opacity;
            auto* fill = d2d_->note_fill_brush.Get(); const auto saved_color = fill->GetColor();
            auto backdrop = color; backdrop.a *= .12f; fill->SetColor(backdrop);
            const auto pill = D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + 46), 23, 23);
            ctx->FillRoundedRectangle(pill, fill); fill->SetColor(color); ctx->DrawRoundedRectangle(pill, fill, 1.5f);
            studio_text(text, studio_fonts.studio_judgement.Get(), D2D1::RectF(x + 18, y + 2, x + 18 + name_width, y + 44), color, DWRITE_TEXT_ALIGNMENT_CENTER, 3);
            auto timing_color = delta < 0 ? ng_color("timing_fast", D2D1::ColorF(0x5DA9FF)) : ng_color("timing_slow", D2D1::ColorF(0xFF6B7A));
            timing_color.a *= animation.opacity;
            if (timing_text) {
                const float split = x + 18 + name_width + 10;
                fill->SetColor(D2D1::ColorF(0xFFFFFF, .22f * animation.opacity));
                ctx->FillRectangle(D2D1::RectF(split, y + 12, split + 1, y + 34), fill);
                const float tx = split + 10 + static_cast<float>(data.gameplay.timing_text_offset_x);
                const float ty = y + 10 + static_cast<float>(data.gameplay.timing_text_offset_y);
                studio_text(scene_hud_cache.studio_timing, studio_fonts.studio_timing.Get(), D2D1::RectF(tx, ty, tx + 76, ty + 28), timing_color, DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            ctx->SetTransform(saved);
            if (timing && data.gameplay.show_timing_bar) {
                const float cx = center_x + static_cast<float>(data.gameplay.timing_bar_offset_x);
                const float by = y + 56 + static_cast<float>(data.gameplay.timing_bar_offset_y);
                fill->SetColor(D2D1::ColorF(0x1E2530)); ctx->FillRectangle(D2D1::RectF(cx - 100, by, cx + 100, by + 2), fill);
                fill->SetColor(D2D1::ColorF(0x7C889B)); ctx->FillRectangle(D2D1::RectF(cx - 1, by - 5, cx + 1, by + 7), fill);
                fill->SetColor(timing_color);
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + static_cast<float>(std::clamp(delta / 80.0, -1.0, 1.0)) * 100, by + 1), 4, 4), fill);
            }
            fill->SetColor(saved_color);
        };
