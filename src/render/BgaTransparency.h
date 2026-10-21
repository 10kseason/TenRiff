#pragma once

#include <cstdint>
#include <vector>

namespace tenriff::render {

// BMS channel 07 keys exact RGB black. Base images retain authored black;
// near-black and existing PNG alpha must not be mistaken for a color key.
inline void apply_bga_black_key(std::vector<std::uint8_t>& bgra) {
    for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) {
        if (bgra[i] == 0 && bgra[i + 1] == 0 && bgra[i + 2] == 0)
            bgra[i + 3] = 0;
    }
}

// RGB restoration must not invent opacity in a keyed layer. Keep its authored
// mask at the model's integer scale, then premultiply for Direct2D composition.
inline void restore_bga_alpha(std::vector<std::uint8_t>& output,
                              const std::vector<std::uint8_t>& alpha,
                              std::uint32_t width, std::uint32_t height,
                              std::uint32_t scale) {
    if (!scale || alpha.size() != std::size_t(width) * height ||
        output.size() != std::size_t(width) * height * scale * scale * 4) return;
    const auto output_width = width * scale;
    for (std::uint32_t y = 0; y < height * scale; ++y) {
        for (std::uint32_t x = 0; x < output_width; ++x) {
            const auto a = alpha[std::size_t(y / scale) * width + x / scale];
            const auto offset = (std::size_t(y) * output_width + x) * 4;
            for (int c = 0; c < 3; ++c)
                output[offset + c] = static_cast<std::uint8_t>((output[offset + c] * a + 127) / 255);
            output[offset + 3] = a;
        }
    }
}

}  // namespace tenriff::render
