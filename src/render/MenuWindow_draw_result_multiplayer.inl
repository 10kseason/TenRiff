            const auto& players = data.result.multiplayer_players;
            const bool complete = !players.empty() && std::all_of(players.begin(), players.end(),
                [](const auto& p) { return p.has_score && p.finished && !p.aborted; });
            const auto local = std::find_if(players.begin(), players.end(), [](const auto& p) { return p.local; });
            const int winners = static_cast<int>(std::count_if(players.begin(), players.end(), [](const auto& p) { return p.rank == 1; }));
            const std::string outcome = !complete || local == players.end() ? loc("RESULT PENDING", "결과 미확정") :
                local->rank != 1 ? loc("LOSE", "패배") : winners > 1 ? loc("DRAW", "공동 1위") : loc("WIN", "승리");
            draw_panel(D2D1::RectF(64.0f, 32.0f, 1856.0f, 156.0f));
            draw_text_clipped(wloc("MULTIPLAYER RESULT", "멀티플레이 결과"), d2d_->header_format.Get(),
                              D2D1::RectF(96.0f, 52.0f, 850.0f, 104.0f), d2d_->text_brush.Get());
            draw_text_clipped(to_wide(data.result.title), d2d_->body_format.Get(),
                              D2D1::RectF(96.0f, 110.0f, 1420.0f, 146.0f), d2d_->muted_brush.Get());
            draw_text_clipped_aligned(to_wide(std::to_string(players.size()) + loc(" PLAYERS", "명")),
                d2d_->title_format.Get(), D2D1::RectF(1440.0f, 66.0f, 1820.0f, 114.0f),
                d2d_->accent_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);

            const auto column_text = [&](const std::wstring& text, float left, float right,
                                         float top, IDWriteTextFormat* format, ID2D1Brush* brush) {
                draw_text_clipped(text, format, D2D1::RectF(left, top, right, top + 40.0f), brush);
            };
            column_text(wloc("RANK / PLAYER", "순위 / 플레이어"), 96, 610, 178, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            column_text(wloc("SCORE", "점수"), 620, 860, 178, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            column_text(wloc("MAX COMBO", "최대 콤보"), 890, 1080, 178, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            column_text(wloc("GAUGE", "게이지"), 1090, 1270, 178, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            column_text(wloc("JUDGEMENTS / STATUS", "판정 / 상태"), 1280, 1820, 178, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            for (std::size_t index = 0; index < players.size(); ++index) {
                const auto& player = players[index];
                const float y = 218.0f + static_cast<float>(index) * 84.0f;
                draw_panel(D2D1::RectF(64.0f, y, 1856.0f, y + 76.0f), player.local);
                auto* ink = player.local ? d2d_->accent_brush.Get() : d2d_->text_brush.Get();
                const std::string name = (player.rank > 0 ? std::to_string(player.rank) : "--") +
                    "   " + player.name + (player.local ? loc(" / YOU", " / 나") : "");
                column_text(to_wide(name), 96, 600, y + 16, d2d_->body_format.Get(), ink);
                column_text(to_wide(player.has_score ? format_int_with_commas(player.score) : "--"),
                    620, 875, y + 14, d2d_->title_format.Get(), ink);
                column_text(to_wide(player.has_score ? std::to_string(player.max_combo) : "--"),
                    890, 1070, y + 16, d2d_->body_format.Get(), d2d_->text_brush.Get());
                column_text(to_wide(player.has_score ? format_decimal(player.gauge) + "%" : "--"),
                    1090, 1260, y + 16, d2d_->body_format.Get(), d2d_->text_brush.Get());
                const std::string status = !player.has_score ? loc("WAITING", "결과 대기") :
                    player.aborted ? loc("ABORTED", "중단") : player.game_over ? loc("FAILED", "실패") :
                    player.finished ? loc("FINISHED", "완료") : loc("PLAYING", "플레이 중");
                const std::string counts = player.has_score ?
                    "PG " + std::to_string(player.perfect) + "  GR " + std::to_string(player.great) +
                    "  GD " + std::to_string(player.good) + "  BD " + std::to_string(player.bad) +
                    "  PR " + std::to_string(player.poor) : "--";
                column_text(to_wide(counts), 1280, 1830, y + 7, d2d_->hud_format.Get(), d2d_->text_brush.Get());
                column_text(to_wide(status), 1280, 1830, y + 39, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            }
            // Remote results remain peer claims, not ranked/replay-verified results.
            column_text(to_wide(outcome + loc(" / Room scores; remote results unverified", " / 방 점수 · 상대 결과 미검증")),
                96, 1220, 910, d2d_->hud_format.Get(), d2d_->muted_brush.Get());
            const D2D1_RECT_F back = D2D1::RectF(1160.0f, 960.0f, 1856.0f, 1040.0f);
            draw_panel(back, true);
            register_hit(back, MenuHitTargetKind::SettingsRow, 0);
            const auto alignment = d2d_->title_format->GetParagraphAlignment();
            d2d_->title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            draw_text_clipped_aligned(wloc("BACK TO LOBBY", "로비로 돌아가기"), d2d_->title_format.Get(),
                back, d2d_->accent_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->title_format->SetParagraphAlignment(alignment);
