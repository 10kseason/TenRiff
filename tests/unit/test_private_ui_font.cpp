#include "doctest/doctest.h"

#include "app/MenuAppSkinUtils.h"
#include "config/Config.h"

TEST_CASE("custom UI font persists its local UTF8 path and leaves built in choices available") {
    tenriff::config::SkinConfig original;
    original.ui_font = "CUSTOM";
    original.ui_font_path = "fonts/사용자 글꼴.ttf";
    tenriff::config::SkinConfig loaded;
    REQUIRE(tenriff::config::deserialize_skin_config(tenriff::config::serialize_skin_config(original), loaded));
    CHECK(loaded.ui_font == "custom");
    CHECK(loaded.ui_font_path == original.ui_font_path);
    CHECK(tenriff::app::cycle_skin_ui_font("consolas", 1) == "custom");
    CHECK(tenriff::app::cycle_skin_ui_font("custom", 1) == "default");
    CHECK(tenriff::app::cycle_skin_ui_font("default", -1) == "custom");
    REQUIRE(tenriff::config::deserialize_skin_config("{}", loaded));
    CHECK(loaded.ui_font == "default");
    CHECK(loaded.ui_font_path.empty());
}

#ifdef _WIN32
#include "render/PrivateUiFont.h"
#include <array>
#include <chrono>
#include <fstream>

TEST_CASE("private UI font loads Unicode local TTF and safely clears invalid selections") {
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    REQUIRE(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()))));
    std::array<wchar_t, MAX_PATH> windows{};
    REQUIRE(GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size())) > 0);
    const auto source = std::filesystem::path(windows.data()) / "Fonts" / "segoeui.ttf";
    REQUIRE(std::filesystem::is_regular_file(source));
    struct Scratch {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("tenriff-private-font-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ~Scratch() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } scratch;
    std::filesystem::create_directory(scratch.path);
    const auto copied = scratch.path / std::filesystem::u8path("사용자 폰트.ttf");
    std::filesystem::copy_file(source, copied);
    auto runtime = tenriff::config::ConfigLoader{}.defaults();
    runtime.skin.ui_font = "custom";
    runtime.skin.ui_font_path = copied.u8string();
    const auto profile = scratch.path / "profile";
    REQUIRE(tenriff::config::ConfigLoader{}.save_profile(profile.u8string(), runtime));
    const auto restored = tenriff::config::ConfigLoader{}.load_profile(profile.u8string());
    REQUIRE(restored.success());
    CHECK(restored.config.skin.ui_font == "custom");
    CHECK(restored.config.skin.ui_font_path == copied.u8string());
    tenriff::render::PrivateUiFont font;
    REQUIRE(font.load(factory.Get(), copied.u8string()));
    REQUIRE(font.collection);
    CHECK_FALSE(font.family.empty());
    CHECK(font.collection->GetFontFamilyCount() == 1);
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    REQUIRE(SUCCEEDED(factory->CreateTextFormat(font.family.c_str(), font.collection.Get(),
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        24.0f, L"", &format)));
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    REQUIRE(SUCCEEDED(factory->CreateTextLayout(L"TENRIFF 한글 123", 14, format.Get(), 400, 100, &layout)));
    DWRITE_TEXT_METRICS metrics{};
    REQUIRE(SUCCEEDED(layout->GetMetrics(&metrics)));
    CHECK(metrics.width > 0.0f);
    layout.Reset();
    format.Reset();

    const auto invalid = scratch.path / "invalid.ttf";
    { std::ofstream out(invalid); out << "not a font"; }
    CHECK_FALSE(font.load(factory.Get(), invalid.u8string()));
    CHECK_FALSE(font.collection);
    CHECK(font.family.empty());
    CHECK_FALSE(font.load(factory.Get(), (scratch.path / "missing.otf").u8string()));
    CHECK_FALSE(font.load(factory.Get(), (scratch.path / "invalid.txt").u8string()));
    CHECK_FALSE(font.load(factory.Get(), ""));
    factory.Reset();
}
#endif
