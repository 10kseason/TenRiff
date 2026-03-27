#include "config/KeycodeMap.h"

#include <algorithm>
#include <array>
#include <cstdio>

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
constexpr uint32_t kScanKeycodePrefix = 0x10000u;

struct ScanAlias {
    std::string_view normalized_name;
    uint16_t scan_code;
};

constexpr std::array<ScanAlias, 11> kLayoutSensitiveScanAliases{{
    {"SEMICOLON", 0x0027u},
    {"PLUS", 0x000Du},
    {"EQUALS", 0x000Du},
    {"COMMA", 0x0033u},
    {"MINUS", 0x000Cu},
    {"PERIOD", 0x0034u},
    {"DOT", 0x0034u},
    {"SLASH", 0x0035u},
    {"GRAVE", 0x0029u},
    {"BACKTICK", 0x0029u},
    {"LBRACKET", 0x001Au},
}};

constexpr std::array<ScanAlias, 3> kAdditionalLayoutSensitiveScanAliases{{
    {"BACKSLASH", 0x002Bu},
    {"RBRACKET", 0x001Bu},
    {"APOSTROPHE", 0x0028u},
}};

constexpr uint16_t encode_scan_code(uint16_t make_code, uint16_t flags) {
    uint16_t scan_code = static_cast<uint16_t>(make_code & 0x00FFu);
    if ((flags & RI_KEY_E1) != 0u) {
        scan_code |= 0xE100u;
    } else if ((flags & RI_KEY_E0) != 0u) {
        scan_code |= 0xE000u;
    }
    return scan_code;
}

constexpr uint32_t scancode_keycode(uint16_t scan_code) {
    return kScanKeycodePrefix | static_cast<uint32_t>(scan_code);
}

constexpr bool is_scancode_keycode(uint32_t keycode) {
    return (keycode & kScanKeycodePrefix) != 0u;
}

constexpr uint16_t decode_scancode_keycode(uint32_t keycode) {
    return static_cast<uint16_t>(keycode & 0xFFFFu);
}

std::optional<uint16_t> scan_code_from_name(std::string_view normalized_name) {
    for (const auto& alias : kLayoutSensitiveScanAliases) {
        if (alias.normalized_name == normalized_name) {
            return alias.scan_code;
        }
    }
    for (const auto& alias : kAdditionalLayoutSensitiveScanAliases) {
        if (alias.normalized_name == normalized_name) {
            return alias.scan_code;
        }
    }
    if (normalized_name == "LEFTBRACKET") {
        return static_cast<uint16_t>(0x001Au);
    }
    if (normalized_name == "RIGHTBRACKET") {
        return static_cast<uint16_t>(0x001Bu);
    }
    if (normalized_name == "QUOTE") {
        return static_cast<uint16_t>(0x0028u);
    }
    return std::nullopt;
}

std::optional<uint16_t> known_scan_code_from_layout_sensitive_vk(uint32_t vkey) {
    switch (vkey) {
        case VK_OEM_1: return static_cast<uint16_t>(0x0027u);
        case VK_OEM_PLUS: return static_cast<uint16_t>(0x000Du);
        case VK_OEM_COMMA: return static_cast<uint16_t>(0x0033u);
        case VK_OEM_MINUS: return static_cast<uint16_t>(0x000Cu);
        case VK_OEM_PERIOD: return static_cast<uint16_t>(0x0034u);
        case VK_OEM_2: return static_cast<uint16_t>(0x0035u);
        case VK_OEM_3: return static_cast<uint16_t>(0x0029u);
        case VK_OEM_4: return static_cast<uint16_t>(0x001Au);
        case VK_OEM_5: return static_cast<uint16_t>(0x002Bu);
        case VK_OEM_6: return static_cast<uint16_t>(0x001Bu);
        case VK_OEM_7: return static_cast<uint16_t>(0x0028u);
        default: break;
    }
    return std::nullopt;
}

std::optional<uint32_t> layout_sensitive_vk_from_scan_code(uint16_t scan_code) {
    switch (scan_code) {
        case 0x0027u: return static_cast<uint32_t>(VK_OEM_1);
        case 0x000Du: return static_cast<uint32_t>(VK_OEM_PLUS);
        case 0x0033u: return static_cast<uint32_t>(VK_OEM_COMMA);
        case 0x000Cu: return static_cast<uint32_t>(VK_OEM_MINUS);
        case 0x0034u: return static_cast<uint32_t>(VK_OEM_PERIOD);
        case 0x0035u: return static_cast<uint32_t>(VK_OEM_2);
        case 0x0029u: return static_cast<uint32_t>(VK_OEM_3);
        case 0x001Au: return static_cast<uint32_t>(VK_OEM_4);
        case 0x002Bu: return static_cast<uint32_t>(VK_OEM_5);
        case 0x001Bu: return static_cast<uint32_t>(VK_OEM_6);
        case 0x0028u: return static_cast<uint32_t>(VK_OEM_7);
        default: break;
    }
    return std::nullopt;
}

std::string name_from_scan_code(uint16_t scan_code) {
    switch (scan_code) {
        case 0x000Du: return "Plus";
        case 0x001Au: return "LBracket";
        case 0x001Bu: return "RBracket";
        case 0x0027u: return "Semicolon";
        case 0x0028u: return "Apostrophe";
        case 0x0029u: return "Grave";
        case 0x002Bu: return "Backslash";
        case 0x0033u: return "Comma";
        case 0x0034u: return "Period";
        case 0x0035u: return "Slash";
        default: break;
    }

    char buffer[16] = {};
    if ((scan_code & 0xFF00u) == 0xE000u || (scan_code & 0xFF00u) == 0xE100u) {
        std::snprintf(buffer, sizeof(buffer), "SC_%04X", static_cast<unsigned int>(scan_code));
    } else {
        std::snprintf(buffer, sizeof(buffer), "SC_%02X", static_cast<unsigned int>(scan_code & 0x00FFu));
    }
    return buffer;
}

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

std::optional<uint32_t> parse_named_vk_token(std::string_view token) {
    if (token == "VK_OEM_1" || token == "OEM_1" || token == "OEM1") return static_cast<uint32_t>(VK_OEM_1);
    if (token == "VK_OEM_PLUS" || token == "OEM_PLUS" || token == "OEMPLUS") {
        return static_cast<uint32_t>(VK_OEM_PLUS);
    }
    if (token == "VK_OEM_COMMA" || token == "OEM_COMMA" || token == "OEMCOMMA") {
        return static_cast<uint32_t>(VK_OEM_COMMA);
    }
    if (token == "VK_OEM_MINUS" || token == "OEM_MINUS" || token == "OEMMINUS") {
        return static_cast<uint32_t>(VK_OEM_MINUS);
    }
    if (token == "VK_OEM_PERIOD" || token == "OEM_PERIOD" || token == "OEMPERIOD") {
        return static_cast<uint32_t>(VK_OEM_PERIOD);
    }
    if (token == "VK_OEM_2" || token == "OEM_2" || token == "OEM2") return static_cast<uint32_t>(VK_OEM_2);
    if (token == "VK_OEM_3" || token == "OEM_3" || token == "OEM3") return static_cast<uint32_t>(VK_OEM_3);
    if (token == "VK_OEM_4" || token == "OEM_4" || token == "OEM4") return static_cast<uint32_t>(VK_OEM_4);
    if (token == "VK_OEM_5" || token == "OEM_5" || token == "OEM5") return static_cast<uint32_t>(VK_OEM_5);
    if (token == "VK_OEM_6" || token == "OEM_6" || token == "OEM6") return static_cast<uint32_t>(VK_OEM_6);
    if (token == "VK_OEM_7" || token == "OEM_7" || token == "OEM7") return static_cast<uint32_t>(VK_OEM_7);
    return std::nullopt;
}

std::string fallback_vk_name(uint32_t keycode) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string name = "VK_00";
    name[3] = kHex[(keycode >> 4) & 0xF];
    name[4] = kHex[keycode & 0xF];
    return name;
}

std::optional<uint32_t> parse_scancode_token(std::string_view token) {
    if (token.size() < 5 || token[0] != 'S' || token[1] != 'C' || token[2] != '_') {
        return std::nullopt;
    }

    uint16_t scan_code = 0;
    for (std::size_t i = 3; i < token.size(); ++i) {
        const char ch = token[i];
        scan_code <<= 4;
        if (ch >= '0' && ch <= '9') {
            scan_code = static_cast<uint16_t>(scan_code | static_cast<uint16_t>(ch - '0'));
            continue;
        }
        if (ch >= 'A' && ch <= 'F') {
            scan_code = static_cast<uint16_t>(scan_code | static_cast<uint16_t>(10 + (ch - 'A')));
            continue;
        }
        return std::nullopt;
    }
    return scancode_keycode(scan_code);
}

uint16_t scan_code_from_vk(uint32_t vkey) {
    if (const auto known = known_scan_code_from_layout_sensitive_vk(vkey)) {
        return *known;
    }
    const HKL layout = GetKeyboardLayout(0);
    const UINT scan = MapVirtualKeyExW(vkey, MAPVK_VK_TO_VSC_EX, layout);
    if (scan == 0u) {
        return 0u;
    }
    if ((scan & 0xFF00u) == 0xE000u || (scan & 0xFF00u) == 0xE100u) {
        return static_cast<uint16_t>(scan);
    }
    return static_cast<uint16_t>(scan & 0x00FFu);
}

bool raw_vkey_needs_scan_recovery(uint32_t vkey) {
    return vkey == 0u || vkey == 0xFFu || vkey == static_cast<uint32_t>(VK_PROCESSKEY) ||
           vkey == static_cast<uint32_t>(VK_PACKET);
}

uint32_t recover_raw_vkey_from_scan_code(uint32_t vkey, uint16_t scan_code) {
    if (!raw_vkey_needs_scan_recovery(vkey) || scan_code == 0u) {
        return vkey;
    }

    const HKL layout = GetKeyboardLayout(0);
    const UINT mapped = MapVirtualKeyExW(scan_code, MAPVK_VSC_TO_VK_EX, layout);
    if (mapped != 0u) {
        return static_cast<uint32_t>(mapped);
    }

    return vkey;
}

uint32_t normalize_layout_sensitive_key(uint32_t fallback_vkey, uint16_t scan_code) {
    if (scan_code_from_name("SEMICOLON").value() == scan_code ||
        scan_code_from_name("PLUS").value() == scan_code ||
        scan_code_from_name("COMMA").value() == scan_code ||
        scan_code_from_name("MINUS").value() == scan_code ||
        scan_code_from_name("PERIOD").value() == scan_code ||
        scan_code_from_name("SLASH").value() == scan_code ||
        scan_code_from_name("GRAVE").value() == scan_code ||
        scan_code_from_name("LBRACKET").value() == scan_code ||
        scan_code_from_name("BACKSLASH").value() == scan_code ||
        scan_code_from_name("RBRACKET").value() == scan_code ||
        scan_code_from_name("APOSTROPHE").value() == scan_code) {
        return scancode_keycode(scan_code);
    }
    return fallback_vkey;
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
        switch (ch) {
            case ';': return scancode_keycode(0x0027u);
            case '=': return scancode_keycode(0x000Du);
            case ',': return scancode_keycode(0x0033u);
            case '-': return scancode_keycode(0x000Cu);
            case '.': return scancode_keycode(0x0034u);
            case '/': return scancode_keycode(0x0035u);
            case '`': return scancode_keycode(0x0029u);
            case '[': return scancode_keycode(0x001Au);
            case '\\': return scancode_keycode(0x002Bu);
            case ']': return scancode_keycode(0x001Bu);
            case '\'': return scancode_keycode(0x0028u);
            default: break;
        }
    }

    if (auto generic_vk = parse_vk_token(normalized)) {
        return generic_vk;
    }
    if (auto named_vk = parse_named_vk_token(normalized)) {
        return normalize_windows_polling_keycode(*named_vk);
    }
    if (auto generic_scan = parse_scancode_token(normalized)) {
        return generic_scan;
    }
    if (auto scan_code = scan_code_from_name(normalized)) {
        return scancode_keycode(*scan_code);
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
    if (is_scancode_keycode(keycode)) {
        return name_from_scan_code(decode_scancode_keycode(keycode));
    }

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

#ifdef _WIN32
uint32_t KeycodeMap::normalize_windows_raw_keycode(uint32_t vkey, uint16_t make_code, uint16_t flags) {
    const uint16_t scan_code = encode_scan_code(make_code, flags);
    const uint32_t resolved_vkey = recover_raw_vkey_from_scan_code(vkey, scan_code);
    return normalize_layout_sensitive_key(resolved_vkey, scan_code);
}

uint32_t KeycodeMap::normalize_windows_polling_keycode(uint32_t vkey) {
    return normalize_layout_sensitive_key(vkey, scan_code_from_vk(vkey));
}

std::optional<uint32_t> KeycodeMap::polling_vk_for_keycode(uint32_t keycode) {
    if (!is_scancode_keycode(keycode)) {
        if (keycode <= 0xFFu) {
            return keycode;
        }
        return std::nullopt;
    }

    const uint16_t scan_code = decode_scancode_keycode(keycode);
    if (const auto known = layout_sensitive_vk_from_scan_code(scan_code)) {
        return *known;
    }
    const HKL layout = GetKeyboardLayout(0);
    const UINT vkey = MapVirtualKeyExW(scan_code, MAPVK_VSC_TO_VK_EX, layout);
    if (vkey != 0u) {
        return static_cast<uint32_t>(vkey);
    }
    return std::nullopt;
}
#endif

}  // namespace tenriff::config
