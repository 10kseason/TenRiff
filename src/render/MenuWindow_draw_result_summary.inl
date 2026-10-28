        // The native summary owns the visual hierarchy; score computation and
        // reveal timing remain in ResultStats and ResultPresentation.
        const D2D1_RECT_F summary_panel = native_rect("result_summary.rect.001", D2D1::RectF(620.0f, 154.0f, 1270.0f, 738.0f));
        draw_result_panel(summary_panel, presentation.information, false);
        draw_result_outline(summary_panel, 0xA9BAC6, 1.6f, presentation.information);
        if (presentation.information > 0.8f) {
            draw_native_focus(summary_panel, 12, 90, 0, result_success);
        }
        draw_native_asset(native_menu_assets::kMark, native_rect("result_summary.rect.002", D2D1::RectF(1140, 178, 1212, 246)),
                          0.45f * presentation.information);
        // Award accents follow the existing result reveal and skip state.
        const float award = presentation.rank;
        if (result_success && award > 0.0f) {
            draw_native_asset(native_menu_assets::kSpark, native_rect("result_summary.rect.003", D2D1::RectF(884, 463, 910, 489)), award * 0.7f);
            draw_native_spectrum(native_rect("result_summary.rect.004", D2D1::RectF(660, 689, 1228, 712)), 0.36f * award, 42);
            if (native_menu_motion_.moving() && !data.result.presentation_skipped &&
                presentation.rank > 0.0f && presentation.rank < 1.0f) {
                for (int i = 0; i < 8; ++i) {
                    const float x = 678.0f + i * 72.0f;
                    const float y = 424.0f - award * (24.0f + (i % 3) * 20.0f);
                    draw_native_asset(native_menu_assets::kSpark, native_rect("result_summary.rect.005", D2D1::RectF(x, y, x + 10, y + 10)),
                                      (1.0f - award) * 0.65f);
                }
            }
        }
        const float summary_left = summary_panel.left + 36.0f;
        const float summary_right = summary_panel.right - 36.0f;
        const std::string grade_label = result_loc("GRADE", "등급") +
            (data.result.all_perfect ? result_loc(" · ALL PERFECT", " · 올 퍼펙트")
             : data.result.full_combo ? result_loc(" · FULL COMBO", " · 풀 콤보") : "");
        draw_result_text(grade_label,
                         d2d_->result_label_format.Get(),
                         native_rect("result_summary.rect.006", D2D1::RectF(summary_left, 186.0f, summary_right, 216.0f)),
                         d2d_->muted_brush.Get(), presentation.rank);
        draw_grade(0, to_wide(data.result.rank.empty() ? "--" : data.result.rank),
                   d2d_->result_grade_format.Get(),
                   native_rect("result_summary.rect.007", D2D1::RectF(summary_left - 4.0f, 216.0f, summary_right, 398.0f)),
                   presentation.rank);

        if (d2d_->button_border_brush) {
            const float saved = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(0.65f * presentation.rank);
            ctx->DrawLine(D2D1::Point2F(summary_left, 410.0f),
                          D2D1::Point2F(summary_right, 410.0f),
                          d2d_->button_border_brush.Get(), 1.0f);
            d2d_->button_border_brush->SetOpacity(saved);
        }
        draw_result_text(data.result.new_record ? "NEW Record" : result_loc("YOUR SCORE", "이번 플레이 점수"),
                         d2d_->result_label_format.Get(),
                         native_rect("result_summary.rect.009", D2D1::RectF(summary_left, 424.0f, summary_left + 230.0f, 452.0f)),
                         data.result.new_record ? semantic_brush(0xFFE477) : d2d_->muted_brush.Get(), presentation.score);
        draw_result_text(std::to_string(result_counted_score(data.result.score, presentation.score)),
                         d2d_->result_score_format.Get(),
                         native_rect("result_summary.rect.010", D2D1::RectF(summary_left, 458.0f, summary_left + 250.0f, 536.0f)),
                         d2d_->text_brush.Get(), presentation.score);
        draw_result_text(result_loc("OUT OF ", "최대 ") + std::to_string(data.result.max_score),
                         d2d_->result_label_format.Get(),
                         native_rect("result_summary.rect.008", D2D1::RectF(summary_left, 526.0f, summary_left + 250.0f, 554.0f)),
                         d2d_->muted_brush.Get(), presentation.score);
        draw_result_text(result_loc("ACCURACY", "정확도"), d2d_->result_label_format.Get(),
                         native_rect("result_summary.rect.011", D2D1::RectF(summary_left + 274.0f, 424.0f, summary_right, 452.0f)),
                         d2d_->muted_brush.Get(), presentation.rank);
        draw_result_text(format_decimal(data.result.accuracy, 2) + "%",
                         d2d_->result_accuracy_format.Get(),
                         native_rect("result_summary.rect.012", D2D1::RectF(summary_left + 274.0f, 458.0f, summary_right, 536.0f)),
                         semantic_brush(0x63E9FF), presentation.rank);

        const D2D1_RECT_F status_chip =
            native_rect("result_summary.rect.013", D2D1::RectF(summary_left, 558.0f, summary_right, 610.0f));
        if (d2d_->button_selected_brush) {
            const float saved = d2d_->button_selected_brush->GetOpacity();
            d2d_->button_selected_brush->SetOpacity(0.50f * presentation.status);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(status_chip, 8.0f, 8.0f),
                                      d2d_->button_selected_brush.Get());
            d2d_->button_selected_brush->SetOpacity(saved);
        }
        const auto status_text_rect = native_rect("result_summary.rect.014", D2D1::RectF(
            status_chip.left + 16.0f, status_chip.top + 8.0f, status_chip.right - 16.0f, status_chip.bottom - 6.0f));
        const auto status_text = localized_result_status();
        draw_result_text(status_text, d2d_->result_action_format.Get(), status_text_rect,
                         result_status_brush(result_success ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                                           : static_cast<ID2D1Brush*>(d2d_->text_brush.Get()),
                                             status_text, d2d_->result_action_format.Get(), status_text_rect,
                                             DWRITE_TEXT_ALIGNMENT_LEADING),
                         presentation.status);
        const std::string summary_detail = result_loc("DETAIL SCORE  ", "상세 점수  ") +
                  std::to_string(data.result.detail_score) + " / " +
                  std::to_string(data.result.max_detail_score);
        draw_result_text(summary_detail, d2d_->body_format.Get(),
                         native_rect("result_summary.rect.015", D2D1::RectF(summary_left, 634.0f, summary_left + 300.0f, 666.0f)),
                         d2d_->muted_brush.Get(), presentation.status);
        draw_result_text(result_loc("DETAIL ACCURACY  ", "상세 정확도  ") + format_decimal(data.result.detailed_accuracy, 2) + "%",
                         d2d_->body_format.Get(),
                         native_rect("result_summary.rect.016", D2D1::RectF(summary_left + 312.0f, 634.0f, summary_right, 666.0f)),
                         semantic_brush(0x63E9FF), presentation.status);
