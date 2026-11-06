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
            ctx->FillRectangle(native_background_rect("account.rect.001"),
                               d2d_->footer_brush.Get());
            d2d_->footer_brush->SetOpacity(saved);
        }

        const float width = native_metric("account.width", 820.0f);
        const float height = data.account_overlay.mode_selection ? 400.0f : 700.0f;
        const float scale = 0.96f + 0.04f * eased;
        const float left = (kBaseWidth - width * scale) * 0.5f;
        const float top = (kBaseHeight - height * scale) * 0.5f;
        const D2D1_RECT_F modal = native_rect("account.rect.002", D2D1::RectF(
            left, top, left + width * scale, top + height * scale));
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
            draw_text_clipped(to_wide(data.account_overlay.mode_selection
                                  ? loc("PLAY MODE", "플레이 모드") : loc("TENRIFF LEADERBOARD", "텐리프 리더보드")),
                              d2d_->title_format.Get(),
                              native_rect("account.rect.003", D2D1::RectF(modal.left + 38.0f, modal.top + 24.0f,
                                          modal.right - 220.0f, modal.top + 72.0f)),
                              d2d_->text_brush.Get());
        }

        if (data.account_overlay.mode_selection) {
            const float middle = (modal.left + modal.right) * 0.5f;
            account_button(D2D1::RectF(modal.left + 38.0f, modal.top + 100.0f, middle - 8.0f, modal.top + 170.0f),
                loc("GUEST MODE", "게스트 모드"), !data.account_overlay.mode_choice_online,
                MenuHitTargetKind::AccountPlayMode, 0);
            account_button(D2D1::RectF(middle + 8.0f, modal.top + 100.0f, modal.right - 38.0f, modal.top + 170.0f),
                loc("ONLINE MODE", "온라인 모드"), data.account_overlay.mode_choice_online,
                MenuHitTargetKind::AccountPlayMode, 1);
            if (d2d_->body_format && d2d_->text_brush) {
                draw_text_clipped(to_wide(loc("Guest records stay local and are never uploaded later.",
                    "게스트 기록은 로컬에만 저장하며 나중에도 업로드하지 않습니다.")),
                    d2d_->body_format.Get(), D2D1::RectF(modal.left + 38.0f, modal.top + 198.0f,
                        modal.right - 38.0f, modal.top + 250.0f), d2d_->text_brush.Get());
                draw_text_clipped(to_wide(loc("Online mode uploads eligible new plays to the connected leaderboard.",
                    "온라인 모드는 연결된 리더보드에 새 플레이 기록을 등록합니다.")),
                    d2d_->body_format.Get(), D2D1::RectF(modal.left + 38.0f, modal.top + 258.0f,
                        modal.right - 38.0f, modal.top + 310.0f), d2d_->text_brush.Get());
            }
            if (d2d_->hud_format && d2d_->muted_brush)
                draw_text_clipped(to_wide(loc("Left / Right: select   Enter: confirm   F10 / ESC: close",
                    "좌우: 선택   Enter: 확인   F10 / ESC: 닫기")), d2d_->hud_format.Get(),
                    D2D1::RectF(modal.left + 38.0f, modal.bottom - 52.0f, modal.right - 38.0f, modal.bottom - 14.0f),
                    d2d_->muted_brush.Get());
            return;
        }
        account_button(D2D1::RectF(modal.right - 188.0f, modal.top + 24.0f, modal.right - 38.0f, modal.top + 72.0f),
            loc("PLAY MODE", "플레이 모드"), false, MenuHitTargetKind::AccountPlayMode, 2);
        auto sites_text = [&](float y, std::string_view text, bool accent = false) {
            if (d2d_->body_format && d2d_->text_brush)
                draw_text_clipped(to_wide(std::string(text)), d2d_->body_format.Get(),
                    native_rect("account.rect.005", D2D1::RectF(modal.left + 40.0f, modal.top + y, modal.right - 40.0f, modal.top + y + 40.0f)),
                    accent && d2d_->accent_brush ? d2d_->accent_brush.Get() : d2d_->text_brush.Get());
        };
        sites_text(159.0f, !data.account_overlay.online_mode ? loc("GUEST MODE", "게스트 모드") : data.account_overlay.sites_connected
            ? loc("AUTO UPLOAD: ON", "자동 업로드: 사용 중") : loc("NOT CONNECTED", "아직 연결되지 않음"), true);
        sites_text(204.0f, data.account_overlay.sites_url);
        sites_text(260.0f, loc("1. Sign in on the website and copy connection information.",
                              "1. 웹사이트에서 로그인하고 연결 정보를 복사하세요."));
        account_button(native_rect("account.rect.006", D2D1::RectF(modal.left + 40.0f, modal.top + 309.0f, modal.right - 40.0f, modal.top + 363.0f)),
            loc("OPEN LEADERBOARD WEBSITE", "리더보드 웹사이트 열기"), false,
            MenuHitTargetKind::SitesLeaderboardAction, static_cast<int>(SitesLeaderboardAction::OpenWebsite));
        sites_text(383.0f, loc("2. Paste it here, or import the downloaded connection file.",
                              "2. 여기에 붙여넣거나 받은 연결 파일을 불러오세요."));
        const float middle = (modal.left + modal.right) * 0.5f;
        account_button(native_rect("account.rect.007", D2D1::RectF(modal.left + 40.0f, modal.top + 431.0f, middle - 5.0f, modal.top + 487.0f)),
            loc("PASTE CONNECTION  Ctrl+V", "연결 정보 붙여넣기  Ctrl+V"), true,
            MenuHitTargetKind::SitesLeaderboardAction, static_cast<int>(SitesLeaderboardAction::PasteConnection));
        account_button(native_rect("account.rect.008", D2D1::RectF(middle + 5.0f, modal.top + 431.0f, modal.right - 40.0f, modal.top + 487.0f)),
            loc("IMPORT CONNECTION FILE", "연결 파일 불러오기"), false,
            MenuHitTargetKind::SitesLeaderboardAction, static_cast<int>(SitesLeaderboardAction::ImportFile));
        if (d2d_->hud_format && d2d_->text_brush)
            draw_text_clipped(to_wide(data.account_overlay.sites_status), d2d_->hud_format.Get(),
                D2D1::RectF(modal.left + 40.0f, modal.top + 511.0f, modal.right - 40.0f, modal.top + 565.0f),
                d2d_->text_brush.Get());
        if (data.account_overlay.sites_connected) {
            account_button(native_rect("account.rect.009", D2D1::RectF(modal.left + 40.0f, modal.top + 569.0f, middle - 5.0f, modal.top + 619.0f)),
                loc("DISCONNECT THIS PC", "이 PC 연결 해제"), false,
                MenuHitTargetKind::SitesLeaderboardAction, static_cast<int>(SitesLeaderboardAction::Disconnect));
            account_button(D2D1::RectF(middle + 5.0f, modal.top + 569.0f, modal.right - 40.0f, modal.top + 619.0f),
                loc("CONTINUE", "계속"), true, MenuHitTargetKind::AccountPlayMode, 3);
        }
        if (d2d_->hud_format && d2d_->muted_brush)
            draw_text_clipped(to_wide(loc("Enter: continue or connect   F10 / ESC: close",
                "Enter 계속 또는 연결   F10 / ESC 닫기")),
                d2d_->hud_format.Get(), native_rect("account.rect.010", D2D1::RectF(modal.left + 40.0f, modal.bottom - 48.0f, modal.right - 40.0f, modal.bottom - 16.0f)),
                d2d_->muted_brush.Get());
