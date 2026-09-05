        // The native summary owns the visual hierarchy; score computation and
        // reveal timing remain in ResultStats and ResultPresentation.
        const D2D1_RECT_F summary_panel = D2D1::RectF(620.0f, 154.0f, 1270.0f, 738.0f);
        draw_result_panel(summary_panel, presentation.information, false);
        const float summary_left = summary_panel.left + 36.0f;
        const float summary_right = summary_panel.right - 36.0f;
        draw_result_text(result_loc("YOUR SCORE", "이번 플레이 점수"),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(summary_left, 186.0f, summary_right, 216.0f),
                         d2d_->muted_brush.Get(), presentation.score);
        draw_result_text(format_int_with_commas(result_counted_score(data.result.score, presentation.score)),
                         d2d_->result_score_format.Get(),
                         D2D1::RectF(summary_left - 4.0f, 224.0f, summary_right, 352.0f),
                         d2d_->text_brush.Get(), presentation.score);
        draw_result_text(result_loc("OUT OF ", "최대 ") + format_int_with_commas(data.result.max_score),
                         d2d_->hud_format.Get(),
                         D2D1::RectF(summary_left, 354.0f, summary_right, 380.0f),
                         d2d_->muted_brush.Get(), presentation.score);

        if (d2d_->button_border_brush) {
            const float saved = d2d_->button_border_brush->GetOpacity();
            d2d_->button_border_brush->SetOpacity(0.65f * presentation.rank);
            ctx->DrawLine(D2D1::Point2F(summary_left, 402.0f),
                          D2D1::Point2F(summary_right, 402.0f),
                          d2d_->button_border_brush.Get(), 1.0f);
            d2d_->button_border_brush->SetOpacity(saved);
        }
        draw_result_text(result_loc("GRADE", "등급"), d2d_->hud_format.Get(),
                         D2D1::RectF(summary_left, 424.0f, summary_left + 230.0f, 452.0f),
                         d2d_->muted_brush.Get(), presentation.rank);
        draw_result_text(data.result.rank.empty() ? "--" : data.result.rank,
                         d2d_->header_format.Get(),
                         D2D1::RectF(summary_left, 458.0f, summary_left + 250.0f, 530.0f),
                         d2d_->accent_brush.Get(), presentation.rank);
        draw_result_text(result_loc("ACCURACY", "정확도"), d2d_->hud_format.Get(),
                         D2D1::RectF(summary_left + 300.0f, 424.0f, summary_right, 452.0f),
                         d2d_->muted_brush.Get(), presentation.rank);
        draw_result_text(format_decimal(data.result.accuracy, 2) + "%",
                         d2d_->header_format.Get(),
                         D2D1::RectF(summary_left + 300.0f, 458.0f, summary_right, 530.0f),
                         d2d_->text_brush.Get(), presentation.rank);

        const D2D1_RECT_F status_chip =
            D2D1::RectF(summary_left, 558.0f, summary_right, 610.0f);
        if (d2d_->button_selected_brush) {
            const float saved = d2d_->button_selected_brush->GetOpacity();
            d2d_->button_selected_brush->SetOpacity(0.50f * presentation.status);
            ctx->FillRoundedRectangle(D2D1::RoundedRect(status_chip, 8.0f, 8.0f),
                                      d2d_->button_selected_brush.Get());
            d2d_->button_selected_brush->SetOpacity(saved);
        }
        draw_result_text(localized_result_status(), d2d_->song_title_format.Get(),
                         D2D1::RectF(status_chip.left + 16.0f, status_chip.top + 8.0f,
                                     status_chip.right - 16.0f, status_chip.bottom - 6.0f),
                         result_success ? static_cast<ID2D1Brush*>(d2d_->accent_brush.Get())
                                        : static_cast<ID2D1Brush*>(d2d_->text_brush.Get()),
                         presentation.status);
        const std::string summary_detail = data.result.all_perfect
            ? result_loc("ALL PERFECT", "올 퍼펙트")
            : data.result.full_combo ? result_loc("FULL COMBO", "풀 콤보")
            : result_loc("DETAIL SCORE  ", "상세 점수  ") +
                  format_int_with_commas(data.result.detail_score) + " / " +
                  format_int_with_commas(data.result.max_detail_score);
        draw_result_text(summary_detail, d2d_->hud_format.Get(),
                         D2D1::RectF(summary_left, 634.0f, summary_right, 666.0f),
                         d2d_->muted_brush.Get(), presentation.status);
