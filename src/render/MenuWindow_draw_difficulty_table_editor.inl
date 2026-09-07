            // The editor owns pointer input; cards behind the scrim must not activate.
            hit_regions_.clear();
            const auto saved_footer_color = d2d_->footer_brush->GetColor();
            d2d_->footer_brush->SetColor(D2D1::ColorF(0x05090F, 0.82f));
            ctx->FillRectangle(D2D1::RectF(0, 0, kBaseWidth, kBaseHeight), d2d_->footer_brush.Get());
            d2d_->footer_brush->SetColor(saved_footer_color);
            const D2D1_RECT_F editor = D2D1::RectF(470, 310, 1450, 690);
            draw_glass_panel(editor, 18, 1.0f, 0, false, 0);
            draw_text_clipped(wloc("DIFFICULTY TABLE URL", "난이도표 URL"), d2d_->title_format.Get(),
                D2D1::RectF(500, 338, 1420, 382), d2d_->text_brush.Get());
            draw_text_clipped(wloc("Paste a BMSTable page or header JSON link.", "BMSTable 페이지 또는 헤더 JSON 주소를 붙여넣으세요."),
                d2d_->body_format.Get(), D2D1::RectF(500, 391, 1420, 427), d2d_->muted_brush.Get());
            const D2D1_RECT_F input = D2D1::RectF(500, 437, 1420, 526);
            draw_glass_panel(input, 10, 1, 0, false, 0);
            ctx->DrawRoundedRectangle(D2D1::RoundedRect(input, 10, 10), d2d_->accent_brush.Get(), 1.5f);
            std::wstring value = to_wide(data.song_select.difficulty_table_url_input);
            if (value.size() > 100) {
                std::size_t first = value.size() - 100;
                if (value[first] >= 0xDC00 && value[first] <= 0xDFFF) ++first;
                value = L"…" + value.substr(first);
            }
            value += L"|";
            const auto wrapping = d2d_->body_format->GetWordWrapping();
            d2d_->body_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            draw_text_clipped(value, d2d_->body_format.Get(),
                D2D1::RectF(516, 449, 1404, 516), d2d_->text_brush.Get());
            const auto message = data.song_select.difficulty_table_status.empty()
                ? wloc("Ctrl+V Paste    Enter Apply    Esc Cancel", "Ctrl+V 붙여넣기    Enter 적용    Esc 취소")
                : to_wide(data.song_select.difficulty_table_status);
            draw_text_clipped(message, d2d_->body_format.Get(),
                D2D1::RectF(500, 542, 1420, 602), d2d_->muted_brush.Get());
            d2d_->body_format->SetWordWrapping(wrapping);
            const D2D1_RECT_F cancel = D2D1::RectF(960, 614, 1178, 666);
            const D2D1_RECT_F apply = D2D1::RectF(1190, 614, 1420, 666);
            draw_glass_panel(cancel, 10, 1, 0, false, 0);
            draw_glass_panel(apply, 10, 1, 0, true, 0);
            register_hit(cancel, MenuHitTargetKind::SongDifficultyTable, static_cast<int>(SongDifficultyTableAction::Cancel));
            register_hit(apply, MenuHitTargetKind::SongDifficultyTable, static_cast<int>(SongDifficultyTableAction::Apply));
            draw_centered_text(wloc("CANCEL", "취소"), d2d_->body_format.Get(), cancel, d2d_->text_brush.Get(), true);
            draw_centered_text(wloc("APPLY", "적용"), d2d_->body_format.Get(), apply, d2d_->accent_brush.Get(), true);
