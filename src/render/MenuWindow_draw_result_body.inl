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
            const D2D1_RECT_F header_rect = D2D1::RectF(100.0f, 48.0f, 850.0f, 126.0f);
            ID2D1Brush* header_brush =
                d2d_->logo_brush ? static_cast<ID2D1Brush*>(d2d_->logo_brush.Get())
                                 : static_cast<ID2D1Brush*>(d2d_->accent_brush.Get());
            if (d2d_->logo_brush) {
                set_brush_points(d2d_->logo_brush.Get(), header_rect);
            }
            if (d2d_->header_format && header_brush) {
                draw_text_clipped(wloc("BATTLE RESULT", "대전 결과"),
                                  d2d_->header_format.Get(), header_rect, header_brush);
            }
            if (d2d_->song_title_format && d2d_->text_brush) {
                const std::string chart_title =
                    data.result.title.empty() ? loc("Unknown Chart", "알 수 없는 차트")
                                              : data.result.title;
                draw_text_clipped_aligned(
                    to_wide(chart_title),
                    d2d_->song_title_format.Get(),
                    D2D1::RectF(760.0f, 64.0f, 1820.0f, 118.0f),
                    d2d_->text_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_TRAILING);
            }

            std::string outcome = loc("RESULT UNAVAILABLE", "상대 결과 없음");
            if (data.result.peer_outcome == "WIN") outcome = loc("WIN", "승리");
            if (data.result.peer_outcome == "LOSE") outcome = loc("LOSE", "패배");
            if (data.result.peer_outcome == "DRAW") outcome = loc("DRAW", "무승부");
            if (data.result.peer_outcome == "NO CONTEST") outcome = loc("NO CONTEST", "무효 경기");
            std::string difference = "--";
            if (data.result.peer_result_available) {
                difference = format_int_with_commas(data.result.peer_score_difference);
                if (data.result.peer_score_difference > 0) {
                    difference.insert(difference.begin(), '+');
                }
            }
            if (d2d_->title_format && d2d_->accent_brush) {
                draw_text_clipped_aligned(
                    to_wide(outcome + "   /   " + loc("SCORE DIFFERENCE ", "점수차 ") + difference),
                    d2d_->title_format.Get(),
                    D2D1::RectF(100.0f, 132.0f, 1820.0f, 190.0f),
                    d2d_->accent_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            const D2D1_RECT_F local_rect = D2D1::RectF(100.0f, 210.0f, 920.0f, 870.0f);
            const D2D1_RECT_F peer_rect = D2D1::RectF(1000.0f, 210.0f, 1820.0f, 870.0f);
            const bool local_won = data.result.peer_result_available && data.result.peer_outcome == "WIN";
            const bool peer_won = data.result.peer_result_available && data.result.peer_outcome == "LOSE";

            auto draw_battle_card = [&](const D2D1_RECT_F& rect,
                                        const std::string& name,
                                        const std::string& status,
                                        int64_t score,
                                        double gauge,
                                        int max_combo,
                                        int perfect,
                                        int great,
                                        int good,
                                        int bad,
                                        int poor,
                                        bool available,
                                        bool winner) {
                draw_panel(rect, winner);
                if (d2d_->title_format && d2d_->text_brush) {
                    draw_text_clipped(to_wide(name),
                                      d2d_->title_format.Get(),
                                      D2D1::RectF(rect.left + 36.0f, rect.top + 30.0f,
                                                  rect.right - 36.0f, rect.top + 78.0f),
                                      d2d_->text_brush.Get());
                }
                if (d2d_->hud_format && d2d_->muted_brush) {
                    draw_text_clipped_aligned(to_wide(status),
                                              d2d_->hud_format.Get(),
                                              D2D1::RectF(rect.left + 36.0f, rect.top + 34.0f,
                                                          rect.right - 36.0f, rect.top + 72.0f),
                                              d2d_->muted_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_TRAILING);
                }
                if (d2d_->rank_format && d2d_->text_brush) {
                    ID2D1Brush* score_brush =
                        winner && d2d_->accent_brush
                            ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                            : static_cast<ID2D1Brush*>(d2d_->text_brush.Get());
                    draw_text_clipped_aligned(
                        to_wide(available ? format_int_with_commas(score) : std::string("--")),
                        d2d_->rank_format.Get(),
                        D2D1::RectF(rect.left + 36.0f, rect.top + 112.0f,
                                    rect.right - 36.0f, rect.top + 246.0f),
                        score_brush,
                        DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                if (d2d_->hud_format && d2d_->muted_brush) {
                    draw_text_clipped_aligned(wloc("SCORE", "점수"),
                                              d2d_->hud_format.Get(),
                                              D2D1::RectF(rect.left + 36.0f, rect.top + 242.0f,
                                                          rect.right - 36.0f, rect.top + 278.0f),
                                              d2d_->muted_brush.Get(),
                                              DWRITE_TEXT_ALIGNMENT_CENTER);
                }

                const std::string gauge_value =
                    available ? format_decimal(gauge) + "%" : std::string("--");
                const std::string combo_value =
                    available ? std::to_string(max_combo) : std::string("--");
                const std::string primary_counts =
                    available
                        ? ("PG " + std::to_string(perfect) + "     GR " + std::to_string(great) +
                           "     GD " + std::to_string(good))
                        : "PG --     GR --     GD --";
                const std::string miss_counts =
                    available
                        ? ("BAD " + std::to_string(bad) + "     PR " + std::to_string(poor))
                        : "BAD --     PR --";
                if (d2d_->title_format && d2d_->text_brush) {
                    draw_text_clipped_aligned(
                        to_wide(loc("GAUGE ", "게이지 ") + gauge_value +
                                "     " + loc("MAX COMBO ", "최대 콤보 ") + combo_value),
                        d2d_->title_format.Get(),
                        D2D1::RectF(rect.left + 42.0f, rect.top + 330.0f,
                                    rect.right - 42.0f, rect.top + 382.0f),
                        d2d_->text_brush.Get(),
                        DWRITE_TEXT_ALIGNMENT_CENTER);
                }
                if (d2d_->body_format && d2d_->text_brush) {
                    draw_text_clipped_aligned(
                        to_wide(primary_counts),
                        d2d_->body_format.Get(),
                        D2D1::RectF(rect.left + 42.0f, rect.top + 434.0f,
                                    rect.right - 42.0f, rect.top + 478.0f),
                        d2d_->text_brush.Get(),
                        DWRITE_TEXT_ALIGNMENT_CENTER);
                    draw_text_clipped_aligned(
                        to_wide(miss_counts),
                        d2d_->body_format.Get(),
                        D2D1::RectF(rect.left + 42.0f, rect.top + 500.0f,
                                    rect.right - 42.0f, rect.top + 544.0f),
                        d2d_->text_brush.Get(),
                        DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            };

            draw_battle_card(
                local_rect,
                data.result.profile.empty() ? loc("YOU", "나")
                                            : data.result.profile + loc(" / YOU", " / 나"),
                localized_result_status(),
                data.result.score,
                data.result.gauge_value,
                data.result.max_combo,
                data.result.perfect,
                data.result.great,
                data.result.good,
                data.result.bad,
                data.result.poor,
                true,
                local_won);
            std::string localized_peer_status = data.result.peer_status;
            if (localized_peer_status == "FINISHED") localized_peer_status = loc("FINISHED", "종료");
            if (localized_peer_status == "GAME OVER") localized_peer_status = loc("GAME OVER", "게임 오버");
            if (localized_peer_status == "ABORTED") localized_peer_status = loc("ABORTED", "중도 종료");
            if (localized_peer_status == "WAITING") localized_peer_status = loc("WAITING", "대기 중");
            if (localized_peer_status == "DISCONNECTED") localized_peer_status = loc("DISCONNECTED", "연결 끊김");
            draw_battle_card(
                peer_rect,
                data.result.peer_name.empty() ? loc("OPPONENT", "상대") : data.result.peer_name,
                localized_peer_status,
                data.result.peer_score,
                data.result.peer_gauge_value,
                data.result.peer_max_combo,
                data.result.peer_perfect,
                data.result.peer_great,
                data.result.peer_good,
                data.result.peer_bad,
                data.result.peer_poor,
                data.result.peer_result_available,
                peer_won);

            const D2D1_RECT_F back_rect = D2D1::RectF(660.0f, 900.0f, 1260.0f, 970.0f);
            register_hit(back_rect, MenuHitTargetKind::SettingsRow, 0);
            if (d2d_->button_selected_brush) {
                ctx->FillRoundedRectangle(D2D1::RoundedRect(back_rect, 14.0f, 14.0f),
                                          d2d_->button_selected_brush.Get());
            }
            if (d2d_->accent_brush) {
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(back_rect, 14.0f, 14.0f),
                                          d2d_->accent_brush.Get(), 1.5f);
            }
            if (d2d_->title_format && d2d_->text_brush) {
                draw_text_clipped_aligned(wloc("BACK TO MULTIPLAYER", "멀티플레이로 돌아가기"),
                                          d2d_->title_format.Get(),
                                          back_rect,
                                          d2d_->text_brush.Get(),
                                          DWRITE_TEXT_ALIGNMENT_CENTER);
            }
            return;
        }

#include "MenuWindow_draw_result_single_body.inl"
