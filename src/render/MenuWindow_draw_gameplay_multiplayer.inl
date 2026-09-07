            std::size_t opponent_index = 0;
            const float panel_left = opponents_left;
            const float panel_right = opponents_right;
            for (const auto& player : data.gameplay.multiplayer_players) {
                if (player.local) continue;
                const float top = 210.0f + static_cast<float>(opponent_index++) * 104.0f;
                const D2D1_RECT_F rect = D2D1::RectF(panel_left, top, panel_right, top + 96.0f);
                const auto saved_panel_color = d2d_->panel_brush->GetColor();
                d2d_->panel_brush->SetColor(D2D1::ColorF(0x141D28, 0.96f));
                ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 12.0f, 12.0f), d2d_->panel_brush.Get());
                d2d_->panel_brush->SetColor(saved_panel_color);
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 12.0f, 12.0f), d2d_->button_border_brush.Get(), 1.0f);
                const std::string name = (player.rank > 0 ? "#" + std::to_string(player.rank) : "--") + "  " + player.name;
                draw_text_clipped(to_wide(name), d2d_->hud_format.Get(),
                    D2D1::RectF(rect.left + 14, top + 7, rect.right - 14, top + 31), d2d_->accent_brush.Get());
                const bool compact = rect.right - rect.left < 360.0f;
                draw_text_clipped(to_wide(player.has_score ? format_int_with_commas(player.score) : "--"),
                    d2d_->title_format.Get(), D2D1::RectF(rect.left + 14, top + 32, rect.right - (compact ? 14.0f : 140.0f), top + (compact ? 65.0f : 73.0f)),
                    d2d_->text_brush.Get());
                const std::string state = data.gameplay.peer_disconnected ? loc("OFFLINE", "연결 끊김") :
                    !player.has_score ? loc("WAITING", "대기") : player.aborted ? loc("ABORTED", "중단") :
                    player.game_over ? loc("FAILED", "실패") : player.finished ? loc("FINISHED", "완료") :
                    "C " + std::to_string(player.combo);
                draw_text_clipped_aligned(to_wide(state), d2d_->hud_format.Get(),
                    D2D1::RectF(compact ? rect.left + 14 : rect.right - 140, top + (compact ? 61.0f : 41.0f), rect.right - 14, top + 82),
                    d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                const D2D1_RECT_F track = D2D1::RectF(rect.left + 14, top + 82, rect.right - 14, top + 88);
                ctx->FillRoundedRectangle(D2D1::RoundedRect(track, 3, 3), d2d_->button_border_brush.Get());
                const float end = track.left + (track.right - track.left) * static_cast<float>(std::clamp(player.gauge / 100.0, 0.0, 1.0));
                ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(track.left, track.top, end, track.bottom), 3, 3), d2d_->accent_brush.Get());
            }
