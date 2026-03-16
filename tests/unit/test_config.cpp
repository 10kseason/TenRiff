#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "app/RuntimeConfigMigration.h"
#include "config/Config.h"

using tenriff::config::ConfigLoader;

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

struct CurrentPathGuard {
    std::filesystem::path old_path = std::filesystem::current_path();

    ~CurrentPathGuard() {
        std::error_code ec;
        std::filesystem::current_path(old_path, ec);
    }
};

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_config_tests";
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

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << content;
}

}  // namespace

TEST_CASE("config defaults prefer 44100 Hz audio") {
    ConfigLoader loader;
    const auto config = loader.defaults();

    CHECK(config.audio.sample_rate == 44100);
    CHECK(config.audio_ui.preset == "high");
    CHECK(config.audio_ui.bgm_volume == doctest::Approx(0.75));
    CHECK(config.audio_ui.keysound_volume == doctest::Approx(1.0));
    CHECK(config.input.debounce_ms == doctest::Approx(8.0));
    CHECK(config.mode.format == "bms");
    CHECK_FALSE(config.mode.enable_osu_charts);
    CHECK(config.mode.song_index_profile == "safe");
    CHECK(config.graphics.resolution == "native");
    CHECK(config.graphics.display_mode == "borderless");
    CHECK(config.graphics.refresh_hz == 1050);
    CHECK(config.gauge.hard_to_normal_threshold == doctest::Approx(66.0));
    CHECK(config.gauge.normal_to_easy_threshold == doctest::Approx(33.0));
    CHECK(config.gauge.hard.pg == doctest::Approx(0.03666667));
    CHECK(config.gauge.hard.gr == doctest::Approx(0.02444444));
    CHECK(config.gauge.hard.gd == doctest::Approx(0.00611111));
    CHECK(config.gauge.hard.bd == doctest::Approx(-5.50000));
    CHECK(config.gauge.hard.pr == doctest::Approx(-7.50000));
    CHECK(config.gauge.normal.pg == doctest::Approx(0.05238095));
    CHECK(config.gauge.normal.gr == doctest::Approx(0.03492063));
    CHECK(config.gauge.normal.gd == doctest::Approx(0.00873016));
    CHECK(config.gauge.normal.bd == doctest::Approx(-2.75000));
    CHECK(config.gauge.normal.pr == doctest::Approx(-3.75000));
    CHECK(config.gauge.easy.pg == doctest::Approx(0.10000000));
    CHECK(config.gauge.easy.gr == doctest::Approx(0.06666667));
    CHECK(config.gauge.easy.gd == doctest::Approx(0.01666667));
    CHECK(config.gauge.easy.bd == doctest::Approx(-2.06250));
    CHECK(config.gauge.easy.pr == doctest::Approx(-2.81250));
    CHECK(config.judge.bd_ms == doctest::Approx(200.0));
    CHECK(config.judge.indirect_miss_ms == doctest::Approx(500.0));
    CHECK(config.judge.hold_grace_ms == doctest::Approx(35.0));
    CHECK(config.judge.hold_break_ms == doctest::Approx(100.0));
    CHECK(config.skin.note_shape == "rect");
    CHECK(config.skin.note_border_enabled);
    CHECK(config.skin.combo_position == doctest::Approx(tenriff::config::kComboPositionDefault));
    CHECK(config.skin.hold_body_width_scale == doctest::Approx(0.60));
    CHECK(tenriff::config::resolved_skin_lane_colors(config.skin, "16k").size() == 16u);
}

TEST_CASE("config save and load preserve indirect miss setting") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.judge.indirect_miss_ms = 640.0;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.judge.indirect_miss_ms == doctest::Approx(640.0));
}

TEST_CASE("config save and load preserve input debounce setting") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.input.debounce_ms = 12.0;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.input.debounce_ms == doctest::Approx(12.0));
}

TEST_CASE("audio presets do not override explicit sample rate") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"audio\": {\n"
               "    \"rate\": 44100,\n"
               "    \"preset\": \"high\"\n"
               "  }\n"
               "}\n");
    write_file(temp.path / "profiles" / "test" / "config.json", "{ }\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    const auto result = loader.load_profile("profiles/test");

    REQUIRE(result.success());
    CHECK(result.config.audio.sample_rate == 44100);
    CHECK(result.config.audio.frames_per_buffer == 320);
    CHECK(result.config.audio.periods == 3);
    CHECK(result.config.audio_ui.bms_keysound_policy == "follow");
}

TEST_CASE("config save and load preserve volume and speed settings") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.audio_ui.master_volume = 0.65;
    config.audio_ui.bgm_volume = 0.55;
    config.audio_ui.keysound_volume = 1.35;
    config.speed.rate = 1.25;
    config.speed.hi_speed = 4.75;
    config.mode.enable_osu_charts = true;
    config.mode.format = "osu";
    config.mode.song_index_profile = "fast";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.audio_ui.master_volume == doctest::Approx(0.65));
    CHECK(result.config.audio_ui.bgm_volume == doctest::Approx(0.55));
    CHECK(result.config.audio_ui.keysound_volume == doctest::Approx(1.35));
    CHECK(result.config.speed.rate == doctest::Approx(1.25));
    CHECK(result.config.speed.hi_speed == doctest::Approx(4.75));
    CHECK(result.config.mode.enable_osu_charts);
    CHECK(result.config.mode.format == "osu");
    CHECK(result.config.mode.song_index_profile == "fast");
}

TEST_CASE("config save and load preserve graphics display settings") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.graphics.display_mode = "windowed";
    config.graphics.resolution = "qhd";
    config.graphics.refresh_hz = 240;
    config.graphics.performance_overlay = true;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.graphics.display_mode == "windowed");
    CHECK(result.config.graphics.resolution == "qhd");
    CHECK(result.config.graphics.refresh_hz == 240);
    CHECK(result.config.graphics.performance_overlay);
}

TEST_CASE("config clamps refresh_hz and normalizes invalid resolution preset") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"graphics\": {\n"
               "    \"display_mode\": \"floating\",\n"
               "    \"resolution\": \"weird\",\n"
               "    \"refresh_hz\": 5000\n"
               "  }\n"
               "}\n");
    write_file(temp.path / "profiles" / "test" / "config.json", "{ }\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    const auto result = loader.load_profile("profiles/test");

    REQUIRE(result.success());
    CHECK(result.config.graphics.display_mode == "borderless");
    CHECK(result.config.graphics.resolution == "native");
    CHECK(result.config.graphics.refresh_hz == 1050);
}

TEST_CASE("config save and load preserve visual offset setting") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.visual_offset_ms = 85.0;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.visual_offset_ms == doctest::Approx(85.0));
}

TEST_CASE("config clamps visual offset into supported range") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"offsets\": {\n"
               "    \"visual\": 999.0\n"
               "  }\n"
               "}\n");
    write_file(temp.path / "profiles" / "test" / "config.json", "{ }\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    const auto result = loader.load_profile("profiles/test");

    REQUIRE(result.success());
    CHECK(result.config.visual_offset_ms == doctest::Approx(500.0));
}

TEST_CASE("config save and load preserve skin gameplay settings") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.note_shape = "circle";
    config.skin.note_border_enabled = false;
    config.skin.judgement_line_position = 0.76;
    config.skin.combo_position = 0.52;
    config.skin.note_width_scale = 1.15;
    config.skin.hold_body_width_scale = 1.10;
    config.skin.note_height_scale = 1.35;
    config.skin.note_width_scales["4k"] = 0.85;
    config.skin.note_width_scales["16k"] = 0.65;
    config.skin.note_height_scales["4k"] = 1.20;
    config.skin.note_height_scales["16k"] = 1.70;
    config.skin.lane_colors["4k"] = {"rose", "gold", "gold", "rose"};
    config.skin.lane_colors["5k"] = {"rose", "mint", "gold", "azure", "ice"};
    config.skin.lane_colors["16k"] = {"rose", "mint", "gold", "azure", "ice", "teal", "violet", "orange",
                                      "orange", "violet", "teal", "ice", "azure", "gold", "mint", "rose"};

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.note_shape == "circle");
    CHECK_FALSE(result.config.skin.note_border_enabled);
    CHECK(result.config.skin.judgement_line_position == doctest::Approx(0.76));
    CHECK(result.config.skin.combo_position == doctest::Approx(0.52));
    CHECK(result.config.skin.note_width_scale == doctest::Approx(1.15));
    CHECK(result.config.skin.hold_body_width_scale == doctest::Approx(1.10));
    CHECK(result.config.skin.note_height_scale == doctest::Approx(1.35));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "4k") == doctest::Approx(0.85));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "16k") == doctest::Approx(0.65));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "10k") == doctest::Approx(1.15));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "4k") == doctest::Approx(1.20));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "16k") == doctest::Approx(1.70));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "10k") == doctest::Approx(1.35));
    const auto saved_4k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "4k");
    REQUIRE(saved_4k.size() == 4u);
    CHECK(saved_4k[0] == "rose");
    CHECK(saved_4k[1] == "gold");
    CHECK(saved_4k[3] == "rose");
    const auto saved_5k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "5k");
    REQUIRE(saved_5k.size() == 5u);
    CHECK(saved_5k[0] == "rose");
    CHECK(saved_5k[1] == "mint");
    CHECK(saved_5k[2] == "gold");
    const auto saved_16k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "16k");
    REQUIRE(saved_16k.size() == 16u);
    CHECK(saved_16k[0] == "rose");
    CHECK(saved_16k[7] == "orange");
    CHECK(saved_16k[15] == "rose");
}

TEST_CASE("config clamps skin gameplay settings into supported range") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"skin\": {\n"
               "    \"note_shape\": \"hexagon\",\n"
               "    \"note_border_enabled\": false,\n"
               "    \"judgement_line_position\": 1.5,\n"
               "    \"combo_position\": 9.0,\n"
               "    \"note_width_scale\": 9.0,\n"
               "    \"hold_body_width_scale\": 9.0,\n"
               "    \"note_height_scale\": 0.1,\n"
               "    \"note_width_scales\": {\n"
               "      \"4k\": 0.1,\n"
               "      \"16k\": 9.0\n"
               "    },\n"
               "    \"note_height_scales\": {\n"
               "      \"5k\": 0.1,\n"
               "      \"16k\": 9.0\n"
               "    },\n"
               "    \"lane_colors\": {\n"
               "      \"5k\": [\"badtoken\", \"azure\"],\n"
               "      \"10k\": [\"rose\", \"mint\", \"gold\", \"azure\", \"ice\", \"ice\", \"azure\", \"gold\", \"mint\", \"rose\"]\n"
               "    }\n"
               "  }\n"
               "}\n");
    write_file(temp.path / "profiles" / "test" / "config.json", "{ }\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    const auto result = loader.load_profile("profiles/test");

    REQUIRE(result.success());
    CHECK(result.config.skin.note_shape == "rect");
    CHECK_FALSE(result.config.skin.note_border_enabled);
    CHECK(result.config.skin.judgement_line_position ==
          doctest::Approx(tenriff::config::kJudgementLinePositionMax));
    CHECK(result.config.skin.combo_position == doctest::Approx(tenriff::config::kComboPositionMax));
    CHECK(result.config.skin.note_width_scale == doctest::Approx(tenriff::config::kNoteWidthScaleMax));
    CHECK(result.config.skin.hold_body_width_scale == doctest::Approx(tenriff::config::kHoldBodyWidthScaleMax));
    CHECK(result.config.skin.note_height_scale == doctest::Approx(tenriff::config::kNoteHeightScaleMin));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "4k") ==
          doctest::Approx(tenriff::config::kNoteWidthScaleMin));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "16k") ==
          doctest::Approx(tenriff::config::kNoteWidthScaleMax));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "5k") ==
          doctest::Approx(tenriff::config::kNoteHeightScaleMin));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "16k") ==
          doctest::Approx(tenriff::config::kNoteHeightScaleMax));
    const auto clamped_5k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "5k");
    REQUIRE(clamped_5k.size() == 5u);
    CHECK(clamped_5k[0] == "ice");
    CHECK(clamped_5k[1] == "azure");
    const auto saved_10k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "10k");
    REQUIRE(saved_10k.size() == 10u);
    CHECK(saved_10k[0] == "rose");
    CHECK(saved_10k[9] == "rose");
}

TEST_CASE("config save and load preserve recent song sources") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.ui.active_song_source = "D:/Songs/PackB";
    config.ui.recent_song_sources = {"D:/Songs/PackB", "D:/Songs/PackA"};

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.ui.active_song_source == "D:/Songs/PackB");
    REQUIRE(result.config.ui.recent_song_sources.size() == 2u);
    CHECK(result.config.ui.recent_song_sources[0] == "D:/Songs/PackB");
    CHECK(result.config.ui.recent_song_sources[1] == "D:/Songs/PackA");
}

TEST_CASE("bms-first runtime migration keeps valid keysound modes while forcing bms 10k") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.audio_ui.preset = "basic";
    config.audio_ui.bms_keysound_policy = "autoplay";
    config.mode.format = "osu";
    config.mode.key_mode = "7k";

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.audio_ui.preset == "basic");
    CHECK(config.audio_ui.bms_keysound_policy == "autoplay");
    CHECK(config.mode.format == "bms");
    CHECK(config.mode.key_mode == "10k");
}

TEST_CASE("runtime migration preserves valid enabled osu chart filters") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.enable_osu_charts = true;
    config.mode.format = "osu";
    config.mode.key_mode = "7k";

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK_FALSE(changed);
    CHECK(config.mode.enable_osu_charts);
    CHECK(config.mode.format == "osu");
    CHECK(config.mode.key_mode == "7k");
}

TEST_CASE("runtime migration upgrades the legacy bad judge window default to 200ms") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.judge.bd_ms = 80.0;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.judge.bd_ms == doctest::Approx(200.0));
}

TEST_CASE("runtime migration upgrades legacy default gauge deltas to the harsher table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.13752, 0.09144, 0.02304, -1.94425, -3.88850};
    config.gauge.normal = {0.23123, 0.15438, 0.03877, -1.54583, -3.11025};
    config.gauge.easy = {0.30664, 0.20443, 0.05143, -1.16116, -2.32232};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(0.03666667));
    CHECK(config.gauge.hard.gr == doctest::Approx(0.02444444));
    CHECK(config.gauge.hard.gd == doctest::Approx(0.00611111));
    CHECK(config.gauge.hard.bd == doctest::Approx(-5.50000));
    CHECK(config.gauge.hard.pr == doctest::Approx(-7.50000));
    CHECK(config.gauge.normal.pg == doctest::Approx(0.05238095));
    CHECK(config.gauge.normal.gr == doctest::Approx(0.03492063));
    CHECK(config.gauge.normal.gd == doctest::Approx(0.00873016));
    CHECK(config.gauge.normal.bd == doctest::Approx(-2.75000));
    CHECK(config.gauge.normal.pr == doctest::Approx(-3.75000));
    CHECK(config.gauge.easy.pg == doctest::Approx(0.10000000));
    CHECK(config.gauge.easy.gr == doctest::Approx(0.06666667));
    CHECK(config.gauge.easy.gd == doctest::Approx(0.01666667));
    CHECK(config.gauge.easy.bd == doctest::Approx(-2.06250));
    CHECK(config.gauge.easy.pr == doctest::Approx(-2.81250));
}

TEST_CASE("runtime migration upgrades the previous gauge defaults to the latest recovery and bd/pr losses") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.06120, 0.04069, 0.01025, -2.33310, -4.27735};
    config.gauge.normal = {0.10290, 0.06870, 0.01725, -1.85500, -3.42128};
    config.gauge.easy = {0.13645, 0.09097, 0.02289, -1.39339, -2.55455};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(0.03666667));
    CHECK(config.gauge.hard.gr == doctest::Approx(0.02444444));
    CHECK(config.gauge.hard.gd == doctest::Approx(0.00611111));
    CHECK(config.gauge.hard.bd == doctest::Approx(-5.50000));
    CHECK(config.gauge.hard.pr == doctest::Approx(-7.50000));
    CHECK(config.gauge.normal.pg == doctest::Approx(0.05238095));
    CHECK(config.gauge.normal.gr == doctest::Approx(0.03492063));
    CHECK(config.gauge.normal.gd == doctest::Approx(0.00873016));
    CHECK(config.gauge.normal.bd == doctest::Approx(-2.75000));
    CHECK(config.gauge.normal.pr == doctest::Approx(-3.75000));
    CHECK(config.gauge.easy.pg == doctest::Approx(0.10000000));
    CHECK(config.gauge.easy.gr == doctest::Approx(0.06666667));
    CHECK(config.gauge.easy.gd == doctest::Approx(0.01666667));
    CHECK(config.gauge.easy.bd == doctest::Approx(-2.06250));
    CHECK(config.gauge.easy.pr == doctest::Approx(-2.81250));
}

TEST_CASE("runtime migration upgrades the immediate prior gauge defaults to the latest recovery and bd/pr losses") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01576, 0.01048, 0.00264, -3.84962, -7.05763};
    config.gauge.normal = {0.02650, 0.01769, 0.00444, -3.06075, -5.64511};
    config.gauge.easy = {0.03514, 0.02342, 0.00589, -2.29909, -4.21501};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(-5.50000));
    CHECK(config.gauge.hard.pr == doctest::Approx(-7.50000));
    CHECK(config.gauge.normal.bd == doctest::Approx(-2.75000));
    CHECK(config.gauge.normal.pr == doctest::Approx(-3.75000));
    CHECK(config.gauge.easy.bd == doctest::Approx(-2.06250));
    CHECK(config.gauge.easy.pr == doctest::Approx(-2.81250));
}

TEST_CASE("runtime migration upgrades the last shipped gauge defaults to the latest recovery and bd/pr losses") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01576, 0.01048, 0.00264, -8.84962, -7.05763};
    config.gauge.normal = {0.02650, 0.01769, 0.00444, -5.56075, -5.64511};
    config.gauge.easy = {0.03514, 0.02342, 0.00589, -4.04909, -4.21501};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(0.03666667));
    CHECK(config.gauge.normal.pg == doctest::Approx(0.05238095));
    CHECK(config.gauge.easy.pg == doctest::Approx(0.10000000));
    CHECK(config.gauge.hard.bd == doctest::Approx(-5.50000));
    CHECK(config.gauge.hard.pr == doctest::Approx(-7.50000));
    CHECK(config.gauge.normal.bd == doctest::Approx(-2.75000));
    CHECK(config.gauge.normal.pr == doctest::Approx(-3.75000));
    CHECK(config.gauge.easy.bd == doctest::Approx(-2.06250));
    CHECK(config.gauge.easy.pr == doctest::Approx(-2.81250));
}

TEST_CASE("runtime migration upgrades the interim bd/pr-only gauge defaults to the latest recovery table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01576, 0.01048, 0.00264, -5.50000, -7.50000};
    config.gauge.normal = {0.02650, 0.01769, 0.00444, -5.50000, -7.50000};
    config.gauge.easy = {0.03514, 0.02342, 0.00589, -5.50000, -7.50000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(0.03666667));
    CHECK(config.gauge.hard.gr == doctest::Approx(0.02444444));
    CHECK(config.gauge.hard.gd == doctest::Approx(0.00611111));
    CHECK(config.gauge.normal.pg == doctest::Approx(0.05238095));
    CHECK(config.gauge.normal.gr == doctest::Approx(0.03492063));
    CHECK(config.gauge.normal.gd == doctest::Approx(0.00873016));
    CHECK(config.gauge.easy.pg == doctest::Approx(0.10000000));
    CHECK(config.gauge.easy.gr == doctest::Approx(0.06666667));
    CHECK(config.gauge.easy.gd == doctest::Approx(0.01666667));
}

TEST_CASE("runtime migration upgrades the immediate prior shared normal and easy penalties") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.03666667, 0.02444444, 0.00611111, -5.50000, -7.50000};
    config.gauge.normal = {0.05238095, 0.03492063, 0.00873016, -5.50000, -7.50000};
    config.gauge.easy = {0.10000000, 0.06666667, 0.01666667, -5.50000, -7.50000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(-5.50000));
    CHECK(config.gauge.hard.pr == doctest::Approx(-7.50000));
    CHECK(config.gauge.normal.bd == doctest::Approx(-2.75000));
    CHECK(config.gauge.normal.pr == doctest::Approx(-3.75000));
    CHECK(config.gauge.easy.bd == doctest::Approx(-2.06250));
    CHECK(config.gauge.easy.pr == doctest::Approx(-2.81250));
}

TEST_CASE("config normalizes invalid keysound policy to follow") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"audio\": {\n"
               "    \"bms_keysound_policy\": \"weird\"\n"
               "  }\n"
               "}\n");
    write_file(temp.path / "profiles" / "test" / "config.json", "{ }\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    const auto result = loader.load_profile("profiles/test");

    REQUIRE(result.success());
    CHECK(result.config.audio_ui.bms_keysound_policy == "follow");
}

TEST_CASE("runtime migration upgrades legacy hold and debounce defaults") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.judge.hold_grace_ms = 20.0;
    config.judge.hold_break_ms = 50.0;
    config.input.debounce_ms = 5.0;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.judge.hold_grace_ms == doctest::Approx(35.0));
    CHECK(config.judge.hold_break_ms == doctest::Approx(100.0));
    CHECK(config.input.debounce_ms == doctest::Approx(8.0));
}
