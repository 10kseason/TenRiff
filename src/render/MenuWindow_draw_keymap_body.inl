        const float keymap_panel_right = right - 648.0f;
        const float keymap_inner_left = left + 24.0f;
        const float keymap_inner_right = keymap_panel_right - 24.0f;
        draw_glass_panel(D2D1::RectF(left, top, keymap_panel_right, bottom), 14, 0.92f, 0, false, 0);
        draw_generic_help(fit_rect_below_performance_overlay(
                              D2D1::RectF(keymap_panel_right + 24.0f, top, right, bottom), bottom, 20.0f),
                          data.generic.heading, data.generic.notes, data.generic.footer_notes);

        const bool testing_keys = data.generic.keymap_test;
        const std::size_t first_key_row = testing_keys ? 0 : 1;
        const std::size_t action_count = testing_keys ? 1 : 3;
        const int key_count = static_cast<int>(data.generic.rows.size() - first_key_row - action_count);
        const float keymap_top = top + 74.0f;
        const auto short_binding = [](std::string value) {
            if (value == "Unassigned" || value == "미할당" || value == "未割り当て") return std::string("—");
            const std::pair<const char*, const char*> names[] = {
                {"Semicolon", ";"}, {"Apostrophe", "'"}, {"LBracket", "["}, {"RBracket", "]"},
                {"Backslash", "\\"}, {"Backspace", "BS"}, {"LControl", "LCtrl"}, {"RControl", "RCtrl"},
                {"LShift", "LShft"}, {"RShift", "RShft"}, {"Space", "Space"}
            };
            for (const auto& [name, short_name] : names) {
                const auto end = value.find(" [");
                if (value.substr(0, end) == name) return std::string(short_name);
            }
            if (const auto end = value.find(" ["); end != std::string::npos) value.resize(end);
            return value;
        };
        const auto draw_keymap_button = [&](const D2D1_RECT_F& rect, const std::wstring& label,
                                            bool selected, MenuHitTargetKind target, int index,
                                            MenuHitPart part, bool enabled) {
            auto* fill = selected ? d2d_->button_selected_brush.Get() : d2d_->button_brush.Get();
            auto* border = selected ? d2d_->accent_brush.Get() : d2d_->button_border_brush.Get();
            if (fill) ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, 8, 8), fill);
            if (border) ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8, 8), border, selected ? 2.0f : 1.0f);
            const auto paragraph = d2d_->body_format->GetParagraphAlignment();
            d2d_->body_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            draw_text_clipped_aligned(label, d2d_->body_format.Get(),
                                      D2D1::RectF(rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4),
                                      enabled || testing_keys ? d2d_->text_brush.Get() : d2d_->muted_brush.Get(),
                                      DWRITE_TEXT_ALIGNMENT_CENTER);
            d2d_->body_format->SetParagraphAlignment(paragraph);
            if (enabled) register_hit(rect, target, index, part);
        };

        if (!testing_keys && !data.generic.rows.empty()) {
            const auto& row = data.generic.rows.front();
            const auto mode_rect = D2D1::RectF(keymap_inner_left, keymap_top,
                                               keymap_inner_right, keymap_top + 58.0f);
            if (d2d_->card_brush) ctx->FillRoundedRectangle(D2D1::RoundedRect(mode_rect, 10, 10), d2d_->card_brush.Get());
            draw_text_clipped(to_wide(row.label), d2d_->option_format.Get(),
                              D2D1::RectF(mode_rect.left + 16, mode_rect.top + 12, mode_rect.right - 220, mode_rect.bottom - 8),
                              d2d_->text_brush.Get());
            draw_text_clipped_aligned(to_wide(row.value), d2d_->option_format.Get(),
                                      D2D1::RectF(mode_rect.right - 220, mode_rect.top + 12, mode_rect.right - 120, mode_rect.bottom - 8),
                                      d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
            if (row.activatable)
                register_hit(D2D1::RectF(mode_rect.left, mode_rect.top, mode_rect.right - 120, mode_rect.bottom),
                             MenuHitTargetKind::SettingsRow, 0, MenuHitPart::Activate);
            draw_keymap_button(D2D1::RectF(mode_rect.right - 108, mode_rect.top + 6, mode_rect.right - 62, mode_rect.bottom - 6),
                               L"-", false, MenuHitTargetKind::SettingsRow, 0, MenuHitPart::Decrement,
                               !data.generic.keymap_capture_active);
            draw_keymap_button(D2D1::RectF(mode_rect.right - 52, mode_rect.top + 6, mode_rect.right - 6, mode_rect.bottom - 6),
                               L"+", false, MenuHitTargetKind::SettingsRow, 0, MenuHitPart::Increment,
                               !data.generic.keymap_capture_active);
        }

        draw_text_clipped(wloc("Primary key", "기본 키"), d2d_->hud_format.Get(),
                          D2D1::RectF(keymap_inner_left + 4, keymap_top + 94, keymap_inner_right, keymap_top + 124),
                          d2d_->muted_brush.Get());
        const bool split_keys = key_count == 6 || key_count == 8 || key_count == 10;
        const float middle_gap = split_keys ? 32.0f : 0.0f;
        const float key_gap = 8.0f;
        const float key_width = (keymap_inner_right - keymap_inner_left -
                                 middle_gap - key_gap * std::max(0, key_count - 1)) / std::max(1, key_count);
        const float keys_top = keymap_top + 134.0f;
        const float keys_bottom = std::min(keys_top + 260.0f, bottom - 270.0f);
        const float secondary_top = keys_bottom + 56.0f;
        draw_text_clipped(wloc("Secondary key", "보조 키"), d2d_->hud_format.Get(),
                          D2D1::RectF(keymap_inner_left + 4, keys_bottom + 16, keymap_inner_right, secondary_top - 6),
                          d2d_->muted_brush.Get());

        for (int key_index = 0; key_index < key_count; ++key_index) {
            const auto& row = data.generic.rows[first_key_row + static_cast<std::size_t>(key_index)];
            const float x = keymap_inner_left + key_index * (key_width + key_gap) +
                            (split_keys && key_index >= key_count / 2 ? middle_gap : 0.0f);
            const auto key_rect = D2D1::RectF(x, keys_top, x + key_width, keys_bottom);
            if (d2d_->card_brush) ctx->FillRoundedRectangle(D2D1::RoundedRect(key_rect, 9, 9), d2d_->card_brush.Get());
            if (d2d_->button_border_brush) ctx->DrawRoundedRectangle(D2D1::RoundedRect(key_rect, 9, 9), d2d_->button_border_brush.Get());
            draw_text_clipped_aligned(to_wide(row.label), d2d_->hud_format.Get(),
                                      D2D1::RectF(x + 3, keys_top + 14, x + key_width - 3, keys_top + 44),
                                      d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            const bool primary_selected = row.selected && (!data.generic.keymap_secondary_selected || testing_keys);
            const bool secondary_selected = row.selected && data.generic.keymap_secondary_selected && !testing_keys;
            draw_keymap_button(D2D1::RectF(x + 4, keys_top + 60, x + key_width - 4, keys_bottom - 8),
                               to_wide(short_binding(row.value)), primary_selected,
                               row.target_kind, row.row_index, MenuHitPart::Activate, row.activatable);
            draw_keymap_button(D2D1::RectF(x, secondary_top, x + key_width, secondary_top + 64),
                               to_wide(short_binding(row.secondary_value)), secondary_selected,
                               row.target_kind, row.row_index, MenuHitPart::Increment, row.activatable);
            if (!testing_keys && row.activatable) {
                // A dedicated clear control leaves the primary binding intact.
                const auto clear = D2D1::RectF(x + key_width - 23, secondary_top - 22, x + key_width, secondary_top);
                draw_keymap_button(clear, L"×", false, row.target_kind, row.row_index,
                                   MenuHitPart::Decrement, true);
            }
        }

        const float actions_top = bottom - 94.0f;
        const float action_width = (keymap_inner_right - keymap_inner_left - 16.0f * (action_count - 1)) / action_count;
        for (std::size_t index = 0; index < action_count; ++index) {
            const auto& row = data.generic.rows[data.generic.rows.size() - action_count + index];
            const float x = keymap_inner_left + static_cast<float>(index) * (action_width + 16);
            draw_keymap_button(D2D1::RectF(x, actions_top, x + action_width, actions_top + 62),
                               to_wide(row.label), row.selected, row.target_kind, row.row_index,
                               MenuHitPart::Activate, row.activatable);
        }
