#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/ChartFileHash.h"
#include "app/DifficultyTable.h"
#include "app/DifficultyTableLink.h"
#include "config/BuiltinDifficultyTables.h"

namespace {

struct TempDirGuard {
    std::filesystem::path path;

    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_difficulty_table_tests";
    std::filesystem::create_directories(base);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto candidate = base / ("case_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

void write_file(const std::filesystem::path& path, std::string_view content) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    REQUIRE(out.good());
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('a') && ch <= static_cast<unsigned char>('f')) {
            return static_cast<char>(ch - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

}  // namespace

TEST_CASE("chart file hashing matches standard MD5 and SHA-256 vectors") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    const auto chart = temp.path / "chart.bms";

    write_file(chart, "");
    std::string error;
    auto hashes = tenriff::app::hash_chart_file(chart, &error);
    REQUIRE(error.empty());
    REQUIRE(hashes.valid());
    CHECK(hashes.size == 0u);
    CHECK(hashes.md5 == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(hashes.sha256 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    write_file(chart, "abc");
    hashes = tenriff::app::hash_chart_file(chart, &error);
    REQUIRE(error.empty());
    REQUIRE(hashes.valid());
    CHECK(hashes.size == 3u);
    CHECK(hashes.md5 == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(hashes.sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    write_file(chart, "The quick brown fox jumps over the lazy dog");
    hashes = tenriff::app::hash_chart_file(chart, &error);
    REQUIRE(error.empty());
    CHECK(hashes.md5 == "9e107d9d372bb6826bd81d3542a419d6");
    CHECK(hashes.sha256 == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");

    // This 56-byte vector crosses the padding boundary and exercises a
    // two-block finalization in both digest implementations.
    write_file(chart, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    hashes = tenriff::app::hash_chart_file(chart, &error);
    REQUIRE(error.empty());
    CHECK(hashes.md5 == "8215ef0796a20bcaaae116d3876c664a");
    CHECK(hashes.sha256 == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("chart file hashing reports missing files without partial hashes") {
    std::string error;
    const auto hashes = tenriff::app::hash_chart_file(
        std::filesystem::path("definitely_missing_tenriff_chart.bms"), &error);
    CHECK_FALSE(hashes.valid());
    CHECK(hashes.md5.empty());
    CHECK(hashes.sha256.empty());
    CHECK_FALSE(error.empty());
}

TEST_CASE("difficulty table loads a standard data array with explicit identity") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    const auto data_path = temp.path / "data.json";

    write_file(data_path,
               "["
               "{\"md5\":\"900150983cd24fb0d6963f7d28e17f72\","
               " \"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\","
               " \"level\":\"2\",\"title\":\"Ignored metadata\"},"
               "{\"md5\":\"9e107d9d372bb6826bd81d3542a419d6\",\"level\":3},"
               "{\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\","
               " \"level\":\"99\"},"
               "{\"md5\":\"not-a-hash\",\"level\":4}"
               "]");

    tenriff::app::DifficultyTableLoadOptions options;
    options.name = "Direct Table";
    options.symbol = "dt";
    options.level_order = {"1", "2", "3"};
    const auto result = tenriff::app::load_difficulty_table(data_path, options);

    REQUIRE(result.success());
    CHECK(result.table.name() == "Direct Table");
    CHECK(result.table.symbol() == "dt");
    CHECK(result.table.entry_count() == 2u);
    CHECK_FALSE(result.warnings.empty());

    tenriff::app::ChartFileHashes abc;
    abc.md5 = upper_ascii("900150983cd24fb0d6963f7d28e17f72");
    abc.sha256 = upper_ascii("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const auto match = result.table.lookup(abc);
    REQUIRE(match.has_value());
    CHECK(match->name == "Direct Table");
    CHECK(match->symbol == "dt");
    CHECK(match->level == "2");
    CHECK(match->order == 1);
    CHECK(match->label() == "dt2");

    // SHA-256 takes precedence if callers accidentally supply digests from
    // different chart records.
    const auto preferred_sha = result.table.lookup(
        "9e107d9d372bb6826bd81d3542a419d6",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(preferred_sha.has_value());
    CHECK(preferred_sha->level == "2");
    CHECK_FALSE(result.table.lookup("00000000000000000000000000000000", "").has_value());
}

TEST_CASE("difficulty table resolves a local standard header and relative data_url") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    const auto header_path = temp.path / "header.json";
    const auto data_path = temp.path / "table-data" / "charts.json";

    write_file(data_path,
               "["
               "{\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\","
               " \"level\":2,\"artist\":\"Ignored\"}"
               "]");
    write_file(header_path,
               "\xEF\xBB\xBF"
               "{\"name\":\"Satellite Local\",\"symbol\":\"sl\","
               " \"data_url\":\"table-data/charts.json\","
               " \"level_order\":[0,\"1\",2,\"2+\"]}");

    const auto result = tenriff::app::load_difficulty_table(header_path);
    REQUIRE(result.success());
    CHECK(result.table.entry_count() == 1u);

    tenriff::app::ChartFileHashes hashes;
    hashes.sha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const auto match = result.table.lookup(hashes);
    REQUIRE(match.has_value());
    CHECK(match->name == "Satellite Local");
    CHECK(match->symbol == "sl");
    CHECK(match->level == "2");
    CHECK(match->order == 2);
    CHECK(match->label() == "sl2");
}

TEST_CASE("difficulty table keeps network fetching and ambiguous arrays out of the loader") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto remote_header = temp.path / "remote-header.json";
    write_file(remote_header,
               "{\"name\":\"Remote\",\"symbol\":\"r\","
               " \"data_url\":\"https://example.invalid/table.json\"}");
    auto result = tenriff::app::load_difficulty_table(remote_header);
    CHECK_FALSE(result.success());
    CHECK(result.error.find("local relative path") != std::string::npos);

    const auto direct_data = temp.path / "direct.json";
    write_file(direct_data,
               "[{\"md5\":\"900150983cd24fb0d6963f7d28e17f72\",\"level\":1}]");
    result = tenriff::app::load_difficulty_table(direct_data);
    CHECK_FALSE(result.success());
    CHECK(result.error.find("name and symbol") != std::string::npos);

    const auto absolute_header = temp.path / "absolute-header.json";
    write_file(absolute_header,
               "{\"name\":\"Absolute\",\"symbol\":\"a\","
               " \"data_url\":\"C:\\\\private\\\\table.json\"}");
    result = tenriff::app::load_difficulty_table(absolute_header);
    CHECK_FALSE(result.success());
    CHECK(result.error.find("local relative path") != std::string::npos);
}

TEST_CASE("difficulty table link parser reads standard BMSTable HTML metadata") {
    const auto link = tenriff::app::extract_bmstable_header_url(
        "<html><head><META CONTENT='headers/main.json?x=1&amp;y=2' NAME=BMSTABLE></head></html>");
    REQUIRE(link.has_value());
    CHECK(*link == "headers/main.json?x=1&y=2");
    CHECK_FALSE(tenriff::app::extract_bmstable_header_url(
                    "<meta name=\"description\" content=\"bmstable\">")
                    .has_value());
}

TEST_CASE("difficulty table link resolver handles relative HTTP paths") {
    const auto relative = tenriff::app::resolve_difficulty_table_url(
        "https://example.test/tables/page/index.html?old=1",
        "../header.json?q=1#ignored");
    REQUIRE(relative.has_value());
    CHECK(*relative == "https://example.test/tables/header.json?q=1");

    const auto root = tenriff::app::resolve_difficulty_table_url(
        "https://example.test/tables/index.html", "/data/table.json");
    REQUIRE(root.has_value());
    CHECK(*root == "https://example.test/data/table.json");

    const auto scheme_relative = tenriff::app::resolve_difficulty_table_url(
        "https://example.test/tables/index.html", "//cdn.example.test/header.json");
    REQUIRE(scheme_relative.has_value());
    CHECK(*scheme_relative == "https://cdn.example.test/header.json");
    CHECK_FALSE(tenriff::app::resolve_difficulty_table_url(
                    "file:///table/index.html", "header.json")
                    .has_value());
}

TEST_CASE("difficulty table link import caches HTML header and data for the local loader") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const std::unordered_map<std::string, std::string> documents{
        {"https://example.test/table/index.html",
         "<html><head><meta name=\"bmstable\" content=\"header.json\"></head></html>"},
        {"https://example.test/table/header.json",
         "{\"name\":\"Remote Table\",\"symbol\":\"rt\","
         "\"data_url\":\"data/charts.json\",\"level_order\":[\"1\",\"2\"]}"},
        {"https://example.test/table/data/charts.json",
         "[{\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\","
         "\"level\":\"2\"}]"},
    };
    std::vector<std::string> requested;
    auto fetch = [&](std::string_view url, std::size_t max_bytes) {
        requested.emplace_back(url);
        const auto it = documents.find(std::string(url));
        if (it == documents.end()) {
            return tenriff::app::DifficultyTableHttpResponse{
                404, std::string(url), {}, {}};
        }
        CHECK(it->second.size() <= max_bytes);
        return tenriff::app::DifficultyTableHttpResponse{
            200, std::string(url), it->second, {}};
    };

    const auto imported = tenriff::app::import_difficulty_table_link(
        "https://example.test/table/index.html#page",
        temp.path / "cache",
        fetch);
    REQUIRE(imported.success());
    CHECK(imported.source_url == "https://example.test/table/index.html");
    CHECK(imported.table_name == "Remote Table");
    CHECK((requested == std::vector<std::string>{
                            "https://example.test/table/index.html",
                            "https://example.test/table/header.json",
                            "https://example.test/table/data/charts.json"}));
    CHECK(std::filesystem::exists(std::filesystem::u8path(imported.cached_header_path)));

    const auto loaded = tenriff::app::load_difficulty_table_utf8(imported.cached_header_path);
    REQUIRE(loaded.success());
    const auto match = loaded.table.lookup(
        "",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(match.has_value());
    CHECK(match->name == "Remote Table");
    CHECK(match->label() == "rt2");
    CHECK(match->order == 1);

    requested.clear();
    const auto direct_header = tenriff::app::import_difficulty_table_link(
        "https://example.test/table/header.json",
        temp.path / "direct-cache",
        fetch);
    REQUIRE(direct_header.success());
    CHECK((requested == std::vector<std::string>{
                            "https://example.test/table/header.json",
                            "https://example.test/table/data/charts.json"}));
}

TEST_CASE("difficulty table link import rejects pages without BMSTable metadata") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    auto fetch = [](std::string_view url, std::size_t) {
        return tenriff::app::DifficultyTableHttpResponse{
            200, std::string(url), "<html><head></head></html>", {}};
    };
    const auto imported = tenriff::app::import_difficulty_table_link(
        "https://example.test/not-a-table", temp.path / "cache", fetch);
    CHECK_FALSE(imported.success());
    CHECK(imported.error.find("BMSTable meta") != std::string::npos);
}

TEST_CASE("difficulty table live link import smoke is opt-in") {
    const char* live_url = std::getenv("TENRIFF_LIVE_BMSTABLE_URL");
    if (!live_url || *live_url == '\0') {
        return;
    }

    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    const auto imported = tenriff::app::import_difficulty_table_link(
        live_url, temp.path / "live-cache");
    if (!imported.success()) {
        std::cerr << "[live BMSTable import] " << imported.error << '\n';
    }
    REQUIRE(imported.success());
    CHECK_FALSE(imported.table_name.empty());
    if (std::string(live_url).find("asumatoki.kr/table/aery/") != std::string::npos) {
        const auto loaded = tenriff::app::load_difficulty_table_utf8(imported.cached_header_path);
        REQUIRE(loaded.success());
        const auto match = loaded.table.lookup(
            "1d99a3728312c6b689b6a6dc51770732",
            "");
        REQUIRE(match.has_value());
        CHECK(match->level == "LEVEL 13");
        CHECK(match->label() == "⑤LEVEL 13");
        CHECK(match->order >= 0);
    }
}

TEST_CASE("built-in tables import publisher header and relative page layouts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    struct PublisherLayout {
        std::string header_url;
        std::string page_header;
        std::string data_file;
        bool header_bom = false;
    };
    const std::vector<PublisherLayout> layouts{
        {"https://asumatoki.kr/table/aery/header.json", "", "data.json"},
        {"https://asumatoki.kr/table/aery7/header.json", "", "data.json"},
        {"https://calc.10k-revive.cloud/table/header.json", "table/header.json", "body.json"},
        {"https://stellabms.xyz/st/header.json", "header.json", "score.json"},
        {"https://stellabms.xyz/sl/header.json", "header.json", "score.json"},
        {"https://classmaterma.github.io/4UE/header.json", "header.json", "score.json", true},
        {"https://classmaterma.github.io/UE/header.json", "header.json", "score.json"},
        {"https://classmaterma.github.io/8UE/header.json", "header.json", "score.json", true}};
    REQUIRE(layouts.size() == tenriff::config::kBuiltinDifficultyTables.size());
    for (std::size_t i = 0; i < tenriff::config::kBuiltinDifficultyTables.size(); ++i) {
        const auto& preset = tenriff::config::kBuiltinDifficultyTables[i];
        const auto& layout = layouts[i];
        const auto body_url = tenriff::app::resolve_difficulty_table_url(layout.header_url, layout.data_file);
        REQUIRE(body_url.has_value());
        auto fetch = [&](std::string_view url, std::size_t) {
            std::string body;
            if (!layout.page_header.empty() && url == preset.url) {
                body = "<meta name=\"bmstable\" content=\"" + layout.page_header + "\">";
            } else if (url == layout.header_url) {
                body = std::string("{\"name\":\"") + std::string(preset.name_ko) +
                    "\",\"symbol\":\"Lv\",\"data_url\":\"" + layout.data_file + "\"}";
                if (layout.header_bom) body.insert(0, "\xef\xbb\xbf");
            } else if (url == *body_url) {
                body = "[{\"md5\":\"1d99a3728312c6b689b6a6dc51770732\",\"level\":\"13\"}]";
            } else {
                return tenriff::app::DifficultyTableHttpResponse{404, std::string(url), {}, {}};
            }
            return tenriff::app::DifficultyTableHttpResponse{200, std::string(url), body, {}};
        };
        const auto imported = tenriff::app::import_difficulty_table_link(
            preset.url, temp.path / std::to_string(i), fetch);
        REQUIRE(imported.success());
        CHECK(imported.source_url == preset.url);
        const auto loaded = tenriff::app::load_difficulty_table_utf8(imported.cached_header_path);
        REQUIRE(loaded.success());
        const auto matched = loaded.table.lookup("1d99a3728312c6b689b6a6dc51770732", "");
        REQUIRE(matched.has_value());
        CHECK(matched->name == preset.name_ko);
        CHECK(matched->level == "13");
    }
}

TEST_CASE("difficulty table shortcuts preserve legacy keys and reserve F4 for native levels") {
    const auto& presets = tenriff::config::kBuiltinDifficultyTables;
    REQUIRE(presets.size() == 8);
    CHECK(presets[0].name_en == "5K Aery");
    CHECK(presets[1].name_en == "7K Aery");
    CHECK(presets[2].name_en == "10K Revive");
    CHECK(presets[0].function_key == 1);
    CHECK(presets[1].function_key == 2);
    CHECK(presets[2].function_key == 3);
    std::array<bool, 10> used{};
    used[4] = true; // Native LV must not be shadowed by a downloaded table.
    for (const auto& preset : presets) {
        REQUIRE(preset.function_key >= 1);
        REQUIRE(preset.function_key <= 9);
        CHECK_FALSE(used[static_cast<std::size_t>(preset.function_key)]);
        used[static_cast<std::size_t>(preset.function_key)] = true;
    }
    CHECK(std::all_of(used.begin() + 1, used.end(), [](bool bound) { return bound; }));
}

TEST_CASE("all built-in difficulty tables live import is opt-in") {
    const char* enabled = std::getenv("TENRIFF_LIVE_BUILTIN_TABLES");
    if (!enabled || std::string_view(enabled) != "1") return;
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    for (std::size_t i = 0; i < tenriff::config::kBuiltinDifficultyTables.size(); ++i) {
        const auto& preset = tenriff::config::kBuiltinDifficultyTables[i];
        const auto imported = tenriff::app::import_difficulty_table_link(
            preset.url, temp.path / std::to_string(i));
        std::cout << "[builtin table] " << preset.name_ko << ": "
                  << (imported.success() ? imported.table_name : imported.error) << '\n';
        REQUIRE(imported.success());
        const auto loaded = tenriff::app::load_difficulty_table_utf8(imported.cached_header_path);
        REQUIRE(loaded.success());
        CHECK(loaded.table.entry_count() > 0);
        if (i >= 3) {
            const std::array<std::string_view, 5> names{
                "Stella", "Satellite", "4UE Difficulty Table", "6UE Difficulty Table", "8UE Difficulty Table"};
            const std::array<std::string_view, 5> symbols{"st", "sl", "4UE", "6UE", "8UE"};
            CHECK(loaded.table.name() == names[i - 3]);
            CHECK(loaded.table.symbol() == symbols[i - 3]);
        }
        std::cout << "[builtin table loaded] " << loaded.table.name() << " ("
                  << loaded.table.symbol() << "): " << loaded.table.entry_count() << " entries\n";
    }
}
