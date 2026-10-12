        // Included inside the result renderer; all values are observational.
        const auto& analysis = data.result.analysis;
        const auto text_at = [&](const std::string& text, float left, float top, float right,
                                 bool muted = false, float height = 32.0f) {
            draw_result_text(text, muted ? d2d_->result_body_format.Get() : d2d_->result_label_format.Get(),
                D2D1::RectF(left, top, right, top + height),
                muted ? d2d_->muted_brush.Get() : d2d_->text_brush.Get(), 1.0f);
        };
        const auto number = [&](double value, uint64_t count, bool deviation = false) {
            return count == 0 || (deviation && count < 2) ? std::string("--") : format_decimal(value);
        };
        draw_result_panel(D2D1::RectF(64, 148, 664, 978), 1.0f);
        draw_result_panel(D2D1::RectF(688, 148, 1856, 978), 1.0f);
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(64, 148, 664, 978), 12, 12), semantic_brush(0x28C76B), 3);
        ctx->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(688, 148, 1856, 978), 12, 12), semantic_brush(0x28C76B), 3);
        draw_result_text(result_loc("RESULT ANALYSIS", "결과 분석"), d2d_->title_format.Get(),
            D2D1::RectF(72, 48, 640, 104), d2d_->text_brush.Get(), 1.0f);
        draw_analysis_tabs();
        text_at(data.result.title.empty() ? data.result.track : data.result.title, 94, 178, 638, false, 68);
        text_at(localized_result_status() + "  /  " + format_decimal(data.result.accuracy) + "%", 94, 254, 638, true);
        const auto& total = analysis.total;
        if (!analysis.available) {
            text_at(result_loc("No analysis data in this record.", "이 기록에는 분석 데이터가 없습니다."), 94, 340, 638);
            text_at(data.result.replay_available
                ? result_loc("Replay it to calculate fresh analysis.", "리플레이를 재생하면 새로 분석합니다.")
                : result_loc("Replay file is missing or unavailable.", "리플레이 파일이 없거나 사용할 수 없습니다."),
                94, 384, 638, true, 64);
            text_at(result_loc("Old averages cannot reconstruct a distribution.", "이전 평균값으로 분포를 추정하지 않습니다."),
                    718, 250, 1826, true);
        } else {
            text_at(result_loc("ALL MEASURED INPUTS", "측정된 입력 전체") + "  N=" + std::to_string(total.measured_count),
                    94, 310, 638);
            draw_measured_histogram(total, D2D1::RectF(100, 372, 628, 530), 0.90f);
            text_at("<-100", 94, 538, 224, true);
            text_at("0 ms", 348, 538, 432, true);
            text_at(">=100", 542, 538, 638, true);
            text_at(result_loc("5 ms bins / end bins include overflow", "5ms 구간 / 양 끝은 범위 밖 표본 포함"),
                    94, 578, 638, true);
            text_at(result_loc("Signed mean", "부호 있는 평균") + "  " + number(total.mean_delta_ms, total.measured_count) + " ms",
                    94, 632, 638);
            text_at(result_loc("Mean absolute error", "평균 절대 오차") + "  " + number(total.mean_abs_delta_ms, total.measured_count) + " ms",
                    94, 676, 638);
            text_at(result_loc("Sample standard deviation", "표본 표준편차") + "  " + number(total.stddev_delta_ms(), total.measured_count, true) + " ms",
                    94, 720, 638);
            text_at(result_loc("Unmeasured misses", "측정 불가 미스") + "  " + std::to_string(total.unmeasured_miss_count),
                    94, 778, 638);
            draw_result_text("FAST " + std::to_string(total.fast_count), d2d_->result_action_format.Get(),
                D2D1::RectF(94, 822, 355, 860), semantic_brush(0x69A6FF), 1.0f);
            draw_result_text("SLOW " + std::to_string(total.slow_count), d2d_->result_action_format.Get(),
                D2D1::RectF(355, 822, 638, 860), semantic_brush(0xFF7189), 1.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
            text_at(result_loc("FAST/SLOW excludes P-GREAT; histogram includes it.", "FAST/SLOW는 P-GREAT 제외, 분포는 포함합니다."),
                    94, 868, 638, true, 48);
            text_at(result_loc("Mods: ", "모드: ") + data.result.analysis_mods, 94, 924, 638, true, 44);

            if (data.result.analysis_page == 1) {
                const int first = analysis.lane_count > 16 ? std::clamp(data.result.analysis_lane_bank, 0, 1) * 16 : 0;
                text_at(result_loc("PER LANE", "레인별 분포"), 718, 178, 1310);
                if (analysis.lane_count > 16) {
                    const auto bank = D2D1::RectF(1432, 166, 1826, 210);
                    draw_result_panel(bank, 1.0f, true);
                    draw_result_control_text(result_loc("Up/Down: lanes ", "위/아래: 레인 ") +
                        std::to_string(first + 1) + "-" + std::to_string((std::min)(first + 16, analysis.lane_count)),
                        d2d_->result_label_format.Get(), bank, d2d_->text_brush.Get(), 1.0f);
                    register_hit(bank, MenuHitTargetKind::SettingsRow, 7);
                }
                text_at(result_loc("Lane", "레인"), 718, 230, 810, true);
                text_at(result_loc("Measured distribution", "실측 분포"), 812, 230, 1050, true);
                text_at("N", 1066, 230, 1150, true);
                text_at("F / S", 1160, 230, 1280, true);
                text_at(result_loc("Miss*", "미스*"), 1290, 230, 1390, true);
                text_at(result_loc("Mean", "평균"), 1400, 230, 1528, true);
                text_at(result_loc("Abs.", "절대"), 1540, 230, 1676, true);
                text_at(result_loc("Std.dev.", "표준편차"), 1690, 230, 1826, true);
                for (int lane = first; lane < (std::min)(first + 16, analysis.lane_count); ++lane) {
                    const auto& d = analysis.by_lane[static_cast<std::size_t>(lane)];
                    const float y = 280.0f + static_cast<float>(lane - first) * 38.0f;
                    text_at(std::to_string(lane + 1), 734, y, 806);
                    draw_measured_histogram(d, D2D1::RectF(812, y + 2, 1040, y + 28), 0.85f);
                    text_at(std::to_string(d.measured_count), 1066, y, 1154);
                    text_at(std::to_string(d.fast_count) + "/" + std::to_string(d.slow_count), 1160, y, 1284);
                    text_at(std::to_string(d.unmeasured_miss_count), 1290, y, 1390);
                    text_at(number(d.mean_delta_ms, d.measured_count), 1400, y, 1528);
                    text_at(number(d.mean_abs_delta_ms, d.measured_count), 1540, y, 1676);
                    text_at(number(d.stddev_delta_ms(), d.measured_count, true), 1690, y, 1826);
                }
                text_at(result_loc("* Unmeasured misses. Timing in ms; each histogram has its own height scale.",
                                   "* 측정 불가 미스. 시간 단위 ms, 각 분포의 높이는 개별 최댓값 기준입니다."),
                        718, 914, 1826, true, 52);
            } else if (data.result.analysis_page == 2) {
                text_at(result_loc("NOTE TYPES / LN EVENTS", "노트 종류 / LN 이벤트"), 718, 178, 1826);
                const std::array<std::string, gameplay::kTimingEventKindCount> labels{{
                    result_loc("TAP", "단노트"), result_loc("LN HEAD", "LN 시작 타건"),
                    result_loc("LN RELEASE", "LN 키 해제"), result_loc("HOLD FAILURE", "LN 유지 실패"),
                    result_loc("AUTO COMPLETION", "LN 자동 완료"), result_loc("EMPTY PRESS", "빈 타건")}};
                float y = 234.0f;
                for (std::size_t i = 0; i < analysis.by_kind.size(); ++i) {
                    const auto& d = analysis.by_kind[i];
                    if (d.event_count == 0) continue;  // Only events meaningful to this play's rules.
                    draw_result_panel(D2D1::RectF(712, y, 1832, y + 100), 1.0f);
                    text_at(labels[i] + "  (" + std::to_string(d.event_count) + ")", 728, y + 7, 1170);
                    text_at("N=" + std::to_string(d.measured_count) + "   " + result_loc("Miss* ", "미스* ") +
                        std::to_string(d.unmeasured_miss_count) + "   F/S " + std::to_string(d.fast_count) + "/" +
                        std::to_string(d.slow_count), 728, y + 47, 1170);
                    draw_measured_histogram(d, D2D1::RectF(1194, y + 12, 1478, y + 84), 0.90f);
                    text_at(result_loc("Mean / Abs. / Std.dev. (ms)", "평균 / 절대 / 표준편차 (ms)"), 1500, y + 7, 1818, true);
                    text_at(number(d.mean_delta_ms, d.measured_count) + " / " + number(d.mean_abs_delta_ms, d.measured_count) +
                        " / " + number(d.stddev_delta_ms(), d.measured_count, true), 1500, y + 47, 1818);
                    y += 108.0f;
                }
                text_at(result_loc("* Missing head / required release timeout. Auto completion is not a 0 ms hit.",
                                   "* 놓친 시작 타건 / 필수 해제 시간 초과. 자동 완료는 0ms 타건이 아닙니다."),
                        718, 888, 1826, true, 40);
                text_at(result_loc("Hold failure = measured release graded BAD; no scoring rule is added.",
                                   "유지 실패 = 실제 키 해제가 BAD인 경우. 새 점수 규칙을 추가하지 않습니다."),
                        718, 930, 1826, true);
            } else {
                text_at(result_loc("ACCURACY OVER SONG TIME", "곡 진행 시간별 정확도"), 718, 178, 1826);
                text_at(result_loc("12 chart-time intervals / native categorical accuracy, including existing LN weights",
                                   "채보 시간을 12등분 / 기존 LN 가중치를 포함한 본체 판정 정확도"), 718, 226, 1826, true);
                for (std::size_t i = 0; i < analysis.segments.size(); ++i) {
                    const auto& bucket = analysis.segments[i];
                    const float y = 286.0f + static_cast<float>(i) * 48.0f;
                    const double duration = static_cast<double>(analysis.duration_samples) / analysis.sample_rate;
                    const auto from = duration * static_cast<double>(i) / analysis.segments.size();
                    const auto to = duration * static_cast<double>(i + 1) / analysis.segments.size();
                    text_at(format_decimal(from) + "-" + format_decimal(to) + "s", 718, y, 964);
                    if (d2d_->card_brush) ctx->FillRectangle(D2D1::RectF(988, y + 4, 1508, y + 30), d2d_->card_brush.Get());
                    if (bucket.event_count > 0 && d2d_->accent_brush) {
                        ctx->FillRectangle(D2D1::RectF(988, y + 4,
                            988 + static_cast<float>(bucket.accuracy_percent()) * 5.2f, y + 30), d2d_->accent_brush.Get());
                    }
                    text_at(bucket.event_count ? format_decimal(bucket.accuracy_percent()) + "%" : "--", 1530, y, 1664);
                    text_at("N=" + std::to_string(bucket.event_count), 1678, y, 1826);
                }
                text_at(result_loc("N = graded events, not timing samples. Empty intervals are --, not 0%.",
                                   "N = 판정 이벤트 수 (타이밍 표본 수 아님). 빈 구간은 0% 대신 --입니다."),
                        718, 884, 1826, true, 40);
                text_at(result_loc("Heads/tails use their chart position; unplayed sections remain empty.",
                                   "시작·끝 판정은 채보 위치에 집계합니다. 플레이하지 않은 구간은 비어 있습니다."),
                        718, 928, 1826, true);
            }
        }
        const auto back = D2D1::RectF(72, 996, 560, 1050);
        const auto proceed = D2D1::RectF(1360, 996, 1848, 1050);
        draw_result_panel(back, 1.0f);
        draw_result_panel(proceed, 1.0f, true);
        ctx->FillRoundedRectangle(D2D1::RoundedRect(proceed, 12, 12), semantic_brush(0x79D7FF));
        draw_result_control_text(result_loc("ESC / SUMMARY", "ESC / 요약"), d2d_->result_label_format.Get(), back, d2d_->text_brush.Get(), 1.0f);
        draw_result_control_text(result_loc("ENTER / CONTINUE", "ENTER / 계속"), d2d_->result_label_format.Get(), proceed, semantic_brush(0x0B1620), 1.0f);
        register_hit(back, MenuHitTargetKind::SettingsRow, 3);
        register_hit(proceed, MenuHitTargetKind::SettingsRow, 0);
        draw_result_text(result_loc("F2 / Next tab    R / Retry    F1 / Replay", "F2 / 다음 탭    R / 재도전    F1 / 리플레이"),
            d2d_->result_body_format.Get(), D2D1::RectF(600, 992, 1340, 1020), d2d_->muted_brush.Get(),
            1.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
        draw_result_text(result_loc("F9 / Screenshot (PNG)  >  screenshots/", "F9 / 스크린샷 (PNG)  >  screenshots/"),
            d2d_->result_body_format.Get(), D2D1::RectF(600, 1024, 1340, 1052), d2d_->muted_brush.Get(),
            1.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
