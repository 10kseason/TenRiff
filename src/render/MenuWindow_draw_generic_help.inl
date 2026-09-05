        // Screen notes are paged independently of selection so long explanations
        // never consume the settings list's visible rows. Keep a measured layout
        // cached until the text, width or UI font changes.
        auto draw_generic_help = [&](const D2D1_RECT_F& rect,
                                     std::string_view heading,
                                     const std::vector<std::string>& guide_notes,
                                     const std::vector<std::string>& footer_notes,
                                     bool separate_paragraphs = true) {
            draw_glass_panel(rect, 14.0f, 0.92f, 0, false, 0);
            draw_text_clipped(wloc("GUIDE", "사용 안내"), d2d_->song_title_format.Get(),
                              D2D1::RectF(rect.left + 24, rect.top + 20, rect.right - 24, rect.top + 54),
                              d2d_->text_brush.Get());
            std::wstring notes;
            for (const auto& note : guide_notes) {
                if (!notes.empty()) notes += separate_paragraphs ? L"\n\n" : L"\n";
                notes += to_wide(note);
            }
            for (const auto& note : footer_notes) {
                if (!notes.empty()) notes += separate_paragraphs ? L"\n\n" : L"\n";
                notes += to_wide(note);
            }
            if (notes.empty()) notes = wloc("Select an item to open it or change its value. Press F1 for keyboard help.",
                                            "항목을 선택해 열거나 값을 바꿀 수 있습니다. 키보드 조작은 F1 도움말을 참고하세요.");
            constexpr float line_height = 26.0f;
            const float width = rect.right - rect.left - 48.0f;
            const float page_height = std::floor((rect.bottom - rect.top - 140.0f) / line_height) * line_height;
            if (width <= 0 || page_height < line_height || !d2d_->dwrite_factory || !d2d_->body_format) return;
            const std::wstring signature = to_wide(std::string(heading)) + L"\n" + notes;
            if (!d2d_->generic_help_layout || d2d_->generic_help_text != signature || d2d_->generic_help_width != width) {
                d2d_->generic_help_layout.Reset();
                const HRESULT hr = d2d_->dwrite_factory->CreateTextLayout(
                    notes.c_str(), static_cast<UINT32>(notes.size()), d2d_->body_format.Get(),
                    width, 100000.0f, d2d_->generic_help_layout.ReleaseAndGetAddressOf());
                if (FAILED(hr)) return;
                d2d_->generic_help_layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                d2d_->generic_help_layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                d2d_->generic_help_layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                d2d_->generic_help_layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, line_height, 20.0f);
                d2d_->generic_help_text = signature;
                d2d_->generic_help_width = width;
                generic_help_page_.store(0, std::memory_order_relaxed);
            }
            DWRITE_TEXT_METRICS metrics{};
            if (FAILED(d2d_->generic_help_layout->GetMetrics(&metrics))) return;
            const int page_count = std::max(1, static_cast<int>(std::ceil(metrics.height / page_height)));
            const int page = std::clamp(generic_help_page_.load(std::memory_order_relaxed), 0, page_count - 1);
            const D2D1_RECT_F body = D2D1::RectF(rect.left + 24, rect.top + 72, rect.right - 24, rect.top + 72 + page_height);
            ctx->PushAxisAlignedClip(body, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            ctx->DrawTextLayout(D2D1::Point2F(body.left, body.top - page * page_height),
                                d2d_->generic_help_layout.Get(), d2d_->muted_brush.Get());
            ctx->PopAxisAlignedClip();
            draw_text_clipped(to_wide(std::to_string(page + 1) + " / " + std::to_string(page_count)),
                              d2d_->hud_format.Get(), D2D1::RectF(rect.left + 24, rect.bottom - 44, rect.right - 170, rect.bottom - 18),
                              d2d_->muted_brush.Get());
            const auto page_button = [&](const D2D1_RECT_F& button, const wchar_t* label, int target, bool enabled) {
                draw_glass_panel(button, 8, enabled ? 1.0f : 0.4f, 0, enabled, 0);
                if (enabled) register_hit(button, MenuHitTargetKind::GenericHelpPage, target);
                const auto saved = d2d_->song_title_format->GetParagraphAlignment();
                d2d_->song_title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                draw_text_clipped_aligned(label, d2d_->song_title_format.Get(), button,
                                          enabled ? d2d_->text_brush.Get() : d2d_->muted_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                d2d_->song_title_format->SetParagraphAlignment(saved);
            };
            if (page_count > 1) {
                page_button(D2D1::RectF(rect.right - 150, rect.bottom - 56, rect.right - 94, rect.bottom - 16), L"\u2039", page - 1, page > 0);
                page_button(D2D1::RectF(rect.right - 82, rect.bottom - 56, rect.right - 26, rect.bottom - 16), L"\u203a", page + 1, page + 1 < page_count);
            }
        };
