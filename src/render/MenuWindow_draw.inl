void MenuWindow::draw(const MenuRenderData& data) {
    static int draw_count = 0;
    if (draw_count++ < 5) {
        std::cerr << "[MenuWindow::draw] called, d2d_=" << (d2d_ ? "yes" : "no")
                  << ", d2d_context=" << (d2d_ && d2d_->d2d_context ? "yes" : "no")
                  << ", swap_chain=" << (d2d_ && d2d_->swap_chain ? "yes" : "no")
                  << ", target=" << (d2d_ && d2d_->d2d_target ? "yes" : "no") << std::endl;
    }
    if (!d2d_ || !d2d_->d2d_context || !d2d_->swap_chain) {
        return;
    }

    if (resize_pending_) {
        resize_pending_ = false;
        if (!resize_swap_chain(pending_width_, pending_height_)) {
            return;
        }
    }

    if (!d2d_->d2d_target) {
        if (!recreate_targets()) {
            fail_fatal("Failed to recover Direct2D render target.");
            shutdown();
            return;
        }
    }
    if (d2d_->d2d_target) {
        d2d_->d2d_context->SetTarget(d2d_->d2d_target.Get());
    } else {
        fail_fatal("Render target is unavailable.");
        shutdown();
        return;
    }

    auto* ctx = d2d_->d2d_context.Get();
    const int64_t render_now_ns = timing::HighResClock::now_ns();
    const bool has_menu_scene = render_menu_scene(data.kind, render_now_ns);
    if (data.kind == MenuScreenKind::GameplayHud) {
        if (!ensure_gameplay_note_sprites(data.gameplay)) {
            invalidate_gameplay_note_sprite_cache();
        }
        if (!ensure_gameplay_background_bitmap(data.gameplay)) {
            invalidate_gameplay_background_cache();
        }
        if (!ensure_gameplay_static_cache(data.gameplay)) {
            invalidate_gameplay_static_cache();
        }
    } else if (data.kind == MenuScreenKind::GenericList &&
               data.generic.skin_preview.visible) {
        // The live skin preview uses the same imported head/body/tail sprite
        // cache as gameplay so LR2/TenRiff LN art is not replaced by placeholders.
        GameplayHudData preview_sprite_data;
        preview_sprite_data.lane_count = data.generic.skin_preview.lane_count;
        preview_sprite_data.note_border_enabled =
            data.generic.skin_preview.note_border_enabled;
        preview_sprite_data.note_shape = data.generic.skin_preview.note_shape;
        preview_sprite_data.note_image_aspect =
            data.generic.skin_preview.note_image_aspect;
        preview_sprite_data.skin_source = data.generic.skin_preview.skin_source;
        preview_sprite_data.external_skin_root =
            data.generic.skin_preview.external_skin_root;
        preview_sprite_data.external_skin_name =
            data.generic.skin_preview.external_skin_name;
        preview_sprite_data.lr2_resolution_override =
            data.generic.skin_preview.lr2_resolution_override;
        preview_sprite_data.lane_colors = data.generic.skin_preview.lane_colors;
        if (!ensure_gameplay_note_sprites(preview_sprite_data)) {
            invalidate_gameplay_note_sprite_cache();
        }
    } else if (data.kind == MenuScreenKind::SongSelect) {
        update_song_select_preview_loading_state(data.song_select, render_now_ns);
        pump_song_select_preview_loads(data.song_select, render_now_ns);
    } else if (data.kind == MenuScreenKind::ResultScreen) {
        if (!data.result.background_path.empty()) {
            static_cast<void>(load_song_card_preview_bitmap(data.result.background_path));
        }
        if (!data.result.profile_avatar_path.empty()) {
            static_cast<void>(load_song_card_preview_bitmap(data.result.profile_avatar_path));
        }
    } else {
        song_select_preview_signature_.clear();
        song_select_preview_load_hold_until_ns_ = 0;
    }
    if (data.kind != MenuScreenKind::GameplayHud && data.lobby_skin.enabled) {
        static_cast<void>(load_song_card_preview_bitmap(data.lobby_skin.background_path));
        static_cast<void>(load_song_card_preview_bitmap(data.lobby_skin.logo_path));
    } else if (data.kind == MenuScreenKind::GameplayHud) {
        static_cast<void>(load_song_card_preview_bitmap(data.gameplay.skin_background_path));
    }
    // Rebuild the text formats when the player picks a different UI font.
    if (const wchar_t* ui_family = ui_font_family_for_token(data.ui_font);
        d2d_->ui_font_family != ui_family) {
        static_cast<void>(create_text_formats(ui_family));
    }
    ctx->BeginDraw();

    ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    if (has_menu_scene && d2d_->bg_brush) {
        const float saved_opacity = d2d_->bg_brush->GetOpacity();
        d2d_->bg_brush->SetOpacity(data.kind == MenuScreenKind::TitleMenu ? 0.16f : 0.18f);
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_),
                                       static_cast<float>(height_)),
                           d2d_->bg_brush.Get());
        d2d_->bg_brush->SetOpacity(saved_opacity);
    } else if (d2d_->bg_brush) {
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_),
                                       static_cast<float>(height_)),
                           d2d_->bg_brush.Get());
    } else {
        ctx->Clear(D2D1::ColorF(0.05f, 0.05f, 0.06f, 1.0f));
    }

    const D2D1_MATRIX_3X2_F transform =
        D2D1::Matrix3x2F(scale_, 0.0f, 0.0f, scale_, offset_x_, offset_y_);
    ctx->SetTransform(transform);

    const D2D1_RECT_F full_screen_rect =
        D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight);
    if (data.kind != MenuScreenKind::GameplayHud && data.lobby_skin.enabled) {
        if (ID2D1Bitmap* bitmap = find_song_card_preview_bitmap(data.lobby_skin.background_path)) {
            const D2D1_RECT_F source_rect =
                centered_bitmap_source_rect(bitmap->GetSize(), full_screen_rect);
            ctx->DrawBitmap(bitmap,
                            full_screen_rect,
                            std::clamp(data.lobby_skin.background_opacity, 0.0f, 1.0f),
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source_rect);
        }
    }


    if (data.kind == MenuScreenKind::GameplayHud) {
        if (ID2D1Bitmap* bitmap = find_song_card_preview_bitmap(data.gameplay.skin_background_path)) {
            const D2D1_RECT_F source_rect =
                centered_bitmap_source_rect(bitmap->GetSize(), full_screen_rect);
            ctx->DrawBitmap(bitmap,
                            full_screen_rect,
                            std::clamp(data.gameplay.skin_background_opacity, 0.0f, 1.0f),
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source_rect);
        }
        const D2D1_RECT_F background_rect =
            D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight);
        if (d2d_->gameplay_background_base_bitmap) {
            const D2D1_RECT_F source_rect = centered_bitmap_source_rect(
                d2d_->gameplay_background_base_bitmap->GetSize(), background_rect);
            ctx->DrawBitmap(d2d_->gameplay_background_base_bitmap.Get(),
                            background_rect,
                            0.72f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source_rect);
        }
        if (d2d_->gameplay_background_overlay_bitmap) {
            const D2D1_RECT_F source_rect = centered_bitmap_source_rect(
                d2d_->gameplay_background_overlay_bitmap->GetSize(), background_rect);
            ctx->DrawBitmap(d2d_->gameplay_background_overlay_bitmap.Get(),
                            background_rect,
                            0.82f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                            &source_rect);
        }
        if ((d2d_->gameplay_background_base_bitmap ||
             d2d_->gameplay_background_overlay_bitmap) &&
            d2d_->panel_brush) {
            const float saved_opacity = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetOpacity(0.30f);
            ctx->FillRectangle(background_rect, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }
    }

    if (d2d_->glow_brush) {
        const float saved_opacity = d2d_->glow_brush->GetOpacity();
        if (has_menu_scene) {
            d2d_->glow_brush->SetOpacity(data.kind == MenuScreenKind::TitleMenu ? 0.26f : 0.14f);
        }
        ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight), d2d_->glow_brush.Get());
        d2d_->glow_brush->SetOpacity(saved_opacity);
    }

    hit_regions_.clear();
    if (data.kind != MenuScreenKind::SongSelect) {
        clear_song_scrollbar_state();
    }
    if (data.kind != MenuScreenKind::GameplayHud && !gameplay_field_drag_state_.active) {
        gameplay_field_drag_state_.visible = false;
        gameplay_field_drag_state_.hovered = false;
    }
    auto register_hit = [this](const D2D1_RECT_F& rect,
                               MenuHitTargetKind kind,
                               int index,
                               MenuHitPart part = MenuHitPart::Activate) {
        if (kind == MenuHitTargetKind::None || index < 0) {
            return;
        }
        hit_regions_.push_back(HitRegion{kind, index, part, rect.left, rect.top, rect.right, rect.bottom});
    };

    auto draw_text_clipped = [&](const std::wstring& text,
                                 IDWriteTextFormat* format,
                                 const D2D1_RECT_F& rect,
                                 ID2D1Brush* brush) {
        if (text.empty() || !format || !brush) {
            return;
        }
        ctx->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                      format, rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
    };

    auto draw_text_clipped_aligned = [&](const std::wstring& text,
                                         IDWriteTextFormat* format,
                                         const D2D1_RECT_F& rect,
                                         ID2D1Brush* brush,
                                         DWRITE_TEXT_ALIGNMENT alignment) {
        if (text.empty() || !format || !brush) {
            return;
        }
        format->SetTextAlignment(alignment);
        draw_text_clipped(text, format, rect, brush);
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    };

    const bool ui_korean = data.ui_korean;
    auto loc = [&](std::string_view english, std::string_view korean) {
        return std::string(ui_korean ? korean : english);
    };
    auto wloc = [&](std::string_view english, std::string_view korean) {
        return to_wide(std::string(ui_korean ? korean : english));
    };

    auto inset_rect = [](const D2D1_RECT_F& rect, float dx, float dy) {
        return D2D1::RectF(rect.left + dx, rect.top + dy, rect.right - dx, rect.bottom - dy);
    };

    auto offset_rect = [](const D2D1_RECT_F& rect, float dx, float dy) {
        return D2D1::RectF(rect.left + dx, rect.top + dy, rect.right + dx, rect.bottom + dy);
    };

    auto draw_song_select_glow_border = [&](const D2D1_ROUNDED_RECT& rr, float opacity, float width) {
        if (!d2d_->accent_brush || opacity <= 0.0f || width <= 0.0f) {
            return;
        }
        const float saved_opacity = d2d_->accent_brush->GetOpacity();
        d2d_->accent_brush->SetOpacity(opacity);
        ctx->DrawRoundedRectangle(rr, d2d_->accent_brush.Get(), width);
        d2d_->accent_brush->SetOpacity(saved_opacity);
    };

    auto draw_glass_panel = [&](const D2D1_RECT_F& rect,
                                float radius,
                                float fill_opacity,
                                float glow_strength,
                                bool strong_edge,
                                float shadow_offset = 9.0f) {
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
        const D2D1_RECT_F shadow_rect = offset_rect(rect, 0.0f, shadow_offset);
        const D2D1_ROUNDED_RECT shadow_rr = D2D1::RoundedRect(shadow_rect, radius, radius);

        if (d2d_->footer_brush) {
            const float saved_opacity = d2d_->footer_brush->GetOpacity();
            d2d_->footer_brush->SetOpacity(std::clamp(0.10f + glow_strength * 0.10f, 0.08f, 0.24f));
            ctx->FillRoundedRectangle(shadow_rr, d2d_->footer_brush.Get());
            d2d_->footer_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->panel_brush) {
            const float saved_opacity = d2d_->panel_brush->GetOpacity();
            d2d_->panel_brush->SetOpacity(fill_opacity);
            ctx->FillRoundedRectangle(rr, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->card_brush) {
            const D2D1_RECT_F sheen_rect =
                D2D1::RectF(rect.left + 8.0f, rect.top + 8.0f, rect.right - 8.0f,
                            std::min(rect.bottom - 8.0f, rect.top + std::max(30.0f, (rect.bottom - rect.top) * 0.28f)));
            const D2D1_ROUNDED_RECT sheen_rr =
                D2D1::RoundedRect(sheen_rect, std::max(4.0f, radius - 8.0f), std::max(4.0f, radius - 8.0f));
            const float saved_opacity = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetOpacity(std::clamp(0.22f + glow_strength * 0.04f, 0.16f, 0.28f));
            ctx->FillRoundedRectangle(sheen_rr, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->text_brush) {
            const D2D1_RECT_F top_glint =
                D2D1::RectF(rect.left + 18.0f, rect.top + 14.0f, rect.right - 18.0f, rect.top + 16.0f);
            const float saved_opacity = d2d_->text_brush->GetOpacity();
            d2d_->text_brush->SetOpacity(std::clamp(0.03f + glow_strength * 0.06f, 0.03f, 0.10f));
            ctx->FillRectangle(top_glint, d2d_->text_brush.Get());
            d2d_->text_brush->SetOpacity(saved_opacity);
        }

        if (d2d_->button_border_brush) {
            const float saved_opacity = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(strong_edge ? 0.85f : 0.60f);
            ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), strong_edge ? 1.6f : 1.2f);
            const D2D1_RECT_F inner_rect = inset_rect(rect, 3.0f, 3.0f);
            const D2D1_ROUNDED_RECT inner_rr =
                D2D1::RoundedRect(inner_rect, std::max(4.0f, radius - 3.0f), std::max(4.0f, radius - 3.0f));
            d2d_->button_border_brush->SetOpacity(strong_edge ? 0.28f : 0.20f);
            ctx->DrawRoundedRectangle(inner_rr, d2d_->button_border_brush.Get(), 0.9f);
            d2d_->button_border_brush->SetOpacity(saved_opacity);
        }

        draw_song_select_glow_border(rr, 0.05f + glow_strength * 0.06f, 10.0f);
        draw_song_select_glow_border(rr, 0.10f + glow_strength * 0.12f, 4.0f);
        draw_song_select_glow_border(rr, strong_edge ? 0.36f + glow_strength * 0.16f : 0.14f + glow_strength * 0.10f,
                                     strong_edge ? 1.8f : 1.2f);
    };

    auto draw_song_select_horizon = [&](float y,
                                        float left,
                                        float right,
                                        float bright_width,
                                        float base_alpha,
                                        float bright_alpha) {
        if (d2d_->button_border_brush) {
            const float saved_opacity = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(base_alpha);
            ctx->DrawLine(D2D1::Point2F(left, y), D2D1::Point2F(right, y), d2d_->button_border_brush.Get(), 1.2f);
            d2d_->button_border_brush->SetOpacity(saved_opacity);
        }
        if (d2d_->accent_brush) {
            const float center = (left + right) * 0.5f;
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(bright_alpha * 0.45f);
            ctx->DrawLine(D2D1::Point2F(center - bright_width * 0.5f, y - 1.0f),
                          D2D1::Point2F(center + bright_width * 0.5f, y - 1.0f),
                          d2d_->accent_brush.Get(),
                          4.0f);
            d2d_->accent_brush->SetOpacity(bright_alpha);
            ctx->DrawLine(D2D1::Point2F(center - bright_width * 0.5f, y),
                          D2D1::Point2F(center + bright_width * 0.5f, y),
                          d2d_->accent_brush.Get(),
                          1.8f);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }
    };

    auto draw_song_select_stardust = [&](const D2D1_RECT_F& rect, int count, uint32_t seed_base, float base_alpha) {
        if ((!d2d_->accent_brush && !d2d_->text_brush) || count <= 0) {
            return;
        }
        const float width = rect.right - rect.left;
        const float height = rect.bottom - rect.top;
        for (int i = 0; i < count; ++i) {
            const uint32_t seed = seed_base + static_cast<uint32_t>(i) * 97u;
            const float x = rect.left + unit_hash_01(seed) * width;
            const float y = rect.top + unit_hash_01(seed ^ 0x68bc21ebu) * height;
            const float radius = 0.7f + unit_hash_01(seed ^ 0x13579bdfu) * 2.1f;
            const float alpha =
                base_alpha * (0.30f + unit_hash_01(seed ^ 0x24a5f123u) * 0.70f) *
                static_cast<float>(0.80 + 0.20 * pulse_wave_01(render_now_ns, 5.0, static_cast<double>(i) * 0.09));
            ID2D1SolidColorBrush* brush =
                (i % 3 == 0 && d2d_->text_brush) ? d2d_->text_brush.Get() :
                (d2d_->accent_brush ? d2d_->accent_brush.Get() : d2d_->text_brush.Get());
            if (!brush) {
                continue;
            }
            const float saved_opacity = brush->GetOpacity();
            brush->SetOpacity(alpha);
            ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), brush);
            brush->SetOpacity(saved_opacity);
        }
    };

    auto draw_footer = [&](std::string_view profile, int64_t high_score, std::string_view track, bool song_select_style = false) {
        const float margin = song_select_style ? 26.0f : 80.0f;
        const float bar_height = song_select_style ? 78.0f : 84.0f;
        const float bar_bottom = kBaseHeight - (song_select_style ? 20.0f : 24.0f);
        const float bar_top = bar_bottom - bar_height;
        const D2D1_RECT_F rect =
            skin_layout_rect(data, "title.footer",
                             D2D1::RectF(margin, bar_top, kBaseWidth - margin, bar_bottom));
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
        if (song_select_style) {
            draw_glass_panel(rect, 18.0f, 0.82f, 0.72f, true, 6.0f);
            draw_song_select_horizon(rect.top - 2.0f, rect.left + 6.0f, rect.right - 6.0f, 460.0f, 0.18f, 0.34f);
        } else {
            if (d2d_->footer_brush) {
                ctx->FillRoundedRectangle(rr, d2d_->footer_brush.Get());
            }
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), 1.5f);
            }
        }

        const std::string profile_text = loc("PROFILE: ", "프로필: ") +
                                         (profile.empty() ? std::string("PLAYER01") : std::string(profile));
        const std::string track_text = loc("TRACK: ", "트랙: ") + (track.empty() ? std::string("-") : std::string(track));
        const std::string score_text = loc("HIGH SCORE  ", "최고 점수  ") + format_int_with_commas(high_score);

        const std::wstring profile_w = to_wide(profile_text);
        const std::wstring track_w = to_wide(track_text);
        const std::wstring score_w = to_wide(score_text);

        if (!d2d_->hud_format || !d2d_->text_brush) {
            return;
        }

        const float text_pad = 22.0f;
        const D2D1_RECT_F left_rect = D2D1::RectF(rect.left + text_pad, rect.top, rect.left + 520.0f, rect.bottom);
        const D2D1_RECT_F center_rect =
            D2D1::RectF(rect.left + 520.0f, rect.top, rect.right - 520.0f, rect.bottom);
        const D2D1_RECT_F right_rect = D2D1::RectF(rect.right - 520.0f, rect.top, rect.right - text_pad, rect.bottom);
        const D2D1_RECT_F clip_rect =
            D2D1::RectF(rect.left + 8.0f, rect.top + 6.0f, rect.right - 8.0f, rect.bottom - 6.0f);

        ctx->PushAxisAlignedClip(clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        draw_text_clipped_aligned(profile_w,
                                  d2d_->hud_format.Get(),
                                  left_rect,
                                  d2d_->text_brush.Get(),
                                  DWRITE_TEXT_ALIGNMENT_LEADING);
        draw_text_clipped_aligned(score_w,
                                  d2d_->hud_format.Get(),
                                  center_rect,
                                  d2d_->text_brush.Get(),
                                  DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_text_clipped_aligned(track_w,
                                  d2d_->hud_format.Get(),
                                  right_rect,
                                  d2d_->text_brush.Get(),
                                  DWRITE_TEXT_ALIGNMENT_TRAILING);
        ctx->PopAxisAlignedClip();
    };

    auto footer_bar_rect = [&](bool song_select_style) {
        const float margin = song_select_style ? 26.0f : 80.0f;
        const float bar_height = song_select_style ? 78.0f : 84.0f;
        const float bar_bottom = kBaseHeight - (song_select_style ? 20.0f : 24.0f);
        const D2D1_RECT_F rect =
            D2D1::RectF(margin, bar_bottom - bar_height, kBaseWidth - margin, bar_bottom);
        // Content bands measure against this, so a moved footer also moves what
        // sits above it. Only draw_footer's non-song-select form is skinnable.
        return song_select_style ? rect : skin_layout_rect(data, "title.footer", rect);
    };

    struct ScreenContentBands {
        float header_top = 0.0f;
        float header_bottom = 0.0f;
        float body_top = 0.0f;
        float body_bottom = 0.0f;
        float footer_top = 0.0f;
    };

    auto make_screen_content_bands = [&](float header_top,
                                         float header_height,
                                         bool song_select_style,
                                         float body_gap,
                                         float footer_gap) {
        const D2D1_RECT_F footer_rect = footer_bar_rect(song_select_style);
        ScreenContentBands bands;
        bands.header_top = header_top;
        bands.header_bottom = header_top + header_height;
        bands.body_top = bands.header_bottom + body_gap;
        bands.footer_top = footer_rect.top;
        bands.body_bottom = std::max(bands.body_top, bands.footer_top - footer_gap);
        return bands;
    };

    auto use_compact_performance_overlay = [&]() {
        return data.kind == MenuScreenKind::SongSelect || data.kind == MenuScreenKind::TitleMenu;
    };

    auto performance_overlay_panel_rect = [&]() {
        const bool compact_overlay = use_compact_performance_overlay();
        const bool gameplay_metrics_visible =
            data.performance.gameplay_metrics_visible && !compact_overlay;
        return compact_overlay
                   ? D2D1::RectF(kBaseWidth - 438.0f, 22.0f, kBaseWidth - 24.0f, 208.0f)
                   : D2D1::RectF(kBaseWidth - 470.0f,
                                 44.0f,
                                 kBaseWidth - 44.0f,
                                 gameplay_metrics_visible ? 592.0f : 432.0f);
    };

    auto performance_overlay_safe_left = [&](float gap) {
        if (!data.performance.visible) {
            return kBaseWidth - 24.0f;
        }
        return performance_overlay_panel_rect().left - gap;
    };

    auto fit_rect_below_performance_overlay = [&](const D2D1_RECT_F& rect,
                                                  float bottom_limit,
                                                  float top_gap) {
        if (!data.performance.visible) {
            return rect;
        }
        const D2D1_RECT_F overlay_rect = performance_overlay_panel_rect();
        if (rect.right <= overlay_rect.left || rect.left >= overlay_rect.right || rect.top >= overlay_rect.bottom) {
            return rect;
        }
        const float height = rect.bottom - rect.top;
        const float new_top = std::max(rect.top, overlay_rect.bottom + top_gap);
        const float max_bottom = std::max(new_top, bottom_limit);
        const float new_bottom = std::min(new_top + height, max_bottom);
        return D2D1::RectF(rect.left, new_top, rect.right, new_bottom);
    };

    auto draw_performance_overlay = [&]() {
        if (!data.performance.visible) {
            return;
        }

        const bool compact_overlay = use_compact_performance_overlay();
        const bool gameplay_metrics_visible =
            data.performance.gameplay_metrics_visible && !compact_overlay;
        const D2D1_RECT_F panel_rect = performance_overlay_panel_rect();
        const D2D1_ROUNDED_RECT panel_rr = D2D1::RoundedRect(panel_rect, 20.0f, 20.0f);
        if (d2d_->panel_brush) {
            d2d_->panel_brush->SetOpacity(0.88f);
            ctx->FillRoundedRectangle(panel_rr, d2d_->panel_brush.Get());
            d2d_->panel_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(panel_rr, d2d_->button_border_brush.Get(), 1.4f);
        }

        const std::wstring title_w = L"FRAME PACING";
        if (d2d_->body_format && d2d_->text_brush) {
            d2d_->body_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            ctx->DrawText(title_w.c_str(), static_cast<UINT32>(title_w.size()),
                          d2d_->body_format.Get(),
                          D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 16.0f,
                                      panel_rect.right - 140.0f, panel_rect.top + 48.0f),
                          d2d_->text_brush.Get());
        }

        const bool graph_changed =
            performance_overlay_cache_.graph_revision != data.performance.graph_revision ||
            performance_overlay_cache_.compact_layout != compact_overlay;
        const bool metrics_changed =
            graph_changed || performance_overlay_cache_.metrics_revision != data.performance.metrics_revision;
        const bool gameplay_metrics_changed =
            performance_overlay_cache_.gameplay_metrics_visible != gameplay_metrics_visible ||
            performance_overlay_cache_.gameplay_metrics_revision != data.performance.gameplay_metrics_revision;

        const D2D1_RECT_F graph_rect = compact_overlay
                                           ? D2D1::RectF(panel_rect.left + 20.0f, panel_rect.top + 52.0f,
                                                         panel_rect.right - 20.0f, panel_rect.top + 116.0f)
                                           : D2D1::RectF(panel_rect.left + 24.0f, panel_rect.top + 58.0f,
                                                         panel_rect.right - 24.0f, panel_rect.top + 198.0f);
        const D2D1_ROUNDED_RECT graph_rr = D2D1::RoundedRect(graph_rect, 14.0f, 14.0f);

        if (graph_changed) {
            performance_overlay_cache_.graph_ceiling_ms = 16.67f;
            d2d_->performance_graph_geometry.Reset();

            if (data.performance.valid && data.performance.graph_sample_count > 0) {
                float max_frame_ms = 0.0f;
                for (std::size_t i = 0; i < data.performance.graph_sample_count; ++i) {
                    max_frame_ms = std::max(max_frame_ms, data.performance.frame_times_ms[i]);
                }
                performance_overlay_cache_.graph_ceiling_ms =
                    std::max(16.67f, std::ceil(std::max(2.0f, max_frame_ms) / 2.0f) * 2.0f);

                if (data.performance.graph_sample_count >= 2 && d2d_->d2d_factory) {
                    Microsoft::WRL::ComPtr<ID2D1PathGeometry> graph_geometry;
                    if (SUCCEEDED(d2d_->d2d_factory->CreatePathGeometry(&graph_geometry))) {
                        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                        if (SUCCEEDED(graph_geometry->Open(&sink))) {
                            const float graph_width = graph_rect.right - graph_rect.left - 20.0f;
                            const float graph_height = graph_rect.bottom - graph_rect.top - 18.0f;
                            const float step =
                                graph_width / static_cast<float>(data.performance.graph_sample_count - 1);

                            auto point_for_sample = [&](std::size_t index) {
                                const float sample_ms = std::clamp(
                                    data.performance.frame_times_ms[index], 0.0f, performance_overlay_cache_.graph_ceiling_ms);
                                const float x = graph_rect.left + 10.0f + step * static_cast<float>(index);
                                const float y =
                                    graph_rect.bottom - 8.0f - (sample_ms / performance_overlay_cache_.graph_ceiling_ms) * graph_height;
                                return D2D1::Point2F(x, y);
                            };

                            sink->BeginFigure(point_for_sample(0), D2D1_FIGURE_BEGIN_HOLLOW);
                            for (std::size_t i = 1; i < data.performance.graph_sample_count; ++i) {
                                sink->AddLine(point_for_sample(i));
                            }
                            sink->EndFigure(D2D1_FIGURE_END_OPEN);
                            if (SUCCEEDED(sink->Close())) {
                                d2d_->performance_graph_geometry = std::move(graph_geometry);
                            }
                        }
                    }
                }
            }

            performance_overlay_cache_.graph_revision = data.performance.graph_revision;
            performance_overlay_cache_.compact_layout = compact_overlay;
        }

        if (metrics_changed || performance_overlay_cache_.sample_text.empty()) {
            performance_overlay_cache_.avg_line_ratio = static_cast<float>(std::clamp(
                data.performance.average_frame_ms / static_cast<double>(performance_overlay_cache_.graph_ceiling_ms), 0.0, 1.0));
            performance_overlay_cache_.sample_text =
                to_wide(std::string("SAMPLES ") + std::to_string(data.performance.sample_count));
            performance_overlay_cache_.top_label_text =
                to_wide(format_decimal(performance_overlay_cache_.graph_ceiling_ms, 2) + " ms");
            performance_overlay_cache_.avg_label_text =
                to_wide("AVG " + format_decimal(data.performance.average_frame_ms, 2));

            const double row_values[] = {
                data.performance.average_frame_ms,
                data.performance.average_fps,
                data.performance.max_fps,
                data.performance.fps_0_1_low,
                data.performance.fps_0_01_low,
            };

            for (int i = 0; i < 5; ++i) {
                performance_overlay_cache_.value_texts[static_cast<std::size_t>(i)] =
                    to_wide(format_decimal(row_values[i], (i == 0) ? 2 : 1));
            }
            performance_overlay_cache_.metrics_revision = data.performance.metrics_revision;
        }
        if (gameplay_metrics_changed) {
            const double gameplay_values[] = {
                data.performance.gameplay_audio_age_ms,
                data.performance.gameplay_hud_delta_ms,
                data.performance.gameplay_extrapolated_ms,
                data.performance.gameplay_buffer_ms,
            };
            for (int i = 0; i < 4; ++i) {
                performance_overlay_cache_.gameplay_value_texts[static_cast<std::size_t>(i)] =
                    to_wide(format_decimal(gameplay_values[i], 2));
            }
            performance_overlay_cache_.gameplay_metrics_visible = gameplay_metrics_visible;
            performance_overlay_cache_.gameplay_metrics_revision = data.performance.gameplay_metrics_revision;
        }

        if (d2d_->mono_format && d2d_->muted_brush) {
            d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            ctx->DrawText(performance_overlay_cache_.sample_text.c_str(),
                          static_cast<UINT32>(performance_overlay_cache_.sample_text.size()),
                          d2d_->mono_format.Get(),
                          D2D1::RectF(panel_rect.left + 160.0f, panel_rect.top + 16.0f,
                                      panel_rect.right - 24.0f, panel_rect.top + 48.0f),
                          d2d_->muted_brush.Get());
            d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        if (d2d_->card_brush) {
            d2d_->card_brush->SetOpacity(0.80f);
            ctx->FillRoundedRectangle(graph_rr, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(graph_rr, d2d_->button_border_brush.Get(), 1.1f);
        }

        if (!data.performance.valid || data.performance.graph_sample_count == 0) {
            const std::wstring waiting_w = L"Collecting frame samples...";
            if (d2d_->hud_format && d2d_->muted_brush) {
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ctx->DrawText(waiting_w.c_str(), static_cast<UINT32>(waiting_w.size()),
                              d2d_->hud_format.Get(), graph_rect, d2d_->muted_brush.Get());
                d2d_->hud_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            return;
        }

        const float avg_line_y = graph_rect.bottom - performance_overlay_cache_.avg_line_ratio *
                                                       (graph_rect.bottom - graph_rect.top);

        if (d2d_->muted_brush) {
            for (int i = 0; i < 4; ++i) {
                const float t = static_cast<float>(i) / 3.0f;
                const float y = graph_rect.bottom - t * (graph_rect.bottom - graph_rect.top);
                d2d_->muted_brush->SetOpacity((i == 0 || i == 3) ? 0.18f : 0.10f);
                ctx->DrawLine(D2D1::Point2F(graph_rect.left + 10.0f, y),
                              D2D1::Point2F(graph_rect.right - 10.0f, y),
                              d2d_->muted_brush.Get(), 1.0f);
            }
            d2d_->muted_brush->SetOpacity(1.0f);
        }
        if (d2d_->button_selected_brush) {
            const float original_opacity = d2d_->button_selected_brush->GetOpacity();
            d2d_->button_selected_brush->SetOpacity(0.55f);
            ctx->DrawLine(D2D1::Point2F(graph_rect.left + 10.0f, avg_line_y),
                          D2D1::Point2F(graph_rect.right - 10.0f, avg_line_y),
                          d2d_->button_selected_brush.Get(), 1.2f);
            d2d_->button_selected_brush->SetOpacity(original_opacity);
        }

        if (d2d_->mono_format && d2d_->muted_brush) {
            ctx->DrawText(performance_overlay_cache_.top_label_text.c_str(),
                          static_cast<UINT32>(performance_overlay_cache_.top_label_text.size()),
                          d2d_->mono_format.Get(),
                          D2D1::RectF(graph_rect.left + 12.0f, graph_rect.top + 6.0f,
                                      graph_rect.left + 130.0f, graph_rect.top + 28.0f),
                          d2d_->muted_brush.Get());
            ctx->DrawText(performance_overlay_cache_.avg_label_text.c_str(),
                          static_cast<UINT32>(performance_overlay_cache_.avg_label_text.size()),
                          d2d_->mono_format.Get(),
                          D2D1::RectF(graph_rect.right - 160.0f, avg_line_y - 18.0f,
                                      graph_rect.right - 12.0f, avg_line_y + 12.0f),
                          d2d_->muted_brush.Get());
        }

        if (d2d_->performance_graph_geometry.Get() && d2d_->accent_brush) {
            ctx->DrawGeometry(d2d_->performance_graph_geometry.Get(), d2d_->accent_brush.Get(), 1.8f);
        }

        if (compact_overlay) {
            const float stats_top = graph_rect.bottom + 8.0f;
            const float column_gap = 18.0f;
            const float column_width = (panel_rect.right - panel_rect.left - 40.0f - column_gap) * 0.5f;
            const float row_height = 22.0f;

            auto draw_metric_cell = [&](const wchar_t* label, const std::wstring& value_text,
                                        const D2D1_POINT_2F& origin) {
                const D2D1_RECT_F cell_rect =
                    D2D1::RectF(origin.x, origin.y, origin.x + column_width, origin.y + row_height);
                const D2D1_RECT_F label_rect =
                    D2D1::RectF(cell_rect.left, cell_rect.top, cell_rect.right - 72.0f, cell_rect.bottom);
                const D2D1_RECT_F value_rect =
                    D2D1::RectF(cell_rect.right - 78.0f, cell_rect.top, cell_rect.right, cell_rect.bottom);
                if (d2d_->body_format && d2d_->muted_brush) {
                    ctx->DrawText(label, static_cast<UINT32>(wcslen(label)),
                                  d2d_->body_format.Get(), label_rect, d2d_->muted_brush.Get());
                }
                if (d2d_->mono_format && d2d_->text_brush) {
                    d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                    ctx->DrawText(value_text.c_str(), static_cast<UINT32>(value_text.size()),
                                  d2d_->mono_format.Get(), value_rect, d2d_->text_brush.Get());
                    d2d_->mono_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            };

            const float left_x = panel_rect.left + 20.0f;
            const float right_x = left_x + column_width + column_gap;
            draw_metric_cell(L"AVG MS", performance_overlay_cache_.value_texts[0], D2D1::Point2F(left_x, stats_top));
            draw_metric_cell(L"AVG FPS", performance_overlay_cache_.value_texts[1], D2D1::Point2F(right_x, stats_top));
            draw_metric_cell(L"MAX FPS", performance_overlay_cache_.value_texts[2],
                             D2D1::Point2F(left_x, stats_top + row_height));
            draw_metric_cell(L"0.1% FPS", performance_overlay_cache_.value_texts[3],
                             D2D1::Point2F(right_x, stats_top + row_height));
            draw_metric_cell(L"0.01% FPS", performance_overlay_cache_.value_texts[4],
                             D2D1::Point2F(left_x, stats_top + row_height * 2.0f));
            return;
        }

        constexpr const wchar_t* kPerfRowLabels[] = {
            L"AVG MS",
            L"AVG FPS",
            L"MAX FPS",
            L"0.1% FPS",
            L"0.01% FPS",
        };
        constexpr const wchar_t* kGameplayTimingLabels[] = {
            L"AUDIO AGE",
            L"HUD DELTA",
            L"EXTRAP",
            L"BUFFER",
        };

        const float stats_top = graph_rect.bottom + 18.0f;
        const float row_height = 33.0f;
        for (int i = 0; i < 5; ++i) {
            const float top = stats_top + row_height * static_cast<float>(i);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(panel_rect.left + 24.0f, top, panel_rect.left + 170.0f, top + row_height);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(panel_rect.left + 180.0f, top, panel_rect.right - 24.0f, top + row_height);
            if (d2d_->stats_label_format && d2d_->muted_brush) {
                ctx->DrawText(kPerfRowLabels[i], static_cast<UINT32>(wcslen(kPerfRowLabels[i])),
                              d2d_->stats_label_format.Get(), label_rect, d2d_->muted_brush.Get());
            }
            if (d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring& value_text = performance_overlay_cache_.value_texts[static_cast<std::size_t>(i)];
                ctx->DrawText(value_text.c_str(), static_cast<UINT32>(value_text.size()),
                              d2d_->stats_value_format.Get(), value_rect, d2d_->text_brush.Get());
            }
        }

        if (!gameplay_metrics_visible) {
            return;
        }

        const float section_top = stats_top + row_height * 5.0f + 18.0f;
        if (d2d_->button_border_brush) {
            d2d_->button_border_brush->SetOpacity(0.55f);
            ctx->DrawLine(D2D1::Point2F(panel_rect.left + 24.0f, section_top - 8.0f),
                          D2D1::Point2F(panel_rect.right - 24.0f, section_top - 8.0f),
                          d2d_->button_border_brush.Get(),
                          1.0f);
            d2d_->button_border_brush->SetOpacity(1.0f);
        }
        if (d2d_->body_format && d2d_->text_brush) {
            const std::wstring gameplay_title_w = L"GAMEPLAY TIMING";
            ctx->DrawText(gameplay_title_w.c_str(),
                          static_cast<UINT32>(gameplay_title_w.size()),
                          d2d_->body_format.Get(),
                          D2D1::RectF(panel_rect.left + 24.0f, section_top, panel_rect.right - 24.0f, section_top + 28.0f),
                          d2d_->text_brush.Get());
        }

        const float gameplay_rows_top = section_top + 30.0f;
        for (int i = 0; i < 4; ++i) {
            const float top = gameplay_rows_top + row_height * static_cast<float>(i);
            const D2D1_RECT_F label_rect =
                D2D1::RectF(panel_rect.left + 24.0f, top, panel_rect.left + 170.0f, top + row_height);
            const D2D1_RECT_F value_rect =
                D2D1::RectF(panel_rect.left + 180.0f, top, panel_rect.right - 24.0f, top + row_height);
            if (d2d_->stats_label_format && d2d_->muted_brush) {
                ctx->DrawText(kGameplayTimingLabels[i],
                              static_cast<UINT32>(wcslen(kGameplayTimingLabels[i])),
                              d2d_->stats_label_format.Get(),
                              label_rect,
                              d2d_->muted_brush.Get());
            }
            if (d2d_->stats_value_format && d2d_->text_brush) {
                const std::wstring& value_text =
                    performance_overlay_cache_.gameplay_value_texts[static_cast<std::size_t>(i)];
                ctx->DrawText(value_text.c_str(),
                              static_cast<UINT32>(value_text.size()),
                              d2d_->stats_value_format.Get(),
                              value_rect,
                              d2d_->text_brush.Get());
            }
        }
    };

    auto draw_generic_list = [&]() {
#include "MenuWindow_draw_generic_body.inl"
    };

    auto draw_title_menu = [&]() {
#include "MenuWindow_draw_title_body.inl"
    };

    auto draw_song_select = [&]() {
#include "MenuWindow_draw_songselect_body.inl"
    };

    auto draw_help_overlay = [&]() {
#include "MenuWindow_draw_help_body.inl"
    };

    auto draw_result_screen = [&]() {
#include "MenuWindow_draw_result_body.inl"
    };

    auto draw_gameplay_hud = [&]() {
#include "MenuWindow_draw_gameplay_body.inl"
    };

    auto draw_loading_progress = [&]() {
        if (!data.loading_progress.visible) {
            return;
        }
        constexpr float bar_height = 12.0f;
        const D2D1_RECT_F track = D2D1::RectF(0.0f, 0.0f, kBaseWidth, bar_height);
        if (d2d_->card_brush) {
            const float saved_opacity = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetOpacity(0.96f);
            ctx->FillRectangle(track, d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved_opacity);
        }
        if (d2d_->accent_brush) {
            D2D1_RECT_F fill = track;
            if (data.loading_progress.percent >= 0) {
                const float ratio = static_cast<float>(std::clamp(
                    data.loading_progress.percent, 0, 100)) / 100.0f;
                fill.right = kBaseWidth * ratio;
            } else {
                constexpr float segment_width = 360.0f;
                const double seconds = static_cast<double>(render_now_ns) / 1'000'000'000.0;
                const float phase = static_cast<float>(std::fmod(seconds * 0.65, 1.0));
                fill.left = phase * (kBaseWidth + segment_width) - segment_width;
                fill.right = fill.left + segment_width;
            }
            ctx->FillRectangle(fill, d2d_->accent_brush.Get());
        }
        if (d2d_->hud_format && d2d_->text_brush) {
            std::string label = data.loading_progress.stage.empty()
                                    ? std::string("INDEXING")
                                    : data.loading_progress.stage;
            if (data.loading_progress.percent >= 0) {
                label += "  " + std::to_string(data.loading_progress.percent) + "%";
            }
            if (data.loading_progress.total > 0) {
                label += "  " + std::to_string(data.loading_progress.processed) +
                         "/" + std::to_string(data.loading_progress.total);
            }
            if (!data.loading_progress.eta.empty()) {
                label += "  ETA " + data.loading_progress.eta;
            }
            draw_text_clipped_aligned(to_wide(label),
                                      d2d_->hud_format.Get(),
                                      D2D1::RectF(24.0f, 14.0f, kBaseWidth - 24.0f, 42.0f),
                                      d2d_->text_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
    };

    switch (data.kind) {
        case MenuScreenKind::TitleMenu:
            draw_title_menu();
            break;
        case MenuScreenKind::SongSelect:
            draw_song_select();
            break;
        case MenuScreenKind::ResultScreen:
            draw_result_screen();
            break;
        case MenuScreenKind::GameplayHud:
            draw_gameplay_hud();
            break;
        case MenuScreenKind::GenericList:
        default:
            draw_generic_list();
            break;
    }

    draw_loading_progress();

    if (!data.help_overlay.visible) {
        draw_performance_overlay();
    }
    draw_help_overlay();

    const HRESULT hr = ctx->EndDraw();
    if (FAILED(hr)) {
        std::cerr << "[MenuWindow::draw] EndDraw failed hr=0x" << std::hex
                  << static_cast<unsigned long>(hr) << std::dec << std::endl;
    }
    if (hr == D2DERR_RECREATE_TARGET || hr == D2DERR_WRONG_STATE) {
        if (!recreate_targets()) {
            fail_fatal("Failed to recreate render target after device state change.");
            shutdown();
            return;
        }
    }
    if (SUCCEEDED(hr) &&
        screenshot_requested_.exchange(false, std::memory_order_acq_rel)) {
        (void)save_screenshot_to_png();
    }

    UINT present_flags = 0;
    if (app::should_allow_tearing_present(
            config_.vsync,
            fullscreen_,
            (swap_chain_flags_ & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0)) {
        present_flags = DXGI_PRESENT_ALLOW_TEARING;
    }
    const HRESULT present_hr = d2d_->swap_chain->Present(config_.vsync ? 1 : 0, present_flags);
    if (present_hr == DXGI_ERROR_DEVICE_REMOVED || present_hr == DXGI_ERROR_DEVICE_RESET) {
        std::cerr << "[MenuWindow::draw] Present failed: device removed/reset hr=0x" << std::hex
                  << static_cast<unsigned long>(present_hr) << std::dec << std::endl;
        fail_fatal("Graphics device was removed/reset. Update GPU drivers and attach logs/run.log.");
        shutdown();
        return;
    }
    if (FAILED(present_hr)) {
        const HWND hwnd = static_cast<HWND>(hwnd_);
        const bool window_minimized = hwnd && IsIconic(hwnd);
        const bool window_in_foreground = is_input_foreground();
        bool client_size_mismatch = false;
        if (hwnd) {
            RECT client_rect{};
            if (GetClientRect(hwnd, &client_rect)) {
                const unsigned int client_width =
                    static_cast<unsigned int>(std::max(0L, client_rect.right - client_rect.left));
                const unsigned int client_height =
                    static_cast<unsigned int>(std::max(0L, client_rect.bottom - client_rect.top));
                const bool fullscreen_invalid_call =
                    static_cast<std::uint32_t>(present_hr) == kDxgiErrorInvalidCall &&
                    config_.display_mode == "fullscreen";
                client_size_mismatch =
                    client_width > 0 && client_height > 0 &&
                    (client_width != width_ || client_height != height_);
                if (static_cast<std::uint32_t>(present_hr) == kDxgiErrorInvalidCall &&
                    (client_size_mismatch || fullscreen_restore_pending_ || fullscreen_invalid_call)) {
                    pending_width_ = fullscreen_invalid_call && width_ > 0 ? width_ : client_width;
                    pending_height_ = fullscreen_invalid_call && height_ > 0 ? height_ : client_height;
                    resize_pending_ = true;
                    std::cerr << "[MenuWindow::draw] Present returned retryable invalid call hr=0x"
                              << std::hex << static_cast<unsigned long>(present_hr) << std::dec
                              << " client=" << client_width << "x" << client_height
                              << " current=" << width_ << "x" << height_
                              << " pending=" << pending_width_ << "x" << pending_height_
                              << " mode=" << config_.display_mode << std::endl;
                    return;
                }
            }
        }
        if (app::should_treat_present_failure_as_transient(
                static_cast<std::uint32_t>(present_hr),
                config_.display_mode == "fullscreen",
                window_in_foreground,
                window_minimized)) {
            if (config_.display_mode == "fullscreen") {
                fullscreen_ = false;
                fullscreen_restore_pending_ = true;
            }
            std::cerr << "[MenuWindow::draw] Present returned transient hr=0x" << std::hex
                      << static_cast<unsigned long>(present_hr) << std::dec << std::endl;
            return;
        }
        std::cerr << "[MenuWindow::draw] Present failed hr=0x" << std::hex
                  << static_cast<unsigned long>(present_hr) << std::dec << std::endl;
        fail_fatal("Failed to present the menu frame. Attach logs/run.log.");
        shutdown();
        return;
    }
    if (app::should_record_presented_frame(static_cast<std::uint32_t>(present_hr))) {
        last_present_completion_ns_.store(timing::HighResClock::now_ns(), std::memory_order_release);
    }
}
