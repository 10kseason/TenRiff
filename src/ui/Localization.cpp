#include "ui/Localization.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>
#include <utility>

namespace tenriff::ui {
namespace {

constexpr std::pair<std::string_view, std::string_view> kJapaneseStrings[] = {
#include "ui/JapaneseStrings.inc"
};

const auto& japanese_catalog() {
    // Construct once. Keys and values refer to static UTF-8 literals, and lookup
    // does not allocate even on the renderer's per-frame path.
    static const std::unordered_map<std::string_view, std::string_view> catalog(
        std::begin(kJapaneseStrings), std::end(kJapaneseStrings));
    return catalog;
}

}  // namespace

Language language_from_token(std::string_view token) {
    std::string normalized(token);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "ko" || normalized == "kr" || normalized == "korean" || normalized == "ko-kr") {
        return Language::Korean;
    }
    if (normalized == "ja" || normalized == "jp" || normalized == "japanese" ||
        normalized == "ja-jp" || normalized == "ja_jp") {
        return Language::Japanese;
    }
    return Language::English;
}

std::string_view language_token(Language language) noexcept {
    switch (language) {
        case Language::Korean: return "ko";
        case Language::Japanese: return "ja";
        default: return "en";
    }
}

Language cycle_language(Language language, int direction) noexcept {
    constexpr std::array<Language, 3> order{Language::English, Language::Korean, Language::Japanese};
    const auto found = std::find(order.begin(), order.end(), language);
    const int index = found == order.end() ? 0 : static_cast<int>(found - order.begin());
    if (direction == 0) return order[static_cast<std::size_t>(index)];
    return order[static_cast<std::size_t>((index + (direction < 0 ? 2 : 1)) % 3)];
}

std::string text(Language language, std::string_view english, std::string_view korean) {
    if (language == Language::Korean) return std::string(korean);
    if (language == Language::Japanese) {
        const auto& catalog = japanese_catalog();
        const auto found = catalog.find(english);
        if (found != catalog.end() && !found->second.empty()) {
            return std::string(found->second);
        }
    }
    return std::string(english);
}

}  // namespace tenriff::ui
