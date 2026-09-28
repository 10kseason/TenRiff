#pragma once

#include <string>
#include <string_view>

namespace tenriff::ui {

enum class Language { English, Korean, Japanese };

// Config tokens stay independent of translated display labels. Unknown tokens
// keep the historical English fallback; aliases are accepted only on input.
[[nodiscard]] Language language_from_token(std::string_view token);
[[nodiscard]] std::string_view language_token(Language language) noexcept;
[[nodiscard]] Language cycle_language(Language language, int direction) noexcept;

// Keep existing English/Korean call sites intact while sharing one Japanese
// catalog. Unrecognized strings fall back to English, never to Korean.
[[nodiscard]] std::string text(Language language,
                               std::string_view english,
                               std::string_view korean);
}  // namespace tenriff::ui
