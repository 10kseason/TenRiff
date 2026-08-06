#include "doctest/doctest.h"

#include <cstdint>
#include <vector>

#include "render/TargaImage.h"

namespace {

std::vector<std::uint8_t> make_header(std::uint8_t image_type,
                                      int width,
                                      int height,
                                      std::uint8_t depth,
                                      std::uint8_t descriptor) {
    std::vector<std::uint8_t> header(18u, 0u);
    header[2] = image_type;
    header[12] = static_cast<std::uint8_t>(width & 0xFF);
    header[13] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
    header[14] = static_cast<std::uint8_t>(height & 0xFF);
    header[15] = static_cast<std::uint8_t>((height >> 8) & 0xFF);
    header[16] = depth;
    header[17] = descriptor;
    return header;
}

const std::uint8_t* pixel_at(const tenriff::render::TargaImage& image, int x, int y) {
    return image.pixels.data() + (static_cast<std::size_t>(y) * image.width + x) * 4u;
}

}  // namespace

TEST_CASE("targa decoder flips bottom-up uncompressed frames and keeps channel order") {
    // Two rows of two pixels, stored bottom-up: the first row in the file is the
    // bottom row of the picture.
    auto bytes = make_header(2u, 2, 2, 32u, 0x08u);
    const std::uint8_t body[] = {
        1, 2, 3, 255,      4, 5, 6, 255,     // bottom row: BGRA
        7, 8, 9, 255,      10, 11, 12, 255,  // top row
    };
    bytes.insert(bytes.end(), std::begin(body), std::end(body));

    const auto image = tenriff::render::decode_targa(bytes.data(), bytes.size());
    REQUIRE(image.valid);
    CHECK(image.width == 2);
    CHECK(image.height == 2);
    REQUIRE(image.pixels.size() == 16u);

    const std::uint8_t* top_left = pixel_at(image, 0, 0);
    CHECK(top_left[0] == 7);
    CHECK(top_left[1] == 8);
    CHECK(top_left[2] == 9);
    CHECK(top_left[3] == 255);

    const std::uint8_t* bottom_right = pixel_at(image, 1, 1);
    CHECK(bottom_right[0] == 4);
    CHECK(bottom_right[1] == 5);
    CHECK(bottom_right[2] == 6);
}

TEST_CASE("targa decoder expands run-length packets and premultiplies alpha") {
    auto bytes = make_header(10u, 4, 1, 32u, 0x28u);  // top-down, single row
    const std::uint8_t body[] = {
        0x81, 10, 20, 30, 128,        // RLE packet: two pixels at half alpha
        0x01, 40, 50, 60, 255, 70, 80, 90, 255,  // raw packet: two opaque pixels
    };
    bytes.insert(bytes.end(), std::begin(body), std::end(body));

    const auto image = tenriff::render::decode_targa(bytes.data(), bytes.size());
    REQUIRE(image.valid);
    CHECK(image.width == 4);
    CHECK(image.height == 1);

    for (int x = 0; x < 2; ++x) {
        const std::uint8_t* p = pixel_at(image, x, 0);
        CHECK(p[0] == static_cast<std::uint8_t>((10 * 128 + 127) / 255));
        CHECK(p[1] == static_cast<std::uint8_t>((20 * 128 + 127) / 255));
        CHECK(p[2] == static_cast<std::uint8_t>((30 * 128 + 127) / 255));
        CHECK(p[3] == 128);
    }
    const std::uint8_t* third = pixel_at(image, 2, 0);
    CHECK(third[0] == 40);
    CHECK(third[3] == 255);
    const std::uint8_t* fourth = pixel_at(image, 3, 0);
    CHECK(fourth[2] == 90);
}

TEST_CASE("targa decoder fills the alpha channel for 24-bit frames") {
    auto bytes = make_header(2u, 1, 1, 24u, 0x20u);
    const std::uint8_t body[] = {11, 22, 33};
    bytes.insert(bytes.end(), std::begin(body), std::end(body));

    const auto image = tenriff::render::decode_targa(bytes.data(), bytes.size());
    REQUIRE(image.valid);
    const std::uint8_t* p = pixel_at(image, 0, 0);
    CHECK(p[0] == 11);
    CHECK(p[1] == 22);
    CHECK(p[2] == 33);
    CHECK(p[3] == 255);
}

TEST_CASE("targa decoder rejects truncated and unsupported frames") {
    const auto truncated = make_header(2u, 8, 8, 32u, 0x20u);
    CHECK_FALSE(tenriff::render::decode_targa(truncated.data(), truncated.size()).valid);

    auto paletted = make_header(1u, 2, 2, 8u, 0x20u);
    paletted[1] = 1u;  // colour-mapped, which LR2 skins never use
    paletted.resize(paletted.size() + 64u, 0u);
    CHECK_FALSE(tenriff::render::decode_targa(paletted.data(), paletted.size()).valid);

    CHECK_FALSE(tenriff::render::decode_targa(nullptr, 0u).valid);
}
