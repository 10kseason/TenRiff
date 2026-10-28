        auto& scene_hud_cache = is_skin_preview ? skin_preview_hud_cache_ : gameplay_hud_cache_;
        if (benchmark_timings_enabled_) benchmark_combo_placement_ = {};
        const bool studio = gameplay_studio_deck_enabled(data.gameplay.hud_layout,
            normalize_gameplay_skin_source(data.gameplay.skin_source),
            data.gameplay.ghost_visible, data.gameplay.peer_visible);
        const auto& scene_title_format = d2d_->preview_fonts.title_format;
        const auto& scene_body_format = d2d_->preview_fonts.body_format;
        const auto& scene_header_format = d2d_->preview_fonts.header_format;
        const auto& scene_hud_format = d2d_->preview_fonts.hud_format;
        const auto& scene_rank_format = d2d_->preview_fonts.rank_format;
        const auto& scene_gameplay_combo_format = d2d_->preview_fonts.gameplay_combo_format;
        // Gameplay fonts and preview fonts share their base size; menu text
        // scaling must not change the player's independently sized HUD.
        auto& readable_layouts = is_skin_preview ? d2d_->preview_readable_text : d2d_->gameplay_readable_text;
        std::size_t readable_slot = 0;
        auto draw_readable_text_aligned = [&](const std::wstring& text, IDWriteTextFormat* format,
                                              const D2D1_RECT_F& rect, ID2D1Brush* brush,
                                              DWRITE_TEXT_ALIGNMENT alignment, bool plain = false, float spacing = 0.0f) {
            const float width = rect.right - rect.left, height = rect.bottom - rect.top;
            if (text.empty() || !format || !brush || width <= 0 || height <= 0) return;
            if (readable_slot >= readable_layouts.size() || !d2d_->text_outline_brush) {
                draw_text_clipped_aligned(text, format, rect, brush, alignment);
                return;
            }
            auto& cached = readable_layouts[readable_slot++];
            if (!cached.layout || cached.text != text || cached.format.Get() != format ||
                cached.width != width || cached.height != height || cached.alignment != alignment || cached.spacing != spacing) {
                cached.layout.Reset();
                if (FAILED(d2d_->dwrite_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                    format, width, height, cached.layout.ReleaseAndGetAddressOf()))) return;
                cached.layout->SetTextAlignment(alignment);
                if (spacing > 0.0f) {
                    Microsoft::WRL::ComPtr<IDWriteTextLayout1> spaced;
                    if (SUCCEEDED(cached.layout.As(&spaced)))
                        spaced->SetCharacterSpacing(spacing * 0.5f, spacing * 0.5f, 0, DWRITE_TEXT_RANGE{0, static_cast<UINT32>(text.size())});
                }
                cached.spacing = spacing;
                cached.text = text; cached.format = format;
                cached.width = width; cached.height = height; cached.alignment = alignment;
            }
            if (plain) ctx->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), cached.layout.Get(), brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
            else draw_text_layout_readable(D2D1::Point2F(rect.left, rect.top), cached.layout.Get(), brush,
                                           D2D1_DRAW_TEXT_OPTIONS_CLIP);
        };
        auto draw_readable_text = [&](const std::wstring& text, IDWriteTextFormat* format,
                                      const D2D1_RECT_F& rect, ID2D1Brush* brush) {
            draw_readable_text_aligned(text, format, rect, brush, DWRITE_TEXT_ALIGNMENT_LEADING);
        };
        auto draw_scaled_readable_text = [&](const std::wstring& text, IDWriteTextFormat* format,
                                             const D2D1_RECT_F& rect, ID2D1Brush* brush,
                                             DWRITE_TEXT_ALIGNMENT alignment, float scale) {
            if (!format || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) return;
            if (scale >= 1.0f) {
                draw_readable_text_aligned(text, format, rect, brush, alignment);
                return;
            }
            D2D1_MATRIX_3X2_F saved{};
            ctx->GetTransform(&saved);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale,
                D2D1::Point2F(rect.left, rect.top)) * saved);
            draw_readable_text_aligned(text, format,
                D2D1::RectF(rect.left, rect.top, rect.left + (rect.right - rect.left) / scale,
                            rect.top + (rect.bottom - rect.top) / scale), brush, alignment);
            ctx->SetTransform(saved);
        };
        auto draw_fitted_readable_text = [&](const std::wstring& text, IDWriteTextFormat* format,
                                               const D2D1_RECT_F& rect, ID2D1Brush* brush,
                                               DWRITE_TEXT_ALIGNMENT alignment) {
            if (!format || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) return;
            draw_scaled_readable_text(text, format, rect, brush, alignment,
                estimate_single_line_text_scale(text, format->GetFontSize(),
                    rect.right - rect.left, rect.bottom - rect.top, 0.10f));
        };
        // The width estimate is deliberately pessimistic and shrank Latin capitals
        // that fit. Header strings change rarely, so measure them once instead.
        auto& measured_text = is_skin_preview ? d2d_->preview_measured_text : d2d_->gameplay_measured_text;
        auto measured_text_scale = [&](std::size_t slot, const std::wstring& text, IDWriteTextFormat* format,
                                       float available_width, float minimum_scale) {
            if (slot >= measured_text.size() || !format || text.empty() || available_width <= 0.0f) return 1.0f;
            auto& entry = measured_text[slot];
            if (entry.text != text || entry.format.Get() != format) {
                entry.text = text;
                entry.format = format;
                entry.width = 0.0f;
                Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
                DWRITE_TEXT_METRICS metrics{};
                if (SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                        format, 100000.0f, 1000.0f, layout.GetAddressOf())) &&
                    SUCCEEDED(layout->GetMetrics(&metrics))) {
                    entry.width = metrics.widthIncludingTrailingWhitespace;
                }
            }
            return entry.width <= available_width ? 1.0f
                : std::clamp(available_width / entry.width, std::clamp(minimum_scale, 0.05f, 1.0f), 1.0f);
        };
        auto hud_font_scale = [](double value) {
            return static_cast<float>(std::clamp(std::isfinite(value) ? value : 1.0, 0.5, 2.0));
        };
        auto readable_text_rect = [](D2D1_RECT_F rect, IDWriteTextFormat* format) {
            if (format) {
                const float extra = std::max(0.0f, format->GetFontSize()*1.4f+2.0f-(rect.bottom-rect.top));
                rect.top -= extra*0.5f; rect.bottom += extra*0.5f;
            }
            return rect;
        };
        auto draw_full_song_title = [&](IDWriteTextFormat* format, const D2D1_RECT_F& rect,
                                        ID2D1Brush* brush, bool plain = false) {
            const auto& text = scene_hud_cache.title_text;
            if (!format || !brush || text.empty()) return;
            auto& title = is_skin_preview ? d2d_->preview_song_title : d2d_->gameplay_song_title;
            if (!title.layout || title.text != text || title.format.Get() != format) {
                title.layout.Reset();
                if (FAILED(d2d_->dwrite_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                    format, 1000000.0f, 100000.0f, title.layout.ReleaseAndGetAddressOf()))) return;
                title.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                title.layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                title.layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
                title.layout->SetTrimming(&trimming, nullptr);
                DWRITE_TEXT_METRICS metrics{};
                if (FAILED(title.layout->GetMetrics(&metrics))) { title.layout.Reset(); return; }
                // Tight layout bounds let overhang account for italic/fallback
                // glyphs, rather than measuring against the wide temporary box.
                title.layout->SetMaxWidth(std::max(1.0f, metrics.widthIncludingTrailingWhitespace + 2.0f));
                title.layout->SetMaxHeight(std::max(1.0f, metrics.height + 2.0f));
                DWRITE_OVERHANG_METRICS overhang{};
                if (FAILED(title.layout->GetOverhangMetrics(&overhang))) { title.layout.Reset(); return; }
                title.inset_x = std::max(0.0f, overhang.left);
                title.inset_y = std::max(0.0f, overhang.top);
                title.width = title.layout->GetMaxWidth() + title.inset_x + std::max(0.0f, overhang.right);
                title.height = title.layout->GetMaxHeight() + title.inset_y + std::max(0.0f, overhang.bottom);
                title.line_count = metrics.lineCount;
                title.text = text; title.format = format; ++title.builds;
            }
            const float scale = gameplay_song_title_fit_scale(title.width, title.height,
                rect.right - rect.left, rect.bottom - rect.top);
            if (scale <= 0) return;
            D2D1_MATRIX_3X2_F saved{}; ctx->GetTransform(&saved);
            ctx->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(rect.left, rect.top)) * saved);
            const auto origin = D2D1::Point2F(rect.left + title.inset_x, rect.top + title.inset_y);
            if (plain) ctx->DrawTextLayout(origin, title.layout.Get(), brush);
            else draw_text_layout_readable(origin, title.layout.Get(), brush);
            ctx->SetTransform(saved);
            if (benchmark_timings_enabled_) {
                benchmark_song_title_.left = rect.left; benchmark_song_title_.top = rect.top;
                benchmark_song_title_.right = rect.right; benchmark_song_title_.bottom = rect.bottom;
                benchmark_song_title_.font_size = format->GetFontSize() * scale;
                benchmark_song_title_.text_width = title.width * scale;
                benchmark_song_title_.text_height = title.height * scale;
                benchmark_song_title_.characters = static_cast<uint32_t>(text.size());
                benchmark_song_title_.line_count = title.line_count;
                benchmark_song_title_.layout_builds = title.builds;
            }
        };
        // Single-line layouts are top-anchored; buttons and rows taller than one
        // line need their text box centred on the row instead.
        auto vcenter_text_rect = [](D2D1_RECT_F rect, IDWriteTextFormat* format) {
            if (!format) return rect;
            const float half = format->GetFontSize() * 0.68f + 1.0f;
            const float middle = (rect.top + rect.bottom) * 0.5f;
            rect.top = middle - half; rect.bottom = middle + half;
            return rect;
        };
        if (!gameplay_field_drag_state_.active) {
            gameplay_field_drag_state_.visible = false;
        }
        const int64_t render_now_ns = timing::HighResClock::now_ns();
        const double native_key_delta_seconds = native_key_frame_time_ns_ > 0
            ? static_cast<double>(render_now_ns - native_key_frame_time_ns_) / 1'000'000'000.0 : 1.0 / 144.0;
        native_key_frame_time_ns_ = render_now_ns;
        const float header_left = 84.0f;
        const float header_top = 42.0f;
        const float header_right = kBaseWidth - 84.0f;
        const float header_safe_right =
            data.performance.visible
                ? std::min(header_right, performance_overlay_safe_left(28.0f))
                : header_right;
        const bool use_imported_metrics = normalize_gameplay_skin_source(data.gameplay.skin_source) != "native";
        const auto& native_style = native_gameplay_style(data.gameplay.resolved_tenriff_skin,!use_imported_metrics);
        auto ng_metric=[&](const char* key){return native_gameplay_number(native_style,key);};
        auto ng_motion=[&](const char* key){return native_gameplay_number(native_style,key,true);};
        auto ng_alpha=[&](const char* key){return native_gameplay_alpha(native_style,key);};
        auto ng_rgb=[&](const char* key,uint32_t fallback){return native_gameplay_rgb(native_style,key,fallback);};
        auto ng_rect=[&](const char* key,D2D1_RECT_F rect){return native_gameplay_d2d_rect(native_style,key,rect);};
        auto ng_color=[&](const char* key,D2D1_COLOR_F color){return native_gameplay_d2d_color(native_style,key,color);};
        auto gauge_rainbow_brush = [&](const D2D1_RECT_F& rect, bool vertical, float alpha) -> ID2D1Brush* {
            if (!d2d_->gameplay_gauge_gradient) {
                std::array<D2D1_GRADIENT_STOP, kClearRainbowRgb.size()> stops;
                for (std::size_t s = 0; s < stops.size(); ++s)
                    stops[s] = {kClearRainbowStops[s], D2D1::ColorF(kClearRainbowRgb[s])};
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                if (SUCCEEDED(ctx->CreateGradientStopCollection(stops.data(), static_cast<UINT>(stops.size()), &collection))) {
                    ctx->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(0, 0), D2D1::Point2F(1, 0)), collection.Get(),
                        &d2d_->gameplay_gauge_gradient);
                }
            }
            if (d2d_->gameplay_gauge_gradient) {
                d2d_->gameplay_gauge_gradient->SetStartPoint(D2D1::Point2F(rect.left, rect.top));
                d2d_->gameplay_gauge_gradient->SetEndPoint(vertical
                    ? D2D1::Point2F(rect.left, rect.bottom) : D2D1::Point2F(rect.right, rect.top));
                d2d_->gameplay_gauge_gradient->SetOpacity(std::clamp(alpha, 0.0f, 1.0f));
                return d2d_->gameplay_gauge_gradient.Get();
            }
            d2d_->accent_brush->SetOpacity(1.0f);
            d2d_->accent_brush->SetColor(D2D1::ColorF(kClearExRgb, std::clamp(alpha, 0.0f, 1.0f)));
            return d2d_->accent_brush.Get();
        };
        auto ng_font=[&](const char* key,IDWriteTextFormat* fallback){
            const auto it=d2d_->native_gameplay_formats.find(key);
            return !use_imported_metrics && it!=d2d_->native_gameplay_formats.end()?it->second.Get():fallback;
        };
        auto ng_judge_color=[&](std::string_view judgement) {
            const char* role=judgement=="PG"?"judgement_pg":judgement=="GR"?"judgement_gr":judgement=="G"?"judgement_gd":judgement=="BAD"?"judgement_bd":judgement=="POOR"?"judgement_pr":"judgement";
            return ng_color(role,gameplay_feedback_color(judgement));
        };
        const double judgement_line_position =
            use_imported_metrics && gameplay_note_sprite_cache_.has_imported_judgement_line_position
                ? gameplay_note_sprite_cache_.imported_judgement_line_position
                : clamp_gameplay_judgement_line(data.gameplay.judgement_line_position);
        const double combo_position = clamp_gameplay_combo_position(data.gameplay.combo_position);
        const float note_width_scale = clamp_gameplay_note_width_scale(data.gameplay.note_width_scale);
        const float note_art_width_ratio = effective_gameplay_note_art_width_ratio(
            gameplay_note_sprite_cache_.imported_note_width_ratio, use_imported_metrics);
        const float note_height_scale = effective_gameplay_note_height_scale(
            data.gameplay.note_height_scale,
            gameplay_note_sprite_cache_.imported_note_height_ratio,
            use_imported_metrics);
        const float hold_body_width_scale =
            clamp_gameplay_hold_body_width_scale(data.gameplay.hold_body_width_scale);
        const bool note_border_enabled = data.gameplay.note_border_enabled;
        // A skin that declares note_aspect knows its own art, so it wins over the
        // player's Image Aspect setting; otherwise that setting decides.
        const NoteImageAspect note_image_aspect =
            data.gameplay.note_image_aspect != NoteImageAspect::Auto && use_imported_metrics && gameplay_note_sprite_cache_.has_imported_note_aspect
                ? gameplay_note_sprite_cache_.imported_note_aspect
                : data.gameplay.note_image_aspect;
        // Hit-burst brightness, 0% turns the explosion off entirely.
        const float key_pulse_brightness =
            std::clamp(data.gameplay.key_pulse_brightness, 0.0f, 1.0f);
        const std::string hit_burst_style =
            config::normalize_skin_hit_burst_style_token(data.gameplay.hit_burst_style);
        // The hit burst is stacked from a wide outer bloom, a lane-coloured beam, a
        // bright glow slab and a white-hot core at the judgement line. Player and
        // ghost fields draw the identical stack so both read the same.
        auto draw_gameplay_hit_burst = [&](ID2D1DeviceContext* burst_ctx,
                                           float lane_center,
                                           float burst_note_width,
                                           float burst_hit_line_y,
                                           float burst_field_top,
                                           float burst_field_bottom,
                                           float pulse,
                                           uint32_t pulse_color,
                                           float burst_opacity,
                                           bool sustained_hold) {
            if (!burst_ctx || !d2d_->note_fill_brush || burst_opacity <= 0.0f) {
                return;
            }
            const float top_limit = burst_field_top + 2.0f;
            const float bottom_limit = burst_field_bottom - 2.0f;
            auto fill_band = [&](float half_width, float half_height, float radius,
                                 uint32_t color, float alpha) {
                if (alpha <= 0.002f) {
                    return;
                }
                const D2D1_RECT_F rect =
                    D2D1::RectF(lane_center - half_width,
                                std::max(top_limit, burst_hit_line_y - half_height),
                                lane_center + half_width,
                                std::min(bottom_limit, burst_hit_line_y + half_height));
                d2d_->note_fill_brush->SetColor(
                    color_from_rgb(color, std::min(1.0f, alpha) * burst_opacity));
                burst_ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius),
                                                d2d_->note_fill_brush.Get());
            };

            const float half_w = burst_note_width * (0.52f + 0.18f * pulse);
            if (!use_imported_metrics && hit_burst_style == "prism") {
                // Native signal packets rise only a short distance above the contact line.
                // Their lane-bounded width keeps adjacent dense notes unobstructed.
                fill_band(half_w * 0.92f, sustained_hold ? 30.0f : 12.0f + ng_motion("burst_height") * pulse,
                          4.0f, pulse_color, 0.12f + 0.16f * pulse);
                fill_band(half_w * 0.88f, 2.0f + 3.0f * pulse, 2.0f,
                          blend_rgb(pulse_color, ng_rgb("burst_core",0xFFFFFF), 0.80f), (0.38f + 0.50f * pulse)*ng_alpha("burst_core"));
                if (!sustained_hold) {
                    const float y = std::max(top_limit, burst_hit_line_y - 12.0f - (1.0f - pulse) * ng_motion("burst_rise"));
                    d2d_->note_fill_brush->SetColor(color_from_rgb(
                        blend_rgb(pulse_color, ng_rgb("burst_trail",0xD5FFF4), 0.55f), pulse * 0.65f * burst_opacity*ng_alpha("burst_trail")));
                    const float width = half_w * 0.65f;
                    burst_ctx->FillRectangle(D2D1::RectF(lane_center - width, y, lane_center + width,
                                                         std::min(bottom_limit, y + 2.0f)), d2d_->note_fill_brush.Get());
                }
                return;
            }
            if (sustained_hold) {
                // A held LN gets a stable vertical flame instead of repeatedly
                // replaying the single-note flash. The small pulse keeps it alive
                // without masking the judgement line.
                fill_band(half_w * 0.72f, 48.0f + 16.0f * pulse, 12.0f,
                          blend_rgb(pulse_color, 0x76E8FF, 0.32f), 0.16f + 0.18f * pulse);
                fill_band(half_w * 0.92f, 6.0f + 4.0f * pulse, 6.0f,
                          blend_rgb(pulse_color, 0xFFFFFF, 0.66f), 0.30f + 0.28f * pulse);
            }

            if (hit_burst_style == "ring") {
                const float radius_x = half_w * (sustained_hold ? 0.92f : 1.18f + 0.22f * pulse);
                const float radius_y = sustained_hold ? 14.0f + 5.0f * pulse : 18.0f + 34.0f * pulse;
                d2d_->note_fill_brush->SetColor(color_from_rgb(
                    blend_rgb(pulse_color, 0xFFFFFF, 0.55f),
                    (sustained_hold ? 0.46f : 0.28f + 0.54f * pulse) * burst_opacity));
                burst_ctx->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(lane_center, burst_hit_line_y),
                                                     radius_x, radius_y),
                                       d2d_->note_fill_brush.Get(),
                                       sustained_hold ? 4.0f : 3.0f + 4.0f * pulse);
                fill_band(half_w * 0.58f, 3.0f + 4.0f * pulse, 4.0f,
                          blend_rgb(pulse_color, 0xFFFFFF, 0.82f), 0.36f + 0.42f * pulse);
                return;
            }

            if (hit_burst_style == "spark") {
                const float ray_x = half_w * (sustained_hold ? 0.82f : 1.30f + 0.42f * pulse);
                const float ray_y = sustained_hold ? 35.0f + 9.0f * pulse : 38.0f + 70.0f * pulse;
                d2d_->note_fill_brush->SetColor(color_from_rgb(
                    blend_rgb(pulse_color, 0xFFFFFF, 0.70f),
                    (sustained_hold ? 0.34f : 0.25f + 0.60f * pulse) * burst_opacity));
                const float stroke = sustained_hold ? 2.5f : 2.0f + 4.0f * pulse;
                burst_ctx->DrawLine(D2D1::Point2F(lane_center - ray_x, burst_hit_line_y),
                                    D2D1::Point2F(lane_center + ray_x, burst_hit_line_y),
                                    d2d_->note_fill_brush.Get(), stroke);
                burst_ctx->DrawLine(D2D1::Point2F(lane_center, std::max(top_limit, burst_hit_line_y - ray_y)),
                                    D2D1::Point2F(lane_center, std::min(bottom_limit, burst_hit_line_y + ray_y)),
                                    d2d_->note_fill_brush.Get(), stroke);
                burst_ctx->DrawLine(D2D1::Point2F(lane_center - ray_x * 0.68f, burst_hit_line_y - ray_y * 0.55f),
                                    D2D1::Point2F(lane_center + ray_x * 0.68f, burst_hit_line_y + ray_y * 0.55f),
                                    d2d_->note_fill_brush.Get(), stroke * 0.72f);
                burst_ctx->DrawLine(D2D1::Point2F(lane_center + ray_x * 0.68f, burst_hit_line_y - ray_y * 0.55f),
                                    D2D1::Point2F(lane_center - ray_x * 0.68f, burst_hit_line_y + ray_y * 0.55f),
                                    d2d_->note_fill_brush.Get(), stroke * 0.72f);
                fill_band(half_w * 0.54f, 4.0f + 5.0f * pulse, 4.0f,
                          blend_rgb(pulse_color, 0xFFFFFF, 0.90f), 0.42f + 0.46f * pulse);
                return;
            }

            // Prism: the original layered bloom, retained as the default.
            fill_band(half_w * 1.42f, (sustained_hold ? 20.0f : 34.0f) + 62.0f * pulse, 14.0f,
                      blend_rgb(pulse_color, 0x6EE7F2, 0.30f), 0.07f + 0.22f * pulse);
            fill_band(half_w * 0.86f, (sustained_hold ? 16.0f : 24.0f) + 46.0f * pulse, 10.0f,
                      blend_rgb(pulse_color, 0x9AF2FF, 0.34f), 0.14f + 0.44f * pulse);
            fill_band(half_w, 7.0f + 13.0f * pulse, 7.0f,
                      blend_rgb(pulse_color, 0xFFFFFF, 0.46f), 0.30f + 0.60f * pulse);
            fill_band(half_w * 0.62f, 3.0f + 6.0f * pulse, 4.0f,
                      blend_rgb(pulse_color, 0xFFFFFF, 0.86f), 0.40f + 0.60f * pulse);
        };
        const std::string note_shape = normalize_gameplay_note_shape(data.gameplay.note_shape);
        // Procedural shapes use flat lane-colored holds; the original rect material
        // and authored imported sprites retain their existing appearance.
        const bool simple_hold_body = !use_imported_metrics && note_shape != "rect";
        ID2D1Geometry* note_polygon_geometry = gameplay_note_polygon_geometry(
            d2d_->gameplay_note_shape_geometries, note_shape);
        const float visual_opacity =
            static_cast<float>(std::clamp(data.gameplay.visual_opacity, 0.20, 1.0));
        const float note_outline_opacity =
            static_cast<float>(std::clamp(data.gameplay.note_outline_opacity, 0.0, 1.0) * visual_opacity);
        const float hold_body_opacity =
            gameplay_hold_body_alpha(data.gameplay.hold_body_opacity, visual_opacity);
        const float native_hold_body_opacity =
            gameplay_native_hold_body_opacity(hold_body_opacity, visual_opacity);
        const std::string key_label_position =
            config::normalize_skin_key_label_position_token(data.gameplay.key_label_position);

        const GameplayMotionState motion_state{
                data.gameplay.current_sample,
                data.gameplay.duration_samples,
                data.gameplay.sample_rate,
                data.gameplay.audio_sample_time_ns,
                0,
                data.gameplay.audio_buffer_frames,
                data.gameplay.visual_offset_ms,
                data.gameplay.finished || data.gameplay.paused,
                data.gameplay.game_over && !data.gameplay.spectating_peer,
                data.gameplay.audio_clock_rate,
            };
        if (scene_hud_cache.display_chart != data.gameplay.visual_scroll_segments) {
            scene_hud_cache.display_sample.reset();
            scene_hud_cache.display_chart = data.gameplay.visual_scroll_segments;
        }
        // Preview loops intentionally seek; gameplay alone carries the last
        // rendered audio sample across delayed HUD publication.
        const GameplayMotionDiagnostics motion_diagnostics = is_skin_preview
            ? compute_gameplay_motion_diagnostics(motion_state, render_now_ns)
            : compute_continuous_gameplay_motion_diagnostics(
                  motion_state, render_now_ns, scene_hud_cache.display_sample);
        const int64_t display_sample = motion_diagnostics.display_sample;
        auto lane_has_active_hold = [&](const auto& notes,
                                        std::size_t note_count,
                                        std::size_t lane) {
            const std::size_t count = std::min(note_count, notes.size());
            for (std::size_t i = 0; i < count; ++i) {
                const auto& note = notes[i];
                if (note.hold && !note.head_visible && note.lane == static_cast<int>(lane + 1) &&
                    note.tail_sample >= display_sample) {
                    return true;
                }
            }
            return false;
        };
        const int64_t hold_handoff_grace_samples = gameplay_hold_handoff_grace_samples(
            data.gameplay.sample_rate,
            motion_diagnostics.extrapolation_limit_samples);
        const double kGameplayTimingIndicatorRangeMs = ng_metric("timing_range_ms");
        const float kGameplayTimingIndicatorHalfWidth = ng_metric("timing_half_width");
        const float kGameplayTimingIndicatorHeight = ng_metric("timing_height");

        const double display_visual_position = compute_gameplay_display_visual_position(
            data.gameplay.visual_scroll_segments.get(),
            display_sample,
            data.gameplay.current_sample,
            data.gameplay.current_visual_position,
            data.gameplay.visual_velocity);
        auto visual_to_y = [&](double position) -> float {
            return static_cast<float>(compute_gameplay_visual_y_normalized(
                position,
                display_visual_position,
                data.gameplay.future_visual_span,
                data.gameplay.past_visual_span,
                judgement_line_position));
        };
        // The loading and countdown cards draw the header before the note pass,
        // so the field geometry is resolved once up front for every branch.
        const int lane_count = std::clamp(data.gameplay.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
        const uint32_t hidden_scratch_mask = gameplay_hidden_scratch_mask(data.gameplay);
        const auto lane_hidden = [&](std::size_t lane) { return (hidden_scratch_mask & (uint32_t{1} << lane)) != 0; };
        std::array<double, kGameplayHudMaxLanes> effective_lane_width_scales{};
        effective_lane_width_scales.fill(kGameplayLaneWidthScaleDefault);
        std::size_t effective_lane_width_scale_count =
            std::min(data.gameplay.lane_width_scale_count, effective_lane_width_scales.size());
        for (std::size_t lane = 0; lane < effective_lane_width_scale_count; ++lane) {
            effective_lane_width_scales[lane] = data.gameplay.lane_width_scales[lane];
        }
        if (use_imported_metrics &&
            gameplay_note_sprite_cache_.imported_lane_width_scale_count ==
                static_cast<std::size_t>(lane_count)) {
            effective_lane_width_scale_count = static_cast<std::size_t>(lane_count);
            for (std::size_t lane = 0; lane < effective_lane_width_scale_count; ++lane) {
                effective_lane_width_scales[lane] = std::clamp(
                    effective_lane_width_scales[lane] *
                        gameplay_note_sprite_cache_.imported_lane_width_scales[lane],
                    kGameplayLaneWidthScaleMin,
                    kGameplayLaneWidthScaleMax);
            }
        }
        std::array<double, kGameplayHudMaxLanes> effective_lane_spacing_scales{};
        effective_lane_spacing_scales.fill(kGameplayLaneSpacingScaleDefault);
        std::size_t effective_lane_spacing_scale_count =
            std::min(data.gameplay.lane_spacing_scale_count, effective_lane_spacing_scales.size());
        for (std::size_t gap = 0; gap < effective_lane_spacing_scale_count; ++gap) {
            effective_lane_spacing_scales[gap] = data.gameplay.lane_spacing_scales[gap];
        }
        if (use_imported_metrics &&
            gameplay_note_sprite_cache_.imported_lane_spacing_scale_count ==
                static_cast<std::size_t>(std::max(0, lane_count - 1))) {
            effective_lane_spacing_scale_count =
                gameplay_note_sprite_cache_.imported_lane_spacing_scale_count;
            for (std::size_t gap = 0; gap < effective_lane_spacing_scale_count; ++gap) {
                effective_lane_spacing_scales[gap] = std::clamp(
                    effective_lane_spacing_scales[gap] +
                        gameplay_note_sprite_cache_.imported_lane_spacing_scales[gap],
                    kGameplayLaneSpacingScaleMin,
                    kGameplayLaneSpacingScaleMax);
            }
        }
        const GameplaySurfaceLayout surface_layout =
            build_gameplay_surface_layout(
                lane_count,
                note_width_scale,
                note_art_width_ratio,
                effective_lane_width_scale_count,
                effective_lane_width_scales,
                effective_lane_spacing_scale_count,
                effective_lane_spacing_scales,
                data.gameplay.ghost_visible,
                data.gameplay.lane_center_gap_scale,
                gameplay_field_drag_state_.has_local_override
                    ? gameplay_field_drag_state_.offset_x
                    : data.gameplay.gameplay_field_offset_x,
                data.gameplay.note_divider_gap_px, hidden_scratch_mask);
        const float solo_header_right =
            gameplay_solo_header_right(header_left, header_right, surface_layout.player_field.left);
        // Non-Studio paths do not consume this geometry.
        const auto studio_deck = studio ? compute_gameplay_studio_deck_layout(surface_layout.player_field.left,
            surface_layout.player_field.right, gameplay_studio_header_safe_right(data.performance.visible),
            data.gameplay.hud_riff_map_visible) : GameplayStudioDeckLayout{};
        const auto song_title_layout = studio ? gameplay_studio_song_title_layout(studio_deck)
            : compute_gameplay_song_title_layout(surface_layout.player_field.left,
                surface_layout.player_field.right, surface_layout.player_field.left - 32.0f, false);
        const auto song_title_rect = D2D1::RectF(song_title_layout.left, song_title_layout.top,
            song_title_layout.right, song_title_layout.bottom);
        if (benchmark_timings_enabled_) {
            benchmark_song_title_.field_left = surface_layout.player_field.left;
            benchmark_song_title_.field_right = surface_layout.player_field.right;
        }

        auto draw_timing_indicator = [&](float indicator_left,
                                         float indicator_right,
                                         float combo_anchor_y,
                                         const std::array<double, kGameplayTimingHistoryMaxEntries>& timing_history,
                                         std::size_t timing_history_count,
                                         const std::array<bool, kGameplayTimingHistoryMaxEntries>& maximums,
                                         double maximum_ms) {
            if (!scene_body_format || !d2d_->text_brush) {
                return;
            }
            D2D1_MATRIX_3X2_F timing_transform{};
            ctx->GetTransform(&timing_transform);
            const auto base_indicator=D2D1::RectF(indicator_left,combo_anchor_y,indicator_right,combo_anchor_y+48.0f);
            auto adjusted_indicator=ng_rect("timing",base_indicator);
            adjusted_indicator.left += static_cast<float>(data.gameplay.timing_bar_offset_x);
            adjusted_indicator.right += static_cast<float>(data.gameplay.timing_bar_offset_x);
            adjusted_indicator.top += static_cast<float>(data.gameplay.timing_bar_offset_y);
            adjusted_indicator.bottom += static_cast<float>(data.gameplay.timing_bar_offset_y);
            const float bar_scale = hud_font_scale(data.gameplay.timing_bar_scale);
            const auto bar_center = D2D1::Point2F((adjusted_indicator.left + adjusted_indicator.right) * 0.5f,
                                                 (adjusted_indicator.top + adjusted_indicator.bottom) * 0.5f);
            ctx->SetTransform(D2D1::Matrix3x2F::Translation(-base_indicator.left,-base_indicator.top) *
                D2D1::Matrix3x2F::Scale((adjusted_indicator.right-adjusted_indicator.left)/std::max(1.0f,base_indicator.right-base_indicator.left),
                                      (adjusted_indicator.bottom-adjusted_indicator.top)/48.0f) *
                D2D1::Matrix3x2F::Translation(adjusted_indicator.left,adjusted_indicator.top) *
                D2D1::Matrix3x2F::Scale(bar_scale, bar_scale, bar_center) * timing_transform);
            const float feedback_center_x = (indicator_left + indicator_right) * 0.5f;
            const D2D1_RECT_F indicator_rect =
                D2D1::RectF(feedback_center_x - kGameplayTimingIndicatorHalfWidth,
                            combo_anchor_y + 16.0f,
                            feedback_center_x + kGameplayTimingIndicatorHalfWidth,
                            combo_anchor_y + 16.0f + kGameplayTimingIndicatorHeight);
            // Keep only the timing marks, including for older presets that enable a backing.
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const float indicator_center_y = (indicator_rect.top + indicator_rect.bottom) * 0.5f;
            const float maximum_half_width = kGameplayTimingIndicatorHalfWidth * timing_maximum_fraction(maximum_ms);
            const auto range_segment = [&](float left, float right, uint32_t color) {
                d2d_->text_brush->SetColor(D2D1::ColorF(color, 0.9f));
                ctx->FillRectangle(D2D1::RectF(left, indicator_center_y - 1.5f,
                    right, indicator_center_y + 1.5f), d2d_->text_brush.Get());
            };
            range_segment(indicator_rect.left, feedback_center_x - maximum_half_width, kTimingFastColor);
            range_segment(feedback_center_x - maximum_half_width, feedback_center_x + maximum_half_width, kTimingMaximumColor);
            range_segment(feedback_center_x + maximum_half_width, indicator_rect.right, kTimingLateColor);
            d2d_->text_brush->SetColor(ng_color("timing",D2D1::ColorF(0xF7FAFD, 0.92f)));
            ctx->FillRectangle(D2D1::RectF(feedback_center_x - 1.0f, indicator_rect.top - 3.0f,
                                           feedback_center_x + 1.0f, indicator_rect.bottom + 3.0f),
                               d2d_->text_brush.Get());

            for (std::size_t i = 0; i < timing_history_count; ++i) {
                const double delta_ms = timing_history[i];
                const float history_weight =
                    static_cast<float>(i + 1) / static_cast<float>(std::max<std::size_t>(1, timing_history_count));
                const float marker_center_x =
                    feedback_center_x +
                    static_cast<float>(std::clamp(delta_ms / kGameplayTimingIndicatorRangeMs, -1.0, 1.0)) *
                        kGameplayTimingIndicatorHalfWidth;
                const float marker_half_height = 1.4f + 2.4f * history_weight;
                const auto marker_color = D2D1::ColorF(timing_marker_color(delta_ms, maximum_ms, maximums[i]),
                    0.12f + 0.50f * history_weight * history_weight);
                d2d_->text_brush->SetColor(marker_color);
                if (data.gameplay.timing_marker_style == "barcode") {
                    ctx->FillRectangle(D2D1::RectF(marker_center_x - 1.0f, indicator_center_y - 9.0f,
                        marker_center_x + 1.0f, indicator_center_y + 9.0f), d2d_->text_brush.Get());
                } else {
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(marker_center_x, indicator_center_y),
                        marker_half_height, marker_half_height), d2d_->text_brush.Get());
                }
            }

            if (timing_history_count > 0) {
                const double live_feedback_delta_ms = timing_history[timing_history_count - 1];
                const float live_marker_center_x =
                    feedback_center_x +
                    static_cast<float>(std::clamp(live_feedback_delta_ms / kGameplayTimingIndicatorRangeMs,
                                                  -1.0,
                                                  1.0)) *
                        kGameplayTimingIndicatorHalfWidth;
                d2d_->text_brush->SetColor(D2D1::ColorF(timing_marker_color(
                    live_feedback_delta_ms, maximum_ms, maximums[timing_history_count - 1]), 0.98f));
                if (data.gameplay.timing_marker_style == "barcode") {
                    ctx->FillRectangle(D2D1::RectF(live_marker_center_x - 1.5f, indicator_center_y - 10.0f,
                        live_marker_center_x + 1.5f, indicator_center_y + 10.0f), d2d_->text_brush.Get());
                } else {
                    ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(live_marker_center_x, indicator_center_y), 5.0f, 5.0f),
                                     d2d_->text_brush.Get());
                }
            }

            d2d_->text_brush->SetColor(saved_text_color);
            ctx->SetTransform(timing_transform);
        };

        auto field_judgement_counts_rect = [&](const GameplayFieldLayout& field) {
            // Follow the actual timing bar transform, independently of its marker lifetime.
            // Clamp the counter inside the field even when a custom indicator is moved outside it.
            const float anchor = gameplay_combo_anchor_y(field, 0.24, 74.0f, 82.0f);
            const float center = (field.left + field.right) * 0.5f;
            auto timing = ng_rect("timing", studio
                ? D2D1::RectF(center - 100, anchor + 16, center + 100, anchor + 18)
                : D2D1::RectF(field.left, anchor, field.right, anchor + 48));
            const float bar_scale = hud_font_scale(data.gameplay.timing_bar_scale);
            const float cx = (timing.left + timing.right) * .5f + static_cast<float>(data.gameplay.timing_bar_offset_x);
            const float cy = (timing.top + timing.bottom) * .5f + static_cast<float>(data.gameplay.timing_bar_offset_y);
            const float marker_bottom = studio ? cy + 10 * bar_scale
                : cy + ((16 + kGameplayTimingIndicatorHeight + 10) / 48 - .5f) *
                    (timing.bottom - timing.top) * bar_scale;

            const float padding = std::min(8.0f, (field.right - field.left) * .1f);
            const float width = std::min(340.0f, field.right - field.left - 2 * padding);
            const float height = 44.0f * std::min(1.0f, width / 260.0f);
            const float left = std::clamp(cx - width * .5f, field.left + padding, field.right - padding - width);
            const float top = std::clamp(marker_bottom + 6, field.top + 8, std::max(field.top + 8, field.bottom - height - 8));
            return D2D1::RectF(left, top, left + width, top + height);
        };
        auto avoid_field_counts = [&](const GameplayFieldLayout& field, const D2D1_RECT_F& combo,
                                       float scale, float animation_y, float label_height) {
            if (!data.gameplay.show_judgement_counts) return 0.0f;
            const auto counts = field_judgement_counts_rect(field);
            const float cx = (combo.left + combo.right) * .5f, cy = (combo.top + combo.bottom) * .5f;
            const float x = static_cast<float>(data.gameplay.combo_offset_x);
            const float left = cx + (combo.left - cx) * scale + x;
            const float right = cx + (combo.right - cx) * scale + x;
            const float top = cy + (combo.top - cy) * scale + animation_y;
            const float bottom = cy + (combo.bottom + label_height - cy) * scale + animation_y;
            return gameplay_combo_counts_offset(field.top, field.bottom, left, top, right, bottom,
                counts.left, counts.top, counts.right, counts.bottom);
        };
        auto record_combo_placement = [&](const D2D1_RECT_F& rect, float scale, float shift_y,
                                           float label_height, bool ghost) {
            if (!benchmark_timings_enabled_) return;
            const float cx = (rect.left + rect.right) * .5f, cy = (rect.top + rect.bottom) * .5f;
            const float x = static_cast<float>(data.gameplay.combo_offset_x);
            auto& metrics = benchmark_combo_placement_[ghost ? 1 : 0];
            metrics.combo_visible = true;
            metrics.combo_rect = {cx + (rect.left - cx) * scale + x, cy + (rect.top - cy) * scale + shift_y,
                cx + (rect.right - cx) * scale + x, cy + (rect.bottom + label_height - cy) * scale + shift_y};
        };
        auto draw_field_judgement_counts = [&](const GameplayFieldLayout& field,
                                               const std::array<int, 5>& values, bool ghost = false) {
            if (!data.gameplay.show_judgement_counts || !scene_body_format || !d2d_->text_brush) return;
            auto& cached_values = scene_hud_cache.field_judge_values[ghost ? 1 : 0];
            auto& texts = scene_hud_cache.field_judge_counts[ghost ? 1 : 0];
            if (cached_values != values) {
                cached_values = values;
                for (std::size_t i = 0; i < values.size(); ++i) texts[i] = std::to_wstring(values[i]);
            }
            const auto bounds = field_judgement_counts_rect(field);
            if (benchmark_timings_enabled_) {
                auto& metrics = benchmark_combo_placement_[ghost ? 1 : 0];
                metrics.counts_visible = true;
                metrics.counts_rect = {bounds.left, bounds.top, bounds.right, bounds.bottom};
            }
            const float left = bounds.left, top = bounds.top;
            const float width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
            static const std::array<std::wstring, 5> labels{L"PG", L"GR", L"G", L"BAD", L"PR"};
            static const std::array<const char*, 5> tokens{"PG", "GR", "G", "BAD", "POOR"};
            const auto color = d2d_->text_brush->GetColor();
            for (std::size_t i = 0; i < values.size(); ++i) {
                const float x = left + width * static_cast<float>(i) / 5;
                const float right = x + width / 5 - 2;
                d2d_->text_brush->SetColor(D2D1::ColorF(0xF2F5F8, .96f));
                draw_fitted_readable_text(texts[i], scene_body_format.Get(),
                    D2D1::RectF(x, top, right, top + height * .52f), d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->text_brush->SetColor(ng_judge_color(tokens[i]));
                draw_fitted_readable_text(labels[i], scene_body_format.Get(),
                    D2D1::RectF(x, top + height * .54f, right, top + height), d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            d2d_->text_brush->SetColor(color);
        };

        auto format_progress_clock = [&](int64_t sample_count) -> std::string {
            if (data.gameplay.sample_rate <= 0) {
                return "--:--";
            }
            const int64_t safe_samples = std::max<int64_t>(0, sample_count);
            const int64_t total_seconds =
                std::max<int64_t>(0, static_cast<int64_t>((safe_samples + data.gameplay.sample_rate / 2) /
                                                          data.gameplay.sample_rate));
            const int64_t hours = total_seconds / 3600;
            const int64_t minutes = (total_seconds / 60) % 60;
            const int64_t seconds = total_seconds % 60;
            if (hours > 0) {
                return std::to_string(hours) + ":" +
                       (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
                       (seconds < 10 ? "0" : "") + std::to_string(seconds);
            }
            const int64_t total_minutes = total_seconds / 60;
            return std::to_string(total_minutes) + ":" +
                   (seconds < 10 ? "0" : "") + std::to_string(seconds);
        };

        auto draw_gameplay_progress_bar = [&](const D2D1_RECT_F& track_rect) {
            const float track_width = track_rect.right - track_rect.left;
            if (!scene_hud_format || !d2d_->text_brush ||
                data.gameplay.duration_samples <= 0 || track_width < 24.0f) {
                return;
            }

            const int64_t progress_sample =
                std::clamp<int64_t>(data.gameplay.current_sample, 0, data.gameplay.duration_samples);
            const int64_t remaining_sample = std::max<int64_t>(0, data.gameplay.duration_samples - progress_sample);
            const float fill_ratio =
                std::clamp(static_cast<float>(static_cast<double>(progress_sample) /
                                             static_cast<double>(std::max<int64_t>(1, data.gameplay.duration_samples))),
                           0.0f,
                           1.0f);
            const float track_radius = std::min(10.0f, (track_rect.bottom - track_rect.top) * 0.5f);
            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.72f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(track_rect, track_radius, track_radius), d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush && fill_ratio > 0.0f) {
                const float inner_left = track_rect.left + 3.0f;
                const float inner_right = track_rect.right - 3.0f;
                const float inner_top = track_rect.top + 3.0f;
                const float inner_bottom = track_rect.bottom - 3.0f;
                const float fill_right = inner_left + (inner_right - inner_left) * fill_ratio;
                const D2D1_RECT_F fill_rect =
                    D2D1::RectF(inner_left, inner_top, std::max(inner_left, fill_right), inner_bottom);
                // A sub-radius fill drew a stray sliver at 0%; round only what fits.
                const float fill_radius = std::min({8.0f, (fill_rect.right - fill_rect.left) * 0.5f,
                                                    (fill_rect.bottom - fill_rect.top) * 0.5f});
                if (fill_rect.right - fill_rect.left >= 2.0f)
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(fill_rect, fill_radius, fill_radius),
                                              d2d_->accent_brush.Get());
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(track_rect, track_radius, track_radius),
                                          d2d_->button_border_brush.Get(),
                                          1.0f);
            }
            // The embedded battle track is too thin to seat a line of text; the
            // clock clipped to half glyphs over the notes, so it shows the bar only.
            if (track_rect.bottom - track_rect.top < 18.0f) {
                return;
            }

            const std::wstring elapsed_total_w =
                to_wide(format_progress_clock(progress_sample) + " / " +
                        format_progress_clock(data.gameplay.duration_samples));
            const std::wstring remaining_w =
                to_wide(loc("LEFT ", "남은 ") + format_progress_clock(remaining_sample));
            const std::wstring percent_w =
                to_wide(std::to_string(static_cast<int>(std::llround(fill_ratio * 100.0f))) + "%");

            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.92f));
            if (track_width >= 720.0f) {
                draw_readable_text(elapsed_total_w,
                                  scene_hud_format.Get(),
                                  D2D1::RectF(track_rect.left + 12.0f, track_rect.top - 1.0f,
                                              track_rect.left + 250.0f, track_rect.bottom + 1.0f),
                                  d2d_->text_brush.Get());
                draw_readable_text_aligned(percent_w,
                                          scene_hud_format.Get(),
                                          D2D1::RectF(track_rect.left + 180.0f, track_rect.top - 1.0f,
                                                      track_rect.right - 180.0f, track_rect.bottom + 1.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            } else if (track_width >= 260.0f) {
                draw_readable_text(elapsed_total_w,
                                  scene_hud_format.Get(),
                                  D2D1::RectF(track_rect.left + 10.0f, track_rect.top - 1.0f,
                                              track_rect.right - 74.0f, track_rect.bottom + 1.0f),
                                  d2d_->text_brush.Get());
                draw_readable_text_aligned(percent_w,
                                          scene_hud_format.Get(),
                                          D2D1::RectF(track_rect.right - 68.0f, track_rect.top - 1.0f,
                                                      track_rect.right - 10.0f, track_rect.bottom + 1.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            } else if (track_width >= 130.0f) {
                draw_readable_text_aligned(elapsed_total_w,
                                          scene_hud_format.Get(),
                                          D2D1::RectF(track_rect.left + 8.0f, track_rect.top - 1.0f,
                                                      track_rect.right - 8.0f, track_rect.bottom + 1.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            if (track_width >= 720.0f && d2d_->muted_brush) {
                draw_readable_text_aligned(remaining_w,
                                          scene_hud_format.Get(),
                                          D2D1::RectF(track_rect.right - 260.0f, track_rect.top - 1.0f,
                                                      track_rect.right - 12.0f, track_rect.bottom + 1.0f),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            d2d_->text_brush->SetColor(saved_text_color);
        };

        // Reserve a separate level cell so an unusually long artist never
        // clips or pushes the chart difficulty outside the HUD column.
        const auto draw_artist_and_level = [&](D2D1_RECT_F rect, IDWriteTextFormat* font) {
            if (!scene_hud_cache.level_text.empty()) {
                const float level_width = std::min(180.0f, (rect.right - rect.left) * 0.42f);
                const auto level_rect = D2D1::RectF(rect.right - level_width, rect.top, rect.right, rect.bottom);
                const auto saved = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(D2D1::ColorF(kDifficultyLevelColor));
                draw_fitted_readable_text(scene_hud_cache.level_text, font, level_rect,
                    d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(saved);
                rect.right = level_rect.left - 12.0f;
            }
            draw_fitted_readable_text(scene_hud_cache.artist_text, font, rect,
                d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
        };
        auto draw_all_combo_badge = [&](const D2D1_RECT_F& bounds, float opacity) {
            const auto& source = d2d_->all_combo_source_rect;
            if (d2d_->all_combo_bitmap && source.right > source.left && source.bottom > source.top) {
                const float width = std::min(280.0f, bounds.right - bounds.left);
                const float scale = std::min(width / (source.right - source.left),
                    (bounds.bottom - bounds.top) / (source.bottom - source.top));
                const float half_width = (source.right - source.left) * scale * 0.5f;
                const float half_height = (source.bottom - source.top) * scale * 0.5f;
                const float cx = (bounds.left + bounds.right) * 0.5f;
                const float cy = (bounds.top + bounds.bottom) * 0.5f;
                ctx->DrawBitmap(d2d_->all_combo_bitmap.Get(),
                    D2D1::RectF(cx - half_width, cy - half_height, cx + half_width, cy + half_height),
                    opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &source);
            } else if (d2d_->text_brush && scene_hud_format) {
                const float saved = d2d_->text_brush->GetOpacity();
                d2d_->text_brush->SetOpacity(opacity);
                draw_readable_text_aligned(L"ALL COMBO", scene_hud_format.Get(), bounds,
                    d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->text_brush->SetOpacity(saved);
            }
        };
#include "MenuWindow_draw_studio_deck.inl"
        auto draw_gameplay_header = [&](const GameplayProgressTrackLayout* battle_column = nullptr) {
            if (studio) draw_studio_header();
            const auto header_text_color=d2d_->text_brush->GetColor(), header_muted_color=d2d_->muted_brush->GetColor();
            d2d_->text_brush->SetColor(ng_color("title",header_text_color));
            d2d_->muted_brush->SetColor(ng_color("body",header_muted_color));
            if (studio) { /* The deck owns the left header. */ }
            else if (battle_column) {
                const float left = battle_column->left, right = battle_column->right;
                draw_full_song_title(ng_font("title", scene_title_format.Get()),
                    D2D1::RectF(left, 72.0f, right, 114.0f), d2d_->text_brush.Get());
                draw_artist_and_level(D2D1::RectF(left, 118.0f, right, 146.0f), ng_font("body", scene_body_format.Get()));
                // The long solo status line cannot cross either battle card.
                float top = 156.0f;
                for (const auto& line : scene_hud_cache.battle_speed_text) {
                    draw_fitted_readable_text(line, scene_hud_format.Get(),
                        D2D1::RectF(left, top, right, top + 24.0f),
                        d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
                    top += 26.0f;
                }
            } else {
                // Long titles shrink to the column instead of sliding under the field.
                if (scene_title_format && d2d_->text_brush) {
                    const D2D1_RECT_F title_rect = ng_rect("title", data.gameplay.peer_visible
                        ? D2D1::RectF(header_left, header_top, solo_header_right, header_top + 52.0f)
                        : song_title_rect);
                    IDWriteTextFormat* title_format = ng_font("title",scene_title_format.Get());
                    draw_full_song_title(title_format, title_rect, d2d_->text_brush.Get());
                }
                if (scene_body_format && d2d_->muted_brush) {
                    const D2D1_RECT_F artist_rect = ng_rect("artist",D2D1::RectF(header_left, header_top + 46.0f, solo_header_right, header_top + 84.0f));
                    IDWriteTextFormat* artist_format = ng_font("body",scene_body_format.Get());
                    draw_artist_and_level(artist_rect, artist_format);
                }
                if (scene_hud_format && d2d_->muted_brush) {
                    const D2D1_RECT_F speed_rect = ng_rect("speed",D2D1::RectF(header_left, header_top + 82.0f, solo_header_right, header_top + 118.0f));
                    const float speed_width = speed_rect.right - speed_rect.left;
                    // Two rows stay readable in the column. Opponent cards own the
                    // band just below, so versus play keeps one fitted row.
                    if (data.gameplay.peer_visible) {
                        draw_scaled_readable_text(scene_hud_cache.speed_text, scene_hud_format.Get(), speed_rect,
                            d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING,
                            measured_text_scale(2, scene_hud_cache.speed_text, scene_hud_format.Get(), speed_width, 0.10f));
                    } else {
                        // One two-line layout keeps the outlined text work of the old single row.
                        const float scale = std::min(
                            measured_text_scale(2, scene_hud_cache.speed_row_text[0], scene_hud_format.Get(), speed_width, 0.70f),
                            measured_text_scale(3, scene_hud_cache.speed_row_text[1], scene_hud_format.Get(), speed_width, 0.70f));
                        draw_scaled_readable_text(scene_hud_cache.speed_rows_text, scene_hud_format.Get(),
                            D2D1::RectF(speed_rect.left, speed_rect.top, speed_rect.right,
                                        speed_rect.top + scene_hud_format->GetFontSize() * 3.0f),
                            d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, scale);
                    }
                }
            }
            d2d_->text_brush->SetColor(ng_color("score",header_text_color));
            if ((!studio || !studio_deck.score_visible) && !data.gameplay.ghost_visible && scene_title_format && d2d_->text_brush) {
                const D2D1_RECT_F score_rect = ng_rect("score",D2D1::RectF(std::max(header_left + 700.0f, header_safe_right - 610.0f),
                                header_top,
                                header_safe_right,
                                header_top + 52.0f));
                draw_readable_text_aligned(scene_hud_cache.score_text,
                                          ng_font("score",scene_title_format.Get()),
                                          score_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            if ((!studio || !studio_deck.score_visible) && !data.gameplay.ghost_visible && scene_body_format && d2d_->text_brush) {
                const D2D1_RECT_F combo_rect =
                    D2D1::RectF(std::max(header_left + 620.0f, header_safe_right - 690.0f),
                                header_top + 52.0f,
                                header_safe_right,
                                header_top + 84.0f);
                draw_readable_text_aligned(scene_hud_cache.combo_text,
                                          ng_font("body",scene_body_format.Get()),
                                          combo_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            if ((!studio || !studio_deck.score_visible) && !data.gameplay.ghost_visible && scene_hud_format && d2d_->muted_brush) {
                const D2D1_RECT_F judge_stats_rect = ng_rect("stats",D2D1::RectF(std::max(header_left + 620.0f, header_safe_right - 690.0f),
                                header_top + 82.0f,
                                header_safe_right,
                                header_top + 116.0f));
                draw_readable_text_aligned(scene_hud_cache.judge_stats_text,
                                          scene_hud_format.Get(),
                                          judge_stats_rect,
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            d2d_->text_brush->SetColor(header_text_color);
            d2d_->muted_brush->SetColor(header_muted_color);
        };

        if (scene_hud_cache.text_revision != data.gameplay.text_revision) {
            const int judgement_count =
                data.gameplay.pg + data.gameplay.gr + data.gameplay.gd +
                data.gameplay.bd + data.gameplay.pr;
            const int ghost_judgement_count = data.gameplay.ghost_pg + data.gameplay.ghost_gr +
                data.gameplay.ghost_gd + data.gameplay.ghost_bd + data.gameplay.ghost_pr;
            if (!scene_hud_cache.animation_initialized) {
                scene_hud_cache.animation_initialized = true;
            } else {
                if (data.gameplay.combo > 0 &&
                    data.gameplay.combo != scene_hud_cache.animated_combo) {
                    scene_hud_cache.combo_animation_started_ns = render_now_ns;
                }
                if (data.gameplay.has_feedback &&
                    judgement_count > scene_hud_cache.animated_judgement_count) {
                    scene_hud_cache.judgement_animation_started_ns = render_now_ns;
                }
                if (data.gameplay.ghost_combo > 0 &&
                    data.gameplay.ghost_combo != scene_hud_cache.animated_ghost_combo) {
                    scene_hud_cache.ghost_combo_animation_started_ns = render_now_ns;
                }
                if (data.gameplay.ghost_has_feedback &&
                    ghost_judgement_count > scene_hud_cache.animated_ghost_judgement_count) {
                    scene_hud_cache.ghost_judgement_animation_started_ns = render_now_ns;
                }
            }
            scene_hud_cache.animated_combo = data.gameplay.combo;
            scene_hud_cache.animated_judgement_count = judgement_count;
            scene_hud_cache.animated_ghost_combo = data.gameplay.ghost_combo;
            scene_hud_cache.animated_ghost_judgement_count = ghost_judgement_count;
            const std::string title = data.gameplay.title.empty() ? loc("Unknown Track", "알 수 없는 곡") : data.gameplay.title;
            const std::string artist = data.gameplay.artist.empty() ? loc("Unknown Artist", "알 수 없는 아티스트") : data.gameplay.artist;
            std::string gauge_label = data.gameplay.gauge_label;
            if (gauge_label == "HARD") {
                gauge_label = loc("HARD", "하드");
            } else if (gauge_label == "EASY") {
                gauge_label = loc("EASY", "이지");
            } else if (gauge_label == "NORMAL") {
                gauge_label = loc("NORMAL", "노말");
            }
            std::string ghost_gauge_label = data.gameplay.ghost_gauge_label;
            if (ghost_gauge_label == "HARD") {
                ghost_gauge_label = loc("HARD", "하드");
            } else if (ghost_gauge_label == "EASY") {
                ghost_gauge_label = loc("EASY", "이지");
            } else if (ghost_gauge_label == "NORMAL") {
                ghost_gauge_label = loc("NORMAL", "노말");
            }
            scene_hud_cache.title_text = to_wide(title);
            scene_hud_cache.artist_text = to_wide(artist);
            scene_hud_cache.level_text = to_wide(difficulty_display_label(data.gameplay.difficulty_label, false));
            scene_hud_cache.speed_text =
                to_wide(loc("RATE x", "RATE x") + format_decimal(data.gameplay.rate, 2) +
                        " / HS " + format_decimal(data.gameplay.hispeed, 2) +
                        " / BPM " + std::to_string(static_cast<int>(std::llround(data.gameplay.bpm))) +
                        " / " + loc("Scroll ", "스크롤 ") + std::to_string(static_cast<int>(std::llround(data.gameplay.scroll_speed))) +
                        " / " + loc("Line ", "판정선 ") +
                        std::to_string(static_cast<int>(std::llround(data.gameplay.judgement_line_position * 100.0))) + "%" +
                        " / " + loc("VL ", "레이턴시 ") +
                        format_signed_ms(data.gameplay.visual_offset_ms, 0));
            std::size_t speed_start = 0;
            for (auto& line : scene_hud_cache.battle_speed_text) {
                const auto end = scene_hud_cache.speed_text.find(L" / ", speed_start);
                line = scene_hud_cache.speed_text.substr(speed_start, end - speed_start);
                speed_start = end == std::wstring::npos ? scene_hud_cache.speed_text.size() : end + 3;
            }
            for (std::size_t pair = 0; pair < scene_hud_cache.battle_speed_pair_text.size(); ++pair) {
                scene_hud_cache.battle_speed_pair_text[pair] = scene_hud_cache.battle_speed_text[pair * 2] +
                    L" / " + scene_hud_cache.battle_speed_text[pair * 2 + 1];
            }
            for (std::size_t row = 0; row < scene_hud_cache.speed_row_text.size(); ++row) {
                const auto& items = scene_hud_cache.battle_speed_text;
                scene_hud_cache.speed_row_text[row] = items[row * 3] + L" / " + items[row * 3 + 1] +
                    L" / " + items[row * 3 + 2];
            }
            scene_hud_cache.speed_rows_text =
                scene_hud_cache.speed_row_text[0] + L"\n" + scene_hud_cache.speed_row_text[1];
            // UI text collapses whitespace runs, so separators must survive as
            // glyphs; "0.00% / DETAIL" previously rendered as "0.00%/ DETAIL".
            const std::string separator = " \xC2\xB7 ";
            std::string score_summary =
                loc("SCORE  ", "점수  ") + format_int_with_commas(data.gameplay.score);
            scene_hud_cache.score_text = to_wide(score_summary);
            scene_hud_cache.combo_text =
                to_wide(loc("COMBO ", "콤보 ") + std::to_string(data.gameplay.combo) +
                        separator + loc("MAX ", "최대 ") + std::to_string(data.gameplay.max_combo) +
                        separator + loc("ACC ", "정확도 ") + format_decimal(data.gameplay.accuracy, 2) + "%" +
                        " / DETAIL " + format_decimal(data.gameplay.detailed_accuracy, 2) + "%");
            scene_hud_cache.combo_value_text = to_wide(std::to_string(data.gameplay.combo));
            scene_hud_cache.combo_label_text = wloc("COMBO", "콤보");
            scene_hud_cache.judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.pg) +
                        separator + "GR " + std::to_string(data.gameplay.gr) +
                        separator + "G " + std::to_string(data.gameplay.gd) +
                        separator + "BAD " + std::to_string(data.gameplay.bd) +
                        separator + "PR " + std::to_string(data.gameplay.pr));
            scene_hud_cache.gauge_label_text = to_wide(gauge_label);
            scene_hud_cache.gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.gauge))) + "%");
            scene_hud_cache.peer_name_text =
                to_wide(data.gameplay.peer_name.empty() ? loc("OPPONENT", "\uC0C1\uB300") : data.gameplay.peer_name);
            std::string peer_status = data.gameplay.peer_status;
            if (data.gameplay.peer_disconnected) {
                peer_status = loc("DISCONNECTED", "\uC5F0\uACB0 \uB04A\uAE40");
            } else if (data.gameplay.peer_game_over) {
                peer_status = loc("GAME OVER", "\uAC8C\uC784 \uC624\uBC84");
            } else if (data.gameplay.peer_aborted) {
                peer_status = loc("ABORTED", "\uC911\uB2E8");
            } else if (data.gameplay.peer_finished) {
                peer_status = loc("FINISHED", "\uC644\uB8CC");
            } else if (peer_status.empty()) {
                peer_status = loc("PLAYING", "\uD50C\uB808\uC774 \uC911");
            }
            scene_hud_cache.peer_status_text = to_wide(peer_status);
            scene_hud_cache.peer_score_text =
                to_wide(loc("SCORE  ", "\uC810\uC218  ") + format_int_with_commas(data.gameplay.peer_score));
            scene_hud_cache.peer_combo_text =
                to_wide(loc("COMBO ", "\uCF64\uBCF4 ") + std::to_string(data.gameplay.peer_combo) +
                        separator + loc("MAX ", "\uCD5C\uB300 ") + std::to_string(data.gameplay.peer_max_combo));
            scene_hud_cache.peer_judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.peer_pg) +
                        separator + "GR " + std::to_string(data.gameplay.peer_gr) +
                        separator + "G " + std::to_string(data.gameplay.peer_gd) +
                        separator + "BAD " + std::to_string(data.gameplay.peer_bd) +
                        separator + "PR " + std::to_string(data.gameplay.peer_pr));
            scene_hud_cache.peer_gauge_text =
                to_wide(loc("GAUGE ", "\uAC8C\uC774\uC9C0 ") +
                        std::to_string(static_cast<int>(std::llround(data.gameplay.peer_gauge))) + "%");
            if (!data.gameplay.peer_score_available) {
                scene_hud_cache.versus_score_difference_text = L"SYNC";
            } else {
                const std::string difference_prefix =
                    data.gameplay.versus_score_difference > 0 ? "+" : "";
                scene_hud_cache.versus_score_difference_text =
                    to_wide(difference_prefix +
                            format_int_with_commas(data.gameplay.versus_score_difference));
            }
            scene_hud_cache.spectating_text =
                to_wide(loc("SPECTATING ", "\uAD00\uC804 \uC911  ") +
                        (data.gameplay.peer_name.empty()
                             ? loc("OPPONENT", "\uC0C1\uB300")
                             : data.gameplay.peer_name));
            std::string ghost_score_summary =
                loc("SCORE  ", "점수  ") + format_int_with_commas(data.gameplay.ghost_score);
            scene_hud_cache.ghost_score_text = to_wide(ghost_score_summary);
            scene_hud_cache.ghost_combo_text =
                to_wide(loc("COMBO ", "콤보 ") + std::to_string(data.gameplay.ghost_combo) +
                        separator + loc("MAX ", "최대 ") + std::to_string(data.gameplay.ghost_max_combo) +
                        separator + loc("ACC ", "정확도 ") + format_decimal(data.gameplay.ghost_accuracy, 2) + "%" +
                        " / DETAIL " + format_decimal(data.gameplay.ghost_detailed_accuracy, 2) + "%");
            scene_hud_cache.ghost_combo_value_text =
                to_wide(std::to_string(data.gameplay.ghost_combo));
            scene_hud_cache.ghost_judge_stats_text =
                to_wide("PG " + std::to_string(data.gameplay.ghost_pg) +
                        separator + "GR " + std::to_string(data.gameplay.ghost_gr) +
                        separator + "G " + std::to_string(data.gameplay.ghost_gd) +
                        separator + "BAD " + std::to_string(data.gameplay.ghost_bd) +
                        separator + "PR " + std::to_string(data.gameplay.ghost_pr));
            const auto summary_lines = [&](int combo, int max_combo, double accuracy, double detail,
                                           int pg, int gr, int gd, int bd, int pr) {
                return std::array<std::wstring, 4>{
                    to_wide(loc("COMBO ", "콤보 ") + std::to_string(combo) + separator +
                            loc("MAX ", "최대 ") + std::to_string(max_combo)),
                    to_wide(loc("ACC ", "정확도 ") + format_decimal(accuracy, 2) +
                            "% / DETAIL " + format_decimal(detail, 2) + "%"),
                    to_wide("PG " + std::to_string(pg) + separator + "GR " + std::to_string(gr) +
                            separator + "G " + std::to_string(gd)),
                    to_wide("BAD " + std::to_string(bd) + separator + "PR " + std::to_string(pr))};
            };
            scene_hud_cache.battle_summary_text = summary_lines(data.gameplay.combo, data.gameplay.max_combo,
                data.gameplay.accuracy, data.gameplay.detailed_accuracy, data.gameplay.pg, data.gameplay.gr,
                data.gameplay.gd, data.gameplay.bd, data.gameplay.pr);
            scene_hud_cache.ghost_battle_summary_text = summary_lines(data.gameplay.ghost_combo,
                data.gameplay.ghost_max_combo, data.gameplay.ghost_accuracy, data.gameplay.ghost_detailed_accuracy,
                data.gameplay.ghost_pg, data.gameplay.ghost_gr, data.gameplay.ghost_gd,
                data.gameplay.ghost_bd, data.gameplay.ghost_pr);
            scene_hud_cache.ghost_gauge_label_text = to_wide(ghost_gauge_label);
            scene_hud_cache.ghost_gauge_value_text =
                to_wide(std::to_string(static_cast<int>(std::llround(data.gameplay.ghost_gauge))) + "%");

            // Cache semantic feedback independently of display toggles. The skin
            // preview can toggle either element without changing text_revision.
            scene_hud_cache.feedback_text = data.gameplay.has_feedback
                ? gameplay_feedback_overlay_text(data.gameplay.feedback) : std::wstring{};
            scene_hud_cache.feedback_timing_text = data.gameplay.has_timing_feedback
                ? gameplay_timing_feedback_text(data.gameplay.timing_feedback_delta_ms) : std::wstring{};
            scene_hud_cache.ghost_feedback_text = data.gameplay.ghost_has_feedback
                ? gameplay_feedback_overlay_text(data.gameplay.ghost_feedback) : std::wstring{};
            scene_hud_cache.ghost_feedback_timing_text = data.gameplay.ghost_has_timing_feedback
                ? gameplay_timing_feedback_text(data.gameplay.ghost_timing_feedback_delta_ms) : std::wstring{};
            scene_hud_cache.text_revision = data.gameplay.text_revision;
        }
        if (studio && scene_hud_cache.studio_text_revision != data.gameplay.text_revision) {
            update_studio_text();
            scene_hud_cache.studio_text_revision = data.gameplay.text_revision;
        }

        const double kComboAnimationDurationMs = ng_motion("combo_duration_ms");
        const double kJudgementAnimationDurationMs = ng_motion("judgement_duration_ms");
        const auto animation_age_ms = [render_now_ns](int64_t started_ns, double duration_ms) {
            if (started_ns <= 0) return duration_ms;
            return std::clamp(
                static_cast<double>(render_now_ns - started_ns) / 1'000'000.0,
                0.0,
                duration_ms);
        };
        const GameplayTextPopAnimation combo_text_animation =
            compute_gameplay_text_pop_animation(
                animation_age_ms(scene_hud_cache.combo_animation_started_ns,
                                 kComboAnimationDurationMs),
                kComboAnimationDurationMs,
                ng_motion("combo_scale"),
                ng_motion("combo_lift"));
        const GameplayTextPopAnimation judgement_text_animation = gameplay_judgement_animation(
            data.gameplay.feedback, animation_age_ms(scene_hud_cache.judgement_animation_started_ns,
                                                     kJudgementAnimationDurationMs) * 220.0 / kJudgementAnimationDurationMs);

        const GameplayTextPopAnimation ghost_combo_text_animation = compute_gameplay_text_pop_animation(
            animation_age_ms(scene_hud_cache.ghost_combo_animation_started_ns, kComboAnimationDurationMs),
            kComboAnimationDurationMs, ng_motion("combo_scale"), ng_motion("combo_lift"));
        const GameplayTextPopAnimation ghost_judgement_text_animation = gameplay_judgement_animation(
            data.gameplay.ghost_feedback,
            animation_age_ms(scene_hud_cache.ghost_judgement_animation_started_ns,
                             kJudgementAnimationDurationMs) * 220.0 / kJudgementAnimationDurationMs);

        if (data.gameplay.loading && !data.gameplay.active) {
            draw_gameplay_header();
            const bool waiting_for_peer_result =
                data.gameplay.finished && data.gameplay.peer_visible;
            draw_loading_panel(
                waiting_for_peer_result ? loc("WAITING FOR OPPONENT", "상대 결과 대기 중") : loc("LOADING CHART", "차트 로딩 중"),
                data.gameplay.loading_stage.empty() ? loc("Preparing gameplay", "게임 준비 중") : data.gameplay.loading_stage,
                waiting_for_peer_result ? -1 : data.gameplay.loading_percent,
                waiting_for_peer_result ? loc("RESULT SYNC", "결과 동기화") : "");
            return;
        }

        if (data.gameplay.countdown_active && !data.gameplay.paused) {
            if (d2d_->gameplay_static_command_list) {
                ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
            }
            draw_gameplay_header();

            const D2D1_RECT_F panel_rect =
                fit_rect_below_performance_overlay(D2D1::RectF(700.0f, 300.0f, 1220.0f, 760.0f),
                                                   kBaseHeight - 120.0f,
                                                   22.0f);
            const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 28.0f, 28.0f);
            if (d2d_->panel_brush) {
                d2d_->panel_brush->SetOpacity(0.90f);
                ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(panel_rr, d2d_->accent_brush.Get(), 1.8f);
            }

            if (scene_body_format && d2d_->muted_brush) {
                const std::wstring ready_w = wloc("GET READY", "준비");
                draw_readable_text_aligned(ready_w,
                                          scene_body_format.Get(),
                                          D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 30.0f,
                                                      panel_rect.right - 24.0f, panel_rect.top + 76.0f),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            const D2D1_RECT_F countdown_rect =
                D2D1::RectF(panel_rect.left + 24.0f,
                            panel_rect.top + 86.0f,
                            panel_rect.right - 24.0f,
                            panel_rect.bottom - 116.0f);
            ID2D1Brush* countdown_brush = d2d_->logo_brush
                                              ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                              : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), countdown_rect);
            }
            if (scene_rank_format && countdown_brush) {
                const std::wstring countdown_w =
                    to_wide(std::to_string(std::max(1, data.gameplay.countdown_value)));
                Microsoft::WRL::ComPtr<IDWriteTextLayout> countdown_layout;
                bool countdown_layout_ready = false;
                if (d2d_->dwrite_factory &&
                    SUCCEEDED(d2d_->dwrite_factory->CreateTextLayout(
                        countdown_w.c_str(),
                        static_cast<UINT32>(countdown_w.size()),
                        scene_rank_format.Get(),
                        std::max(1.0f, countdown_rect.right - countdown_rect.left),
                        std::max(1.0f, countdown_rect.bottom - countdown_rect.top),
                        &countdown_layout)) &&
                    countdown_layout) {
                    DWRITE_TEXT_METRICS countdown_metrics{};
                    if (SUCCEEDED(countdown_layout->GetMetrics(&countdown_metrics))) {
                        const float countdown_width =
                            std::ceil(std::max(countdown_metrics.width,
                                               countdown_metrics.widthIncludingTrailingWhitespace));
                        const float countdown_height = std::ceil(countdown_metrics.height);
                        const D2D1_POINT_2F countdown_origin =
                            D2D1::Point2F(
                                std::floor(countdown_rect.left +
                                           ((countdown_rect.right - countdown_rect.left) - countdown_width) * 0.5f),
                                std::floor(countdown_rect.top +
                                           ((countdown_rect.bottom - countdown_rect.top) - countdown_height) * 0.5f));
                        draw_text_layout_readable(countdown_origin,
                                            countdown_layout.Get(),
                                            countdown_brush,
                                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
                        countdown_layout_ready = true;
                    }
                }
                if (!countdown_layout_ready) {
                    draw_readable_text_aligned(countdown_w,
                                              scene_rank_format.Get(),
                                              countdown_rect,
                                              countdown_brush,
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }

            if (scene_title_format && d2d_->text_brush) {
                const std::wstring start_w = data.gameplay.paused ? wloc("RESUME", "재개") : L"START";
                draw_readable_text_aligned(start_w,
                                          scene_title_format.Get(),
                                          D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 132.0f,
                                                      panel_rect.right - 24.0f, panel_rect.bottom - 64.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            if (scene_body_format && d2d_->muted_brush) {
                const std::wstring hispeed_hint_w =
                    wloc("HI-SPEED  F4 +0.25 / F3 -0.25   F5 x0.5 / F6 x2",
                         "노트속도  F4 +0.25 / F3 -0.25   F5 ×0.5 / F6 ×2");
                draw_readable_text_aligned(
                    hispeed_hint_w,
                    scene_body_format.Get(),
                    D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 62.0f,
                                panel_rect.right - 24.0f, panel_rect.bottom - 40.0f),
                    d2d_->muted_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER);
                const std::wstring tuning_hint_w =
                    wloc("JUDGE LINE  F1 UP 1% / F2 DOWN 1%   LATENCY  F7/F8 -/+1ms",
                         "판정선  F1 위 1% / F2 아래 1%   레이턴시  F7/F8 -/+1ms");
                draw_readable_text_aligned(
                    tuning_hint_w,
                    scene_body_format.Get(),
                    D2D1::RectF(panel_rect.left + 24.0f, panel_rect.bottom - 40.0f,
                                panel_rect.right - 24.0f, panel_rect.bottom - 18.0f),
                    d2d_->muted_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            return;
        }

        {
            gameplay_field_drag_state_.scene_scale = std::max(0.001f, scene_scale);
            gameplay_field_drag_state_.visible = data.gameplay.active && !data.gameplay.loading && !data.gameplay.paused;
            gameplay_field_drag_state_.left =
                scene_left + (surface_layout.player_field.right + kGameplayFieldDragHandleGap) * scene_scale;
            gameplay_field_drag_state_.top =
                scene_top + (surface_layout.player_field.top + kGameplayFieldDragHandleTop) * scene_scale;
            gameplay_field_drag_state_.right =
                gameplay_field_drag_state_.left + kGameplayFieldDragHandleWidth * scene_scale;
            gameplay_field_drag_state_.bottom =
                gameplay_field_drag_state_.top + kGameplayFieldDragHandleHeight * scene_scale;
            gameplay_field_drag_state_.offset_x = surface_layout.offset_x;
            gameplay_field_drag_state_.min_offset_x = surface_layout.min_offset_x;
            gameplay_field_drag_state_.max_offset_x = surface_layout.max_offset_x;
        }
        const GameplayFieldLayout field_layout = surface_layout.player_field;
        const float field_left = field_layout.left;
        const float field_right = field_layout.right;
        const float field_top = field_layout.top;
        const float field_bottom = field_layout.bottom;
        const float field_height = field_layout.height;
        const D2D1_RECT_F field_clip_rect =
            D2D1::RectF(field_left + 2.0f, field_top + 2.0f, field_right - 2.0f, field_bottom - 2.0f);
        const float hit_line_y = gameplay_field_y(field_top, field_height, judgement_line_position);
        const auto note_alignment = gameplay_judgement_field(field_layout, hit_line_y,
            note_height_scale, data.gameplay.judgement_line_type, *d2d_, gameplay_note_sprite_cache_,
            note_shape, note_image_aspect);

        // The same black mask covers sprite and vector notes, including LN bodies.
        // Draw inside the note clip, before HUD text and above the receptor area.
        auto draw_note_fog = [&](const GameplayFieldLayout& field, float judge_y) {
            const double fade_in = std::isfinite(data.gameplay.note_fade_in)
                ? std::clamp(data.gameplay.note_fade_in, 0.0, 1.0) : 0.0;
            const double fade_out = std::isfinite(data.gameplay.note_fade_out)
                ? std::clamp(data.gameplay.note_fade_out, 0.0, 1.0) : 0.0;
            if (fade_in <= 0.0 && fade_out <= 0.0) return;
            const float top = field.top + 2.0f;
            const float bottom = std::min(field.bottom - 2.0f, judge_y -
                std::max(2.0f, gameplay_note_head_half_height(note_height_scale) * 0.20f));
            const float span = bottom - top;
            if (span <= 0.0f) return;
            if (!d2d_->note_fog_brush) {
                const D2D1_GRADIENT_STOP stops[] = {
                    {0.0f, D2D1::ColorF(0x000000, 1.0f)},
                    {1.0f, D2D1::ColorF(0x000000, 0.0f)}};
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                if (FAILED(ctx->CreateGradientStopCollection(stops, 2, collection.GetAddressOf()))) return;
                if (FAILED(ctx->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 1)),
                    collection.Get(), d2d_->note_fog_brush.GetAddressOf()))) return;
            }
            const auto fog = [&](float opaque_y, float clear_y) {
                d2d_->note_fog_brush->SetStartPoint(D2D1::Point2F(0, opaque_y));
                d2d_->note_fog_brush->SetEndPoint(D2D1::Point2F(0, clear_y));
                ctx->FillRectangle(D2D1::RectF(field.left + 2.0f, std::min(opaque_y, clear_y),
                    field.right - 2.0f, std::max(opaque_y, clear_y)), d2d_->note_fog_brush.Get());
            };
            if (fade_in > 0.0) fog(top, top + span * static_cast<float>(fade_in));
            if (fade_out > 0.0) fog(bottom, bottom - span * static_cast<float>(fade_out));
        };

        auto draw_vertical_gauge = [&](float gauge_left,
                                       double gauge_value,
                                       std::string_view gauge_token,
                                       const std::wstring& gauge_label_text,
                                       const std::wstring& gauge_value_text,
                                       std::size_t gauge_grid_index) {
            const auto gauge=ng_rect("gauge",D2D1::RectF(gauge_left,kGameplayGaugeTop,gauge_left+kGameplayGaugeWidth,kGameplayGaugeBottom));
            const float gauge_top=gauge.top,gauge_bottom=gauge.bottom,gauge_width=gauge.right-gauge.left;
            gauge_left=gauge.left;
            const float gauge_ratio =
                static_cast<float>(std::clamp(gauge_value / 100.0, 0.0, 1.0));
            const float fill_top =
                gauge_bottom - (gauge_bottom - gauge_top) * gauge_ratio;
            const char* gauge_color_slot=gameplay_gauge_is_ex(gauge_token)?"gauge_ex_hard":gauge_token=="HARD"?"gauge_hard":gauge_token=="EASY"?"gauge_easy":"gauge_normal";
            const uint32_t gauge_rgb = gameplay_gauge_is_ex(gauge_token)
                ? gameplay_gauge_color(gauge_token) : ng_rgb(gauge_color_slot,gameplay_gauge_color(gauge_token));
            const float gauge_alpha=ng_alpha(gauge_color_slot);
            // EX is a semantic rainbow even when an older skin saved a solid EX color.
            const bool rainbow = gameplay_gauge_is_ex(gauge_token);

            if (d2d_->accent_brush && gauge_ratio > 0.0f) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                const float inner_bottom = gauge_bottom - 4.0f;
                const float inner_top = std::clamp(fill_top + 4.0f,
                                                   gauge_top + 4.0f,
                                                   inner_bottom);

                d2d_->accent_brush->SetColor(
                    color_from_rgb(gauge_rgb, 0.16f * visual_opacity*gauge_alpha));
                const D2D1_RECT_F halo =
                    D2D1::RectF(gauge_left + 1.0f,
                                std::max(gauge_top + 1.0f, fill_top - 3.0f),
                                gauge_left + gauge_width - 1.0f,
                                gauge_bottom - 1.0f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(halo, 10.0f, 10.0f),
                    rainbow ? gauge_rainbow_brush(gauge, true, 0.16f * visual_opacity * gauge_alpha)
                            : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get()));

                if (inner_top < inner_bottom) {
                    const D2D1_RECT_F fill =
                        D2D1::RectF(gauge_left + 4.0f,
                                    inner_top,
                                    gauge_left + gauge_width - 4.0f,
                                    inner_bottom);
                    d2d_->accent_brush->SetColor(
                        color_from_rgb(gauge_rgb, 0.80f * visual_opacity*gauge_alpha));
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(fill, 8.0f, 8.0f),
                        rainbow ? gauge_rainbow_brush(gauge, true, 0.80f * visual_opacity * gauge_alpha)
                                : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get()));

                    const D2D1_RECT_F sheen =
                        D2D1::RectF(fill.left + 5.0f,
                                    fill.top + 3.0f,
                                    std::min(fill.right - 4.0f, fill.left + 10.0f),
                                    fill.bottom - 3.0f);
                    if (sheen.bottom > sheen.top && sheen.right > sheen.left) {
                        d2d_->accent_brush->SetColor(
                            color_from_rgb(blend_rgb(gauge_rgb, 0xFFFFFF, 0.72f),
                                           0.46f * visual_opacity*gauge_alpha));
                        ctx->FillRoundedRectangle(D2D1::RoundedRect(sheen, 3.0f, 3.0f),
                                                  d2d_->accent_brush.Get());
                    }
                }

                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }

            if (d2d_->button_border_brush &&
                gauge_grid_index < d2d_->gameplay_gauge_grid_geometries.size() &&
                d2d_->gameplay_gauge_grid_geometries[gauge_grid_index]) {
                const float saved_opacity = d2d_->button_border_brush->GetOpacity();
                d2d_->button_border_brush->SetOpacity(0.42f * visual_opacity);
                ctx->DrawGeometry(d2d_->gameplay_gauge_grid_geometries[gauge_grid_index].Get(),
                                  d2d_->button_border_brush.Get(),
                                  1.0f);
                d2d_->button_border_brush->SetOpacity(saved_opacity);
            }

            const D2D1_RECT_F label_rect =
                D2D1::RectF(gauge_left - 90.0f,
                            gauge_top - 42.0f,
                            gauge_left + gauge_width + 90.0f,
                            gauge_top - 8.0f);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(gauge_left - 90.0f,
                            gauge_bottom + 8.0f,
                            gauge_left + gauge_width + 90.0f,
                            gauge_bottom + 50.0f);
            if (scene_body_format && d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetColor(color_from_rgb(gauge_rgb, 0.92f*gauge_alpha));
                draw_readable_text_aligned(gauge_label_text,
                                          scene_body_format.Get(),
                                          label_rect,
                                          rainbow ? gauge_rainbow_brush(label_rect, false, 0.92f * gauge_alpha)
                                                  : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get()),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (scene_title_format && d2d_->text_brush) {
                draw_readable_text_aligned(gauge_value_text,
                                          scene_title_format.Get(),
                                          value_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        };

        auto draw_combo_overlay = [&](const GameplayFieldLayout& combo_field_layout,
                                      const std::wstring& combo_value_text,
                                      const std::wstring& combo_label_text,
                                      float top_safe_margin,
                                      float bottom_safe_margin,
                                      float vertical_offset,
                                      const GameplayTextPopAnimation& animation, bool ghost = false) {
            if (studio) { draw_studio_combo(combo_field_layout, combo_value_text, animation,
                top_safe_margin, bottom_safe_margin, vertical_offset, ghost); return; }
            if (combo_value_text.empty() || !d2d_->accent_brush || !scene_gameplay_combo_format) {
                return;
            }
            constexpr float kComboHalfHeight = 30.0f;
            const bool full_combo = !is_skin_preview &&
                (ghost ? data.gameplay.ghost_full_combo : data.gameplay.full_combo);
            const float kComboLabelExtension = full_combo ? 34.0f : 18.0f;
            const D2D1_RECT_F combo_rect = readable_text_rect(ng_rect("combo",gameplay_combo_overlay_rect(combo_field_layout,
                                            combo_position,
                                            kComboHalfHeight,
                                            top_safe_margin,
                                            bottom_safe_margin,
                                            vertical_offset,
                                            8.0f,
                                            kComboLabelExtension)), ng_font("combo",scene_gameplay_combo_format.Get()));
            D2D1_MATRIX_3X2_F saved_transform{};
            ctx->GetTransform(&saved_transform);
            const D2D1_POINT_2F animation_center = D2D1::Point2F(
                (combo_rect.left + combo_rect.right) * 0.5f,
                (combo_rect.top + combo_rect.bottom) * 0.5f);
            const float combo_scale = animation.scale * hud_font_scale(data.gameplay.combo_font_scale);
            const float combo_shift_y = animation.offset_y + avoid_field_counts(combo_field_layout,
                combo_rect, combo_scale, animation.offset_y, kComboLabelExtension);
            record_combo_placement(combo_rect, combo_scale, combo_shift_y, kComboLabelExtension, ghost);
            const D2D1_MATRIX_3X2_F animation_transform =
                D2D1::Matrix3x2F::Scale(combo_scale, combo_scale, animation_center) *
                D2D1::Matrix3x2F::Translation(static_cast<float>(data.gameplay.combo_offset_x), combo_shift_y) * saved_transform;
            ctx->SetTransform(animation_transform);
            const auto combo_color=d2d_->accent_brush->GetColor();
            d2d_->accent_brush->SetColor(ng_color("combo",combo_color));
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(0.94f * animation.opacity);
            draw_readable_text_aligned(combo_value_text,
                                      ng_font("combo",scene_gameplay_combo_format.Get()),
                                      combo_rect,
                                      d2d_->accent_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->accent_brush->SetOpacity(saved_opacity);
            d2d_->accent_brush->SetColor(combo_color);
            if (full_combo) {
                draw_all_combo_badge(D2D1::RectF(combo_rect.left, combo_rect.bottom - 2.0f,
                    combo_rect.right, combo_rect.bottom + kComboLabelExtension), animation.opacity);
            } else if (scene_hud_format && d2d_->muted_brush) {
                const float saved_muted_opacity = d2d_->muted_brush->GetOpacity();
                d2d_->muted_brush->SetOpacity(saved_muted_opacity * animation.opacity);
                draw_readable_text_aligned(combo_label_text,
                                          scene_hud_format.Get(),
                                          D2D1::RectF(combo_rect.left,
                                                      combo_rect.bottom - 2.0f,
                                                      combo_rect.right,
                                                      combo_rect.bottom + kComboLabelExtension),
                                          d2d_->muted_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->muted_brush->SetOpacity(saved_muted_opacity);
            }
            ctx->SetTransform(saved_transform);
        };
        if (d2d_->gameplay_static_command_list) {
            ctx->DrawImage(d2d_->gameplay_static_command_list.Get());
        }

        // Native keys travel inside a stationary well. draw_native_digital_key
        // records each moving face so a legend rides its key; recomputing the
        // skin-metric lookups per label cost measurable render time at 16K.
        std::array<D2D1_RECT_F, kGameplayHudMaxLanes> native_key_faces{};
        std::array<D2D1_RECT_F, kGameplayHudMaxLanes> native_ghost_key_faces{};
        // The Luma Keys art reserves a recessed legend window at 204..238 of its
        // 256px face (tools/generate_luma_keys_assets.py). The old field-bottom
        // slot straddled the face's lower edge on every native key sprite.
        constexpr float kLumaKeyLegendCenter = 221.0f / 256.0f;
        const bool keycap_key_labels = !use_imported_metrics && key_label_position != "top";
        auto draw_key_labels = [&](const GameplayFieldLayout& label_field_layout, bool ghost = false) {
            if (key_label_position == "off" || !scene_hud_format || !d2d_->text_brush ||
                data.gameplay.key_label_count == 0) {
                return;
            }
            const bool top_labels = key_label_position == "top";
            const float label_top = top_labels ? (label_field_layout.top + 8.0f)
                                               : (label_field_layout.bottom - 30.0f);
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const D2D1_COLOR_F label_color =
                ng_color("key_label",D2D1::ColorF(0xF7FAFD, (keycap_key_labels ? 0.62f : 0.38f) * visual_opacity));
            d2d_->text_brush->SetColor(label_color);
            const std::size_t label_count =
                std::min(data.gameplay.key_label_count, static_cast<std::size_t>(label_field_layout.lane_count));
            IDWriteTextFormat* label_format = ng_font("key_label",scene_hud_format.Get());
            const auto& key_faces = ghost ? native_ghost_key_faces : native_key_faces;
            for (std::size_t lane = 0; lane < label_count && lane < data.gameplay.key_labels.size(); ++lane) {
                if (lane_hidden(lane)) continue;
                const std::string& label = data.gameplay.key_labels[lane];
                if (label.empty()) {
                    continue;
                }
                const int lane_index = static_cast<int>(lane);
                D2D1_RECT_F base_rect = D2D1::RectF(gameplay_lane_left(label_field_layout, lane_index) + 2.0f,
                                label_top,
                                gameplay_lane_right(label_field_layout, lane_index) - 2.0f,
                                label_top + 22.0f);
                const D2D1_RECT_F& face = key_faces[lane];
                const bool on_keycap = keycap_key_labels && face.right > face.left && face.bottom > face.top;
                if (on_keycap) {
                    const float travel = ghost ? native_ghost_key_travel_[lane] : native_key_travel_[lane];
                    const float face_height = face.bottom - face.top;
                    const float center = face.top + face_height * kLumaKeyLegendCenter;
                    base_rect = D2D1::RectF(face.left + 2.0f, center - 11.0f, face.right - 2.0f, center + 11.0f);
                    // The window's LED strips run through its middle; a plate cut
                    // from the window colour keeps them flanking the legend instead
                    // of striking through it.
                    if (auto* plate_brush = d2d_->note_fill_brush.Get()) {
                        const float font_size = label_format->GetFontSize();
                        const float face_center = (face.left + face.right) * 0.5f;
                        const float half_width = std::min((face.right - face.left) * 0.36f,
                            static_cast<float>(label.size()) * font_size * 0.34f + 7.0f);
                        const uint32_t lane_rgb = lane < data.gameplay.lane_color_count ? data.gameplay.lane_colors[lane]
                            : (gameplay_lane_uses_white_note(lane_index + 1) ? 0xF6F8FF : 0x4F80FF);
                        const auto saved_plate = plate_brush->GetColor();
                        plate_brush->SetColor(color_from_rgb(blend_rgb(lane_rgb, 0x06111B, 0.90f), 0.96f * visual_opacity));
                        // Square, not rounded: the label pass is aliased, and rounded
                        // rectangles re-tessellated per lane cost visible render time at 16K.
                        ctx->FillRectangle(D2D1::RectF(face_center - half_width,
                            face.top + face_height * (207.0f / 256.0f), face_center + half_width,
                            face.top + face_height * (235.0f / 256.0f)), plate_brush);
                        plate_brush->SetColor(saved_plate);
                    }
                    // A held key reads as lit, matching the pressed face crossfade.
                    auto lit = label_color;
                    lit.a = std::min(1.0f, label_color.a + 0.36f * std::clamp(travel, 0.0f, 1.0f) * visual_opacity);
                    d2d_->text_brush->SetColor(lit);
                }
                const D2D1_RECT_F label_rect = ng_rect("key_label", base_rect);
                draw_readable_text_aligned(to_wide(label),
                                          label_format,
                                          on_keycap ? readable_text_rect(label_rect, label_format) : label_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            d2d_->text_brush->SetColor(saved_text_color);
        };

        auto draw_native_digital_key = [&](const GameplayFieldLayout& key_field_layout,
                                           float key_hit_line_y,
                                           int lane,
                                           bool pressed,
                                           float activity,
                                           bool ghost = false) {
            if (use_imported_metrics || !d2d_->note_fill_brush) return;
            const auto lane_index = static_cast<std::size_t>(lane);
            const auto bounds = native_key_bounds(gameplay_lane_left(key_field_layout, lane),
                gameplay_lane_right(key_field_layout, lane), key_field_layout.top, key_field_layout.bottom,
                gameplay_osu_gear_top(key_field_layout, key_hit_line_y, note_height_scale),native_style);
            const float height = bounds.bottom - bounds.top;
            if (bounds.right <= bounds.left || height <= 0.0f) return;
            auto& travel = ghost ? native_ghost_key_travel_[lane_index] : native_key_travel_[lane_index];
            travel = advance_native_key_travel(travel, pressed, native_key_delta_seconds, ng_motion("press_response"),ng_motion("release_response"));
            const float pulse = gameplay_interpolated_activity(activity, data.gameplay.activity_publish_time_ns, render_now_ns);
            const float light = std::clamp((travel * ng_motion("pressed_light") + pulse * ng_motion("hit_light")) * key_pulse_brightness, 0.0f, 1.0f);
            const float depth = std::min(std::max(height * 0.065f,2.0f),ng_motion("press_depth")) * travel;
            const uint32_t color = lane_index < data.gameplay.lane_color_count ? data.gameplay.lane_colors[lane_index]
                : (gameplay_lane_uses_white_note(lane + 1) ? 0xF6F8FF : 0x4F80FF);
            auto* fill = d2d_->note_fill_brush.Get();
            const auto saved = fill->GetColor();
            // The stationary well makes the moving face read as a physical instrument key.
            fill->SetColor(ng_color("key_well",color_from_rgb(0x03080F, visual_opacity)));
            ctx->FillRectangle(D2D1::RectF(bounds.left, bounds.top, bounds.right, bounds.bottom), fill);
            const auto face = D2D1::RectF(bounds.left, bounds.top + depth, bounds.right, bounds.bottom - std::min(height-1.0f,ng_metric("key_face_gap")) + depth);
            (ghost ? native_ghost_key_faces : native_key_faces)[lane_index] = face;
            if (auto* idle = d2d_->lane_key_idle_bitmaps[lane_index].Get()) {
                ctx->DrawBitmap(idle, face, visual_opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                if (light > 0.005f && d2d_->lane_key_pressed_bitmaps[lane_index])
                    ctx->DrawBitmap(d2d_->lane_key_pressed_bitmaps[lane_index].Get(), face,
                                    light * visual_opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            } else {
                fill->SetColor(color_from_rgb(blend_rgb(color, 0x142333, 0.55f), visual_opacity));
                ctx->FillRectangle(face, fill);
            }
            // LED meter stays below the reading area, with bounded work even at 16K.
            const float pad = std::min(10.0f, (bounds.right - bounds.left) * 0.16f);
            if (light > 0.01f) {
                fill->SetColor(color_from_rgb(blend_rgb(color, ng_rgb("key_led",0xB9FFF0), 0.35f), light * visual_opacity*ng_alpha("key_led")));
                ctx->FillRectangle(D2D1::RectF(bounds.left + pad, bounds.bottom - 3.0f,
                    bounds.left + pad + (bounds.right - bounds.left - 2.0f * pad) * light, bounds.bottom - 1.0f), fill);
            }
            fill->SetColor(saved);
        };

        // LR2 gear art is a bottom panel with the receptor art baked in; TenRiff
        // skins declare gear as a full-playfield overlay with separate key slots.
        const bool tenriff_gear_overlay =
            gameplay_note_sprite_cache_.skin_source == "tenriff";
        auto draw_imported_gear_overlay = [&](const GameplayFieldLayout& gear_field_layout,
                                              float gear_hit_line_y) {
            ID2D1Bitmap* bitmap = d2d_->gameplay_gear_overlay_bitmap.Get();
            if (!bitmap) {
                return false;
            }
            const D2D1_RECT_F* source_rect =
                bitmap_source_rect_or_null(d2d_->gameplay_gear_overlay_source_rect);
            if (tenriff_gear_overlay) {
                ctx->DrawBitmap(bitmap,
                                D2D1::RectF(gear_field_layout.left,
                                            gear_field_layout.top,
                                            gear_field_layout.right,
                                            gear_field_layout.bottom),
                                visual_opacity,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                source_rect);
                return true;
            }
            // An LR2 gear panel is authored at a fixed offset from its lanes and the
            // judgement line. Replaying that offset against this playfield keeps the
            // frame registered with the lanes instead of floating it under them.
            if (gameplay_note_sprite_cache_.has_gear_placement) {
                const float unit = gear_field_layout.width;
                const float gear_left =
                    gear_field_layout.left + gameplay_note_sprite_cache_.gear_placement_offset_x * unit;
                const float gear_top =
                    gear_hit_line_y + gameplay_note_sprite_cache_.gear_placement_offset_y * unit;
                ctx->DrawBitmap(bitmap,
                                D2D1::RectF(gear_left,
                                            gear_top,
                                            gear_left + gameplay_note_sprite_cache_.gear_placement_width * unit,
                                            gear_top + gameplay_note_sprite_cache_.gear_placement_height * unit),
                                visual_opacity,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                source_rect);
                return true;
            }
            const D2D1_SIZE_F bitmap_size = bitmap->GetSize();
            const float source_width = source_rect
                ? source_rect->right - source_rect->left
                : bitmap_size.width;
            const float source_height = source_rect
                ? source_rect->bottom - source_rect->top
                : bitmap_size.height;
            const GameplayGearRect bounds{
                gear_field_layout.left + 2.0f,
                gameplay_osu_gear_top(gear_field_layout,
                                      gear_hit_line_y,
                                      note_height_scale),
                gear_field_layout.right - 2.0f,
                gear_field_layout.bottom - 2.0f,
            };
            const GameplayGearRect fitted =
                fit_gameplay_gear_rect(bounds,
                                       source_width,
                                       source_height,
                                       gameplay_gear_scale_multiplier(note_width_scale));
            ctx->PushAxisAlignedClip(D2D1::RectF(bounds.left,
                                                 bounds.top,
                                                 bounds.right,
                                                 bounds.bottom),
                                     D2D1_ANTIALIAS_MODE_ALIASED);
            ctx->DrawBitmap(bitmap,
                            D2D1::RectF(fitted.left,
                                        fitted.top,
                                        fitted.right,
                                        fitted.bottom),
                            visual_opacity,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            source_rect);
            ctx->PopAxisAlignedClip();
            return true;
        };

        const auto draw_key_backdrop = [&](const GameplayFieldLayout& layout, const auto& pressed, std::size_t pressed_count) {
            auto* brush = d2d_->note_fill_brush.Get();
            if (!brush || !data.gameplay.key_backdrop_enabled) return;
            const float bottom = layout.bottom - 2.0f;
            const float top = gameplay_key_backdrop_top(layout.top + 2.0f, bottom, data.gameplay.key_backdrop_height);
            if (top >= bottom) return;
            const auto saved_color = brush->GetColor();
            const auto saved_opacity = brush->GetOpacity();
            brush->SetOpacity(1.0f);
            for (int lane = 0; lane < layout.lane_count; ++lane) {
                if (lane_hidden(lane)) continue;
                const auto index = static_cast<std::size_t>(lane);
                const float alpha = gameplay_key_backdrop_alpha(data.gameplay.key_backdrop_enabled,
                    index < pressed_count && pressed[index] != 0, data.gameplay.key_backdrop_opacity) * visual_opacity;
                if (alpha <= 0) continue;
                const auto rgb = index < data.gameplay.lane_color_count ? data.gameplay.lane_colors[index] : 0xFFFFFFu;
                const auto light_rgb = gameplay_key_backdrop_color(rgb, data.gameplay.key_backdrop_brightness);
                const float lane_left = gameplay_lane_left(layout, lane);
                const D2D1_RECT_F lane_rect = D2D1::RectF(lane_left + 1, top,
                    lane_left + gameplay_lane_width(layout, lane) - 1, bottom);
                // The configured opacity is the light's strength at the keys; it
                // thins toward the top like a beam instead of a flat slab.
                auto& light = d2d_->key_backdrop_brushes[index];
                if (!light || d2d_->key_backdrop_brush_rgb[index] != light_rgb) {
                    const D2D1_GRADIENT_STOP stops[] = {
                        {0.00f, color_from_rgb(light_rgb, 0.10f)},
                        {0.60f, color_from_rgb(light_rgb, 0.55f)},
                        {1.00f, color_from_rgb(light_rgb, 1.00f)}};
                    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
                    light.Reset();
                    if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 3, collection.GetAddressOf())))
                        static_cast<void>(ctx->CreateLinearGradientBrush(
                            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 1)),
                            collection.Get(), light.GetAddressOf()));
                    d2d_->key_backdrop_brush_rgb[index] = light_rgb;
                }
                if (light) {
                    light->SetStartPoint(D2D1::Point2F(0.0f, top));
                    light->SetEndPoint(D2D1::Point2F(0.0f, bottom));
                    light->SetOpacity(alpha);
                    ctx->FillRectangle(lane_rect, light.Get());
                } else {
                    brush->SetColor(color_from_rgb(light_rgb, alpha));
                    ctx->FillRectangle(lane_rect, brush);
                }
            }
            brush->SetColor(saved_color);
            brush->SetOpacity(saved_opacity);
        };

        const auto draw_measure_lines = [&](const GameplayFieldLayout& field, const GameplayJudgementField& alignment) {
            if (!d2d_->lane_divider_brush) return;
            const auto opacity = d2d_->lane_divider_brush->GetOpacity();
            d2d_->lane_divider_brush->SetOpacity(0.44f);
            for (std::size_t i = 0; i < std::min(data.gameplay.measure_line_count,
                                               data.gameplay.measure_line_positions.size()); ++i) {
                const float y = gameplay_field_y(field.top, field.bottom - field.top,
                    visual_to_y(data.gameplay.measure_line_positions[i])) +
                    alignment.measure_offset + 0.5f;
                // Follow the displayed note position in either line type. Attach the top
                // of this one-pixel stroke to the note's bottom edge, including
                // the configured/imported note height, without shifting time.
                ctx->DrawLine(D2D1::Point2F(field.left, y), D2D1::Point2F(field.right, y),
                    d2d_->lane_divider_brush.Get(), 1.0f);
            }
            d2d_->lane_divider_brush->SetOpacity(opacity);
        };
        const D2D1_ANTIALIAS_MODE saved_antialias = ctx->GetAntialiasMode();
        ctx->PushAxisAlignedClip(field_clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
        const bool player_has_gear_overlay =
            draw_imported_gear_overlay(field_layout, hit_line_y);
        draw_key_backdrop(field_layout, data.gameplay.lane_pressed, data.gameplay.lane_pressed_count);
        draw_measure_lines(field_layout, note_alignment);
        if (use_imported_metrics) {
            draw_key_labels(field_layout);
        }
        ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (int lane = 0; lane < lane_count; ++lane) {
            if (lane_hidden(lane)) continue;
            const std::size_t lane_index = static_cast<std::size_t>(lane);
            if (player_has_gear_overlay && !tenriff_gear_overlay) {
                continue;
            }
            const bool lane_is_pressed =
                lane_index < data.gameplay.lane_pressed_count &&
                data.gameplay.lane_pressed[lane_index] != 0;
            const float lane_activity = lane_index < data.gameplay.lane_activity_count
                ? data.gameplay.lane_activity[lane_index]
                : 0.0f;
            ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
            if (should_use_imported_pressed_key(lane_activity) &&
                d2d_->lane_key_pressed_bitmaps[lane_index]) {
                key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
            }
            if (!use_imported_metrics) {
                draw_native_digital_key(field_layout,
                                        hit_line_y,
                                        lane,
                                        lane_is_pressed,
                                        lane_activity);
                continue;
            }
            if (!key_bitmap) {
                continue;
            }
            const D2D1_SIZE_F bitmap_size = key_bitmap->GetSize();
            const float lane_center = gameplay_lane_center(field_layout, lane);
            const float lane_width = gameplay_lane_width(field_layout, lane);
            const float note_width = gameplay_note_width(field_layout, lane);
            const D2D1_RECT_F receptor_rect =
                gameplay_key_bitmap_rect(field_layout,
                                         hit_line_y,
                                         note_height_scale,
                                         lane_center,
                                         lane_width,
                                         note_width,
                                         bitmap_size,
                                         gameplay_note_sprite_cache_.use_full_lane_receptor_layout);
            const D2D1_RECT_F* key_source_rect = nullptr;
            if (lane_index < d2d_->lane_key_pressed_source_rects.size() &&
                key_bitmap == d2d_->lane_key_pressed_bitmaps[lane_index].Get()) {
                key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_pressed_source_rects[lane_index]);
            } else if (lane_index < d2d_->lane_key_idle_source_rects.size()) {
                key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_idle_source_rects[lane_index]);
            }
            draw_gameplay_sprite(ctx, key_bitmap, receptor_rect, visual_opacity,
                                 key_source_rect,
                                 gameplay_lane_sprite_rotation(
                                     gameplay_note_sprite_cache_.key_rotations,
                                     gameplay_note_sprite_cache_.key_rotation_count,
                                     lane_index));
        }
        if (!use_imported_metrics) {
            draw_key_labels(field_layout);
        }
        // Queue in note-array order. Every procedural primitive below flushes
        // first, preserving translucent LN body/tail/head overlap across lanes.
        auto flush_note_sprites = [&]() {
            if (gpu_sprites_active && !gpu_sprites.flush(ctx)) gpu_sprite_frame_failure = E_FAIL;
        };
        auto can_batch_note_field = [&](const auto& notes, std::size_t count) {
            if (!gpu_sprites_active || count < 8) return false;
            for (std::size_t i = 0; i < std::min(count, notes.size()); ++i) {
                const auto& note = notes[i];
                const auto lane = static_cast<std::size_t>(std::clamp(note.lane, 1, lane_count) - 1);
                // Procedural bodies/mines break batches into tiny runs. Keep
                // their complete field on the original path, including heads.
                if (note.mine || (note.hold && (data.gameplay.hold_tail_taper_enabled ||
                    !d2d_->lane_note_hold_body_bitmaps[lane]))) return false;
            }
            return true;
        };
        bool batch_note_field = can_batch_note_field(data.gameplay.notes, data.gameplay.note_count);
        auto draw_note_sprite = [&](ID2D1DeviceContext* target, ID2D1Bitmap* bitmap,
                                    const D2D1_RECT_F& dest, float opacity,
                                    const D2D1_RECT_F* source, float rotation) {
            if (batch_note_field && rotation == 0.0f &&
                gpu_sprites.enqueue(bitmap, dest, opacity, source)) return;
            flush_note_sprites();
            draw_gameplay_sprite(target, bitmap, dest, opacity, source, rotation);
        };
        for (std::size_t note_index = 0; note_index < data.gameplay.note_count; ++note_index) {
            const auto& note = data.gameplay.notes[note_index];
            if (!should_render_gameplay_note(
                    note.start_sample,
                    note.tail_sample,
                    note.hold,
                    note.head_visible,
                    note.pending,
                    data.gameplay.current_sample,
                    display_sample,
                    hold_handoff_grace_samples, note.released)) {
                continue;
            }
            const bool render_head =
                should_render_gameplay_note_head(note.start_sample, note.head_visible, display_sample, note.released);
            const int lane = std::clamp(note.lane, 1, lane_count);
            if (lane_hidden(static_cast<std::size_t>(lane - 1))) continue;
            const std::size_t lane_index = static_cast<std::size_t>(lane - 1);
            const float lane_center = gameplay_lane_center(field_layout, lane - 1);
            const float note_width = gameplay_note_width(field_layout, lane - 1);
            const float x0 = lane_center - note_width * 0.5f;
            const float x1 = lane_center + note_width * 0.5f;
            const double render_visual_position =
                gameplay_note_anchors_to_judgement_line(note.hold, note.head_visible)
                    ? display_visual_position
                    : note.visual_position;
            const float note_alignment_offset = note.hold
                ? note_alignment.hold_offsets[lane_index] : note_alignment.note_offsets[lane_index];
            const float y = gameplay_field_y(field_top, field_height, visual_to_y(render_visual_position)) + note_alignment_offset;
            const float tail_y =
                gameplay_field_y(field_top, field_height, visual_to_y(note.tail_visual_position)) + note_alignment_offset;
            const float head_half_h = gameplay_note_head_half_height(note_height_scale);
            const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
            uint32_t lane_color = 0xF6F8FF;
            if (lane_index < data.gameplay.lane_color_count) {
                lane_color = data.gameplay.lane_colors[lane_index];
            } else if (!gameplay_lane_uses_white_note(lane)) {
                lane_color = 0x4F80FF;
            }
            ID2D1SolidColorBrush* note_fill = d2d_->note_fill_brush.Get();
            ID2D1SolidColorBrush* note_border = d2d_->note_border_brush.Get();
            ID2D1SolidColorBrush* note_hold_fill = d2d_->note_hold_brush.Get();
            if (note_fill) {
                note_fill->SetColor(gameplay_note_fill_color(lane_color, visual_opacity));
            }
            if (note_border) {
                note_border->SetColor(gameplay_note_border_color(
                    lane_color, note_outline_opacity, note_polygon_geometry != nullptr));
            }
            if (note_hold_fill) {
                note_hold_fill->SetColor(simple_hold_body
                    ? color_from_rgb(lane_color, native_hold_body_opacity)
                    : gameplay_note_hold_color(lane_color, native_hold_body_opacity));
            }
            // Flat hold caps match the body; ordinary tap materials stay unchanged.
            ID2D1LinearGradientBrush* note_material = (note_shape == "simple_rect" ||
                (note.hold && simple_hold_body)) ? nullptr
                : d2d_->lane_native_note_brushes[lane_index].Get();
            ID2D1LinearGradientBrush* hold_material = simple_hold_body ? nullptr
                : d2d_->lane_native_hold_brushes[lane_index].Get();
            ID2D1Bitmap* note_head_bitmap = d2d_->lane_note_head_bitmaps[lane_index].Get();
            ID2D1Bitmap* note_hold_head_bitmap =
                d2d_->lane_note_hold_head_bitmaps[lane_index].Get();
            ID2D1Bitmap* note_hold_body_bitmap =
                d2d_->lane_note_hold_body_bitmaps[lane_index].Get();
            ID2D1Bitmap* note_hold_tail_bitmap =
                d2d_->lane_note_tail_bitmaps[lane_index].Get();
            const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
            const bool using_hold_head_bitmap = note.hold && note_hold_head_bitmap;
            ID2D1Bitmap* head_bitmap = using_hold_head_bitmap ? note_hold_head_bitmap : note_head_bitmap;
            const D2D1_RECT_F* head_source_rect =
                using_hold_head_bitmap
                    ? bitmap_source_rect_or_null(d2d_->lane_note_hold_head_source_rects[lane_index])
                    : bitmap_source_rect_or_null(d2d_->lane_note_head_source_rects[lane_index]);
            const D2D1_RECT_F head_visual_rect =
                head_bitmap
                    ? gameplay_note_bitmap_dest_rect(note_rect,
                                                     head_bitmap,
                                                     head_source_rect,
                                                     note_shape,
                                                     note_image_aspect)
                    : note_rect;
            const D2D1_RECT_F tail_rect =
                D2D1::RectF(x0, tail_y - tail_half_h, x1, tail_y + tail_half_h);
            const D2D1_RECT_F* tail_source_rect =
                bitmap_source_rect_or_null(d2d_->lane_note_tail_source_rects[lane_index]);
            const D2D1_RECT_F tail_visual_rect =
                note_hold_tail_bitmap
                    ? gameplay_note_bitmap_dest_rect(tail_rect,
                                                     note_hold_tail_bitmap,
                                                     tail_source_rect,
                                                     note_shape,
                                                     note_image_aspect)
                    : tail_rect;

            if (note.mine) {
                flush_note_sprites();
                const float mine_half_h = std::max(7.0f, head_half_h * 1.15f);
                const D2D1_RECT_F mine_rect = D2D1::RectF(x0, y - mine_half_h, x1, y + mine_half_h);
                if (note_fill) {
                    note_fill->SetColor(D2D1::ColorF(0.96f, 0.08f, 0.15f, visual_opacity));
                    ctx->FillRectangle(mine_rect, note_fill);
                }
                if (note_border) {
                    note_border->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.20f, visual_opacity));
                    ctx->DrawRectangle(mine_rect, note_border, 2.0f);
                    ctx->DrawLine(D2D1::Point2F(x0, y - mine_half_h),
                                  D2D1::Point2F(x1, y + mine_half_h), note_border, 2.0f);
                    ctx->DrawLine(D2D1::Point2F(x1, y - mine_half_h),
                                  D2D1::Point2F(x0, y + mine_half_h), note_border, 2.0f);
                }
                continue;
            }

            if (note.hold && note_hold_fill) {
                const auto body_geometry = compute_gameplay_hold_body_geometry(
                    lane_center,
                    head_visual_rect.left,
                    head_visual_rect.right,
                    head_visual_rect.top,
                    y,
                    tail_visual_rect.bottom,
                    tail_y,
                    render_head,
                    data.gameplay.show_hold_tail,
                    hold_body_width_scale);
                const D2D1_RECT_F hold_body =
                    D2D1::RectF(body_geometry.left,
                                body_geometry.top,
                                body_geometry.right,
                                body_geometry.bottom);
                if (hold_body.bottom > hold_body.top) {
                    if (note_hold_body_bitmap && !data.gameplay.hold_tail_taper_enabled) {
                        const D2D1_RECT_F* hold_body_source_rect =
                            bitmap_source_rect_or_null(d2d_->lane_note_hold_body_source_rects[lane_index]);
                        draw_note_sprite(ctx, note_hold_body_bitmap, hold_body, hold_body_opacity,
                                             hold_body_source_rect, 0.0f);
                    } else {
                        flush_note_sprites();
                        ID2D1Brush* hold_body_brush = configure_gameplay_material_brush(
                            hold_material, note_hold_fill, hold_body, native_hold_body_opacity, true);
                        draw_gameplay_hold_body(ctx,
                                                d2d_->d2d_factory.Get(),
                                                hold_body,
                                                lane_center,
                                                y,
                                                tail_y,
                                                (body_geometry.right - body_geometry.left) * 0.5f,
                                                data.gameplay.hold_tail_taper_enabled,
                                                hold_body_brush);
                    }
                }
            }

            if (note.hold && data.gameplay.show_hold_tail) {
                if (note_hold_tail_bitmap) {
                    draw_note_sprite(ctx,
                                         note_hold_tail_bitmap,
                                         tail_visual_rect,
                                         visual_opacity,
                                         tail_source_rect,
                                         gameplay_lane_sprite_rotation(
                                             gameplay_note_sprite_cache_.note_rotations,
                                             gameplay_note_sprite_cache_.note_rotation_count,
                                             lane_index));
                } else if (note_fill) {
                    flush_note_sprites();
                    ID2D1Brush* tail_fill = configure_gameplay_material_brush(
                        note_material, note_fill, tail_rect, visual_opacity, false);
                    draw_note_primitive(ctx,
                                        tail_rect,
                                        tail_fill,
                                        note_border,
                                        1.5f,
                                        note_shape,
                                        note_border_enabled && note_polygon_geometry != nullptr,
                                        note_polygon_geometry);
                }
            }

            if (render_head) {
                if (head_bitmap) {
                    draw_note_sprite(ctx, head_bitmap, head_visual_rect, visual_opacity,
                                         head_source_rect,
                                         gameplay_lane_sprite_rotation(
                                             gameplay_note_sprite_cache_.note_rotations,
                                             gameplay_note_sprite_cache_.note_rotation_count,
                                             lane_index));
                } else {
                    if (note_fill) {
                        flush_note_sprites();
                        ID2D1Brush* head_fill = configure_gameplay_material_brush(
                            note_material, note_fill, note_rect, visual_opacity, false);
                        draw_note_primitive(ctx, note_rect, head_fill, note_border,
                                            note_polygon_geometry ? 1.5f : 0.85f,
                                            note_shape, note_border_enabled, note_polygon_geometry);
                    }
                }
            }
        }
        flush_note_sprites();
        draw_note_fog(field_layout, hit_line_y);
        ctx->PopAxisAlignedClip();
        ctx->SetAntialiasMode(saved_antialias);

        const float combo_anchor_top_safe = 74.0f;
        const float combo_anchor_bottom_safe = 82.0f;
        // Live play and the replay share one skin/HUD path. A second hand-written
        // ghost path used the combo anchor and bypassed judgement font/rect edits.
        auto draw_feedback_overlay = [&](const GameplayFieldLayout& feedback_field,
                                         bool has_feedback,
                                         const std::string& feedback,
                                         double feedback_delta_ms,
                                         bool has_non_pg_feedback,
                                         bool has_timing_feedback,
                                         const std::wstring& feedback_text,
                                         const std::wstring& feedback_timing_text,
                                         const std::array<double, kGameplayTimingHistoryMaxEntries>& timing_history,
                                         std::size_t timing_history_count,
                                         const std::array<bool, kGameplayTimingHistoryMaxEntries>& maximums,
                                         double maximum_ms,
                                         const GameplayTextPopAnimation& feedback_animation,
                                         int64_t feedback_started_ns) {
            if (studio) { draw_studio_feedback(feedback_field, has_feedback, feedback, feedback_delta_ms,
                                               has_non_pg_feedback, has_timing_feedback, feedback_text,
                                               feedback_timing_text, feedback_animation, timing_history, timing_history_count, maximums, maximum_ms); return; }
            const bool show_feedback_overlay = has_feedback && !feedback_text.empty();
            const bool show_timing_feedback = data.gameplay.show_timing_feedback && has_timing_feedback &&
                !feedback_timing_text.empty();
            const float combo_anchor_y =
                gameplay_combo_anchor_y(feedback_field, data.gameplay.judgement_position, 74.0f, 82.0f);
            const float timing_anchor_y = gameplay_combo_anchor_y(feedback_field, 0.24, 74.0f, 82.0f);
            const bool show_timing_bar = data.gameplay.show_timing_bar &&
                (data.gameplay.timing_bar_always_visible || timing_history_count > 0);
            if ((show_feedback_overlay || show_timing_feedback || show_timing_bar) && d2d_->text_brush) {
                if (show_feedback_overlay && scene_header_format) {
                    D2D1_RECT_F feedback_rect =
                        gameplay_centered_overlay_rect(feedback_field, combo_anchor_y - 34.0f, 48.0f, -24.0f);
                    feedback_rect.left += static_cast<float>(data.gameplay.judgement_offset_x);
                    feedback_rect.right += static_cast<float>(data.gameplay.judgement_offset_x);
                    feedback_rect=readable_text_rect(ng_rect("judgement",feedback_rect),
                                                      ng_font("judgement",scene_header_format.Get()));
                    const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
                    const float saved_text_opacity = d2d_->text_brush->GetOpacity();
                    D2D1_MATRIX_3X2_F saved_feedback_transform{};
                    ctx->GetTransform(&saved_feedback_transform);
                    const D2D1_POINT_2F feedback_animation_center = D2D1::Point2F(
                        (feedback_rect.left + feedback_rect.right) * 0.5f,
                        (feedback_rect.top + feedback_rect.bottom) * 0.5f + 12.0f);
                    const D2D1_MATRIX_3X2_F feedback_animation_transform =
                        D2D1::Matrix3x2F::Scale(feedback_animation.scale * hud_font_scale(data.gameplay.judgement_font_scale),
                                                feedback_animation.scale * hud_font_scale(data.gameplay.judgement_font_scale),
                                                feedback_animation_center) *
                        D2D1::Matrix3x2F::Translation(0.0f, feedback_animation.offset_y) *
                        saved_feedback_transform;
                    ctx->SetTransform(feedback_animation_transform);
                    d2d_->text_brush->SetOpacity(saved_text_opacity * feedback_animation.opacity);

                    d2d_->text_brush->SetColor(ng_judge_color(feedback));
                    draw_readable_text_aligned(feedback_text,
                                              ng_font("judgement",scene_header_format.Get()),
                                              feedback_rect,
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                    if (feedback == "PG") {
                        const double age = animation_age_ms(feedback_started_ns, 320.0);
                        const float life = static_cast<float>(std::clamp(1.0 - age / 320.0, 0.0, 1.0));
                        constexpr std::array<uint32_t, 6> rainbow{0xFF9AAB, 0xFFC68A, 0xFFF29A, 0xA5E9BA, 0x9EDCFF, 0xC5ACFF};
                        for (std::size_t i = 0; i < rainbow.size() && life > 0.0f; ++i) {
                            const float angle = static_cast<float>(i) * 1.04719755f + static_cast<float>(age) * 0.004f;
                            const float x = feedback_animation_center.x + std::cos(angle) * (105.0f + (1.0f - life) * 24.0f);
                            const float y = feedback_animation_center.y + std::sin(angle) * 29.0f;
                            const float radius = (3.0f + 3.0f * std::abs(std::sin(static_cast<float>(age) * 0.04f + angle))) * life;
                            d2d_->text_brush->SetColor(D2D1::ColorF(rainbow[i], life));
                            ctx->DrawLine(D2D1::Point2F(x - radius, y), D2D1::Point2F(x + radius, y), d2d_->text_brush.Get(), 2.0f);
                            ctx->DrawLine(D2D1::Point2F(x, y - radius), D2D1::Point2F(x, y + radius), d2d_->text_brush.Get(), 2.0f);
                        }
                    }
                    d2d_->text_brush->SetColor(saved_text_color);
                    d2d_->text_brush->SetOpacity(saved_text_opacity);
                    ctx->SetTransform(saved_feedback_transform);
                }

                // Text gets its own position outside the judgement pop transform;
                // changing the bar or judgement font size cannot drag it around.
                if (show_timing_feedback && scene_body_format) {
                    // Use the stable field anchor, never the judgement's XY or rect.
                    const auto base_text = gameplay_centered_overlay_rect(feedback_field, timing_anchor_y - 5.0f, 13.0f, -24.0f);
                    auto text_rect = ng_rect("timing_label", base_text);
                    text_rect.left += static_cast<float>(data.gameplay.timing_text_offset_x);
                    text_rect.right += static_cast<float>(data.gameplay.timing_text_offset_x);
                    text_rect.top += static_cast<float>(data.gameplay.timing_text_offset_y);
                    text_rect.bottom += static_cast<float>(data.gameplay.timing_text_offset_y);
                    D2D1_MATRIX_3X2_F saved_timing_text_transform{};
                    ctx->GetTransform(&saved_timing_text_transform);
                    const float text_scale = hud_font_scale(data.gameplay.timing_text_scale);
                    ctx->SetTransform(D2D1::Matrix3x2F::Scale(text_scale, text_scale,
                        D2D1::Point2F((text_rect.left + text_rect.right) * 0.5f,
                                     (text_rect.top + text_rect.bottom) * 0.5f)) * saved_timing_text_transform);
                    const auto color = d2d_->text_brush->GetColor();
                    const auto opacity = d2d_->text_brush->GetOpacity();
                    d2d_->text_brush->SetOpacity(opacity);
                    d2d_->text_brush->SetColor(feedback_delta_ms < 0.0
                        ? ng_color("timing_fast", D2D1::ColorF(0x5DA9FF, 0.98f))
                        : ng_color("timing_slow", D2D1::ColorF(0xFF5A6B, 0.98f)));
                    draw_readable_text_aligned(feedback_timing_text, ng_font("timing", scene_body_format.Get()),
                                              text_rect, d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                    d2d_->text_brush->SetColor(color);
                    d2d_->text_brush->SetOpacity(opacity);
                    ctx->SetTransform(saved_timing_text_transform);
                }
                if (show_timing_bar) {
                    draw_timing_indicator(feedback_field.left,
                                          feedback_field.right,
                                          timing_anchor_y,
                                          timing_history,
                                          timing_history_count,
                                          maximums,
                                          maximum_ms);
                }
            }

        };
        draw_feedback_overlay(field_layout, data.gameplay.has_feedback, data.gameplay.feedback,
                              data.gameplay.timing_feedback_delta_ms, data.gameplay.has_non_pg_feedback,
                              data.gameplay.has_timing_feedback, scene_hud_cache.feedback_text,
                              scene_hud_cache.feedback_timing_text, data.gameplay.timing_history_delta_ms,
                              data.gameplay.timing_history_count, data.gameplay.timing_history_maximums,
                              data.gameplay.timing_maximum_window_ms, judgement_text_animation,
                              scene_hud_cache.judgement_animation_started_ns);
        draw_field_judgement_counts(field_layout,
            {data.gameplay.pg, data.gameplay.gr, data.gameplay.gd, data.gameplay.bd, data.gameplay.pr});

        if (data.gameplay.combo > 0) {
            draw_combo_overlay(field_layout,
                               scene_hud_cache.combo_value_text,
                               scene_hud_cache.combo_label_text,
                               combo_anchor_top_safe,
                               combo_anchor_bottom_safe,
                               60.0f,
                               combo_text_animation);
        }

        auto battle_summary_layout = [&](const GameplayFieldLayout& summary_field, bool ghost = false) {
            const float anchor = gameplay_combo_anchor_y(summary_field, data.gameplay.judgement_position, 74.0f, 82.0f);
            auto feedback = readable_text_rect(ng_rect("judgement", gameplay_centered_overlay_rect(
                summary_field, anchor - 34.0f, 48.0f)), ng_font("judgement", scene_header_format.Get()));
            const float feedback_center = (feedback.top + feedback.bottom) * 0.5f + 12.0f;
            const float feedback_scale = hud_font_scale(data.gameplay.judgement_font_scale) * 1.22f;
            float feedback_top = feedback_center + (feedback.top - feedback_center) * feedback_scale - 8.0f;
            float feedback_bottom = feedback_center + (feedback.bottom - feedback_center) * feedback_scale + 54.0f;
            auto combo = readable_text_rect(ng_rect("combo", gameplay_combo_overlay_rect(summary_field,
                combo_position, 30.0f, 74.0f, 82.0f, 60.0f, 8.0f, 18.0f)), ng_font("combo", scene_gameplay_combo_format.Get()));
            const float combo_center = (combo.top + combo.bottom) * 0.5f;
            const float combo_scale = hud_font_scale(data.gameplay.combo_font_scale) * std::max(1.0f, ng_motion("combo_scale"));
            const float combo_top = combo_center + (combo.top - combo_center) * combo_scale - std::abs(ng_motion("combo_lift"));
            const float combo_bottom = combo_center + (combo.bottom + 18.0f - combo_center) * combo_scale;
            auto summary = compute_gameplay_battle_summary_layout(summary_field.left, summary_field.right,
                summary_field.top, summary_field.bottom, feedback_top, feedback_bottom, combo_top, combo_bottom, ghost);
            // A fitted spectator field can be narrower than the old summary
            // minimum. Keep its card away from the player's field/drag handle.
            summary.left = std::max(summary.left, summary_field.left + 8.0f);
            summary.right = std::min(summary.right, summary_field.right - 8.0f);
            return summary;
        };

        const bool opponents_on_left = field_left - 52.0f >= 1776.0f - field_right;
        const float opponents_left = surface_layout.ghost_visible ? 820.0f :
            opponents_on_left ? 32.0f : field_right + 112.0f;
        const float opponents_right = surface_layout.ghost_visible ? 1100.0f :
            opponents_on_left ? std::min(520.0f, field_left - 20.0f) : std::min(1888.0f, opponents_left + 488.0f);
        GameplayProgressTrackLayout battle_header{};
        const auto player_summary = surface_layout.ghost_visible
            ? battle_summary_layout(surface_layout.player_field) : GameplayBattleSummaryLayout{};
        const auto ghost_summary = surface_layout.ghost_visible
            ? battle_summary_layout(surface_layout.ghost_field, true) : GameplayBattleSummaryLayout{};
        if (surface_layout.ghost_visible) {
            if (!is_skin_preview && player_summary.top < gameplay_field_drag_state_.bottom &&
                player_summary.bottom > gameplay_field_drag_state_.top) {
                gameplay_field_drag_state_.left = std::max(gameplay_field_drag_state_.left,
                    player_summary.right + kGameplayFieldDragHandleGap);
                gameplay_field_drag_state_.right = gameplay_field_drag_state_.left + kGameplayFieldDragHandleWidth;
            }
            battle_header = compute_gameplay_progress_track_layout(
                24.0f, header_safe_right,
                std::min(player_summary.left, surface_layout.player_gauge_left),
                std::max(player_summary.right, surface_layout.player_field.right), true,
                std::min(ghost_summary.left, surface_layout.ghost_field.left),
                std::max(ghost_summary.right, surface_layout.ghost_gauge_left + kGameplayGaugeWidth), 14.0f);
        }
        // At very wide note sizes there is no readable outside column. Use the
        // existing live card's left half instead of shrinking the title to pixels.
        const bool embed_battle_header = surface_layout.ghost_visible &&
            battle_header.right - battle_header.left < 220.0f;
        if (!embed_battle_header) draw_gameplay_header(surface_layout.ghost_visible ? &battle_header : nullptr);
        constexpr float kProgressFieldClearance = 16.0f;
        const float player_handle_right = std::max(
            field_right + kGameplayFieldDragHandleGap + kGameplayFieldDragHandleWidth,
            !is_skin_preview ? gameplay_field_drag_state_.right : field_right);
        const GameplayProgressTrackLayout progress_track_layout = surface_layout.ghost_visible
            ? compute_gameplay_progress_track_layout(24.0f, header_safe_right,
                std::min(player_summary.left, surface_layout.player_gauge_left),
                std::max(player_summary.right, player_handle_right), true,
                std::min(ghost_summary.left, surface_layout.ghost_field.left),
                std::max(ghost_summary.right, surface_layout.ghost_gauge_left + kGameplayGaugeWidth),
                kProgressFieldClearance)
            : compute_gameplay_progress_track_layout(header_left, header_safe_right,
                field_left, player_handle_right, false, 0.0f, 0.0f, kProgressFieldClearance);
        const D2D1_RECT_F progress_track_rect = embed_battle_header
            ? D2D1::RectF(ghost_summary.left + 12.0f, ghost_summary.bottom + 8.0f,
                          ghost_summary.right - 12.0f, ghost_summary.bottom + 20.0f)
            : ng_rect("progress", D2D1::RectF(progress_track_layout.left, 16.0f, progress_track_layout.right, 36.0f));
        if (!studio && !embed_battle_header) draw_gameplay_progress_bar(progress_track_rect);
        if (data.gameplay.peer_visible) {
            const D2D1_RECT_F lead_track =
                D2D1::RectF(opponents_left + 14.0f, 186.0f, opponents_right - 14.0f, 198.0f);
            const float lead_center_x = (lead_track.left + lead_track.right) * 0.5f;
            const float lead_position = static_cast<float>(
                std::clamp(data.gameplay.versus_score_position, 0.0, 1.0));
            const float marker_x =
                lead_track.left + (lead_track.right - lead_track.left) * lead_position;

            if (d2d_->card_brush) {
                d2d_->card_brush->SetOpacity(0.88f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(lead_track, 8.0f, 8.0f),
                                          d2d_->card_brush.Get());
                d2d_->card_brush->SetOpacity(1.0f);
            }
            if (d2d_->accent_brush) {
                const D2D1_COLOR_F saved_color = d2d_->accent_brush->GetColor();
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.32f);
                d2d_->accent_brush->SetColor(ng_color("timing_slow",D2D1::ColorF(0xFF5A6B)));
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(lead_track.left,
                                                 lead_track.top,
                                                 lead_center_x,
                                                 lead_track.bottom),
                                      8.0f,
                                      8.0f),
                    d2d_->accent_brush.Get());
                d2d_->accent_brush->SetColor(D2D1::ColorF(0x89D185));
                ctx->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(lead_center_x,
                                                 lead_track.top,
                                                 lead_track.right,
                                                 lead_track.bottom),
                                      8.0f,
                                      8.0f),
                    d2d_->accent_brush.Get());

                const D2D1_COLOR_F marker_color =
                    lead_position < 0.5f
                        ? ng_color("timing_slow",D2D1::ColorF(0xFF5A6B, 0.98f))
                        : (lead_position > 0.5f
                               ? D2D1::ColorF(0x89D185, 0.98f)
                               : D2D1::ColorF(0x6EE7F2, 0.98f));
                d2d_->accent_brush->SetColor(marker_color);
                d2d_->accent_brush->SetOpacity(1.0f);
                ctx->DrawLine(D2D1::Point2F(marker_x, lead_track.top - 17.0f),
                              D2D1::Point2F(marker_x, lead_track.top + 2.0f),
                              d2d_->accent_brush.Get(),
                              3.0f);
                ctx->DrawLine(D2D1::Point2F(marker_x, lead_track.top + 2.0f),
                              D2D1::Point2F(marker_x - 7.0f, lead_track.top - 6.0f),
                              d2d_->accent_brush.Get(),
                              3.0f);
                ctx->DrawLine(D2D1::Point2F(marker_x, lead_track.top + 2.0f),
                              D2D1::Point2F(marker_x + 7.0f, lead_track.top - 6.0f),
                              d2d_->accent_brush.Get(),
                              3.0f);
                d2d_->accent_brush->SetColor(saved_color);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(lead_track, 8.0f, 8.0f),
                                          d2d_->button_border_brush.Get(),
                                          1.0f);
                ctx->DrawLine(D2D1::Point2F(lead_center_x, lead_track.top - 3.0f),
                              D2D1::Point2F(lead_center_x, lead_track.bottom + 3.0f),
                              d2d_->button_border_brush.Get(),
                              1.4f);
            }
            if (scene_body_format && d2d_->text_brush) {
                const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(ng_color("timing_slow",D2D1::ColorF(0xFF5A6B, 0.96f)));
                draw_readable_text(L"LOSS",
                                  scene_body_format.Get(),
                                  D2D1::RectF(lead_track.left,
                                              lead_track.top - 31.0f,
                                              lead_track.left + 120.0f,
                                              lead_track.top - 3.0f),
                                  d2d_->text_brush.Get());
                d2d_->text_brush->SetColor(D2D1::ColorF(0x89D185, 0.96f));
                draw_readable_text_aligned(L"WIN",
                                          scene_body_format.Get(),
                                          D2D1::RectF(lead_track.right - 120.0f,
                                                      lead_track.top - 31.0f,
                                                      lead_track.right,
                                                      lead_track.top - 3.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 0.94f));
                draw_readable_text_aligned(scene_hud_cache.versus_score_difference_text,
                                          scene_body_format.Get(),
                                          D2D1::RectF(lead_center_x - 120.0f,
                                                      lead_track.top - 31.0f,
                                                      lead_center_x + 120.0f,
                                                      lead_track.top - 3.0f),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->text_brush->SetColor(saved_text_color);
            }

            if (data.gameplay.spectating_peer && d2d_->panel_brush &&
                scene_header_format && d2d_->text_brush) {
                const D2D1_RECT_F spectator_rect =
                    D2D1::RectF(field_left + 80.0f, 216.0f, field_right - 80.0f, 266.0f);
                d2d_->panel_brush->SetOpacity(0.82f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(spectator_rect, 16.0f, 16.0f),
                                          d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
                draw_readable_text_aligned(scene_hud_cache.spectating_text,
                                          scene_header_format.Get(),
                                          spectator_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
        if (data.gameplay.peer_visible) {
#include "MenuWindow_draw_gameplay_multiplayer.inl"
        }
        if (key_pulse_brightness > 0.0f && d2d_->note_fill_brush && data.gameplay.lane_activity_count > 0) {
            const std::size_t count =
                std::min(data.gameplay.lane_activity_count, static_cast<std::size_t>(lane_count));
            for (std::size_t lane = 0; lane < count; ++lane) {
                if (lane_hidden(lane)) continue;
                const float activity = gameplay_interpolated_activity(data.gameplay.lane_activity[lane], data.gameplay.activity_publish_time_ns, render_now_ns);
                const bool sustained_hold =
                    lane < data.gameplay.lane_pressed_count && data.gameplay.lane_pressed[lane] != 0 &&
                    lane_has_active_hold(data.gameplay.notes, data.gameplay.note_count, lane);
                const float hold_wave = 0.55f + 0.08f * static_cast<float>(
                    std::sin(static_cast<double>(render_now_ns) / 180'000'000.0 +
                             static_cast<double>(lane) * 0.7));
                const float pulse = sustained_hold
                                        ? std::max(activity * activity, hold_wave)
                                        : activity * activity;
                if (pulse <= 0.02f) {
                    continue;
                }
                const float lane_center = gameplay_lane_center(field_layout, static_cast<int>(lane));
                const float note_width = gameplay_note_width(field_layout, static_cast<int>(lane));
                uint32_t pulse_color = 0x6EE7F2;
                if (lane < data.gameplay.lane_color_count) {
                    pulse_color = data.gameplay.lane_colors[lane];
                }
                draw_gameplay_hit_burst(ctx,
                                        lane_center,
                                        note_width,
                                        hit_line_y + (sustained_hold ? note_alignment.hold_offsets[lane] : note_alignment.note_offsets[lane]),
                                        field_top,
                                        field_bottom,
                                        pulse,
                                        pulse_color,
                                        visual_opacity * key_pulse_brightness,
                                        sustained_hold);
            }
        }

        if (studio) draw_studio_rails();
        else draw_vertical_gauge(surface_layout.player_gauge_left,
                            data.gameplay.gauge,
                            data.gameplay.gauge_label,
                            scene_hud_cache.gauge_label_text,
                            scene_hud_cache.gauge_value_text,
                            0);

        if (data.gameplay.game_over && !data.gameplay.spectating_peer && d2d_->panel_brush) {
            const D2D1_RECT_F overlay = D2D1::RectF(field_left - 10.0f, field_top - 10.0f,
                                                    field_right + 10.0f, field_bottom + 10.0f);
            d2d_->panel_brush->SetOpacity(0.78f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(overlay, 18.0f, 18.0f), d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(1.0f);
            if (scene_header_format && d2d_->accent_brush) {
                const std::wstring over_w = L"GAME OVER";
                const D2D1_RECT_F over_rect =
                    D2D1::RectF(field_left, (field_top + field_bottom) * 0.5f - 50.0f, field_right,
                                (field_top + field_bottom) * 0.5f + 50.0f);
                scene_header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                draw_text_exact(over_w.c_str(), static_cast<UINT32>(over_w.size()),
                              scene_header_format.Get(), over_rect, d2d_->accent_brush.Get());
                scene_header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
        }

        if (surface_layout.ghost_visible) {
            const GameplayFieldLayout ghost_field_layout = surface_layout.ghost_field;
            const float ghost_field_left = ghost_field_layout.left;
            const float ghost_field_right = ghost_field_layout.right;
            const float ghost_field_top = ghost_field_layout.top;
            const float ghost_field_bottom = ghost_field_layout.bottom;
            const float ghost_field_height = ghost_field_layout.height;
            const D2D1_RECT_F ghost_field_clip_rect =
                D2D1::RectF(ghost_field_left + 2.0f, ghost_field_top + 2.0f,
                            ghost_field_right - 2.0f, ghost_field_bottom - 2.0f);
            const float ghost_hit_line_y =
                gameplay_field_y(ghost_field_top, ghost_field_height, judgement_line_position);
            const auto ghost_note_alignment = gameplay_judgement_field(ghost_field_layout, ghost_hit_line_y,
                note_height_scale, data.gameplay.judgement_line_type, *d2d_, gameplay_note_sprite_cache_,
                note_shape, note_image_aspect);

            const D2D1_ANTIALIAS_MODE ghost_saved_antialias = ctx->GetAntialiasMode();
            ctx->PushAxisAlignedClip(ghost_field_clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
            const bool ghost_has_gear_overlay =
                draw_imported_gear_overlay(ghost_field_layout, ghost_hit_line_y);
            draw_key_backdrop(ghost_field_layout, data.gameplay.ghost_lane_pressed, data.gameplay.ghost_lane_pressed_count);
            draw_measure_lines(ghost_field_layout, ghost_note_alignment);
            if (use_imported_metrics) {
                draw_key_labels(ghost_field_layout);
            }
            ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            for (int lane = 0; lane < lane_count; ++lane) {
                if (lane_hidden(lane)) continue;
                const std::size_t lane_index = static_cast<std::size_t>(lane);
                if (ghost_has_gear_overlay && !tenriff_gear_overlay) {
                    continue;
                }
                const bool lane_is_pressed =
                    lane_index < data.gameplay.ghost_lane_pressed_count &&
                    data.gameplay.ghost_lane_pressed[lane_index] != 0;
                const float lane_activity = lane_index < data.gameplay.ghost_lane_activity_count
                    ? data.gameplay.ghost_lane_activity[lane_index]
                    : 0.0f;
                ID2D1Bitmap* key_bitmap = d2d_->lane_key_idle_bitmaps[lane_index].Get();
                if (should_use_imported_pressed_key(lane_activity) &&
                    d2d_->lane_key_pressed_bitmaps[lane_index]) {
                    key_bitmap = d2d_->lane_key_pressed_bitmaps[lane_index].Get();
                }
                if (!use_imported_metrics) {
                    draw_native_digital_key(ghost_field_layout,
                                            ghost_hit_line_y,
                                            lane,
                                            lane_is_pressed,
                                            lane_activity,
                                            true);
                    continue;
                }
                if (!key_bitmap) {
                    continue;
                }
                const D2D1_SIZE_F bitmap_size = key_bitmap->GetSize();
                const float lane_center = gameplay_lane_center(ghost_field_layout, lane);
                const float ghost_lane_width = gameplay_lane_width(ghost_field_layout, lane);
                const float ghost_note_width = gameplay_note_width(ghost_field_layout, lane);
                const D2D1_RECT_F receptor_rect =
                    gameplay_key_bitmap_rect(ghost_field_layout,
                                             ghost_hit_line_y,
                                             note_height_scale,
                                             lane_center,
                                             ghost_lane_width,
                                             ghost_note_width,
                                             bitmap_size,
                                             gameplay_note_sprite_cache_.use_full_lane_receptor_layout);
                const D2D1_RECT_F* key_source_rect = nullptr;
                if (lane_index < d2d_->lane_key_pressed_source_rects.size() &&
                    key_bitmap == d2d_->lane_key_pressed_bitmaps[lane_index].Get()) {
                    key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_pressed_source_rects[lane_index]);
                } else if (lane_index < d2d_->lane_key_idle_source_rects.size()) {
                    key_source_rect = bitmap_source_rect_or_null(d2d_->lane_key_idle_source_rects[lane_index]);
                }
                draw_gameplay_sprite(ctx, key_bitmap, receptor_rect, visual_opacity,
                                     key_source_rect,
                                     gameplay_lane_sprite_rotation(
                                         gameplay_note_sprite_cache_.key_rotations,
                                         gameplay_note_sprite_cache_.key_rotation_count,
                                         lane_index));
            }
            if (!use_imported_metrics) {
                draw_key_labels(ghost_field_layout, true);
            }
            batch_note_field = can_batch_note_field(data.gameplay.ghost_notes, data.gameplay.ghost_note_count);
            for (std::size_t note_index = 0; note_index < data.gameplay.ghost_note_count; ++note_index) {
                const auto& note = data.gameplay.ghost_notes[note_index];
                if (!should_render_gameplay_note(
                        note.start_sample,
                        note.tail_sample,
                        note.hold,
                        note.head_visible,
                        note.pending,
                        data.gameplay.current_sample,
                        display_sample,
                        hold_handoff_grace_samples, note.released)) {
                    continue;
                }
                const bool render_head =
                    should_render_gameplay_note_head(note.start_sample, note.head_visible, display_sample, note.released);
                const int lane = std::clamp(note.lane, 1, lane_count);
                if (lane_hidden(static_cast<std::size_t>(lane - 1))) continue;
                const std::size_t lane_index = static_cast<std::size_t>(lane - 1);
                const float lane_center = gameplay_lane_center(ghost_field_layout, lane - 1);
                const float ghost_note_width = gameplay_note_width(ghost_field_layout, lane - 1);
                const float x0 = lane_center - ghost_note_width * 0.5f;
                const float x1 = lane_center + ghost_note_width * 0.5f;
                const double render_visual_position =
                    gameplay_note_anchors_to_judgement_line(note.hold, note.head_visible)
                        ? display_visual_position
                        : note.visual_position;
                const float ghost_note_alignment_offset = note.hold
                    ? ghost_note_alignment.hold_offsets[lane_index] : ghost_note_alignment.note_offsets[lane_index];
                const float y =
                    gameplay_field_y(ghost_field_top, ghost_field_height, visual_to_y(render_visual_position)) + ghost_note_alignment_offset;
                const float tail_y = gameplay_field_y(
                    ghost_field_top, ghost_field_height, visual_to_y(note.tail_visual_position)) + ghost_note_alignment_offset;
                const float head_half_h = gameplay_note_head_half_height(note_height_scale);
                const float tail_half_h = gameplay_note_tail_half_height(note_height_scale);
                uint32_t lane_color = 0xF6F8FF;
                if (lane_index < data.gameplay.lane_color_count) {
                    lane_color = data.gameplay.lane_colors[lane_index];
                } else if (!gameplay_lane_uses_white_note(lane)) {
                    lane_color = 0x4F80FF;
                }
                ID2D1SolidColorBrush* note_fill = d2d_->note_fill_brush.Get();
                ID2D1SolidColorBrush* note_border = d2d_->note_border_brush.Get();
                ID2D1SolidColorBrush* note_hold_fill = d2d_->note_hold_brush.Get();
                if (note_fill) {
                    note_fill->SetColor(gameplay_note_fill_color(lane_color, visual_opacity));
                }
                if (note_border) {
                    note_border->SetColor(gameplay_note_border_color(
                        lane_color, note_outline_opacity, note_polygon_geometry != nullptr));
                }
                if (note_hold_fill) {
                    note_hold_fill->SetColor(simple_hold_body
                        ? color_from_rgb(lane_color, native_hold_body_opacity)
                        : gameplay_note_hold_color(lane_color, native_hold_body_opacity));
                }
                ID2D1LinearGradientBrush* note_material = (note_shape == "simple_rect" ||
                    (note.hold && simple_hold_body)) ? nullptr
                    : d2d_->lane_native_note_brushes[lane_index].Get();
                ID2D1LinearGradientBrush* hold_material = simple_hold_body ? nullptr
                    : d2d_->lane_native_hold_brushes[lane_index].Get();
                ID2D1Bitmap* note_head_bitmap = d2d_->lane_note_head_bitmaps[lane_index].Get();
                ID2D1Bitmap* note_hold_head_bitmap =
                    d2d_->lane_note_hold_head_bitmaps[lane_index].Get();
                ID2D1Bitmap* note_hold_body_bitmap =
                    d2d_->lane_note_hold_body_bitmaps[lane_index].Get();
                ID2D1Bitmap* note_hold_tail_bitmap =
                    d2d_->lane_note_tail_bitmaps[lane_index].Get();
                const D2D1_RECT_F note_rect = D2D1::RectF(x0, y - head_half_h, x1, y + head_half_h);
                const bool using_hold_head_bitmap = note.hold && note_hold_head_bitmap;
                ID2D1Bitmap* head_bitmap = using_hold_head_bitmap ? note_hold_head_bitmap : note_head_bitmap;
                const D2D1_RECT_F* head_source_rect =
                    using_hold_head_bitmap
                        ? bitmap_source_rect_or_null(d2d_->lane_note_hold_head_source_rects[lane_index])
                        : bitmap_source_rect_or_null(d2d_->lane_note_head_source_rects[lane_index]);
                const D2D1_RECT_F head_visual_rect =
                    head_bitmap
                        ? gameplay_note_bitmap_dest_rect(note_rect,
                                                        head_bitmap,
                                                        head_source_rect,
                                                        note_shape,
                                                        note_image_aspect)
                        : note_rect;
                const D2D1_RECT_F tail_rect =
                    D2D1::RectF(x0, tail_y - tail_half_h, x1, tail_y + tail_half_h);
                const D2D1_RECT_F* tail_source_rect =
                    bitmap_source_rect_or_null(d2d_->lane_note_tail_source_rects[lane_index]);
                const D2D1_RECT_F tail_visual_rect =
                    note_hold_tail_bitmap
                        ? gameplay_note_bitmap_dest_rect(tail_rect,
                                                        note_hold_tail_bitmap,
                                                        tail_source_rect,
                                                        note_shape,
                                                        note_image_aspect)
                        : tail_rect;

                if (note.mine) {
                    flush_note_sprites();
                    const float mine_half_h = std::max(7.0f, head_half_h * 1.15f);
                    const D2D1_RECT_F mine_rect = D2D1::RectF(x0, y - mine_half_h, x1, y + mine_half_h);
                    if (note_fill) {
                        note_fill->SetColor(D2D1::ColorF(0.96f, 0.08f, 0.15f, visual_opacity));
                        ctx->FillRectangle(mine_rect, note_fill);
                    }
                    if (note_border) {
                        note_border->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.20f, visual_opacity));
                        ctx->DrawRectangle(mine_rect, note_border, 2.0f);
                        ctx->DrawLine(D2D1::Point2F(x0, y - mine_half_h),
                                      D2D1::Point2F(x1, y + mine_half_h), note_border, 2.0f);
                        ctx->DrawLine(D2D1::Point2F(x1, y - mine_half_h),
                                      D2D1::Point2F(x0, y + mine_half_h), note_border, 2.0f);
                    }
                    continue;
                }

                if (note.hold && note_hold_fill) {
                    const auto body_geometry = compute_gameplay_hold_body_geometry(
                        lane_center,
                        head_visual_rect.left,
                        head_visual_rect.right,
                        head_visual_rect.top,
                        y,
                        tail_visual_rect.bottom,
                        tail_y,
                        render_head,
                        data.gameplay.show_hold_tail,
                        hold_body_width_scale);
                    const D2D1_RECT_F hold_body =
                        D2D1::RectF(body_geometry.left,
                                    body_geometry.top,
                                    body_geometry.right,
                                    body_geometry.bottom);
                    if (hold_body.bottom > hold_body.top) {
                        if (note_hold_body_bitmap && !data.gameplay.hold_tail_taper_enabled) {
                            const D2D1_RECT_F* hold_body_source_rect =
                                bitmap_source_rect_or_null(d2d_->lane_note_hold_body_source_rects[lane_index]);
                            draw_note_sprite(ctx, note_hold_body_bitmap, hold_body, hold_body_opacity,
                                             hold_body_source_rect, 0.0f);
                        } else {
                            flush_note_sprites();
                            ID2D1Brush* hold_body_brush = configure_gameplay_material_brush(
                                hold_material, note_hold_fill, hold_body, native_hold_body_opacity, true);
                            draw_gameplay_hold_body(ctx,
                                                    d2d_->d2d_factory.Get(),
                                                    hold_body,
                                                    lane_center,
                                                    y,
                                                    tail_y,
                                                    (body_geometry.right - body_geometry.left) * 0.5f,
                                                    data.gameplay.hold_tail_taper_enabled,
                                                    hold_body_brush);
                        }
                    }
                }

                if (note.hold && data.gameplay.show_hold_tail) {
                    if (note_hold_tail_bitmap) {
                        draw_note_sprite(ctx,
                                             note_hold_tail_bitmap,
                                             tail_visual_rect,
                                             visual_opacity,
                                             tail_source_rect,
                                             gameplay_lane_sprite_rotation(
                                                 gameplay_note_sprite_cache_.note_rotations,
                                                 gameplay_note_sprite_cache_.note_rotation_count,
                                                 lane_index));
                    } else if (note_fill) {
                        flush_note_sprites();
                        ID2D1Brush* tail_fill = configure_gameplay_material_brush(
                            note_material, note_fill, tail_rect, visual_opacity, false);
                        draw_note_primitive(ctx,
                                            tail_rect,
                                            tail_fill,
                                            note_border,
                                            1.5f,
                                            note_shape,
                                            note_border_enabled && note_polygon_geometry != nullptr,
                                            note_polygon_geometry);
                    }
                }

                if (render_head) {
                    if (head_bitmap) {
                        draw_note_sprite(ctx, head_bitmap, head_visual_rect, visual_opacity,
                                             head_source_rect,
                                             gameplay_lane_sprite_rotation(
                                                 gameplay_note_sprite_cache_.note_rotations,
                                                 gameplay_note_sprite_cache_.note_rotation_count,
                                                 lane_index));
                    } else if (note_fill) {
                        flush_note_sprites();
                        ID2D1Brush* head_fill = configure_gameplay_material_brush(
                            note_material, note_fill, note_rect, visual_opacity, false);
                        draw_note_primitive(ctx, note_rect, head_fill, note_border,
                                            note_polygon_geometry ? 1.5f : 0.85f,
                                            note_shape, note_border_enabled, note_polygon_geometry);
                    }
                }
            }
            flush_note_sprites();
            draw_note_fog(ghost_field_layout, ghost_hit_line_y);
            ctx->PopAxisAlignedClip();
            ctx->SetAntialiasMode(ghost_saved_antialias);

            draw_feedback_overlay(ghost_field_layout, data.gameplay.ghost_has_feedback,
                                  data.gameplay.ghost_feedback, data.gameplay.ghost_timing_feedback_delta_ms,
                                  data.gameplay.ghost_has_non_pg_feedback, data.gameplay.ghost_has_timing_feedback,
                                  scene_hud_cache.ghost_feedback_text, scene_hud_cache.ghost_feedback_timing_text,
                                  data.gameplay.ghost_timing_history_delta_ms, data.gameplay.ghost_timing_history_count,
                                  data.gameplay.ghost_timing_history_maximums, data.gameplay.ghost_timing_maximum_window_ms,
                                  ghost_judgement_text_animation, scene_hud_cache.ghost_judgement_animation_started_ns);
            draw_field_judgement_counts(ghost_field_layout,
                {data.gameplay.ghost_pg, data.gameplay.ghost_gr, data.gameplay.ghost_gd,
                 data.gameplay.ghost_bd, data.gameplay.ghost_pr}, true);
            if (data.gameplay.ghost_combo > 0) {
                draw_combo_overlay(ghost_field_layout,
                                   scene_hud_cache.ghost_combo_value_text,
                                   scene_hud_cache.combo_label_text,
                                   combo_anchor_top_safe,
                                   combo_anchor_bottom_safe,
                                   60.0f,
                                   ghost_combo_text_animation, true);
            }

            if (key_pulse_brightness > 0.0f && d2d_->note_fill_brush && data.gameplay.ghost_lane_activity_count > 0) {
                const std::size_t count =
                    std::min(data.gameplay.ghost_lane_activity_count, static_cast<std::size_t>(lane_count));
                for (std::size_t lane = 0; lane < count; ++lane) {
                    if (lane_hidden(lane)) continue;
                    const float activity = gameplay_interpolated_activity(data.gameplay.ghost_lane_activity[lane], data.gameplay.activity_publish_time_ns, render_now_ns);
                    const bool sustained_hold =
                        lane < data.gameplay.ghost_lane_pressed_count &&
                        data.gameplay.ghost_lane_pressed[lane] != 0 &&
                        lane_has_active_hold(data.gameplay.ghost_notes,
                                             data.gameplay.ghost_note_count,
                                             lane);
                    const float hold_wave = 0.55f + 0.08f * static_cast<float>(
                        std::sin(static_cast<double>(render_now_ns) / 180'000'000.0 +
                                 static_cast<double>(lane) * 0.7));
                    const float pulse = sustained_hold
                                            ? std::max(activity * activity, hold_wave)
                                            : activity * activity;
                    if (pulse <= 0.02f) {
                        continue;
                    }
                    const float lane_center = gameplay_lane_center(ghost_field_layout, static_cast<int>(lane));
                    const float ghost_note_width = gameplay_note_width(ghost_field_layout, static_cast<int>(lane));
                    uint32_t pulse_color = 0x6EE7F2;
                    if (lane < data.gameplay.lane_color_count) {
                        pulse_color = data.gameplay.lane_colors[lane];
                    }
                    draw_gameplay_hit_burst(ctx,
                                            lane_center,
                                            ghost_note_width,
                                            ghost_hit_line_y + (sustained_hold ? ghost_note_alignment.hold_offsets[lane] : ghost_note_alignment.note_offsets[lane]),
                                            ghost_field_top,
                                            ghost_field_bottom,
                                            pulse,
                                            pulse_color,
                                            visual_opacity * key_pulse_brightness,
                                            sustained_hold);
                }
            }

            draw_vertical_gauge(surface_layout.ghost_gauge_left,
                                data.gameplay.ghost_gauge,
                                data.gameplay.ghost_gauge_label,
                                scene_hud_cache.ghost_gauge_label_text,
                                scene_hud_cache.ghost_gauge_value_text,
                                1);

            if (data.gameplay.ghost_game_over && d2d_->panel_brush) {
                const D2D1_RECT_F overlay =
                    D2D1::RectF(ghost_field_left - 10.0f, ghost_field_top - 10.0f,
                                ghost_field_right + 10.0f, ghost_field_bottom + 10.0f);
                d2d_->panel_brush->SetOpacity(0.78f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(overlay, 18.0f, 18.0f), d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(1.0f);
                if (scene_header_format && d2d_->accent_brush) {
                    const std::wstring over_w = L"GAME OVER";
                    const D2D1_RECT_F over_rect =
                        D2D1::RectF(ghost_field_left, (ghost_field_top + ghost_field_bottom) * 0.5f - 50.0f,
                                    ghost_field_right, (ghost_field_top + ghost_field_bottom) * 0.5f + 50.0f);
                    scene_header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    draw_text_exact(over_w.c_str(), static_cast<UINT32>(over_w.size()),
                                  scene_header_format.Get(), over_rect, d2d_->accent_brush.Get());
                    scene_header_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            }
        }
        if (surface_layout.ghost_visible && scene_body_format && scene_title_format) {
            auto draw_field_summary = [&](const GameplayBattleSummaryLayout& summary,
                                          const std::wstring& label_text,
                                          const std::wstring& score_text,
                                          const std::array<std::wstring, 4>& rows, bool embed_metadata) {
                const auto columns = compute_gameplay_battle_summary_columns(summary, embed_metadata);
                const auto rect = D2D1::RectF(summary.left, summary.top, summary.right, summary.bottom);
                if (d2d_->panel_brush) {
                    const float opacity = d2d_->panel_brush->GetOpacity();
                    d2d_->panel_brush->SetOpacity(0.88f);
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 16.0f, 16.0f), d2d_->panel_brush.Get());
                    d2d_->panel_brush->SetOpacity(opacity);
                }
                if (d2d_->button_border_brush) {
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 16.0f, 16.0f),
                                              d2d_->button_border_brush.Get(), 1.2f);
                }
                if (embed_metadata) {
                    const auto text_color = d2d_->text_brush->GetColor();
                    const auto muted_color = d2d_->muted_brush->GetColor();
                    d2d_->text_brush->SetColor(ng_color("title", text_color));
                    d2d_->muted_brush->SetColor(ng_color("body", muted_color));
                    const float left = columns.metadata_left + 14.0f, right = columns.metadata_right - 6.0f;
                    draw_full_song_title(ng_font("title", scene_title_format.Get()),
                        D2D1::RectF(left, summary.row_edges[0], right, summary.row_edges[1]),
                        d2d_->text_brush.Get());
                    draw_artist_and_level(D2D1::RectF(left, summary.row_edges[1], right, summary.row_edges[2]),
                        ng_font("body", scene_body_format.Get()));
                    for (std::size_t row = 0; row < scene_hud_cache.battle_speed_pair_text.size(); ++row) {
                        draw_fitted_readable_text(scene_hud_cache.battle_speed_pair_text[row], scene_hud_format.Get(),
                            D2D1::RectF(left, summary.row_edges[row + 2], right, summary.row_edges[row + 3]),
                            d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
                    }
                    d2d_->text_brush->SetColor(text_color);
                    d2d_->muted_brush->SetColor(muted_color);
                    if (d2d_->button_border_brush) {
                        ctx->DrawLine(D2D1::Point2F(columns.stats_left - 8.0f, rect.top + 12.0f),
                                      D2D1::Point2F(columns.stats_left - 8.0f, rect.bottom - 12.0f),
                                      d2d_->button_border_brush.Get(), 1.0f);
                    }
                }
                // Fit each complete row horizontally; clipping a long no-wrap
                // line used to hide accuracy/counts in small split fields.
                draw_fitted_readable_text(label_text, scene_body_format.Get(),
                    D2D1::RectF(columns.stats_left + 14.0f, summary.row_edges[0], columns.stats_left + 110.0f, summary.row_edges[1]),
                    d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING);
                const auto old_color = d2d_->text_brush->GetColor();
                d2d_->text_brush->SetColor(ng_color("score", old_color));
                draw_fitted_readable_text(score_text, ng_font("score", scene_title_format.Get()),
                    D2D1::RectF(columns.stats_left + 118.0f, summary.row_edges[0], rect.right - 14.0f, summary.row_edges[1]),
                    d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                d2d_->text_brush->SetColor(old_color);
                for (std::size_t row = 0; row < rows.size(); ++row) {
                    draw_fitted_readable_text(rows[row], scene_hud_format.Get(),
                        D2D1::RectF(columns.stats_left + 14.0f, summary.row_edges[row + 1],
                                    rect.right - 14.0f, summary.row_edges[row + 2]),
                        row >= 2 ? d2d_->muted_brush.Get() : d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            };
            draw_field_summary(player_summary, wloc("LIVE", "실플레이"),
                               scene_hud_cache.score_text, scene_hud_cache.battle_summary_text, embed_battle_header);
            draw_field_summary(ghost_summary, wloc("GHOST", "고스트"),
                               scene_hud_cache.ghost_score_text, scene_hud_cache.ghost_battle_summary_text, false);
            if (embed_battle_header) draw_gameplay_progress_bar(progress_track_rect);
        }
        if ((!studio || !studio_deck.score_visible) && data.gameplay.pacemaker_mode != "off" && !data.gameplay.pacemaker_mode.empty()) {
            const bool accuracy_target = data.gameplay.pacemaker_mode == "accuracy";
            const std::string delta = (data.gameplay.pacemaker_delta >= 0 ? "+" : "") +
                (accuracy_target ? format_decimal(data.gameplay.pacemaker_delta) :
                 std::to_string(static_cast<int64_t>(std::llround(data.gameplay.pacemaker_delta))));
            const std::wstring pace = L"PACE " + to_wide(delta) + (accuracy_target ? L"%p" : L" pts");
            const auto saved = d2d_->text_brush->GetColor();
            d2d_->text_brush->SetColor(D2D1::ColorF(data.gameplay.pacemaker_delta < 0 ? 0xFF6B78 : 0x66E6B0));
            draw_fitted_readable_text(pace, scene_body_format.Get(),
                D2D1::RectF(surface_layout.player_field.left + 12, 120,
                            surface_layout.player_field.right - 12, 165), d2d_->text_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_TRAILING);
            d2d_->text_brush->SetColor(saved);
        }
        #include "MenuWindow_draw_key_overlay.inl"

        // Resume keeps the frozen note field visible; only the opening countdown
        // uses the early-return card above. This also leaves LN bodies in place.
        // The count sits on the field the player is about to read, on a
        // translucent card so the frozen notes around it stay legible.
        if (data.gameplay.paused && data.gameplay.countdown_active) {
            const float center_x = (field_left + field_right) * 0.5f;
            // Below the combo/judgement stack and above the contact line.
            const float center_y = std::min(field_top + field_height * 0.56f, hit_line_y - 110.0f);
            const float half_width = std::max(60.0f, std::min(132.0f, (field_right - field_left) * 0.5f - 12.0f));
            const D2D1_RECT_F card = D2D1::RectF(center_x - half_width, center_y - 92.0f,
                                                 center_x + half_width, center_y + 92.0f);
            if (d2d_->panel_brush) {
                const float saved_opacity = d2d_->panel_brush->GetOpacity();
                d2d_->panel_brush->SetOpacity(0.66f);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(card, 22.0f, 22.0f), d2d_->panel_brush.Get());
                d2d_->panel_brush->SetOpacity(saved_opacity);
            }
            if (d2d_->accent_brush) {
                const float saved_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(0.55f);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(card, 22.0f, 22.0f), d2d_->accent_brush.Get(), 1.6f);
                d2d_->accent_brush->SetOpacity(saved_opacity);
            }
            if (scene_body_format && d2d_->muted_brush) {
                draw_readable_text_aligned(wloc("RESUME", "재개"), scene_body_format.Get(),
                    vcenter_text_rect(D2D1::RectF(card.left, card.top + 10.0f, card.right, card.top + 44.0f),
                                      scene_body_format.Get()),
                    d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            const D2D1_RECT_F count_rect = D2D1::RectF(card.left, card.top + 40.0f, card.right, card.bottom - 6.0f);
            ID2D1Brush* count_brush = d2d_->logo_brush
                                          ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                          : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) set_brush_points(d2d_->logo_brush.Get(), count_rect);
            if (scene_rank_format && count_brush) {
                draw_readable_text_aligned(to_wide(std::to_string(std::max(1, data.gameplay.countdown_value))),
                    scene_rank_format.Get(), vcenter_text_rect(count_rect, scene_rank_format.Get()),
                    count_brush, DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
        if (data.gameplay.paused && !data.gameplay.countdown_active && d2d_->panel_brush && d2d_->card_brush &&
            d2d_->text_brush && scene_header_format && scene_body_format) {
            const D2D1_RECT_F screen_overlay = data.kind == MenuScreenKind::GameplayHud
                ? full_screen_rect : D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight);
            // Tall enough to seat the three menu rows and the live tuning readout.
            const D2D1_RECT_F pause_panel = D2D1::RectF(650.0f, 210.0f, 1270.0f, 960.0f);
            const float saved_panel_opacity = d2d_->panel_brush->GetOpacity();
            const float saved_card_opacity = d2d_->card_brush->GetOpacity();
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const float saved_text_opacity = d2d_->text_brush->GetOpacity();

            // Pause controls must not inherit a fading gameplay text brush.
            // Their opaque card already supplies contrast over bright BGA.
            d2d_->text_brush->SetOpacity(1.0f);
            d2d_->panel_brush->SetOpacity(0.76f);
            ctx->FillRectangle(screen_overlay, d2d_->panel_brush.Get());
            d2d_->card_brush->SetOpacity(0.98f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(pause_panel, 28.0f, 28.0f), d2d_->card_brush.Get());
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(pause_panel, 28.0f, 28.0f),
                                          d2d_->button_border_brush.Get(),
                                          2.0f);
            }

            d2d_->text_brush->SetColor(D2D1::ColorF(0xF7FAFD, 1.0f));
            draw_readable_text_aligned(L"PAUSED  /  \uC77C\uC2DC\uC815\uC9C0",
                                      scene_header_format.Get(),
                                      D2D1::RectF(pause_panel.left + 40.0f,
                                                  pause_panel.top + 42.0f,
                                                  pause_panel.right - 40.0f,
                                                  pause_panel.top + 118.0f),
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);

            constexpr std::array<const wchar_t*, 3> kPauseLabels{
                L"\uACC4\uC18D\uD558\uAE30  /  CONTINUE",
                L"\uC7AC\uC2DC\uC791  /  RESTART",
                L"\uB098\uAC00\uAE30  /  EXIT",
            };
            const int selected_pause_row = std::clamp(data.gameplay.pause_menu_cursor, 0, 2);
            for (int row = 0; row < static_cast<int>(kPauseLabels.size()); ++row) {
                const float top = pause_panel.top + 160.0f + static_cast<float>(row) * 118.0f;
                const D2D1_RECT_F row_rect =
                    D2D1::RectF(pause_panel.left + 72.0f,
                                top,
                                pause_panel.right - 72.0f,
                                top + 86.0f);
                if (!is_skin_preview) register_hit(row_rect, MenuHitTargetKind::GameplayPauseAction, row);
                const bool selected = row == selected_pause_row;
                if (selected && d2d_->accent_brush) {
                    const float saved_accent_opacity = d2d_->accent_brush->GetOpacity();
                    d2d_->accent_brush->SetOpacity(0.28f);
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(row_rect, 14.0f, 14.0f),
                                              d2d_->accent_brush.Get());
                    // A solid leading rail marks the cursor without relying on fill tint alone.
                    d2d_->accent_brush->SetOpacity(0.95f);
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(
                        D2D1::RectF(row_rect.left + 12.0f, row_rect.top + 20.0f,
                                    row_rect.left + 17.0f, row_rect.bottom - 20.0f), 2.5f, 2.5f),
                        d2d_->accent_brush.Get());
                    d2d_->accent_brush->SetOpacity(saved_accent_opacity);
                }
                if (d2d_->button_border_brush) {
                    ctx->DrawRoundedRectangle(D2D1::RoundedRect(row_rect, 14.0f, 14.0f),
                                              d2d_->button_border_brush.Get(),
                                              selected ? 2.0f : 1.0f);
                }
                d2d_->text_brush->SetColor(
                    selected ? D2D1::ColorF(0xFFFFFF, 1.0f) : D2D1::ColorF(0xDFE7F0, 1.0f));
                draw_readable_text_aligned(kPauseLabels[static_cast<std::size_t>(row)],
                                          scene_body_format.Get(),
                                          vcenter_text_rect(row_rect, scene_body_format.Get()),
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            // The pause screen doubles as the tuning screen, so the three values the
            // F-keys drive are shown live with the keys that move them.
            {
                const float tuning_top = pause_panel.top + 518.0f;
                const D2D1_RECT_F tuning_rect =
                    D2D1::RectF(pause_panel.left + 40.0f, tuning_top,
                                pause_panel.right - 40.0f, tuning_top + 132.0f);
                if (d2d_->button_border_brush) {
                    ctx->DrawLine(D2D1::Point2F(tuning_rect.left, tuning_rect.top - 12.0f),
                                  D2D1::Point2F(tuning_rect.right, tuning_rect.top - 12.0f),
                                  d2d_->button_border_brush.Get(),
                                  1.0f);
                }
                struct PauseTuningRow {
                    const wchar_t* label;
                    std::wstring value;
                    std::wstring keys;
                };
                const std::array<PauseTuningRow, 3> tuning_rows{{
                    {L"\uD310\uC815\uC120",
                     to_wide(format_decimal(data.gameplay.judgement_line_position * 100.0) + "%"),
                     wloc("F1 / F2", "F1 / F2")},
                    {L"\uC18D\uB3C4",
                     to_wide(format_decimal(data.gameplay.hispeed)),
                     wloc("F3 / F4", "F3 / F4")},
                    {L"\uB808\uC774\uD134\uC2DC",
                     to_wide(format_signed_ms(data.gameplay.visual_offset_ms)),
                     wloc("F7 / F8", "F7 / F8")},
                }};
                for (std::size_t row = 0; row < tuning_rows.size(); ++row) {
                    const float top = tuning_rect.top + static_cast<float>(row) * 42.0f;
                    const D2D1_RECT_F line_rect =
                        D2D1::RectF(tuning_rect.left, top, tuning_rect.right - 88.0f, top + 36.0f);
                    for (int direction = 0; direction < 2; ++direction) {
                        const float left = tuning_rect.right - 80.0f + direction * 42.0f;
                        const D2D1_RECT_F button = D2D1::RectF(left, top, left + 36.0f, top + 36.0f);
                        ctx->DrawRoundedRectangle(D2D1::RoundedRect(button, 5, 5), d2d_->button_border_brush.Get(), 1);
                        draw_readable_text_aligned(direction == 0 ? L"-" : L"+", scene_body_format.Get(),
                            vcenter_text_rect(button, scene_body_format.Get()), d2d_->text_brush.Get(),
                            DWRITE_TEXT_ALIGNMENT_CENTER);
                        if (!is_skin_preview) register_hit(button, MenuHitTargetKind::GameplayPauseAdjust,
                            static_cast<int>(row), direction == 0 ? MenuHitPart::Decrement : MenuHitPart::Increment);
                    }
                    d2d_->text_brush->SetColor(D2D1::ColorF(0xDFE7F0, 1.0f));
                    draw_readable_text_aligned(std::wstring(tuning_rows[row].label),
                                              scene_body_format.Get(),
                                              vcenter_text_rect(line_rect, scene_body_format.Get()),
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_LEADING);
                    d2d_->text_brush->SetColor(D2D1::ColorF(0xFFFFFF, 1.0f));
                    draw_readable_text_aligned(tuning_rows[row].value,
                                              scene_body_format.Get(),
                                              vcenter_text_rect(D2D1::RectF(line_rect.left, line_rect.top,
                                                          line_rect.right - 110.0f, line_rect.bottom),
                                                                scene_body_format.Get()),
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_TRAILING);
                    d2d_->text_brush->SetColor(D2D1::ColorF(0xB7C6D8, 1.0f));
                    draw_readable_text_aligned(std::wstring(tuning_rows[row].keys),
                                              scene_hud_format.Get(),
                                              vcenter_text_rect(line_rect, scene_hud_format.Get()),
                                              d2d_->text_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_TRAILING);
                }
            }

            d2d_->text_brush->SetColor(D2D1::ColorF(0xB7C6D8, 1.0f));
            draw_readable_text_aligned(L"\u2191\u2193 \uC120\uD0DD   ENTER \uD655\uC778   ESC \uACC4\uC18D",
                                      scene_body_format.Get(),
                                      D2D1::RectF(pause_panel.left + 30.0f,
                                                  pause_panel.bottom - 82.0f,
                                                  pause_panel.right - 30.0f,
                                                  pause_panel.bottom - 30.0f),
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);

            d2d_->panel_brush->SetOpacity(saved_panel_opacity);
            d2d_->card_brush->SetOpacity(saved_card_opacity);
            d2d_->text_brush->SetColor(saved_text_color);
            d2d_->text_brush->SetOpacity(saved_text_opacity);
        }

        if (gameplay_field_drag_state_.visible && d2d_->card_brush &&
            d2d_->text_brush && scene_body_format) {
            const auto hit_rect = D2D1::RectF(gameplay_field_drag_state_.left, gameplay_field_drag_state_.top,
                gameplay_field_drag_state_.right, gameplay_field_drag_state_.bottom);
            if (is_skin_preview) register_hit(hit_rect, MenuHitTargetKind::GameplayFieldDrag, 0);
            const D2D1_RECT_F handle_rect = D2D1::RectF(
                (hit_rect.left - scene_left) / scene_scale,
                (hit_rect.top - scene_top) / scene_scale,
                (hit_rect.right - scene_left) / scene_scale,
                (hit_rect.bottom - scene_top) / scene_scale);
            const D2D1_ROUNDED_RECT handle = D2D1::RoundedRect(handle_rect, 11.0f, 11.0f);
            const bool highlighted = gameplay_field_drag_state_.hovered ||
                                     gameplay_field_drag_state_.active;
            const float saved_card_opacity = d2d_->card_brush->GetOpacity();
            const D2D1_COLOR_F saved_text_color = d2d_->text_brush->GetColor();
            const float saved_text_opacity = d2d_->text_brush->GetOpacity();

            // The handle stays discoverable but recedes while playing; hovering
            // or dragging restores the full affordance. The hit area is unchanged.
            d2d_->card_brush->SetOpacity(highlighted ? 0.98f : 0.40f);
            ctx->FillRoundedRectangle(handle, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved_card_opacity);
            if (d2d_->accent_brush) {
                const float saved_accent_opacity = d2d_->accent_brush->GetOpacity();
                d2d_->accent_brush->SetOpacity(highlighted ? 1.0f : 0.30f);
                ctx->DrawRoundedRectangle(handle,
                                          d2d_->accent_brush.Get(),
                                          highlighted ? 2.4f : 1.2f);
                d2d_->accent_brush->SetOpacity(saved_accent_opacity);
            }
            d2d_->text_brush->SetColor(D2D1::ColorF(0xFFFFFF, highlighted ? 1.0f : 0.45f));
            d2d_->text_brush->SetOpacity(1.0f);
            draw_readable_text_aligned(L"\u2194",
                                      scene_body_format.Get(),
                                      handle_rect,
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->text_brush->SetColor(saved_text_color);
            d2d_->text_brush->SetOpacity(saved_text_opacity);
        }
