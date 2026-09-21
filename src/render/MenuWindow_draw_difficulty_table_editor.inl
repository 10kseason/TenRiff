            // The editor owns pointer input; cards behind the scrim must not activate.
            hit_regions_.clear();
            const auto saved_footer_color = d2d_->footer_brush->GetColor();
            d2d_->footer_brush->SetColor(D2D1::ColorF(0x05090F, 0.82f));
            ctx->FillRectangle(D2D1::RectF(0, 0, kBaseWidth, kBaseHeight), d2d_->footer_brush.Get());
            d2d_->footer_brush->SetColor(saved_footer_color);
            const D2D1_RECT_F editor = D2D1::RectF(470, 180, 1450, 850);
            draw_glass_panel(editor, 18, 1.0f, 0, false, 0);
            draw_text_clipped(wloc("SELECT DIFFICULTY TABLE", "난이도표 선택"), d2d_->title_format.Get(),
                D2D1::RectF(500, 205, 1420, 250), d2d_->text_brush.Get());
            // F1-F9 form a three-column grid; the F4 slot stays reserved for Native LV.
            const auto preset_button_rect = [](int function_key) {
                const int slot = function_key - 1;
                const float left = 500.0f + static_cast<float>(slot % 3) * 310.0f;
                const float top = 270.0f + static_cast<float>(slot / 3) * 68.0f;
                return D2D1::RectF(left, top, left + 300.0f, top + 59.0f);
            };
            for (std::size_t index = 0; index < config::kBuiltinDifficultyTables.size(); ++index) {
                const auto& preset = config::kBuiltinDifficultyTables[index];
                const D2D1_RECT_F button = preset_button_rect(preset.function_key);
                draw_glass_panel(button, 10, 1, 0, false, 0);
                register_hit(button, MenuHitTargetKind::SongDifficultyTable,
                    static_cast<int>(SongDifficultyTableAction::PresetAery5) + static_cast<int>(index));
                const std::wstring label = L"F" + std::to_wstring(preset.function_key) + L"  " +
                    wloc(preset.name_en.data(), preset.name_ko.data());
                draw_centered_text(label, d2d_->body_format.Get(), button, d2d_->accent_brush.Get(), true);
            }
            const D2D1_RECT_F native_level = preset_button_rect(4);
            draw_glass_panel(native_level, 10, 1, 0, !data.song_select.difficulty_table_active, 0);
            register_hit(native_level, MenuHitTargetKind::SongDifficultyTable,
                static_cast<int>(SongDifficultyTableAction::Reset));
            draw_centered_text(wloc("F4  Native LV", "F4  기본 LV"), d2d_->body_format.Get(),
                native_level, d2d_->accent_brush.Get(), true);
            draw_text_clipped(wloc("Paste a BMSTable page or header JSON link.", "BMSTable 페이지 또는 헤더 JSON 주소를 붙여넣으세요."),
                d2d_->body_format.Get(), D2D1::RectF(500, 482, 1420, 514), d2d_->muted_brush.Get());
            const D2D1_RECT_F input = D2D1::RectF(500, 530, 1420, 619);
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
                D2D1::RectF(516, 542, 1404, 609), d2d_->text_brush.Get());
            const auto message = data.song_select.difficulty_table_status.empty()
                ? wloc("Ctrl+V Paste    Enter Apply    Esc Cancel", "Ctrl+V 붙여넣기    Enter 적용    Esc 취소")
                : to_wide(data.song_select.difficulty_table_status);
            draw_text_clipped(message, d2d_->body_format.Get(),
                D2D1::RectF(500, 635, 1420, 695), d2d_->muted_brush.Get());
            d2d_->body_format->SetWordWrapping(wrapping);
            const D2D1_RECT_F cancel = D2D1::RectF(960, 763, 1178, 815);
            const D2D1_RECT_F apply = D2D1::RectF(1190, 763, 1420, 815);
            draw_glass_panel(cancel, 10, 1, 0, false, 0);
            draw_glass_panel(apply, 10, 1, 0, true, 0);
            register_hit(cancel, MenuHitTargetKind::SongDifficultyTable, static_cast<int>(SongDifficultyTableAction::Cancel));
            register_hit(apply, MenuHitTargetKind::SongDifficultyTable, static_cast<int>(SongDifficultyTableAction::Apply));
            draw_centered_text(wloc("CANCEL", "취소"), d2d_->body_format.Get(), cancel, d2d_->text_brush.Get(), true);
            draw_centered_text(wloc("APPLY", "적용"), d2d_->body_format.Get(), apply, d2d_->accent_brush.Get(), true);
