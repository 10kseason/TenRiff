        const float target = data.chat_overlay.visible ? 1.0f : 0.0f;
        if (chat_overlay_last_frame_ns_ <= 0) {
            chat_overlay_last_frame_ns_ = render_now_ns;
        }
        const int64_t elapsed_ns = std::clamp<int64_t>(
            render_now_ns - chat_overlay_last_frame_ns_, 0, 50'000'000LL);
        chat_overlay_last_frame_ns_ = render_now_ns;
        constexpr float animation_ns = 180'000'000.0f;
        const float step = static_cast<float>(elapsed_ns) / animation_ns;
        if (target > chat_overlay_slide_) {
            chat_overlay_slide_ = std::min(target, chat_overlay_slide_ + step);
        } else if (target < chat_overlay_slide_) {
            chat_overlay_slide_ = std::max(target, chat_overlay_slide_ - step);
        }
        if (chat_overlay_slide_ <= 0.001f && !data.chat_overlay.visible) {
            return;
        }

        const float eased = chat_overlay_slide_ * chat_overlay_slide_ *
                            (3.0f - 2.0f * chat_overlay_slide_);
        constexpr float panel_height = 340.0f;
        const float panel_bottom = kBaseHeight - 26.0f +
                                   panel_height * (1.0f - eased);
        const D2D1_RECT_F panel_rect = D2D1::RectF(
            190.0f, panel_bottom - panel_height,
            kBaseWidth - 190.0f, panel_bottom);
        if (d2d_->card_brush) {
            const float saved_opacity = d2d_->card_brush->GetOpacity();
            d2d_->card_brush->SetOpacity(0.96f);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(panel_rect, 8.0f, 8.0f),
                                      d2d_->card_brush.Get());
            d2d_->card_brush->SetOpacity(saved_opacity);
        }
        draw_glass_panel(panel_rect, 8.0f, 0.96f, 0.72f, true, 8.0f);

        if (d2d_->title_format && d2d_->text_brush) {
            draw_text_clipped(
                to_wide(data.chat_overlay.title), d2d_->title_format.Get(),
                D2D1::RectF(panel_rect.left + 28.0f, panel_rect.top + 18.0f,
                            panel_rect.left + 510.0f, panel_rect.top + 60.0f),
                d2d_->text_brush.Get());
        }
        if (d2d_->hud_format && d2d_->muted_brush) {
            draw_text_clipped_aligned(
                to_wide(data.chat_overlay.status), d2d_->hud_format.Get(),
                D2D1::RectF(panel_rect.left + 520.0f, panel_rect.top + 24.0f,
                            panel_rect.right - 28.0f, panel_rect.top + 58.0f),
                d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
        if (d2d_->accent_brush) {
            const float saved_opacity = d2d_->accent_brush->GetOpacity();
            d2d_->accent_brush->SetOpacity(data.chat_overlay.connected ? 0.72f : 0.22f);
            ctx->DrawLine(
                D2D1::Point2F(panel_rect.left + 28.0f, panel_rect.top + 66.0f),
                D2D1::Point2F(panel_rect.right - 28.0f, panel_rect.top + 66.0f),
                d2d_->accent_brush.Get(), 1.3f);
            d2d_->accent_brush->SetOpacity(saved_opacity);
        }

        float message_y = panel_rect.top + 78.0f;
        if (d2d_->body_format && d2d_->text_brush) {
            for (std::size_t index = 0; index < data.chat_overlay.messages.size(); ++index) {
                const auto& message = data.chat_overlay.messages[index];
                if (message_y + 34.0f > panel_rect.top + 248.0f) break;
                const bool has_url = index < data.chat_overlay.message_urls.size() &&
                                     !data.chat_overlay.message_urls[index].empty();
                const D2D1_RECT_F message_rect = D2D1::RectF(
                    panel_rect.left + 32.0f, message_y,
                    panel_rect.right - 32.0f, message_y + 34.0f);
                draw_text_clipped(
                    to_wide(message), d2d_->body_format.Get(),
                    message_rect,
                    has_url && d2d_->accent_brush
                        ? d2d_->accent_brush.Get()
                        : d2d_->text_brush.Get());
                if (has_url) {
                    hit_regions_.push_back(HitRegion{
                        MenuHitTargetKind::ChatUrl, static_cast<int>(index),
                        MenuHitPart::Activate, message_rect.left, message_rect.top,
                        message_rect.right, message_rect.bottom});
                    if (d2d_->accent_brush) {
                        ctx->DrawLine(D2D1::Point2F(message_rect.left, message_rect.bottom - 3.0f),
                                      D2D1::Point2F(message_rect.right, message_rect.bottom - 3.0f),
                                      d2d_->accent_brush.Get(), 0.8f);
                    }
                }
                message_y += 34.0f;
            }
        }

        const D2D1_RECT_F input_rect = D2D1::RectF(
            panel_rect.left + 28.0f, panel_rect.bottom - 78.0f,
            panel_rect.right - 28.0f, panel_rect.bottom - 30.0f);
        if (d2d_->button_brush) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(input_rect, 6.0f, 6.0f),
                                      d2d_->button_brush.Get());
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(input_rect, 6.0f, 6.0f),
                                      d2d_->button_border_brush.Get(), 1.2f);
        }
        if (d2d_->body_format && d2d_->text_brush) {
            draw_text_clipped(
                to_wide(data.chat_overlay.input), d2d_->body_format.Get(),
                D2D1::RectF(input_rect.left + 16.0f, input_rect.top + 7.0f,
                            input_rect.left + 900.0f, input_rect.bottom - 5.0f),
                data.chat_overlay.connected ? d2d_->text_brush.Get()
                                            : d2d_->muted_brush.Get());
        }
        if (d2d_->hud_format && d2d_->muted_brush) {
            draw_text_clipped_aligned(
                to_wide(data.chat_overlay.hint), d2d_->hud_format.Get(),
                D2D1::RectF(input_rect.left + 930.0f, input_rect.top + 8.0f,
                            input_rect.right - 14.0f, input_rect.bottom - 5.0f),
                d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
