#pragma once

#include <cstdint>

namespace tenriff::render {

// Keep existing palette slot IDs usable by imported skins. Only the built-in
// fallback changes; authored colors are applied after this lookup. The editor
// reads this table too, so its defaults describe the colors actually rendered.
inline constexpr std::uint32_t native_menu_default_rgb(std::uint32_t rgb) {
    switch (rgb) {
        case 0x05090F: return 0x000000;
        case 0x101D32: return 0x000000;
        case 0x101421: return 0x000000;
        case 0x211C35: return 0x000000;
        case 0x020914: return 0x080808;
        case 0x05070A: return 0x080808;
        case 0x0B1620: return 0x111111;
        case 0x233344: return 0x303030;
        case 0x234652: return 0x3A3A3A;
        case 0x63E9F2: return 0xE5E5E5;
        case 0xA499FF: return 0xE5E5E5;
        case 0xA9F5FF: return 0xF2F2F2;
        case 0xC5F7FF: return 0xF2F2F2;
        case 0x89AAE0: return 0xB5B5B5;
        case 0xBBD9FF: return 0xDDDDDD;
        case 0x54E9FF: return 0xE5E5E5;
        case 0xA56BFF: return 0xEAEAEA;
        case 0x68EDF3: return 0xE5E5E5;
        case 0x827A98: return 0x999999;
        case 0x63E9FF: return 0xE5E5E5;
        case 0x73DDF5: return 0xD3D3D3;
        case 0xF4F1FF: return 0xF7F7F7;
        case 0xA4E8F3: return 0xD3D3D3;
        case 0xE0FAFF: return 0xF3F3F3;
        case 0xA1E5F0: return 0xDDDDDD;
        case 0x7B91AC: return 0xA0A0A0;
        case 0x96D5E4: return 0xCCCCCC;
        case 0x8FB8FF: return 0xDADADA;
        case 0xC2A0FF: return 0xCCCCCC;
        case 0x9EA7F5: return 0xD0D0D0;
        case 0xE4B4EF: return 0xD4D4D4;
        case 0xB7A7FF: return 0xF2F2F2;
        case 0x79E9F5: return 0xF2F2F2;
        case 0x65ECFF: return 0xE5E5E5;
        case 0xCFB0FF: return 0xFFFFFF;
        case 0xA86CFF: return 0xE5E5E5;
        case 0x75698E: return 0x999999;
        case 0xB57CFF: return 0xDDDDDD;
        case 0x9E65FF: return 0xE5E5E5;
        case 0xB379FF: return 0xDDDDDD;
        case 0x9D62F2: return 0xE5E5E5;
        case 0xECF6FF: return 0xF2F2F2;
        case 0x03080F: return 0x0F0F0F;
        case 0x061118: return 0x181818;
        case 0x0B0B10: return 0x101010;
        case 0x101820: return 0x202020;
        case 0x14141C: return 0x1C1C1C;
        case 0x141D28: return 0x282828;
        case 0x142333: return 0x333333;
        case 0x19DDE8: return 0xE8E8E8;
        case 0x1F2130: return 0x303030;
        case 0x242638: return 0x383838;
        case 0x2DD4BF: return 0xD4D4D4;
        case 0x31344A: return 0x4A4A4A;
        case 0x34D399: return 0xD3D3D3;
        case 0x4F80FF: return 0xFFFFFF;
        case 0x5CEBFF: return 0xFFFFFF;
        case 0x5DA9FF: return 0xFFFFFF;
        case 0x5EEAD4: return 0xEAEAEA;
        case 0x60A5FA: return 0xFAFAFA;
        case 0x6BE9FF: return 0xFFFFFF;
        case 0x6EE7F2: return 0xF2F2F2;
        case 0x70ECFF: return 0xFFFFFF;
        case 0x76E8FF: return 0xFFFFFF;
        case 0x79E5EF: return 0xEFEFEF;
        case 0x7BD9EA: return 0xEAEAEA;
        case 0x7EE2A8: return 0xE2E2E2;
        case 0x7F8C9B: return 0x9B9B9B;
        case 0x81D5CF: return 0xD5D5D5;
        case 0x83F5FF: return 0xFFFFFF;
        case 0x86EFAC: return 0xEFEFEF;
        case 0x89A9C5: return 0xC5C5C5;
        case 0x89D185: return 0xD1D1D1;
        case 0x93C5FD: return 0xFDFDFD;
        case 0x94A3B8: return 0xB8B8B8;
        case 0x99CDAE: return 0xCDCDCD;
        case 0x9AF2FF: return 0xFFFFFF;
        case 0x9BCEDC: return 0xDCDCDC;
        case 0x9EDCFF: return 0xFFFFFF;
        case 0xA5B4FC: return 0xFCFCFC;
        case 0xA5E9BA: return 0xE9E9E9;
        case 0xA78BFA: return 0xFAFAFA;
        case 0xA8EA58: return 0xEAEAEA;
        case 0xA9DB72: return 0xDBDBDB;
        case 0xAAB7C4: return 0xC4C4C4;
        case 0xB7C4D4: return 0xD4D4D4;
        case 0xB8A6E6: return 0xE6E6E6;
        case 0xB9FFF0: return 0xFFFFFF;
        case 0xC084FC: return 0xFCFCFC;
        case 0xC2CCD3: return 0xD3D3D3;
        case 0xC5ACFF: return 0xFFFFFF;
        case 0xCFACCA: return 0xCFCFCF;
        case 0xCFBD8B: return 0xCFCFCF;
        case 0xD5FFF4: return 0xFFFFFF;
        case 0xDAC89A: return 0xDADADA;
        case 0xE8ECF1: return 0xF1F1F1;
        case 0xEAF3FD: return 0xFDFDFD;
        case 0xF1FAFF: return 0xFFFFFF;
        case 0xF2B84B: return 0xF2F2F2;
        case 0xF3F6FA: return 0xFAFAFA;
        case 0xF472B6: return 0xF4F4F4;
        case 0xF59E0B: return 0xF5F5F5;
        case 0xF6F8FF: return 0xFFFFFF;
        case 0xF7FAFD: return 0xFDFDFD;
        case 0xFB7185: return 0xFBFBFB;
        case 0xFBBF24: return 0xFBFBFB;
        case 0xFDA4AF: return 0xFDFDFD;
        case 0xFDBA74: return 0xFDFDFD;
        case 0xFF4D6D: return 0xFFFFFF;
        case 0xFF5A6B: return 0xFFFFFF;
        case 0xFF6B7D: return 0xFFFFFF;
        case 0xFF7F8C: return 0xFFFFFF;
        case 0xFF9AAB: return 0xFFFFFF;
        case 0xFF9BC5: return 0xFFFFFF;
        case 0xFFAB75: return 0xFFFFFF;
        case 0xFFC68A: return 0xFFFFFF;
        case 0xFFD479: return 0xFFFFFF;
        case 0xFFF29A: return 0xFFFFFF;
        default: return rgb;
    }
}

}  // namespace tenriff::render
