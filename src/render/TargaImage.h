#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tenriff::render {

// Windows ships no WIC codec for Targa, and classic LR2 themes are authored
// entirely in .tga, so imported skins need their own decoder to show anything at
// all. Only the two encodings LR2 skins actually use are handled: uncompressed and
// run-length encoded true colour at 24 or 32 bits per pixel.
struct TargaImage {
    int width = 0;
    int height = 0;
    // Premultiplied BGRA, top-down, tightly packed at width * 4 bytes per row.
    std::vector<std::uint8_t> pixels;
    bool valid = false;
};

namespace targa_detail {

constexpr std::size_t kHeaderSize = 18u;

inline std::uint16_t read_u16_le(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[0]) |
                                      (static_cast<std::uint16_t>(data[1]) << 8u));
}

// Straight alpha to premultiplied, matching GUID_WICPixelFormat32bppPBGRA.
inline void write_pixel(std::uint8_t* out, std::uint8_t b, std::uint8_t g, std::uint8_t r, std::uint8_t a) {
    if (a == 255u) {
        out[0] = b;
        out[1] = g;
        out[2] = r;
        out[3] = a;
        return;
    }
    const std::uint32_t alpha = a;
    out[0] = static_cast<std::uint8_t>((b * alpha + 127u) / 255u);
    out[1] = static_cast<std::uint8_t>((g * alpha + 127u) / 255u);
    out[2] = static_cast<std::uint8_t>((r * alpha + 127u) / 255u);
    out[3] = a;
}

}  // namespace targa_detail

[[nodiscard]] inline TargaImage decode_targa(const std::uint8_t* data, std::size_t size) {
    using namespace targa_detail;

    TargaImage image;
    if (data == nullptr || size < kHeaderSize) {
        return image;
    }

    const std::uint8_t id_length = data[0];
    const std::uint8_t color_map_type = data[1];
    const std::uint8_t image_type = data[2];
    const std::uint16_t color_map_length = read_u16_le(data + 5);
    const std::uint8_t color_map_entry_bits = data[7];
    const int width = read_u16_le(data + 12);
    const int height = read_u16_le(data + 14);
    const std::uint8_t pixel_depth = data[16];
    const std::uint8_t descriptor = data[17];

    const bool run_length_encoded = image_type == 10u;
    if ((image_type != 2u && !run_length_encoded) || color_map_type != 0u) {
        return image;
    }
    if ((pixel_depth != 24u && pixel_depth != 32u) || width <= 0 || height <= 0) {
        return image;
    }

    const std::size_t bytes_per_pixel = pixel_depth / 8u;
    const std::size_t color_map_bytes =
        static_cast<std::size_t>(color_map_length) * ((color_map_entry_bits + 7u) / 8u);
    std::size_t cursor = kHeaderSize + id_length + color_map_bytes;
    if (cursor > size) {
        return image;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> pixels(pixel_count * 4u, 0u);

    // Bit 5 clear is the Targa default: rows are stored bottom-up.
    const bool top_down = (descriptor & 0x20u) != 0u;
    const bool right_to_left = (descriptor & 0x10u) != 0u;
    auto destination_for = [&](std::size_t index) -> std::uint8_t* {
        const std::size_t row = index / static_cast<std::size_t>(width);
        const std::size_t column = index % static_cast<std::size_t>(width);
        const std::size_t out_row =
            top_down ? row : (static_cast<std::size_t>(height) - 1u - row);
        const std::size_t out_column =
            right_to_left ? (static_cast<std::size_t>(width) - 1u - column) : column;
        return pixels.data() + (out_row * static_cast<std::size_t>(width) + out_column) * 4u;
    };

    std::size_t written = 0;
    while (written < pixel_count) {
        if (!run_length_encoded) {
            if (cursor + bytes_per_pixel > size) {
                return image;
            }
            const std::uint8_t alpha = (bytes_per_pixel == 4u) ? data[cursor + 3u] : 255u;
            write_pixel(destination_for(written), data[cursor], data[cursor + 1u], data[cursor + 2u], alpha);
            cursor += bytes_per_pixel;
            ++written;
            continue;
        }

        if (cursor >= size) {
            return image;
        }
        const std::uint8_t packet = data[cursor++];
        const std::size_t run = static_cast<std::size_t>(packet & 0x7Fu) + 1u;
        if (written + run > pixel_count) {
            return image;
        }
        if ((packet & 0x80u) != 0u) {
            if (cursor + bytes_per_pixel > size) {
                return image;
            }
            const std::uint8_t alpha = (bytes_per_pixel == 4u) ? data[cursor + 3u] : 255u;
            for (std::size_t i = 0; i < run; ++i) {
                write_pixel(destination_for(written + i), data[cursor], data[cursor + 1u], data[cursor + 2u], alpha);
            }
            cursor += bytes_per_pixel;
        } else {
            if (cursor + bytes_per_pixel * run > size) {
                return image;
            }
            for (std::size_t i = 0; i < run; ++i) {
                const std::uint8_t* pixel = data + cursor + bytes_per_pixel * i;
                const std::uint8_t alpha = (bytes_per_pixel == 4u) ? pixel[3] : 255u;
                write_pixel(destination_for(written + i), pixel[0], pixel[1], pixel[2], alpha);
            }
            cursor += bytes_per_pixel * run;
        }
        written += run;
    }

    image.width = width;
    image.height = height;
    image.pixels = std::move(pixels);
    image.valid = true;
    return image;
}

}  // namespace tenriff::render
