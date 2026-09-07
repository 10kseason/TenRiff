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
            return data.result.status;
        };
        const auto localized_result_gauge = [&]() {
            if (!ui_korean) {
                return data.result.gauge_label;
            }
            if (data.result.gauge_label == "HARD") {
                return std::string("하드");
            }
            if (data.result.gauge_label == "EASY") {
                return std::string("이지");
            }
            return std::string("노말");
        };

        if (data.result.peer_battle) {
#include "MenuWindow_draw_result_multiplayer.inl"
            return;
        }

#include "MenuWindow_draw_result_single_body.inl"
