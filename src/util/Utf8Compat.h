#pragma once

#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tenriff::util {

#ifdef _WIN32
inline std::wstring byte_widen(std::string_view value) {
    std::wstring wide;
    wide.reserve(value.size());
    for (unsigned char ch : value) {
        wide.push_back(static_cast<wchar_t>(ch));
    }
    return wide;
}

inline std::wstring wide_from_multibyte(std::string_view value, UINT code_page, DWORD flags = 0) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return byte_widen(value);
    }

    const int narrow_size = static_cast<int>(value.size());
    const int wide_size = MultiByteToWideChar(code_page, flags, value.data(), narrow_size, nullptr, 0);
    if (wide_size <= 0) {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(code_page, flags, value.data(), narrow_size, wide.data(), wide_size) <= 0) {
        return {};
    }
    return wide;
}

inline std::string utf8_from_wide_lossy(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }

    const int wide_size = static_cast<int>(value.size());
    const int utf8_size = WideCharToMultiByte(CP_UTF8, 0, value.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(utf8_size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.data(), wide_size, utf8.data(), utf8_size, nullptr, nullptr) <= 0) {
        return {};
    }
    return utf8;
}
#endif

inline std::wstring wide_from_utf8_lossy(std::string_view value) {
#ifdef _WIN32
    if (value.empty()) {
        return {};
    }

    if (std::wstring wide = wide_from_multibyte(value, CP_UTF8, MB_ERR_INVALID_CHARS); !wide.empty()) {
        return wide;
    }
    if (std::wstring wide = wide_from_multibyte(value, 932); !wide.empty()) {
        return wide;
    }
    if (std::wstring wide = wide_from_multibyte(value, CP_ACP); !wide.empty()) {
        return wide;
    }
    return byte_widen(value);
#else
    return std::wstring(value.begin(), value.end());
#endif
}

inline std::string ensure_utf8_text(std::string_view value) {
#ifdef _WIN32
    const std::wstring wide = wide_from_utf8_lossy(value);
    const std::string utf8 = utf8_from_wide_lossy(wide);
    return utf8.empty() ? std::string(value) : utf8;
#else
    return std::string(value);
#endif
}

inline std::string sanitize_ui_text(std::string_view value) {
    const std::string utf8 = ensure_utf8_text(value);
    if (utf8.empty()) {
        return {};
    }

    std::string cleaned;
    cleaned.reserve(utf8.size());
    bool last_was_space = true;
    for (unsigned char ch : utf8) {
        if (ch == '\0') {
            continue;
        }
        if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\v' || ch == ' ') {
            if (!cleaned.empty() && !last_was_space) {
                cleaned.push_back(' ');
                last_was_space = true;
            }
            continue;
        }
        if (ch < 0x20 || ch == 0x7F) {
            continue;
        }
        cleaned.push_back(static_cast<char>(ch));
        last_was_space = false;
    }

    while (!cleaned.empty() && cleaned.back() == ' ') {
        cleaned.pop_back();
    }
    return cleaned;
}

inline std::filesystem::path path_from_utf8_lossy(std::string_view value) {
#ifdef _WIN32
    return std::filesystem::path(wide_from_utf8_lossy(value));
#else
    return std::filesystem::path(value);
#endif
}

}  // namespace tenriff::util
