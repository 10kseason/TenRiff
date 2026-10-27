        const auto& mix = data.session_mix;
        draw_text_clipped(L"SESSION MIX", d2d_->title_format.Get(),
            D2D1::RectF(80, 66, 1320, 142), d2d_->text_brush.Get());
        draw_text_clipped(wloc("Choose a course · review its stages · enter", "코스 선택 · 수록곡 확인 · 입장"),
            d2d_->body_format.Get(), D2D1::RectF(84, 142, 1820, 178), d2d_->muted_brush.Get());
        const auto left = D2D1::RectF(80, 194, 386, 904);
        const auto middle = D2D1::RectF(406, 194, 1038, 904);
        const auto right = D2D1::RectF(1058, 194, 1840, 904);
        draw_glass_panel(left, 16, .94f, .08f, mix.controls_focused);
        draw_glass_panel(middle, 16, .94f, .08f, !mix.controls_focused && !mix.stages_focused);
        draw_glass_panel(right, 16, .94f, .08f, mix.stages_focused);
        draw_text_clipped(wloc("COURSE OPTIONS", "코스 메뉴"), d2d_->body_format.Get(),
            D2D1::RectF(100, 214, 368, 252), d2d_->text_brush.Get());
        for (std::size_t i = 0; i < mix.controls.size(); ++i) {
            const float top = 270.0f + static_cast<float>(i) * 96.0f;
            const auto rect = D2D1::RectF(98, top, 368, top + 80);
            const bool enabled = i != 4 || mix.can_start;
            const bool selected = mix.controls_focused && static_cast<int>(i) == mix.control_cursor;
            draw_glass_panel(rect, 10, enabled ? .96f : .42f, selected ? .3f : .02f, selected);
            auto* brush = enabled ? d2d_->text_brush.Get() : d2d_->muted_brush.Get();
            draw_text_clipped(to_wide(mix.controls[i].label), d2d_->body_format.Get(),
                D2D1::RectF(rect.left + 14, top + 10, rect.right - 14, top + 42), brush);
            draw_text_clipped(to_wide(mix.controls[i].detail), d2d_->hud_format.Get(),
                D2D1::RectF(rect.left + 14, top + 45, rect.right - 14, top + 72), d2d_->muted_brush.Get());
            if (enabled) register_hit(rect, MenuHitTargetKind::SettingsRow, static_cast<int>(i), MenuHitPart::Activate);
        }
        draw_text_clipped(wloc("COURSES", "코스 목록") + L"  " + std::to_wstring(mix.courses.size()),
            d2d_->body_format.Get(), D2D1::RectF(426, 210, 1016, 248), d2d_->text_brush.Get());
        draw_text_clipped(to_wide(mix.source), d2d_->hud_format.Get(),
            D2D1::RectF(426, 245, 1016, 272), d2d_->muted_brush.Get());
        const int first = std::clamp(mix.selected - 3, 0, std::max(0, static_cast<int>(mix.courses.size()) - 7));
        for (int row = 0; row < 7 && first + row < static_cast<int>(mix.courses.size()); ++row) {
            const auto& course = mix.courses[first + row];
            const float top = 280.0f + row * 86.0f;
            const auto rect = D2D1::RectF(424, top, 1020, top + 76);
            draw_glass_panel(rect, 10, .97f, course.selected ? .35f : .02f, course.selected);
            draw_text_clipped(to_wide(course.label), d2d_->body_format.Get(),
                D2D1::RectF(rect.left + 18, top + 9, rect.right - 16, top + 42), d2d_->text_brush.Get());
            draw_text_clipped(to_wide(course.detail), d2d_->hud_format.Get(),
                D2D1::RectF(rect.left + 18, top + 44, rect.right - 16, top + 69), d2d_->muted_brush.Get());
            register_hit(rect, MenuHitTargetKind::SongCard, first + row, MenuHitPart::Activate);
        }
        draw_text_clipped(to_wide(mix.title), d2d_->body_format.Get(),
            D2D1::RectF(1080, 214, 1818, 260), d2d_->text_brush.Get());
        draw_text_clipped(to_wide(mix.summary), d2d_->hud_format.Get(),
            D2D1::RectF(1080, 266, 1818, 310), d2d_->muted_brush.Get());
        draw_text_clipped(wloc("STAGE ORDER", "수록곡 순서") + L"  " + std::to_wstring(mix.stages.size()),
            d2d_->hud_format.Get(), D2D1::RectF(1080, 312, 1818, 344), d2d_->text_brush.Get());
        for (int row = 0; row < 7 && mix.stage_offset + row < static_cast<int>(mix.stages.size()); ++row) {
            const int index = mix.stage_offset + row;
            const auto& stage = mix.stages[index];
            const float top = 350.0f + row * 67.0f;
            const auto rect = D2D1::RectF(1080, top, 1818, top + 62);
            draw_glass_panel(rect, 8, stage.missing ? .45f : .80f, 0, false, 0);
            auto* brush = stage.missing ? d2d_->muted_brush.Get() : d2d_->text_brush.Get();
            draw_text_clipped(std::to_wstring(index + 1) + L". " + to_wide(stage.title), d2d_->body_format.Get(),
                D2D1::RectF(1092, top + 4, 1806, top + 35), brush);
            draw_text_clipped(to_wide(stage.detail), d2d_->hud_format.Get(),
                D2D1::RectF(1092, top + 36, 1806, top + 60), d2d_->muted_brush.Get());
        }
        if (mix.stages.empty()) draw_text_clipped(wloc("No stages yet", "등록된 스테이지가 없습니다"),
            d2d_->body_format.Get(), D2D1::RectF(1080, 380, 1818, 430), d2d_->muted_brush.Get());
        for (int direction = 0; direction < 2; ++direction) {
            const bool enabled = direction == 0 ? mix.stage_offset > 0
                : mix.stage_offset + 7 < static_cast<int>(mix.stages.size());
            if (!enabled) continue;
            const auto rect = D2D1::RectF(1080.0f + direction * 380, 834, 1438.0f + direction * 380, 884);
            draw_glass_panel(rect, 8, .9f, .05f, false, 0);
            draw_text_clipped_aligned(direction == 0 ? wloc("Previous stages", "이전 수록곡") : wloc("Next stages", "다음 수록곡"),
                d2d_->body_format.Get(), rect, d2d_->text_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
            register_hit(rect, MenuHitTargetKind::SessionMixStagePage, direction, MenuHitPart::Activate);
        }
        draw_text_clipped(to_wide(mix.status), d2d_->body_format.Get(),
            D2D1::RectF(84, 924, 1836, 969), d2d_->text_brush.Get());
        draw_text_clipped(wloc("↑↓ / WHEEL SELECT   ENTER / DOUBLE CLICK ENTER   TAB PANEL   ESC BACK",
                              "↑↓ / 휠 선택   ENTER / 더블클릭 입장   TAB 영역 이동   ESC 뒤로"),
            d2d_->hud_format.Get(), D2D1::RectF(84, 985, 1836, 1032), d2d_->muted_brush.Get());
