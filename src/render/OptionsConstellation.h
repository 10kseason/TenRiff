#pragma once
#include <array>

namespace tenriff::render {
inline constexpr float kOptionsGalaxySaturation = .92f;
inline constexpr float kOptionsGalaxyBrightness = .98f;
// Row-vector color transform: preserve alpha, reduce chroma around Rec.709
// luminance, then apply the requested linear brightness multiplier to RGB.
inline constexpr std::array<float, 20> options_galaxy_color_matrix() {
    constexpr float s = kOptionsGalaxySaturation, v = kOptionsGalaxyBrightness;
    constexpr float r = .2126f * (1 - s), g = .7152f * (1 - s), b = .0722f * (1 - s);
    return {(r+s)*v,r*v,r*v,0, g*v,(g+s)*v,g*v,0, b*v,b*v,(b+s)*v,0, 0,0,0,1, 0,0,0,0};
}
struct OptionsStar { float x, y; };
// Eight quiet, distinct authored motifs. They are decoration, not astronomy.
inline constexpr std::array<std::array<OptionsStar, 8>, 8> kOptionsConstellations{{
    {{{.60f,.17f},{.70f,.38f},{.81f,.57f},{.88f,.63f},{.66f,.47f},{.56f,.48f},{.84f,.75f},{.81f,.90f}}},
    {{{.56f,.16f},{.48f,.31f},{.64f,.57f},{.76f,.63f},{.66f,.32f},{.73f,.76f},{.88f,.79f},{.71f,.89f}}},
    {{{.55f,.25f},{.43f,.44f},{.70f,.50f},{.84f,.47f},{.48f,.65f},{.59f,.86f},{.80f,.89f},{.91f,.35f}}},
    {{{.66f,.16f},{.73f,.38f},{.62f,.60f},{.46f,.88f},{.82f,.74f},{.91f,.61f},{.84f,.40f},{.74f,.56f}}},
    {{{.70f,.12f},{.63f,.39f},{.51f,.56f},{.49f,.85f},{.79f,.53f},{.86f,.68f},{.69f,.73f},{.77f,.15f}}},
    {{{.59f,.11f},{.73f,.26f},{.68f,.49f},{.48f,.77f},{.82f,.64f},{.92f,.84f},{.64f,.86f},{.39f,.91f}}},
    {{{.70f,.14f},{.50f,.30f},{.49f,.50f},{.66f,.67f},{.83f,.52f},{.89f,.75f},{.73f,.87f},{.54f,.90f}}},
    {{{.81f,.12f},{.70f,.24f},{.59f,.29f},{.43f,.24f},{.70f,.48f},{.80f,.66f},{.87f,.88f},{.63f,.72f}}}
}};
inline constexpr std::array<std::array<unsigned, 2>, 8> kOptionsStarLinks{{
    {{0,1}},{{1,2}},{{2,3}},{{2,4}},{{4,5}},{{5,6}},{{3,6}},{{6,7}}
}};
}
