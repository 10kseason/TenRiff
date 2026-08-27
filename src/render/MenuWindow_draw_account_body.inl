        const float account_target = data.account_overlay.visible ? 1.0f : 0.0f;
        if (account_overlay_last_frame_ns_ <= 0) account_overlay_last_frame_ns_ = render_now_ns;
        const int64_t account_elapsed = std::clamp<int64_t>(
            render_now_ns - account_overlay_last_frame_ns_, 0, 50'000'000LL);
        account_overlay_last_frame_ns_ = render_now_ns;
        const float account_step = static_cast<float>(account_elapsed) / 180'000'000.0f;
        account_overlay_visibility_ += std::clamp(
            account_target - account_overlay_visibility_, -account_step, account_step);
        account_overlay_visibility_ = std::clamp(account_overlay_visibility_, 0.0f, 1.0f);
        if (account_overlay_visibility_ <= 0.001f && !data.account_overlay.visible) return;

        const float eased = account_overlay_visibility_ * account_overlay_visibility_ *
                            (3.0f - 2.0f * account_overlay_visibility_);
        if (d2d_->footer_brush) {
            const float saved = d2d_->footer_brush->GetOpacity();
            d2d_->footer_brush->SetOpacity(0.56f * eased);
            ctx->FillRectangle(D2D1::RectF(0.0f, 0.0f, kBaseWidth, kBaseHeight),
                               d2d_->footer_brush.Get());
            d2d_->footer_brush->SetOpacity(saved);
        }

        const float width = 820.0f;
        const float height = data.account_overlay.signed_in ? 470.0f : 700.0f;
        const float scale = 0.96f + 0.04f * eased;
        const float left = (kBaseWidth - width * scale) * 0.5f;
        const float top = (kBaseHeight - height * scale) * 0.5f;
        const D2D1_RECT_F modal = D2D1::RectF(
            left, top, left + width * scale, top + height * scale);
        draw_glass_panel(modal, 8.0f, 0.98f * eased, 0.88f * eased, true, 12.0f);

        auto account_button = [&](const D2D1_RECT_F& rect, std::string_view label,
                                  bool selected, MenuHitTargetKind kind, int index) {
            ID2D1SolidColorBrush* fill = selected && d2d_->button_selected_brush
                                             ? d2d_->button_selected_brush.Get()
                                             : d2d_->button_brush.Get();
            if (fill) ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), fill);
            if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f),
                                          d2d_->button_border_brush.Get(), 1.2f);
            }
            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped_aligned(to_wide(std::string(label)), d2d_->body_format.Get(), rect,
                                          selected && d2d_->accent_brush
                                              ? d2d_->accent_brush.Get()
                                              : d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            if (data.account_overlay.visible) {
                hit_regions_.push_back(HitRegion{
                    kind, index, MenuHitPart::Activate,
                    rect.left, rect.top, rect.right, rect.bottom});
            }
        };

        if (d2d_->title_format && d2d_->text_brush) {
            draw_text_clipped(to_wide(loc("TENRIFF ACCOUNT", "텐리프 계정")),
                              d2d_->title_format.Get(),
                              D2D1::RectF(modal.left + 38.0f, modal.top + 24.0f,
                                          modal.right - 38.0f, modal.top + 72.0f),
                              d2d_->text_brush.Get());
        }
        const float server_top = modal.top + 88.0f;
        const float server_button_width = (modal.right - modal.left - 86.0f) * 0.5f;
        account_button(D2D1::RectF(modal.left + 38.0f, server_top,
                                   modal.left + 38.0f + server_button_width,
                                   server_top + 48.0f),
                       loc("TENRIFF MAIN", "텐리프 메인"),
                       !data.account_overlay.private_server,
                       MenuHitTargetKind::AccountServer, 0);
        account_button(D2D1::RectF(modal.left + 48.0f + server_button_width, server_top,
                                   modal.right - 38.0f, server_top + 48.0f),
                       loc("PRIVATE API", "사설 API"),
                       data.account_overlay.private_server,
                       MenuHitTargetKind::AccountServer, 1);
        const D2D1_RECT_F server_rect = D2D1::RectF(
            modal.left + 40.0f, modal.top + 150.0f,
            modal.right - 40.0f, modal.top + 208.0f);
        if (d2d_->button_brush) {
            ctx->FillRoundedRectangle(D2D1::RoundedRect(server_rect, 6.0f, 6.0f),
                                      d2d_->button_brush.Get());
        }
        if (d2d_->button_border_brush) {
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(server_rect, 6.0f, 6.0f),
                                      d2d_->button_border_brush.Get(), 1.2f);
        }
        if (d2d_->body_format && d2d_->text_brush) {
            const std::string& server_value = data.account_overlay.private_server
                                                  ? data.account_overlay.server_url
                                                  : data.account_overlay.main_server_url;
            draw_text_clipped(to_wide(server_value +
                                      (data.account_overlay.private_server &&
                                               data.account_overlay.focused_field == 0
                                           ? " _" : "")),
                              d2d_->body_format.Get(),
                              D2D1::RectF(server_rect.left + 16.0f, server_rect.top + 11.0f,
                                          server_rect.right - 16.0f, server_rect.bottom - 8.0f),
                              data.account_overlay.private_server && d2d_->accent_brush
                                  ? d2d_->accent_brush.Get()
                                  : d2d_->text_brush.Get());
        }
        if (data.account_overlay.visible && data.account_overlay.private_server &&
            !data.account_overlay.signed_in) {
            hit_regions_.push_back(HitRegion{
                MenuHitTargetKind::AccountField, 0, MenuHitPart::Activate,
                server_rect.left, server_rect.top, server_rect.right, server_rect.bottom});
        }

        if (data.account_overlay.signed_in) {
            if (d2d_->header_format && d2d_->accent_brush) {
                draw_text_clipped(
                    to_wide(data.account_overlay.signed_in_as), d2d_->header_format.Get(),
                    D2D1::RectF(modal.left + 40.0f, modal.top + 232.0f,
                                modal.right - 40.0f, modal.top + 285.0f),
                    d2d_->accent_brush.Get());
            }
            if (d2d_->body_format && d2d_->muted_brush) {
                const std::string role = data.account_overlay.role == "admin"
                                             ? loc("Administrator", "관리자")
                                             : (data.account_overlay.role.empty()
                                                    ? loc("Saved account", "저장된 계정")
                                                    : loc("Player account", "플레이어 계정"));
                draw_text_clipped(to_wide(role), d2d_->body_format.Get(),
                                  D2D1::RectF(modal.left + 40.0f, modal.top + 296.0f,
                                              modal.right - 40.0f, modal.top + 335.0f),
                                  d2d_->muted_brush.Get());
            }
            account_button(D2D1::RectF(modal.left + 40.0f, modal.bottom - 102.0f,
                                       modal.right - 40.0f, modal.bottom - 46.0f),
                           loc("LOG OUT", "로그아웃"), false,
                           MenuHitTargetKind::AccountAction, 1);
        } else {
            const float tab_top = modal.top + 238.0f;
            const float tab_width = (modal.right - modal.left - 86.0f) * 0.5f;
            account_button(D2D1::RectF(modal.left + 38.0f, tab_top,
                                       modal.left + 38.0f + tab_width, tab_top + 48.0f),
                           loc("LOGIN", "로그인"), !data.account_overlay.register_mode,
                           MenuHitTargetKind::AccountTab, 0);
            account_button(D2D1::RectF(modal.left + 48.0f + tab_width, tab_top,
                                       modal.right - 38.0f, tab_top + 48.0f),
                           loc("REGISTER", "회원가입"), data.account_overlay.register_mode,
                           MenuHitTargetKind::AccountTab, 1);

            auto field = [&](float y, std::string_view label, const std::string& value, int index) {
                if (d2d_->hud_format && d2d_->muted_brush) {
                    draw_text_clipped(to_wide(std::string(label)), d2d_->hud_format.Get(),
                                      D2D1::RectF(modal.left + 42.0f, y - 28.0f,
                                                  modal.right - 42.0f, y - 4.0f),
                                      d2d_->muted_brush.Get());
                }
                const D2D1_RECT_F rect = D2D1::RectF(
                    modal.left + 40.0f, y, modal.right - 40.0f, y + 58.0f);
                if (d2d_->button_brush) {
                    ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f),
                                              d2d_->button_brush.Get());
                }
                ID2D1SolidColorBrush* border =
                    data.account_overlay.focused_field == index && d2d_->accent_brush
                        ? d2d_->accent_brush.Get()
                        : d2d_->button_border_brush.Get();
                if (border) ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), border, 1.4f);
                if (d2d_->body_format && d2d_->text_brush) {
                    draw_text_clipped(to_wide(value + (data.account_overlay.focused_field == index ? " _" : "")),
                                      d2d_->body_format.Get(),
                                      D2D1::RectF(rect.left + 18.0f, rect.top + 11.0f,
                                                  rect.right - 18.0f, rect.bottom - 8.0f),
                                      d2d_->text_brush.Get());
                }
                if (data.account_overlay.visible) {
                    hit_regions_.push_back(HitRegion{
                        MenuHitTargetKind::AccountField, index, MenuHitPart::Activate,
                        rect.left, rect.top, rect.right, rect.bottom});
                }
            };
            field(modal.top + 346.0f, loc("USERNAME", "아이디"),
                  data.account_overlay.username, 1);
            field(modal.top + 460.0f, loc("PASSWORD", "비밀번호"),
                  data.account_overlay.password_mask, 2);
            account_button(D2D1::RectF(modal.left + 40.0f, modal.bottom - 104.0f,
                                       modal.right - 40.0f, modal.bottom - 46.0f),
                           data.account_overlay.busy
                               ? loc("PLEASE WAIT...", "처리 중...")
                               : (data.account_overlay.register_mode
                                      ? loc("CREATE ACCOUNT", "계정 만들기")
                                      : loc("LOG IN", "로그인")),
                           true, MenuHitTargetKind::AccountAction, 0);
        }
        if (!data.account_overlay.status.empty() && d2d_->hud_format && d2d_->muted_brush) {
            draw_text_clipped_aligned(
                to_wide(data.account_overlay.status), d2d_->hud_format.Get(),
                D2D1::RectF(modal.left + 40.0f, modal.bottom - 38.0f,
                            modal.right - 40.0f, modal.bottom - 12.0f),
                d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
        }
