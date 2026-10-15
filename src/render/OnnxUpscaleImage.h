#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tenriff::render::onnx_image {

// Fixed models retain their original contract. Dynamic spatial models use the
// full source directly; neither path resizes or crops pixels before inference.
inline constexpr std::uint32_t kInputWidth = 960;
inline constexpr std::uint32_t kInputHeight = 540;
inline constexpr std::uint32_t kContext = 32;
// Bound CPU storage and use a conservative 8192-pixel output bound for D2D.
inline constexpr std::uint32_t kMaxSourceDimension = 4096;

inline bool valid_size(std::uint32_t width, std::uint32_t height) {
    return width > 0 && height > 0 && width <= kMaxSourceDimension &&
           height <= kMaxSourceDimension;
}

enum class ModelShape { Unsupported, Fixed, Dynamic };

inline ModelShape model_shape(const std::vector<std::int64_t>& input,
                              const std::vector<std::int64_t>& output) {
    if (input.size() != 4 || output.size() != 4 || input[0] != 1 || input[1] != 3 ||
        output[0] != 1 || output[1] != 3) return ModelShape::Unsupported;
    if (input[2] == kInputHeight && input[3] == kInputWidth &&
        output[2] == kInputHeight * 2 && output[3] == kInputWidth * 2)
        return ModelShape::Fixed;
    // WinML exposes symbolic/unknown axes as -1. Both spatial axes must be
    // dynamic at both boundaries; x2 is checked again on the evaluated tensor.
    if (input[2] == -1 && input[3] == -1 && output[2] == -1 && output[3] == -1)
        return ModelShape::Dynamic;
    return ModelShape::Unsupported;
}

inline bool valid_residual_shape(const std::vector<std::int64_t>& shape,
                                 std::uint32_t input_width, std::uint32_t input_height) {
    return valid_size(input_width, input_height) && shape.size() == 4 &&
           shape[0] == 1 && shape[1] == 3 && shape[2] == std::int64_t(input_height) * 2 &&
           shape[3] == std::int64_t(input_width) * 2;
}

struct Tile {
    std::uint32_t x, y, width, height; // Owned rectangle in original coordinates.
    std::int32_t input_x, input_y;     // Model input origin; edges are replicated.
};

inline std::vector<Tile> plan_tiles(std::uint32_t width, std::uint32_t height) {
    if (!valid_size(width, height)) return {};
    const bool split_x = width > kInputWidth;
    const bool split_y = height > kInputHeight;
    const auto step_x = split_x ? kInputWidth - 2 * kContext : width;
    const auto step_y = split_y ? kInputHeight - 2 * kContext : height;
    std::vector<Tile> tiles;
    for (std::uint32_t y = 0; y < height; y += step_y) {
        for (std::uint32_t x = 0; x < width; x += step_x) {
            tiles.push_back({x, y, std::min(step_x, width - x), std::min(step_y, height - y),
                static_cast<std::int32_t>(x) - (split_x ? static_cast<std::int32_t>(kContext) : 0),
                static_cast<std::int32_t>(y) - (split_y ? static_cast<std::int32_t>(kContext) : 0)});
        }
    }
    return tiles;
}

inline std::vector<std::uint8_t> extract_tile(const std::vector<std::uint8_t>& source,
                                           std::uint32_t width, std::uint32_t height,
                                           const Tile& tile) {
    if (!valid_size(width, height) || source.size() != std::size_t(width) * height * 4) return {};
    std::vector<std::uint8_t> input(std::size_t(kInputWidth) * kInputHeight * 4);
    for (std::uint32_t y = 0; y < kInputHeight; ++y) {
        const auto sy = std::clamp(tile.input_y + static_cast<std::int32_t>(y), 0,
                                   static_cast<std::int32_t>(height) - 1);
        for (std::uint32_t x = 0; x < kInputWidth; ++x) {
            const auto sx = std::clamp(tile.input_x + static_cast<std::int32_t>(x), 0,
                                       static_cast<std::int32_t>(width) - 1);
            const auto from = (std::size_t(sy) * width + sx) * 4;
            const auto to = (std::size_t(y) * kInputWidth + x) * 4;
            std::copy_n(source.data() + from, 4, input.data() + to);
        }
    }
    return input;
}

inline bool compose_tile(const std::vector<std::uint8_t>& source,
                         std::uint32_t width, std::uint32_t height, const Tile& tile,
                         const std::vector<float>& residual,
                         std::vector<std::uint8_t>& output,
                         std::uint32_t input_width = kInputWidth,
                         std::uint32_t input_height = kInputHeight) {
    const std::size_t residual_pixels = std::size_t(input_width) * input_height * 4;
    if (!valid_size(width, height) || !valid_size(input_width, input_height) ||
        source.size() != std::size_t(width) * height * 4 ||
        output.size() != std::size_t(width * 2) * (height * 2) * 4 ||
        residual.size() != residual_pixels * 3 || tile.x + tile.width > width ||
        tile.y + tile.height > height) return false;
    const auto rx = static_cast<std::int32_t>(tile.x) - tile.input_x;
    const auto ry = static_cast<std::int32_t>(tile.y) - tile.input_y;
    if (rx < 0 || ry < 0 || rx + tile.width > input_width || ry + tile.height > input_height) return false;
    // Interpolate the full original image so base pixels agree across tile
    // boundaries. Only the AI residual comes from each tile's owned interior.
    for (std::uint32_t dy = 0; dy < tile.height * 2; ++dy) {
        const auto y = tile.y * 2 + dy;
        const float sy = (y + 0.5f) * 0.5f - 0.5f;
        const int y0 = static_cast<int>(std::floor(sy));
        const float fy = sy - std::floor(sy);
        for (std::uint32_t dx = 0; dx < tile.width * 2; ++dx) {
            const auto x = tile.x * 2 + dx;
            const float sx = (x + 0.5f) * 0.5f - 0.5f;
            const int x0 = static_cast<int>(std::floor(sx));
            const float fx = sx - std::floor(sx);
            const auto sample = [&](int xx, int yy, int channel) {
                return float(source[(std::size_t(std::clamp(yy, 0, int(height) - 1)) * width +
                                     std::clamp(xx, 0, int(width) - 1)) * 4 + channel]);
            };
            const auto interpolate = [&](int channel) {
                const float top = sample(x0, y0, channel) * (1 - fx) + sample(x0 + 1, y0, channel) * fx;
                const float bottom = sample(x0, y0 + 1, channel) * (1 - fx) + sample(x0 + 1, y0 + 1, channel) * fx;
                return top * (1 - fy) + bottom * fy;
            };
            const auto out = (std::size_t(y) * (width * 2) + x) * 4;
            const auto pixel = std::size_t(ry * 2 + dy) * (input_width * 2) + rx * 2 + dx;
            const float alpha = interpolate(3) / 255.0f;
            for (int channel = 0; channel < 3; ++channel) {
                const float detail = residual[(2 - channel) * residual_pixels + pixel];
                if (!std::isfinite(detail)) return false;
                const float corrected = std::clamp(interpolate(channel) / 255.0f + detail, 0.0f, 1.0f);
                output[out + channel] = static_cast<std::uint8_t>(std::lround(corrected * alpha * 255.0f));
            }
            output[out + 3] = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
        }
    }
    return true;
}

} // namespace tenriff::render::onnx_image
