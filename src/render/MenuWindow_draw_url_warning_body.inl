        const float warning_target = data.url_warning_overlay.visible ? 1.0f : 0.0f;
        if (url_warning_last_frame_ns_ <= 0) url_warning_last_frame_ns_ = render_now_ns;
        const int64_t warning_elapsed = std::clamp<int64_t>(
            render_now_ns - url_warning_last_frame_ns_, 0, 50'000'000LL);
        url_warning_last_frame_ns_ = render_now_ns;
        const float warning_step = static_cast<float>(warning_elapsed) / 170'000'000.0f;
        url_warning_visibility_ += std::clamp(
            warning_target - url_warning_visibility_, -warning_step, warning_step);
        url_warning_visibility_ = std::clamp(url_warning_visibility_, 0.0f, 1.0f);
        if (url_warning_visibility_ <= 0.001f && !data.url_warning_overlay.visible) return;

        const float eased = url_warning_visibility_ * url_warning_visibility_ *
                            (3.0f - 2.0f * url_warning_visibility_);
        if (d2d_->footer_brush) {
            const float saved = d2d_->footer_brush->GetOpacity();
            d2d_->footer_brush->SetOpacity(0.72f * eased);
            ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight),
                               d2d_->footer_brush.Get());
            d2d_->footer_brush->SetOpacity(saved);
        }
        const float width = 780.0f;
        const float height = 430.0f;
        const float scale = 0.94f + 0.06f * eased;
        const D2D1_RECT_F modal = D2D1::RectF(
            (kBaseWidth - width * scale) * 0.5f,
            (kBaseHeight - height * scale) * 0.5f,
            (kBaseWidth + width * scale) * 0.5f,
            (kBaseHeight + height * scale) * 0.5f);
        draw_glass_panel(modal, 8.0f, 0.99f * eased, 0.92f * eased, true, 14.0f);

        if (d2d_->header_format && d2d_->text_brush) {
            draw_text_clipped(to_wide(loc("OPEN EXTERNAL LINK?", "외부 링크를 열까요?")),
                              d2d_->header_format.Get(),
                              D2D1::RectF(modal.left + 38.0f, modal.top + 30.0f,
                                          modal.right - 38.0f, modal.top + 80.0f),
                              d2d_->text_brush.Get());
        }
        if (d2d_->body_format && d2d_->muted_brush) {
            draw_text_clipped(
                to_wide(loc("This link leaves TenRiff. It may contain unsafe or malicious content.",
                            "이 링크는 TenRiff 외부로 연결됩니다. 위험하거나 악성인 콘텐츠일 수 있습니다.")),
                d2d_->body_format.Get(),
                D2D1::RectF(modal.left + 40.0f, modal.top + 96.0f,
                            modal.right - 40.0f, modal.top + 146.0f),
                d2d_->muted_brush.Get());
        }
        const D2D1_RECT_F url_rect = D2D1::RectF(
            modal.left + 40.0f, modal.top + 170.0f,
            modal.right - 40.0f, modal.top + 236.0f);
        if (d2d_->button_brush) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(url_rect, 6.0f, 6.0f),
                                      d2d_->button_brush.Get());
        }
        if (d2d_->body_format && d2d_->accent_brush) {
            draw_text_clipped(to_wide(data.url_warning_overlay.url),
                              d2d_->body_format.Get(),
                              D2D1::RectF(url_rect.left + 16.0f, url_rect.top + 13.0f,
                                          url_rect.right - 16.0f, url_rect.bottom - 10.0f),
                              d2d_->accent_brush.Get());
        }
        const float button_top = modal.bottom - 92.0f;
        const D2D1_RECT_F cancel = D2D1::RectF(
            modal.left + 40.0f, button_top, modal.left + 342.0f, button_top + 56.0f);
        const D2D1_RECT_F open = D2D1::RectF(
            modal.right - 342.0f, button_top, modal.right - 40.0f, button_top + 56.0f);
        auto warning_button = [&](const D2D1_RECT_F& rect, std::string_view label,
                                  bool primary, int index) {
            ID2D1SolidColorBrush* fill = primary && d2d_->button_selected_brush
                                             ? d2d_->button_selected_brush.Get()
                                             : d2d_->button_brush.Get();
            if (fill) ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), fill);
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f),
                                          d2d_->button_border_brush.Get(), 1.2f);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped_aligned(to_wide(std::string(label)), d2d_->body_format.Get(), rect,
                                          primary && d2d_->accent_brush
                                              ? d2d_->accent_brush.Get()
                                              : d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            if (data.url_warning_overlay.visible) {
                hit_regions_.push_back(HitRegion{
                    MenuHitTargetKind::UrlWarningButton, index, MenuHitPart::Activate,
                    rect.left, rect.top, rect.right, rect.bottom});
            }
        };
        warning_button(cancel, loc("CANCEL", "취소"), false, 0);
        warning_button(open, loc("OPEN LINK", "링크 열기"), true, 1);
