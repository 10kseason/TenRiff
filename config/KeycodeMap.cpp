#include "config/KeycodeMap.h"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tenriff::config {

namespace {

std::string normalize(std::string_view name) {
    std::string out(name);
    out.erase(std::remove_if(out.begin(), out.end(), [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }), out.end());
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return out;
}

#ifdef _WIN32
std::optional<uint32_t> parse_vk_token(std::string_view token) {
    if (token.size() != 5 || token[0] != 'V' || token[1] != 'K' || token[2] != '_') {
        return std::nullopt;
    }

    auto hex_digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + (ch - 'A');
        }
        return -1;
    };

    const int hi = hex_digit(token[3]);
    const int lo = hex_digit(token[4]);
    if (hi < 0 || lo < 0) {
        return std::nullopt;
    }
    return static_cast<uint32_t>((hi << 4) | lo);
}

std::string fallback_vk_name(uint32_t keycode) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string name = "VK_00";
    name[3] = kHex[(keycode >> 4) & 0xF];
    name[4] = kHex[keycode & 0xF];
    return name;
}
#endif

}  // namespace

std::optional<uint32_t> KeycodeMap::to_keycode(std::string_view name) {
    std::string normalized = normalize(name);
    if (normalized.empty()) {
        return std::nullopt;
    }

#ifdef _WIN32
    if (normalized.size() == 1) {
        char ch = normalized[0];
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<uint32_t>(ch);
        }
        if (ch >= '0' && ch <= '9') {
            return static_cast<uint32_t>(ch);
        }
    }

    if (auto generic_vk = parse_vk_token(normalized)) {
        return generic_vk;
    }

    if (normalized == "SPACE") return static_cast<uint32_t>(VK_SPACE);
    if (normalized == "ENTER" || normalized == "RETURN") return static_cast<uint32_t>(VK_RETURN);
    if (normalized == "TAB") return static_cast<uint32_t>(VK_TAB);
    if (normalized == "BACKSPACE" || normalized == "BACK") return static_cast<uint32_t>(VK_BACK);
    if (normalized == "ESC" || normalized == "ESCAPE") return static_cast<uint32_t>(VK_ESCAPE);
    if (normalized == "DELETE" || normalized == "DEL") return static_cast<uint32_t>(VK_DELETE);
    if (normalized == "INSERT" || normalized == "INS") return static_cast<uint32_t>(VK_INSERT);
    if (normalized == "HOME") return static_cast<uint32_t>(VK_HOME);
    if (normalized == "END") return static_cast<uint32_t>(VK_END);
    if (normalized == "PAGEUP" || normalized == "PGUP" || normalized == "PRIOR") {
        return static_cast<uint32_t>(VK_PRIOR);
    }
    if (normalized == "PAGEDOWN" || normalized == "PGDN" || normalized == "NEXT") {
        return static_cast<uint32_t>(VK_NEXT);
    }
    if (normalized == "CAPSLOCK") return static_cast<uint32_t>(VK_CAPITAL);
    if (normalized == "NUMLOCK") return static_cast<uint32_t>(VK_NUMLOCK);
    if (normalized == "SCROLLLOCK") return static_cast<uint32_t>(VK_SCROLL);
    if (normalized == "PRINTSCREEN" || normalized == "PRTSC") return static_cast<uint32_t>(VK_SNAPSHOT);
    if (normalized == "PAUSE") return static_cast<uint32_t>(VK_PAUSE);

    if (normalized == "LSHIFT") return static_cast<uint32_t>(VK_LSHIFT);
    if (normalized == "RSHIFT") return static_cast<uint32_t>(VK_RSHIFT);
    if (normalized == "LCTRL" || normalized == "LCONTROL") return static_cast<uint32_t>(VK_LCONTROL);
    if (normalized == "RCTRL" || normalized == "RCONTROL") return static_cast<uint32_t>(VK_RCONTROL);
    if (normalized == "LALT" || normalized == "LMENU") return static_cast<uint32_t>(VK_LMENU);
    if (normalized == "RALT" || normalized == "RMENU") return static_cast<uint32_t>(VK_RMENU);
    if (normalized == "LWIN") return static_cast<uint32_t>(VK_LWIN);
    if (normalized == "RWIN") return static_cast<uint32_t>(VK_RWIN);
    if (normalized == "APPS" || normalized == "MENU") return static_cast<uint32_t>(VK_APPS);

    if (normalized == "LEFT") return static_cast<uint32_t>(VK_LEFT);
    if (normalized == "RIGHT") return static_cast<uint32_t>(VK_RIGHT);
    if (normalized == "UP") return static_cast<uint32_t>(VK_UP);
    if (normalized == "DOWN") return static_cast<uint32_t>(VK_DOWN);

    if (normalized == "F1") return static_cast<uint32_t>(VK_F1);
    if (normalized == "F2") return static_cast<uint32_t>(VK_F2);
    if (normalized == "F3") return static_cast<uint32_t>(VK_F3);
    if (normalized == "F4") return static_cast<uint32_t>(VK_F4);
    if (normalized == "F5") return static_cast<uint32_t>(VK_F5);
    if (normalized == "F6") return static_cast<uint32_t>(VK_F6);
    if (normalized == "F7") return static_cast<uint32_t>(VK_F7);
    if (normalized == "F8") return static_cast<uint32_t>(VK_F8);
    if (normalized == "F9") return static_cast<uint32_t>(VK_F9);
    if (normalized == "F10") return static_cast<uint32_t>(VK_F10);
    if (normalized == "F11") return static_cast<uint32_t>(VK_F11);
    if (normalized == "F12") return static_cast<uint32_t>(VK_F12);

    if (normalized == "NUMPAD0") return static_cast<uint32_t>(VK_NUMPAD0);
    if (normalized == "NUMPAD1") return static_cast<uint32_t>(VK_NUMPAD1);
    if (normalized == "NUMPAD2") return static_cast<uint32_t>(VK_NUMPAD2);
    if (normalized == "NUMPAD3") return static_cast<uint32_t>(VK_NUMPAD3);
    if (normalized == "NUMPAD4") return static_cast<uint32_t>(VK_NUMPAD4);
    if (normalized == "NUMPAD5") return static_cast<uint32_t>(VK_NUMPAD5);
    if (normalized == "NUMPAD6") return static_cast<uint32_t>(VK_NUMPAD6);
    if (normalized == "NUMPAD7") return static_cast<uint32_t>(VK_NUMPAD7);
    if (normalized == "NUMPAD8") return static_cast<uint32_t>(VK_NUMPAD8);
    if (normalized == "NUMPAD9") return static_cast<uint32_t>(VK_NUMPAD9);
    if (normalized == "MULTIPLY") return static_cast<uint32_t>(VK_MULTIPLY);
    if (normalized == "ADD" || normalized == "NUMPADPLUS") return static_cast<uint32_t>(VK_ADD);
    if (normalized == "SUBTRACT" || normalized == "NUMPADMINUS") return static_cast<uint32_t>(VK_SUBTRACT);
    if (normalized == "DECIMAL" || normalized == "NUMPADDECIMAL") return static_cast<uint32_t>(VK_DECIMAL);
    if (normalized == "DIVIDE" || normalized == "NUMPADDIVIDE") return static_cast<uint32_t>(VK_DIVIDE);

    if (normalized == "SEMICOLON") return static_cast<uint32_t>(VK_OEM_1);
    if (normalized == "PLUS" || normalized == "EQUALS") return static_cast<uint32_t>(VK_OEM_PLUS);
    if (normalized == "COMMA") return static_cast<uint32_t>(VK_OEM_COMMA);
    if (normalized == "MINUS") return static_cast<uint32_t>(VK_OEM_MINUS);
    if (normalized == "PERIOD" || normalized == "DOT") return static_cast<uint32_t>(VK_OEM_PERIOD);
    if (normalized == "SLASH") return static_cast<uint32_t>(VK_OEM_2);
    if (normalized == "GRAVE" || normalized == "BACKTICK") return static_cast<uint32_t>(VK_OEM_3);
    if (normalized == "LBRACKET" || normalized == "LEFTBRACKET") return static_cast<uint32_t>(VK_OEM_4);
    if (normalized == "BACKSLASH") return static_cast<uint32_t>(VK_OEM_5);
    if (normalized == "RBRACKET" || normalized == "RIGHTBRACKET") return static_cast<uint32_t>(VK_OEM_6);
    if (normalized == "APOSTROPHE" || normalized == "QUOTE") return static_cast<uint32_t>(VK_OEM_7);

    return std::nullopt;
#else
    if (normalized.size() == 1) {
        return static_cast<uint32_t>(normalized[0]);
    }
    if (normalized == "SPACE") return static_cast<uint32_t>(' ');
    if (normalized == "ENTER") return static_cast<uint32_t>('\n');
    return std::nullopt;
#endif
}

std::string KeycodeMap::to_name(uint32_t keycode) {
#ifdef _WIN32
    if (keycode >= 'A' && keycode <= 'Z') {
        char ch = static_cast<char>(keycode);
        return std::string(1, ch);
    }
    if (keycode >= '0' && keycode <= '9') {
        char ch = static_cast<char>(keycode);
        return std::string(1, ch);
    }

    switch (keycode) {
        case VK_SPACE: return "Space";
        case VK_RETURN: return "Enter";
        case VK_TAB: return "Tab";
        case VK_BACK: return "Backspace";
        case VK_ESCAPE: return "Esc";
        case VK_DELETE: return "Delete";
        case VK_INSERT: return "Insert";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_CAPITAL: return "CapsLock";
        case VK_NUMLOCK: return "NumLock";
        case VK_SCROLL: return "ScrollLock";
        case VK_SNAPSHOT: return "PrintScreen";
        case VK_PAUSE: return "Pause";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_LCONTROL: return "LControl";
        case VK_RCONTROL: return "RControl";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";
        case VK_LWIN: return "LWin";
        case VK_RWIN: return "RWin";
        case VK_APPS: return "Apps";
        case VK_LEFT: return "Left";
        case VK_RIGHT: return "Right";
        case VK_UP: return "Up";
        case VK_DOWN: return "Down";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_NUMPAD0: return "Numpad0";
        case VK_NUMPAD1: return "Numpad1";
        case VK_NUMPAD2: return "Numpad2";
        case VK_NUMPAD3: return "Numpad3";
        case VK_NUMPAD4: return "Numpad4";
        case VK_NUMPAD5: return "Numpad5";
        case VK_NUMPAD6: return "Numpad6";
        case VK_NUMPAD7: return "Numpad7";
        case VK_NUMPAD8: return "Numpad8";
        case VK_NUMPAD9: return "Numpad9";
        case VK_MULTIPLY: return "Multiply";
        case VK_ADD: return "Add";
        case VK_SUBTRACT: return "Subtract";
        case VK_DECIMAL: return "Decimal";
        case VK_DIVIDE: return "Divide";
        case VK_OEM_1: return "Semicolon";
        case VK_OEM_PLUS: return "Plus";
        case VK_OEM_COMMA: return "Comma";
        case VK_OEM_MINUS: return "Minus";
        case VK_OEM_PERIOD: return "Period";
        case VK_OEM_2: return "Slash";
        case VK_OEM_3: return "Grave";
        case VK_OEM_4: return "LBracket";
        case VK_OEM_5: return "Backslash";
        case VK_OEM_6: return "RBracket";
        case VK_OEM_7: return "Apostrophe";
        default: break;
    }

    if (keycode <= 0xFFu) {
        return fallback_vk_name(keycode);
    }
#endif

    return "Unknown";
}

}  // namespace tenriff::config
