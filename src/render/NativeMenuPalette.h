#pragma once

#include <cstdint>

namespace tenriff::render {

// Keep existing palette slot IDs usable by imported skins. Only the built-in
// fallback changes; authored colors are applied after this lookup. The editor
// reads this table too, so its defaults describe the colors actually rendered.
inline constexpr std::uint32_t native_menu_default_rgb(std::uint32_t rgb) {
    switch (rgb) {
        case 0x05090F: return 0x080808;
        case 0x101D32: return 0x101010;
        case 0x101421: return 0x161616;
        case 0x211C35: return 0x202020;
        case 0x020914: return 0x080808;
        case 0x05070A: return 0x080808;
        case 0x0B1620: return 0x111111;
        case 0x233344: return 0x303030;
        case 0x234652: return 0x3A3A3A;
        case 0x63E9F2: return 0xB8E5F5;
        case 0xA499FF: return 0xD6EDF7;
        case 0xA9F5FF: return 0xD8F1FA;
        case 0xC5F7FF: return 0xE5F5FC;
        case 0x89AAE0: return 0xB7C6CC;
        case 0xBBD9FF: return 0xCDEAF7;
        case 0x54E9FF: return 0xB8E5F5;
        case 0xA56BFF: return 0xE1F2F9;
        case 0x68EDF3: return 0xB8E5F5;
        case 0x827A98: return 0x999999;
        case 0x63E9FF: return 0xB8E5F5;
        case 0x73DDF5: return 0xC5E9F7;
        case 0xF4F1FF: return 0xF7F7F7;
        case 0xA4E8F3: return 0xC8EAF7;
        case 0xE0FAFF: return 0xEDF8FC;
        case 0xA1E5F0: return 0xD0ECF7;
        case 0x7B91AC: return 0xA0A0A0;
        case 0x96D5E4: return 0xC4E8F5;
        case 0x8FB8FF: return 0xAEDBEC;
        case 0xC2A0FF: return 0xCCEAF7;
        case 0x9EA7F5: return 0xBDDAE6;
        case 0xE4B4EF: return 0xE1EFF5;
        case 0xB7A7FF: return 0xECF6FF;
        case 0x79E9F5: return 0xE5F5FC;
        case 0x65ECFF: return 0xB8E5F5;
        case 0xCFB0FF: return 0xFFFFFF;
        case 0xA86CFF: return 0xD6EDF7;
        case 0x75698E: return 0x999999;
        case 0xB57CFF: return 0xCDEAF7;
        case 0x9E65FF: return 0xD6EDF7;
        case 0xB379FF: return 0xCDEAF7;
        case 0x9D62F2: return 0xD6EDF7;
        default: return rgb;
    }
}

}  // namespace tenriff::render
