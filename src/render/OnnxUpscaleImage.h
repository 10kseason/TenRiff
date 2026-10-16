#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace tenriff::render::onnx_image {

// Fixed models retain their original contract. Dynamic spatial models use the
// full source directly; neither path resizes or crops pixels before inference.
inline constexpr std::uint32_t kInputWidth = 960;
inline constexpr std::uint32_t kInputHeight = 540;
inline constexpr std::uint32_t kContext = 32;
// Bound CPU storage and use a conservative 8192-pixel output bound for D2D.
inline constexpr std::uint32_t kMaxSourceDimension = 4096;
inline constexpr std::uint32_t kMaxOutputDimension = 8192;

enum class OutputMode { Residual, Image };

inline OutputMode output_mode(std::string_view requested, std::string_view input_name,
                              std::string_view output_name, std::string_view metadata = {}) {
    if (requested == "residual") return OutputMode::Residual;
    if (requested == "image") return OutputMode::Image;
    if (!requested.empty() && requested != "auto")
        throw std::runtime_error("Unknown ONNX output mode");
    if (metadata == "residual") return OutputMode::Residual;
    if (metadata == "image") return OutputMode::Image;
    if (!metadata.empty()) throw std::runtime_error("Unknown tenriff.output_mode metadata");
    // Tensor dimensions/values cannot identify residual semantics. Preserve the
    // legacy named contract; other models default to completed RGB images.
    return input_name == "rgb_lr" && output_name == "rgb_residual_x2"
        ? OutputMode::Residual : OutputMode::Image;
}

inline bool valid_size(std::uint32_t width, std::uint32_t height) {
    return width > 0 && height > 0 && width <= kMaxSourceDimension &&
           height <= kMaxSourceDimension;
}

enum class ModelShape { Unsupported, Fixed, Dynamic };

inline std::uint32_t output_scale(const std::vector<std::int64_t>& shape,
                                  std::uint32_t input_width, std::uint32_t input_height) {
    if (!valid_size(input_width, input_height) || shape.size() != 4 ||
        shape[0] != 1 || shape[1] != 3 || shape[2] <= 0 || shape[3] <= 0 ||
        shape[2] > kMaxOutputDimension || shape[3] > kMaxOutputDimension ||
        shape[2] % input_height || shape[3] % input_width) return 0;
    const auto scale = shape[2] / input_height;
    return scale >= 1 && scale <= 4 && shape[3] / input_width == scale
        ? static_cast<std::uint32_t>(scale) : 0;
}

inline ModelShape model_shape(const std::vector<std::int64_t>& input,
                              const std::vector<std::int64_t>& output) {
    if (input.size() != 4 || output.size() != 4 || (input[0] != 1 && input[0] != -1) ||
        input[1] != 3 || (output[0] != 1 && output[0] != -1) || output[1] != 3)
        return ModelShape::Unsupported;
    const bool dynamic_output = output[2] == -1 && output[3] == -1;
    const bool fixed_output = output[2] > 0 && output[3] > 0 &&
        output[2] <= kMaxOutputDimension && output[3] <= kMaxOutputDimension;
    if (!dynamic_output && !fixed_output) return ModelShape::Unsupported;
    if (input[2] > 0 && input[3] > 0 && input[2] <= kMaxSourceDimension &&
        input[3] <= kMaxSourceDimension) {
        auto actual = output;
        actual[0] = 1;
        return dynamic_output || output_scale(actual, static_cast<std::uint32_t>(input[3]),
            static_cast<std::uint32_t>(input[2])) ? ModelShape::Fixed : ModelShape::Unsupported;
    }
    // WinML exposes symbolic/unknown axes as -1. Both spatial axes must be
    // dynamic at the input; actual scale is checked on every evaluated tensor.
    if (input[2] == -1 && input[3] == -1)
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

inline std::vector<Tile> plan_tiles(std::uint32_t width, std::uint32_t height,
                                    std::uint32_t input_width = kInputWidth,
                                    std::uint32_t input_height = kInputHeight) {
    if (!valid_size(width, height) || !valid_size(input_width, input_height)) return {};
    const bool split_x = width > input_width;
    const bool split_y = height > input_height;
    const auto context_x = std::min(kContext, input_width / 4);
    const auto context_y = std::min(kContext, input_height / 4);
    const auto step_x = split_x ? input_width - 2 * context_x : width;
    const auto step_y = split_y ? input_height - 2 * context_y : height;
    std::vector<Tile> tiles;
    for (std::uint32_t y = 0; y < height; y += step_y) {
        for (std::uint32_t x = 0; x < width; x += step_x) {
            tiles.push_back({x, y, std::min(step_x, width - x), std::min(step_y, height - y),
                static_cast<std::int32_t>(x) - (split_x ? static_cast<std::int32_t>(context_x) : 0),
                static_cast<std::int32_t>(y) - (split_y ? static_cast<std::int32_t>(context_y) : 0)});
        }
    }
    return tiles;
}

inline std::vector<std::uint8_t> extract_tile(const std::vector<std::uint8_t>& source,
                                           std::uint32_t width, std::uint32_t height,
                                           const Tile& tile,
                                           std::uint32_t input_width = kInputWidth,
                                           std::uint32_t input_height = kInputHeight) {
    if (!valid_size(width, height) || !valid_size(input_width, input_height) ||
        source.size() != std::size_t(width) * height * 4) return {};
    std::vector<std::uint8_t> input(std::size_t(input_width) * input_height * 4);
    for (std::uint32_t y = 0; y < input_height; ++y) {
        const auto sy = std::clamp(tile.input_y + static_cast<std::int32_t>(y), 0,
                                   static_cast<std::int32_t>(height) - 1);
        for (std::uint32_t x = 0; x < input_width; ++x) {
            const auto sx = std::clamp(tile.input_x + static_cast<std::int32_t>(x), 0,
                                       static_cast<std::int32_t>(width) - 1);
            const auto from = (std::size_t(sy) * width + sx) * 4;
            const auto to = (std::size_t(y) * input_width + x) * 4;
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
                         std::uint32_t input_height = kInputHeight,
                         std::uint32_t scale = 2, OutputMode mode = OutputMode::Residual) {
    const std::size_t residual_pixels = std::size_t(input_width) * input_height * scale * scale;
    if (!valid_size(width, height) || !valid_size(input_width, input_height) ||
        scale < 1 || scale > 4 || width * scale > kMaxOutputDimension ||
        height * scale > kMaxOutputDimension ||
        source.size() != std::size_t(width) * height * 4 ||
        output.size() != std::size_t(width * scale) * (height * scale) * 4 ||
        residual.size() != residual_pixels * 3 || tile.x + tile.width > width ||
        tile.y + tile.height > height) return false;
    const auto rx = static_cast<std::int32_t>(tile.x) - tile.input_x;
    const auto ry = static_cast<std::int32_t>(tile.y) - tile.input_y;
    if (rx < 0 || ry < 0 || rx + tile.width > input_width || ry + tile.height > input_height) return false;
    // Interpolate the full original image so base pixels agree across tile
    // boundaries. Completed-image models must never receive the RGB base again.
    for (std::uint32_t dy = 0; dy < tile.height * scale; ++dy) {
        const auto y = tile.y * scale + dy;
        const float sy = (y + 0.5f) / scale - 0.5f;
        const int y0 = static_cast<int>(std::floor(sy));
        const float fy = sy - std::floor(sy);
        for (std::uint32_t dx = 0; dx < tile.width * scale; ++dx) {
            const auto x = tile.x * scale + dx;
            const float sx = (x + 0.5f) / scale - 0.5f;
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
            const auto out = (std::size_t(y) * (width * scale) + x) * 4;
            const auto pixel = std::size_t(ry * scale + dy) * (input_width * scale) + rx * scale + dx;
            const float alpha = interpolate(3) / 255.0f;
            for (int channel = 0; channel < 3; ++channel) {
                const float detail = residual[(2 - channel) * residual_pixels + pixel];
                if (!std::isfinite(detail)) return false;
                const float base = mode == OutputMode::Residual ? interpolate(channel) / 255.0f : 0.0f;
                const float corrected = std::clamp(base + detail, 0.0f, 1.0f);
                output[out + channel] = static_cast<std::uint8_t>(std::lround(corrected * alpha * 255.0f));
            }
            output[out + 3] = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
        }
    }
    return true;
}

} // namespace tenriff::render::onnx_image
