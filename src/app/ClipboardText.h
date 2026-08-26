#pragma once

#include <optional>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tenriff::app {

[[nodiscard]] inline std::optional<std::string> clipboard_text_utf8() {
#ifdef _WIN32
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(nullptr)) {
        return std::nullopt;
    }
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const wchar_t* text = data ? static_cast<const wchar_t*>(GlobalLock(data)) : nullptr;
    std::wstring value = text ? std::wstring(text) : std::wstring{};
    if (text) GlobalUnlock(data);
    CloseClipboard();

    const auto is_space = [](wchar_t ch) {
        return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
    };
    while (!value.empty() && is_space(value.front())) value.erase(value.begin());
    while (!value.empty() && is_space(value.back())) value.pop_back();
    if (value.empty()) return std::nullopt;

    const int byte_count = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (byte_count <= 0) return std::nullopt;
    std::string utf8(static_cast<std::size_t>(byte_count), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            0,
                            value.data(),
                            static_cast<int>(value.size()),
                            utf8.data(),
                            byte_count,
                            nullptr,
                            nullptr) != byte_count) {
        return std::nullopt;
    }
    return utf8;
#else
    return std::nullopt;
#endif
}

}  // namespace tenriff::app
