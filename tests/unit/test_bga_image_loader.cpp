#include "doctest/doctest.h"
#include "render/BgaTransparency.h"

TEST_CASE("BGA overlay keys exact black and preserves near black and authored alpha") {
    std::vector<std::uint8_t> pixels{0,0,0,255, 1,0,0,255, 0,0,1,1, 50,100,200,128};
    tenriff::render::apply_bga_black_key(pixels);
    CHECK(pixels[3] == 0);
    CHECK(pixels[7] == 255);
    CHECK(pixels[11] == 1);
    CHECK(pixels[15] == 128);
}

TEST_CASE("BGA restored overlay cannot turn transparent pixels opaque") {
    std::vector<std::uint8_t> pixels(2 * 2 * 2 * 4, 200);
    const std::vector<std::uint8_t> alpha{0, 128};
    tenriff::render::restore_bga_alpha(pixels, alpha, 2, 1, 2);
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 4; ++x) {
        const auto i = (y * 4 + x) * 4;
        CHECK(pixels[i + 3] == (x < 2 ? 0 : 128));
        CHECK(pixels[i] == (x < 2 ? 0 : 100));
    }
}

#ifdef _WIN32

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "render/BgaImageLoader.h"

TEST_CASE("BGA image loader decodes static frames off-thread") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tenriff_bga_image_loader_test.bmp";
    // 1x1, 32-bit BGRA BMP. A generated fixture keeps the test independent of
    // repository assets and also exercises the file decoder used by BMS BMP BGA.
    const std::array<unsigned char, 58> bmp{{
        0x42, 0x4D, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x0B,
        0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80,
        0xC0, 0xFF,
    }};
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        REQUIRE(file.good());
        file.write(reinterpret_cast<const char*>(bmp.data()),
                   static_cast<std::streamsize>(bmp.size()));
        REQUIRE(file.good());
    }

    tenriff::render::BgaImageLoader loader;
    loader.request(path.u8string());
    std::shared_ptr<const tenriff::render::BgaImageFrame> frame;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!frame && std::chrono::steady_clock::now() < deadline) {
        frame = loader.take_ready();
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    REQUIRE(frame);
    CHECK(frame->width == 1);
    CHECK(frame->height == 1);
    CHECK(frame->bgra.size() == 4u);
}

TEST_CASE("BGA same black BMP is opaque on base and transparent on overlay") {
    const auto path = std::filesystem::temp_directory_path() / "tenriff_bga_black_key_test.bmp";
    std::array<unsigned char, 58> bmp{};
    bmp[0] = 'B'; bmp[1] = 'M'; bmp[2] = 58; bmp[10] = 54;
    bmp[14] = 40; bmp[18] = 1; bmp[22] = 1; bmp[26] = 1; bmp[28] = 32; bmp[34] = 4;
    bmp[57] = 255;
    { std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(bmp.data()), bmp.size()); }
    tenriff::render::BgaImageLoader base;
    tenriff::render::BgaImageLoader overlay(true);
    base.request(path.u8string()); overlay.request(path.u8string());
    std::shared_ptr<const tenriff::render::BgaImageFrame> b, o;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((!b || !o) && std::chrono::steady_clock::now() < deadline) {
        if (!b) b = base.take_ready();
        if (!o) o = overlay.take_ready();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    REQUIRE(b); REQUIRE(o);
    REQUIRE(b->bgra.size() == 4); REQUIRE(o->bgra.size() == 4);
    CHECK(b->bgra[3] == 255);
    CHECK(o->bgra[3] == 0);
}

#endif
