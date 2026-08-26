#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

namespace tenriff::render {

// Cheap, allocation-free single-line fit estimate for menu text. DirectWrite
// still clips the final draw, while this scale prevents long localized labels
// from losing their tail inside skin-defined rectangles.
[[nodiscard]] inline float estimate_single_line_text_scale(std::wstring_view text,
                                                           float font_size,
                                                           float available_width,
                                                           float available_height,
                                                           float minimum_scale = 0.45f) {
    if (text.empty() || !std::isfinite(font_size) || font_size <= 0.0f ||
        !std::isfinite(available_width) || !std::isfinite(available_height) ||
        available_width <= 0.0f || available_height <= 0.0f) {
        return 1.0f;
    }

    float width_em = 0.0f;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const wchar_t ch = text[index];
        if (ch >= 0xD800 && ch <= 0xDBFF && index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            width_em += 1.08f;
            ++index;
        } else if (ch == L' ' || ch == L'\t') {
            width_em += 0.36f;
        } else if (ch < 0x80) {
            if (ch == L'i' || ch == L'l' || ch == L'I' || ch == L'1' ||
                ch == L'.' || ch == L',' || ch == L':' || ch == L';' ||
                ch == L'!' || ch == L'|' || ch == L'\'' || ch == L'`') {
                width_em += 0.36f;
            } else if (ch == L'W' || ch == L'M' || ch == L'@' || ch == L'#' ||
                       ch == L'%' || ch == L'&') {
                width_em += 0.92f;
            } else if (ch >= L'A' && ch <= L'Z') {
                width_em += 0.70f;
            } else if ((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9')) {
                width_em += 0.60f;
            } else {
                width_em += 0.52f;
            }
        } else {
            // Hangul, CJK, and most full-width UI symbols occupy roughly one em.
            width_em += 1.04f;
        }
    }

    // Small safety margins absorb family/weight differences without measuring a
    // new DirectWrite layout for every label on every frame.
    const float estimated_width = std::max(1.0f, width_em * font_size * 1.06f);
    const float estimated_height = std::max(1.0f, font_size * 1.24f);
    const float scale = std::min({1.0f,
                                  available_width / estimated_width,
                                  available_height / estimated_height});
    return std::clamp(scale, std::clamp(minimum_scale, 0.05f, 1.0f), 1.0f);
}

}  // namespace tenriff::render
