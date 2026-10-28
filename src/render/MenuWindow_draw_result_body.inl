        auto draw_panel = [&](const D2D1_RECT_F& rect, bool accent_border = false) {
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 18.0f, 18.0f);
            if (d2d_->panel_brush) {
                ctx->FillRoundedRectangle(rr, d2d_->panel_brush.Get());
            }
            if (accent_border && d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->accent_brush.Get(), 1.8f);
            } else if (d2d_->button_border_brush) {
                ctx->DrawRoundedRectangle(rr, d2d_->button_border_brush.Get(), 1.4f);
            }
        };

        const auto localized_result_status = [&]() {
            if (data.result.status == "GAME OVER") {
                return loc("GAME OVER", "게임 오버");
            }
            if (data.result.status == "FAILED") {
                return loc("FAILED", "실패");
            }
            if (data.result.status == "ABORTED") {
                return loc("ABORTED", "중도 종료");
            }
            if (data.result.status == "AUTOPLAY") {
                return loc("AUTOPLAY - NO CLEAR", "오토플레이 - 클리어 미인정");
            }
            if (data.result.status == "CLEAR") {
                return loc("CLEAR", "클리어");
            }
            if (data.result.status == "HARD CLEAR") {
                return loc("HARD CLEAR", "하드 클리어");
            }
            if (data.result.status == "SUDDEN DEATH CLEAR") {
                return loc("SUDDEN DEATH CLEAR", "서든 데스 클리어");
            }
            if (data.result.status == "EASY CLEAR") {
                return loc("EASY CLEAR", "이지 클리어");
            }
            // Compact display only; stored Gauge Shift status and colors stay intact.
            return song_select_lamp_presentation(data.result.status).label;
        };
        const auto localized_result_gauge = [&]() {
            if (data.result.gauge_label == "HARD") return loc("HARD", "하드");
            if (data.result.gauge_label == "EASY") return loc("EASY", "이지");
            if (data.result.gauge_label == "NORMAL") return loc("NORMAL", "노말");
            return data.result.gauge_label;
        };

        // Result notes are not part of the visual layout. This explicit footer
        // keeps asynchronous upload outcomes visible on every result page.
        const auto draw_web_upload_status = [&]() {
            if (!data.result.web_upload_status.empty() && d2d_->hud_format && d2d_->muted_brush) {
                draw_text_clipped_aligned(to_wide(data.result.web_upload_status), d2d_->hud_format.Get(),
                    D2D1::RectF(64.0f, 1050.0f, 1856.0f, 1078.0f), d2d_->muted_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        };
        if (data.result.peer_battle) {
#include "MenuWindow_draw_result_multiplayer.inl"
            draw_web_upload_status();
            return;
        }

#include "MenuWindow_draw_result_single_body.inl"
        draw_web_upload_status();
