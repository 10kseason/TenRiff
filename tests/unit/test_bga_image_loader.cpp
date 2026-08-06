#include "doctest/doctest.h"

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

#endif
