#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "app/PersistedRuntimeConfig.h"
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

constexpr double kCurrentHardGd = 0.01;
constexpr double kCurrentNormalGd = 0.01;
constexpr double kCurrentEasyGd = 0.01;
constexpr double kCurrentExHardPg = 0.08;
constexpr double kCurrentHardPg = 0.16;
constexpr double kCurrentNormalPg = 0.19;
constexpr double kCurrentEasyPg = 0.25;
constexpr double kCurrentExHardGr = 0.04;
constexpr double kCurrentHardGr = 0.09;
constexpr double kCurrentNormalGr = 0.15;
constexpr double kCurrentEasyGr = 0.20;
constexpr double kCurrentExHardGd = 0.0;
constexpr double kCurrentExHardBd = -18.0;
constexpr double kCurrentHardBd = -10.0;
constexpr double kCurrentNormalBd = -6.25;
constexpr double kCurrentEasyBd = -4.1;
constexpr double kCurrentExHardPr = -4.0;
constexpr double kCurrentHardPr = -2.0;
constexpr double kCurrentNormalPr = -2.0;
constexpr double kCurrentEasyPr = -1.6;

}  // namespace

TEST_CASE("config defaults prefer 44100 Hz audio") {
    ConfigLoader loader;
    const auto config = loader.defaults();

    CHECK(config.audio.sample_rate == 44100);
    CHECK(config.audio_ui.preset == "high");
    CHECK(config.audio_ui.background_sound_enabled);
    CHECK(config.audio_ui.bgm_volume == doctest::Approx(0.75));
    CHECK(config.audio_ui.keysound_volume == doctest::Approx(1.0));
    CHECK(config.input.rawinput);
    CHECK(config.input.backend == "rawinput");
    CHECK(config.input.polling_hz == 1000);
    CHECK(config.input.judgement_hz == 4000);
    CHECK(config.input.debounce_ms == doctest::Approx(8.0));
    CHECK_FALSE(config.mode.ghost_battle_enabled);
    CHECK(config.mode.pacemaker_mode == "off");
    CHECK(config.mode.pacemaker_target_accuracy == doctest::Approx(90.0));
    CHECK(config.mode.pacemaker_target_score == 8000);
    CHECK(config.mode.song_index_profile == "safe");
    CHECK_FALSE(config.mode.calculate_song_index_difficulty);
    CHECK(config.mode.key_conversion_algorithm == "krrcream");
    CHECK(config.mode.key_conversion_nk2_preset == "native");
    CHECK(config.graphics.resolution == "native");
    CHECK(config.graphics.display_mode == "borderless");
    CHECK(tenriff::config::kJudgementLinePositionMin == doctest::Approx(0.0));
    CHECK(tenriff::config::kJudgementLinePositionMax == doctest::Approx(1.0));
    CHECK_FALSE(config.graphics.vsync);
    CHECK(config.graphics.refresh_hz == -1);
    CHECK(config.graphics.bga_enabled);
    CHECK(config.graphics.background_upscale_mode == "off");
    CHECK(config.graphics.background_upscale_model_path.empty());
    CHECK_FALSE(config.graphics.background_upscale_prefer_npu);
    CHECK(tenriff::config::normalize_background_upscale_mode("onnx") == "onnx");
    CHECK(tenriff::config::normalize_background_upscale_mode("lunasr") == "onnx");
    CHECK(tenriff::config::normalize_background_upscale_mode("native") == "off");
    CHECK(tenriff::config::normalize_background_upscale_mode("unexpected") == "off");
    CHECK(config.gauge.ex_hard.pg == doctest::Approx(kCurrentExHardPg));
    CHECK(config.gauge.ex_hard.gr == doctest::Approx(kCurrentExHardGr));
    CHECK(config.gauge.ex_hard.gd == doctest::Approx(kCurrentExHardGd));
    CHECK(config.gauge.ex_hard.bd == doctest::Approx(kCurrentExHardBd));
    CHECK(config.gauge.ex_hard.pr == doctest::Approx(kCurrentExHardPr));
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.hard.gd == doctest::Approx(kCurrentHardGd));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.normal.gd == doctest::Approx(kCurrentNormalGd));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(config.gauge.easy.gd == doctest::Approx(kCurrentEasyGd));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
    CHECK(config.judge.pg_ms == doctest::Approx(20.0));
    CHECK(config.judge.gr_ms == doctest::Approx(65.0));
    CHECK(config.judge.gd_ms == doctest::Approx(115.0));
    CHECK(config.judge.bd_ms == doctest::Approx(210.0));
    CHECK(config.judge.indirect_miss_ms == doctest::Approx(210.0));
    CHECK(config.judge.hold_grace_ms == doctest::Approx(80.0));
    CHECK(config.judge.hold_break_ms == doctest::Approx(200.0));
    CHECK(config.skin.note_shape == "rect");
    CHECK(config.skin.source == "native");
    CHECK(config.skin.tenriff_skin_name.empty());
    CHECK(config.skin.lr2_skin_name.empty());
    CHECK(config.skin.lr2_resolution_mode == "auto");
    CHECK(config.skin.visual_preset == "tenriff");
    CHECK(config.skin.note_border_enabled);
    CHECK_FALSE(config.skin.preserve_note_image_aspect_ratio);
    CHECK(config.skin.note_image_aspect == "stretch");
    CHECK(config.skin.note_divider_gap_px == doctest::Approx(12.0));
    CHECK(config.skin.show_lane_dividers);
    CHECK(config.skin.show_judgement_line);
    CHECK(config.skin.show_timing_feedback);
    CHECK_FALSE(config.skin.show_gear_boundary_line);
    CHECK_FALSE(config.skin.show_hold_tail);
    CHECK_FALSE(config.skin.hold_tail_taper_enabled);
    CHECK(config.skin.judgement_line_glow_enabled);
    CHECK(config.skin.key_pulse_enabled);
    CHECK(config.skin.hit_burst_style == "prism");
    CHECK(config.skin.key_label_position == "bottom");
    CHECK(config.skin.gameplay_field_offset_x ==
          doctest::Approx(tenriff::config::kGameplayFieldOffsetXDefault));
    CHECK(config.skin.combo_position == doctest::Approx(tenriff::config::kComboPositionDefault));
    CHECK(config.skin.lane_background_opacity == doctest::Approx(tenriff::config::kSkinLaneBackgroundOpacityDefault));
    CHECK(config.skin.black_playfield_enabled);
    CHECK(config.skin.visual_opacity == doctest::Approx(tenriff::config::kSkinVisualOpacityDefault));
    CHECK(config.skin.note_outline_opacity == doctest::Approx(tenriff::config::kSkinNoteOutlineOpacityDefault));
    CHECK(config.skin.hold_body_opacity == doctest::Approx(tenriff::config::kSkinHoldBodyOpacityDefault));
    CHECK(config.skin.note_height_scale == doctest::Approx(1.80));
    CHECK(config.skin.lane_divider_width_scale == doctest::Approx(1.0));
    CHECK(config.skin.hold_body_width_scale == doctest::Approx(1.00));
    const auto default_lane_widths_10k = tenriff::config::resolved_skin_lane_width_scales(config.skin, "10k");
    REQUIRE(default_lane_widths_10k.size() == 10u);
    CHECK(default_lane_widths_10k[0] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault));
    CHECK(default_lane_widths_10k[9] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault));
    const auto default_lane_spacing_10k = tenriff::config::resolved_skin_lane_spacing_scales(config.skin, "10k");
    REQUIRE(default_lane_spacing_10k.size() == 9u);
    CHECK(default_lane_spacing_10k[0] == doctest::Approx(tenriff::config::kLaneSpacingScaleDefault));
    CHECK(default_lane_spacing_10k[8] == doctest::Approx(tenriff::config::kLaneSpacingScaleDefault));
    CHECK(config.ui.profile_nickname.empty());
    CHECK(config.ui.profile_avatar_path.empty());
    CHECK(config.ui.language == "en");
    CHECK(config.ui.result_tail_ms == doctest::Approx(3000.0));
    CHECK(config.ui.favorite_chart_keys.empty());
    CHECK(config.ui.collections.empty());
    CHECK(config.ui.song_collection_filter == "all");
    CHECK(config.ui.difficulty_table_path.empty());
    CHECK(config.ui.difficulty_table_url.empty());
    CHECK(tenriff::config::resolved_skin_lane_colors(config.skin, "16k").size() == 16u);
}

TEST_CASE("config save and load preserve favorites and collections") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.ui.favorite_chart_keys = {"songA", "songB"};
    config.ui.collections = {
        {"Favorites+", {"songA"}},
        {"Practice", {"songB", "songC"}},
    };
    config.ui.song_collection_filter = "Practice";
    config.ui.profile_nickname = "Luna Pilot";
    config.ui.profile_avatar_path = "D:/avatars/luna.png";
    config.ui.difficulty_table_path = "tables/insane.json";
    config.ui.difficulty_table_url = "https://example.test/insane/";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.ui.favorite_chart_keys == config.ui.favorite_chart_keys);
    CHECK(result.config.ui.collections == config.ui.collections);
    CHECK(result.config.ui.song_collection_filter == "Practice");
    CHECK(result.config.ui.profile_nickname == "Luna Pilot");
    CHECK(result.config.ui.profile_avatar_path == "D:/avatars/luna.png");
    CHECK(result.config.ui.difficulty_table_path == "tables/insane.json");
    CHECK(result.config.ui.difficulty_table_url == "https://example.test/insane/");

}
TEST_CASE("profile nickname normalization is UI-safe and UTF-8 bounded") {
    CHECK(tenriff::config::normalize_profile_nickname("  Luna\n\tPilot  ") ==
          "Luna Pilot");
    const std::string long_korean = u8"가나다라마바사아자차카타파하가나다라";
    const std::string normalized = tenriff::config::normalize_profile_nickname(long_korean);
    CHECK(normalized == u8"가나다라마바사아자차카타파하가나");
}

TEST_CASE("profile avatar path normalization is UI-safe") {
    CHECK(tenriff::config::normalize_profile_avatar_path("  D:/avatars/luna.png\n") ==
          "D:/avatars/luna.png");
}

TEST_CASE("config load folds deprecated indirect miss into the bad window") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.judge.bd_ms = 310.0;
    config.judge.indirect_miss_ms = 640.0;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.judge.bd_ms == doctest::Approx(310.0));
    CHECK(result.config.judge.indirect_miss_ms == doctest::Approx(310.0));
}

TEST_CASE("config load migrates an existing stale profile before returning it") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json", "{ }\n");
    write_file(temp.path / "profiles" / "test" / "config.json",
               "{\n"
               "  \"judge\": {\n"
               "    \"gd\": 55.0,\n"
               "    \"bd\": 200.0,\n"
               "    \"hold_grace\": 20.0,\n"
               "    \"hold_break\": 50.0\n"
               "  },\n"
               "  \"gauge\": {\n"
               "    \"hard_to_normal_threshold\": 66.0,\n"
               "    \"normal_to_easy_threshold\": 33.0,\n"
               "    \"delta\": {\n"
               "      \"hard\": {\"PG\": 0.01576, \"GR\": 0.01048, \"GD\": 0.00264, \"BD\": -8.84962, \"PR\": -7.05763},\n"
               "      \"normal\": {\"PG\": 0.02650, \"GR\": 0.01769, \"GD\": 0.00444, \"BD\": -5.56075, \"PR\": -5.64511},\n"
               "      \"easy\": {\"PG\": 0.03514, \"GR\": 0.02342, \"GD\": 0.00589, \"BD\": -4.04909, \"PR\": -4.21501}\n"
               "    }\n"
               "  },\n"
               "  \"graphics\": {\n"
               "    \"display_mode\": \"borderless\",\n"
               "    \"resolution\": \"native\",\n"
               "    \"vsync\": true,\n"
               "    \"refresh_hz\": 1050,\n"
               "    \"performance_overlay\": false\n"
               "  },\n"
               "  \"ui\": {\n"
               "    \"result_tail_ms\": 500.0\n"
               "  },\n"
               "  \"skin\": {\n"
               "    \"note_height_scale\": 1.0\n"
               "  }\n"
               "}\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    const auto result = loader.load_profile("profiles/test");

    REQUIRE(result.success());
    CHECK_FALSE(result.used_defaults);
    CHECK(result.migrated);
    CHECK(result.config.judge.gd_ms == doctest::Approx(115.0));
    CHECK(result.config.judge.bd_ms == doctest::Approx(210.0));
    CHECK(result.config.judge.hold_grace_ms == doctest::Approx(80.0));
    CHECK(result.config.judge.hold_break_ms == doctest::Approx(200.0));
    CHECK(result.config.gauge.ex_hard.pg == doctest::Approx(kCurrentExHardPg));
    CHECK(result.config.gauge.ex_hard.gr == doctest::Approx(kCurrentExHardGr));
    CHECK(result.config.gauge.ex_hard.gd == doctest::Approx(kCurrentExHardGd));
    CHECK(result.config.gauge.ex_hard.bd == doctest::Approx(kCurrentExHardBd));
    CHECK(result.config.gauge.ex_hard.pr == doctest::Approx(kCurrentExHardPr));
    CHECK(result.config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(result.config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(result.config.gauge.hard.gd == doctest::Approx(kCurrentHardGd));
    CHECK(result.config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(result.config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(result.config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(result.config.gauge.normal.gd == doctest::Approx(kCurrentNormalGd));
    CHECK(result.config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(result.config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(result.config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(result.config.gauge.easy.gd == doctest::Approx(kCurrentEasyGd));
    CHECK(result.config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(result.config.ui.result_tail_ms == doctest::Approx(3000.0));
    CHECK(result.config.skin.note_height_scale == doctest::Approx(1.80));
    CHECK_FALSE(result.config.graphics.vsync);
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
    config.input.polling_hz = 2000;
    config.input.judgement_hz = 8000;
    config.input.debounce_ms = 12.0;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.input.polling_hz == 2000);
    CHECK(result.config.input.judgement_hz == 8000);
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
    config.audio_ui.background_sound_enabled = false;
    config.audio_ui.master_volume = 0.65;
    config.audio_ui.bgm_volume = 0.55;
    config.audio_ui.keysound_volume = 1.35;
    config.speed.rate = 1.25;
    config.speed.hi_speed = 4.75;
    config.mode.ghost_battle_enabled = true;
    config.mode.pacemaker_mode = "accuracy";
    config.mode.pacemaker_target_accuracy = 97.5;
    config.mode.pacemaker_target_score = 9200;
    config.mode.song_index_profile = "fast";
    config.mode.calculate_song_index_difficulty = true;
    config.mode.key_conversion_algorithm = "nk2";
    config.mode.key_conversion_nk2_preset = "transform";
    config.mode.gauge = "shift";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK_FALSE(result.config.audio_ui.background_sound_enabled);
    CHECK(result.config.audio_ui.master_volume == doctest::Approx(0.65));
    CHECK(result.config.audio_ui.bgm_volume == doctest::Approx(0.55));
    CHECK(result.config.audio_ui.keysound_volume == doctest::Approx(1.35));
    CHECK(result.config.speed.rate == doctest::Approx(1.25));
    CHECK(result.config.speed.hi_speed == doctest::Approx(4.75));
    CHECK(result.config.mode.ghost_battle_enabled);
    CHECK(result.config.mode.pacemaker_mode == "accuracy");
    CHECK(result.config.mode.pacemaker_target_accuracy == doctest::Approx(97.5));
    CHECK(result.config.mode.pacemaker_target_score == 9200);
    CHECK(result.config.mode.song_index_profile == "fast");
    CHECK(result.config.mode.calculate_song_index_difficulty);
    CHECK(result.config.mode.key_conversion_algorithm == "nk2");
    CHECK(result.config.mode.key_conversion_nk2_preset == "transform");
    CHECK(result.config.mode.gauge == "shift");
}

TEST_CASE("config save and load normalize mode mods") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.mods = {"judge_hard", "Judge Easy", "full_short_notes",
                        "ln_mix_30", "judge_easy", "mystery"};

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    REQUIRE(result.config.mode.mods.size() == 2u);
    CHECK(result.config.mode.mods[0] == "judge_easy");
    CHECK(result.config.mode.mods[1] == "ln_mix_30");
}

TEST_CASE("persisted runtime config strips session-only judge mods") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.mods = {"judge_easy", "no_ln_release", "judge_hard", "full_long_notes"};

    const auto persisted = tenriff::app::build_persisted_runtime_config(config);

    REQUIRE(persisted.mode.mods.size() == 2u);
    CHECK(persisted.mode.mods[0] == "full_long_notes");
    CHECK(persisted.mode.mods[1] == "no_ln_release");
}

TEST_CASE("persisted input backend config does not leak replay session mode changes") {
    ConfigLoader loader;
    auto persisted_base = loader.defaults();
    persisted_base.mode.key_mode = "7k";
    persisted_base.mode.random = "off";
    persisted_base.mode.gauge = "hard";
    persisted_base.input.backend = "polling";
    persisted_base.input.rawinput = true;

    auto runtime_source = persisted_base;
    runtime_source.mode.key_mode = "10k";
    runtime_source.mode.random = "super_random";
    runtime_source.mode.gauge = "easy";
    runtime_source.mode.mods = {"judge_easy", "full_long_notes"};
    runtime_source.input.backend = "polling";
    runtime_source.input.rawinput = false;

    const auto persisted =
        tenriff::app::build_persisted_input_backend_config(persisted_base, runtime_source);

    CHECK(persisted.input.backend == "polling");
    CHECK_FALSE(persisted.input.rawinput);
    CHECK(persisted.mode.key_mode == "7k");
    CHECK(persisted.mode.random == "off");
    CHECK(persisted.mode.gauge == "hard");
    REQUIRE(persisted.mode.mods.empty());
}

TEST_CASE("config save and load preserve rawinput backend when enabled") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.input.backend = "rawinput";
    config.input.rawinput = true;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.input.rawinput);
    CHECK(result.config.input.backend == "rawinput");
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
    config.graphics.vsync = true;
    config.graphics.refresh_hz = -1;
    config.graphics.performance_overlay = true;
    config.graphics.bga_enabled = false;
    config.graphics.background_upscale_mode = "onnx";
    config.graphics.background_upscale_model_path = "models/custom-upscaler.onnx";
    config.graphics.background_upscale_prefer_npu = false;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.graphics.display_mode == "windowed");
    CHECK(result.config.graphics.resolution == "qhd");
    CHECK(result.config.graphics.vsync);
    CHECK(result.config.graphics.refresh_hz == -1);
    CHECK(result.config.graphics.performance_overlay);
    CHECK_FALSE(result.config.graphics.bga_enabled);
    CHECK(result.config.graphics.background_upscale_mode == "onnx");
    CHECK(result.config.graphics.background_upscale_model_path == "models/custom-upscaler.onnx");
    CHECK_FALSE(result.config.graphics.background_upscale_prefer_npu);
}

TEST_CASE("config save and load preserve unlimited graphics refresh") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.graphics.refresh_hz = 0;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.graphics.refresh_hz == 0);
}

TEST_CASE("config save and load preserve ui language setting") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.ui.language = "ko";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.ui.language == "ko");
}

TEST_CASE("config load normalizes invalid ui language to english") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"ui\": {\n"
               "    \"language\": \"mystery\"\n"
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
    CHECK(result.config.ui.language == "en");
}

TEST_CASE("config save and load normalize unsupported skin sources to native") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.source = "unsupported";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.source == "native");
    CHECK(result.config.skin.lr2_skin_name.empty());
}

TEST_CASE("config save and load preserve TenRiff skin selection") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.source = "tenriff";
    config.skin.tenriff_skin_name = "Aurora-Glass";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.source == "tenriff");
    CHECK(result.config.skin.tenriff_skin_name == "Aurora-Glass");
    CHECK(tenriff::config::normalize_skin_source_token("trskin") == "tenriff");
}
TEST_CASE("config save and load preserve lr2 skin selection") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.source = "lr2";
    config.skin.lr2_skin_name = "BlueWhite";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.source == "lr2");
    CHECK(result.config.skin.lr2_skin_name == "BlueWhite");
}

TEST_CASE("config save and load preserve lr2 resolution mode") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.source = "lr2";
    config.skin.lr2_resolution_mode = "FHD";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.source == "lr2");
    CHECK(result.config.skin.lr2_resolution_mode == "fhd");
}

TEST_CASE("config save and load preserve hit burst brightness") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.key_pulse_brightness = 0.35;
    config.skin.hit_burst_style = "spark";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.key_pulse_brightness == doctest::Approx(0.35));
    CHECK(result.config.skin.key_pulse_enabled);
    CHECK(result.config.skin.hit_burst_style == "spark");
    CHECK(tenriff::config::normalize_skin_hit_burst_style_token("circle") == "ring");
    CHECK(tenriff::config::normalize_skin_hit_burst_style_token("unknown") == "prism");

    // Zero brightness is the off state, and the legacy flag mirrors it.
    config.skin.key_pulse_brightness = 0.0;
    REQUIRE(loader.save_profile("profiles/off", config, &error));
    const auto off = loader.load_profile("profiles/off");
    REQUIRE(off.success());
    CHECK(off.config.skin.key_pulse_brightness == doctest::Approx(0.0));
    CHECK_FALSE(off.config.skin.key_pulse_enabled);

    // Clearing only the legacy flag still turns the burst off.
    config.skin.key_pulse_brightness = 0.8;
    config.skin.key_pulse_enabled = false;
    REQUIRE(loader.save_profile("profiles/legacy", config, &error));
    const auto legacy = loader.load_profile("profiles/legacy");
    REQUIRE(legacy.success());
    CHECK(legacy.config.skin.key_pulse_brightness == doctest::Approx(0.0));
    CHECK_FALSE(legacy.config.skin.key_pulse_enabled);
}

TEST_CASE("config save and load preserve skin visual preset controls") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.visual_preset = "neon";
    config.skin.lane_background_opacity = 0.25;
    config.skin.black_playfield_enabled = false;
    config.skin.visual_opacity = 0.85;
    config.skin.note_outline_opacity = 0.55;
    config.skin.hold_body_opacity = 0.20;
    config.skin.judgement_line_glow_enabled = true;
    config.skin.key_pulse_enabled = false;
    config.skin.key_label_position = "top";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.visual_preset == "neon");
    CHECK(result.config.skin.lane_background_opacity == doctest::Approx(0.25));
    CHECK_FALSE(result.config.skin.black_playfield_enabled);
    CHECK(result.config.skin.visual_opacity == doctest::Approx(0.85));
    CHECK(result.config.skin.note_outline_opacity == doctest::Approx(0.55));
    CHECK(result.config.skin.hold_body_opacity == doctest::Approx(0.20));
    CHECK(result.config.skin.judgement_line_glow_enabled);
    CHECK_FALSE(result.config.skin.key_pulse_enabled);
    CHECK(result.config.skin.key_label_position == "top");
}

TEST_CASE("skin visual presets apply their bundled gameplay appearance") {
    ConfigLoader loader;
    auto config = loader.defaults();

    tenriff::config::apply_skin_visual_preset(config.skin, "minimal");

    CHECK(config.skin.visual_preset == "minimal");
    CHECK_FALSE(config.skin.show_lane_dividers);
    CHECK_FALSE(config.skin.judgement_line_glow_enabled);
    CHECK_FALSE(config.skin.key_pulse_enabled);
    CHECK(config.skin.key_label_position == "top");
    CHECK(config.skin.visual_opacity == doctest::Approx(0.82));
    CHECK(config.skin.hold_body_opacity == doctest::Approx(1.00));

    tenriff::config::apply_skin_visual_preset(config.skin, "unknown");

    CHECK(config.skin.visual_preset == "tenriff");
    CHECK(config.skin.show_lane_dividers);
    CHECK(config.skin.judgement_line_glow_enabled);
    CHECK(config.skin.key_pulse_enabled);
    CHECK(config.skin.key_label_position == "bottom");
}

TEST_CASE("config load normalizes invalid lr2 resolution mode") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"skin\": {\n"
               "    \"source\": \"lr2\",\n"
               "    \"lr2_resolution_mode\": \"mystery\"\n"
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
    CHECK(result.config.skin.source == "lr2");
    CHECK(result.config.skin.lr2_resolution_mode == "auto");
}

TEST_CASE("config save and load use shared lane divider width scaling") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.lane_divider_width_scale = 0.75;
    config.skin.lane_divider_width_scales["10k"] = 1.35;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.lane_divider_width_scale == doctest::Approx(0.75));
    CHECK(tenriff::config::resolved_skin_lane_divider_width_scale(result.config.skin, "10k") == doctest::Approx(0.75));
    CHECK(tenriff::config::resolved_skin_lane_divider_width_scale(result.config.skin, "7k") == doctest::Approx(0.75));
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
    CHECK(result.config.graphics.refresh_hz == -1);
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
    config.skin.note_image_aspect = "width";
    config.skin.show_lane_dividers = false;
    config.skin.show_judgement_line = false;
    config.skin.show_timing_feedback = false;
    config.skin.show_gear_boundary_line = true;
    config.skin.show_hold_tail = false;
    config.skin.hold_tail_taper_enabled = true;
    config.skin.judgement_line_position = 0.0;
    config.skin.gameplay_field_offset_x = -275.0;
    config.skin.combo_position = 0.52;
    config.skin.lane_width_scales["4k"] = {0.75, 1.10, 1.10, 0.75};
    config.skin.note_width_scale = 1.15;
    config.skin.lane_spacing_scales["4k"] = {0.10, 0.25, 0.10};
    config.skin.hold_body_width_scale = 1.10;
    config.skin.note_height_scale = 1.35;
    config.skin.lane_center_gap_scale = 0.25;
    config.skin.note_width_scales["4k"] = 0.85;
    config.skin.note_width_scales["16k"] = 0.65;
    config.skin.note_height_scales["4k"] = 1.20;
    config.skin.note_height_scales["16k"] = 1.70;
    config.skin.lane_center_gap_scales["16k"] = 1.10;
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
    CHECK(result.config.skin.note_image_aspect == "width");
    // The legacy boolean is written in step so older builds still see a sane value.
    CHECK(result.config.skin.preserve_note_image_aspect_ratio);
    CHECK_FALSE(result.config.skin.show_lane_dividers);
    CHECK_FALSE(result.config.skin.show_judgement_line);
    CHECK_FALSE(result.config.skin.show_timing_feedback);
    CHECK(result.config.skin.show_gear_boundary_line);
    CHECK_FALSE(result.config.skin.show_hold_tail);
    CHECK(result.config.skin.hold_tail_taper_enabled);
    CHECK(result.config.skin.judgement_line_position == doctest::Approx(0.0));
    CHECK(result.config.skin.gameplay_field_offset_x == doctest::Approx(-275.0));
    CHECK(result.config.skin.combo_position == doctest::Approx(0.52));
    const auto saved_lane_widths_4k = tenriff::config::resolved_skin_lane_width_scales(result.config.skin, "4k");
    REQUIRE(saved_lane_widths_4k.size() == 4u);
    CHECK(saved_lane_widths_4k[0] == doctest::Approx(0.75));
    CHECK(saved_lane_widths_4k[1] == doctest::Approx(1.10));
    CHECK(result.config.skin.note_width_scale == doctest::Approx(1.15));
    const auto saved_lane_spacing_4k = tenriff::config::resolved_skin_lane_spacing_scales(result.config.skin, "4k");
    REQUIRE(saved_lane_spacing_4k.size() == 3u);
    CHECK(saved_lane_spacing_4k[0] == doctest::Approx(0.10));
    CHECK(saved_lane_spacing_4k[1] == doctest::Approx(0.25));
    CHECK(result.config.skin.hold_body_width_scale == doctest::Approx(1.10));
    CHECK(result.config.skin.note_height_scale == doctest::Approx(1.35));
    CHECK(result.config.skin.lane_center_gap_scale == doctest::Approx(0.25));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "4k") == doctest::Approx(0.85));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "16k") == doctest::Approx(0.65));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "10k") == doctest::Approx(1.15));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "4k") == doctest::Approx(1.20));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "16k") == doctest::Approx(1.70));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "10k") == doctest::Approx(1.35));
    CHECK(tenriff::config::resolved_skin_lane_center_gap_scale(result.config.skin, "16k") == doctest::Approx(1.10));
    CHECK(tenriff::config::resolved_skin_lane_center_gap_scale(result.config.skin, "10k") == doctest::Approx(0.25));
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

TEST_CASE("skin note shape normalization supports polygon choices") {
    CHECK(tenriff::config::normalize_skin_note_shape_token("triangle") == "triangle");
    CHECK(tenriff::config::normalize_skin_note_shape_token("pentagon") == "pentagon");
    CHECK(tenriff::config::normalize_skin_note_shape_token("hexagon") == "hexagon");
    CHECK(tenriff::config::normalize_skin_note_shape_token("rectangle") == "rect");
    CHECK(tenriff::config::normalize_skin_note_shape_token("star") == "rect");
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
               "    \"note_shape\": \"star\",\n"
               "    \"note_border_enabled\": false,\n"
               "    \"judgement_line_position\": 1.5,\n"
               "    \"gameplay_field_offset_x\": 9999.0,\n"
               "    \"combo_position\": 9.0,\n"
               "    \"lane_width_scales\": {\n"
               "      \"4k\": [0.1, 5.0, 1.2],\n"
               "      \"16k\": [9.0]\n"
               "    },\n"
               "    \"note_width_scale\": 9.0,\n"
               "    \"lane_spacing_scales\": {\n"
               "      \"4k\": [0.1, 9.0, -1.0],\n"
               "      \"16k\": [9.0]\n"
               "    },\n"
               "    \"hold_body_width_scale\": 9.0,\n"
               "    \"note_height_scale\": 0.1,\n"
               "    \"lane_center_gap_scale\": 9.0,\n"
               "    \"note_width_scales\": {\n"
               "      \"4k\": 0.1,\n"
               "      \"16k\": 9.0\n"
               "    },\n"
               "    \"note_height_scales\": {\n"
               "      \"5k\": 0.1,\n"
               "      \"16k\": 9.0\n"
               "    },\n"
               "    \"lane_center_gap_scales\": {\n"
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
    CHECK(result.config.skin.gameplay_field_offset_x ==
          doctest::Approx(tenriff::config::kGameplayFieldOffsetXMax));
    CHECK(result.config.skin.combo_position == doctest::Approx(tenriff::config::kComboPositionMax));
    const auto clamped_lane_widths_4k = tenriff::config::resolved_skin_lane_width_scales(result.config.skin, "4k");
    REQUIRE(clamped_lane_widths_4k.size() == 4u);
    CHECK(clamped_lane_widths_4k[0] == doctest::Approx(tenriff::config::kLaneWidthScaleMin));
    CHECK(clamped_lane_widths_4k[1] == doctest::Approx(tenriff::config::kLaneWidthScaleMax));
    CHECK(clamped_lane_widths_4k[3] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault));
    CHECK(result.config.skin.note_width_scale == doctest::Approx(tenriff::config::kNoteWidthScaleMax));
    const auto clamped_lane_spacing_4k = tenriff::config::resolved_skin_lane_spacing_scales(result.config.skin, "4k");
    REQUIRE(clamped_lane_spacing_4k.size() == 3u);
    CHECK(clamped_lane_spacing_4k[0] == doctest::Approx(0.10));
    CHECK(clamped_lane_spacing_4k[1] == doctest::Approx(tenriff::config::kLaneSpacingScaleMax));
    CHECK(clamped_lane_spacing_4k[2] == doctest::Approx(tenriff::config::kLaneSpacingScaleMin));
    CHECK(result.config.skin.hold_body_width_scale == doctest::Approx(tenriff::config::kHoldBodyWidthScaleMax));
    CHECK(result.config.skin.note_height_scale == doctest::Approx(tenriff::config::kNoteHeightScaleMin));
    CHECK(result.config.skin.lane_center_gap_scale == doctest::Approx(tenriff::config::kLaneCenterGapScaleMax));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "4k") ==
          doctest::Approx(tenriff::config::kNoteWidthScaleMin));
    CHECK(tenriff::config::resolved_skin_note_width_scale(result.config.skin, "16k") ==
          doctest::Approx(tenriff::config::kNoteWidthScaleMax));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "5k") ==
          doctest::Approx(tenriff::config::kNoteHeightScaleMin));
    CHECK(tenriff::config::resolved_skin_note_height_scale(result.config.skin, "16k") ==
          doctest::Approx(tenriff::config::kNoteHeightScaleMax));
    CHECK(tenriff::config::resolved_skin_lane_center_gap_scale(result.config.skin, "16k") ==
          doctest::Approx(tenriff::config::kLaneCenterGapScaleMax));
    const auto clamped_5k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "5k");
    REQUIRE(clamped_5k.size() == 5u);
    CHECK(clamped_5k[0] == "ice");
    CHECK(clamped_5k[1] == "azure");
    const auto saved_10k = tenriff::config::resolved_skin_lane_colors(result.config.skin, "10k");
    REQUIRE(saved_10k.size() == 10u);
    CHECK(saved_10k[0] == "rose");
    CHECK(saved_10k[9] == "rose");
}

TEST_CASE("config clamps judgement line positions below zero") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.judgement_line_position = -0.25;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.skin.judgement_line_position ==
          doctest::Approx(tenriff::config::kJudgementLinePositionMin));
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
    config.ui.session_mix_lr2_course_path = "D:/Courses/GENOSIDE.lr2crs";

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.ui.active_song_source == "D:/Songs/PackB");
    CHECK(result.config.ui.session_mix_lr2_course_path == "D:/Courses/GENOSIDE.lr2crs");
    REQUIRE(result.config.ui.recent_song_sources.size() == 2u);
    CHECK(result.config.ui.recent_song_sources[0] == "D:/Songs/PackB");
    CHECK(result.config.ui.recent_song_sources[1] == "D:/Songs/PackA");
}

TEST_CASE("config save and load preserve gameplay mode flags") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.autoplay_enabled = true;
    config.mode.practice_no_fail_enabled = true;
    config.mode.one_miss_fail_enabled = true;

    std::string error;
    REQUIRE(loader.save_profile("profiles/test", config, &error));
    CHECK(error.empty());

    const auto result = loader.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.config.mode.autoplay_enabled);
    CHECK(result.config.mode.practice_no_fail_enabled);
    CHECK(result.config.mode.one_miss_fail_enabled);
}

TEST_CASE("bms-first runtime migration preserves valid keysound and key mode settings") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.audio_ui.preset = "basic";
    config.audio_ui.bms_keysound_policy = "autoplay";
    config.mode.key_mode = "none";
    config.mode.key_conversion_algorithm = "krrcream";

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK_FALSE(changed);
    CHECK(config.audio_ui.preset == "basic");
    CHECK(config.audio_ui.bms_keysound_policy == "autoplay");
    CHECK(config.mode.key_mode == "none");
    CHECK(config.mode.key_conversion_algorithm == "krrcream");
}

TEST_CASE("runtime migration normalizes KeyWeaver converter aliases") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.key_conversion_algorithm = "keyweaver";

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.mode.key_conversion_algorithm == "nk2");
}

TEST_CASE("runtime migration normalizes nK2 preset aliases and rejects invalid values") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.key_conversion_nk2_preset = "transform35";

    CHECK(tenriff::app::migrate_bms_first_runtime_config(config));
    CHECK(config.mode.key_conversion_nk2_preset == "transform");

    config.mode.key_conversion_nk2_preset = "unsupported";
    CHECK(tenriff::app::migrate_bms_first_runtime_config(config));
    CHECK(config.mode.key_conversion_nk2_preset == "native");
}

TEST_CASE("runtime migration replaces invalid key mode tokens with none") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.mode.key_mode = "mystery";

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.mode.key_mode == "none");
}

TEST_CASE("runtime migration upgrades old judge defaults into the current windows") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.judge.gd_ms = 55.0;
    config.judge.bd_ms = 95.0;
    config.judge.indirect_miss_ms = 500.0;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.judge.pg_ms == doctest::Approx(20.0));
    CHECK(config.judge.gr_ms == doctest::Approx(65.0));
    CHECK(config.judge.gd_ms == doctest::Approx(115.0));
    CHECK(config.judge.bd_ms == doctest::Approx(210.0));
    CHECK(config.judge.indirect_miss_ms == doctest::Approx(210.0));
}

TEST_CASE("runtime migration upgrades legacy default gauge deltas to the harsher table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.13752, 0.09144, 0.02304, -1.94425, -3.88850};
    config.gauge.normal = {0.23123, 0.15438, 0.03877, -1.54583, -3.11025};
    config.gauge.easy = {0.30664, 0.20443, 0.05143, -1.16116, -2.32232};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.hard.gd == doctest::Approx(kCurrentHardGd));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.normal.gd == doctest::Approx(kCurrentNormalGd));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(config.gauge.easy.gd == doctest::Approx(kCurrentEasyGd));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the previous gauge defaults to the latest recovery and bd/pr losses") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.06120, 0.04069, 0.01025, -2.33310, -4.27735};
    config.gauge.normal = {0.10290, 0.06870, 0.01725, -1.85500, -3.42128};
    config.gauge.easy = {0.13645, 0.09097, 0.02289, -1.39339, -2.55455};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.hard.gd == doctest::Approx(kCurrentHardGd));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.normal.gd == doctest::Approx(kCurrentNormalGd));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(config.gauge.easy.gd == doctest::Approx(kCurrentEasyGd));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the immediate prior gauge defaults to the latest recovery and bd/pr losses") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01576, 0.01048, 0.00264, -3.84962, -7.05763};
    config.gauge.normal = {0.02650, 0.01769, 0.00444, -3.06075, -5.64511};
    config.gauge.easy = {0.03514, 0.02342, 0.00589, -2.29909, -4.21501};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the last shipped gauge defaults to the latest recovery and bd/pr losses") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01576, 0.01048, 0.00264, -8.84962, -7.05763};
    config.gauge.normal = {0.02650, 0.01769, 0.00444, -5.56075, -5.64511};
    config.gauge.easy = {0.03514, 0.02342, 0.00589, -4.04909, -4.21501};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the interim bd/pr-only gauge defaults to the latest recovery table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01576, 0.01048, 0.00264, -5.50000, -7.50000};
    config.gauge.normal = {0.02650, 0.01769, 0.00444, -5.50000, -7.50000};
    config.gauge.easy = {0.03514, 0.02342, 0.00589, -5.50000, -7.50000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.hard.gd == doctest::Approx(kCurrentHardGd));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.normal.gd == doctest::Approx(kCurrentNormalGd));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(config.gauge.easy.gd == doctest::Approx(kCurrentEasyGd));
}

TEST_CASE("runtime migration upgrades the immediate prior shared normal and easy penalties") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.03666667, 0.02444444, 0.00611111, -5.50000, -7.50000};
    config.gauge.normal = {0.05238095, 0.03492063, 0.00873016, -5.50000, -7.50000};
    config.gauge.easy = {0.10000000, 0.06666667, 0.01666667, -5.50000, -7.50000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the previous release bad penalties to the latest table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.03666667, 0.02444444, 0.00000000, -7.04000, -7.04000};
    config.gauge.normal = {0.05238095, 0.03492063, 0.00000000, -3.52000, -3.52000};
    config.gauge.easy = {0.10000000, 0.06666667, 0.00000000, -2.64000, -2.64000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the former current bad penalties to the latest table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.00500000, 0.01333333, 0.00000000, -15.48800, -15.48800};
    config.gauge.normal = {0.01000000, 0.02000000, 0.00000000, -9.68000, -9.68000};
    config.gauge.easy = {0.01500000, 0.03333333, 0.00000000, -6.82440, -6.82440};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the previous current bad penalties to the latest table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.00500000, 0.01333333, 0.00000000, -14.24896, -14.24896};
    config.gauge.normal = {0.01000000, 0.02000000, 0.00000000, -8.90560, -8.90560};
    config.gauge.easy = {0.01500000, 0.03333333, 0.00000000, -6.27845, -6.27845};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the most recent shipped gauge defaults to the latest recovery table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.01000000, 0.05000000, 1.0 / 65.0, -4.00000, -4.00000};
    config.gauge.normal = {0.01000000, 0.05000000, 1.0 / 65.0, -2.00000, -2.00000};
    config.gauge.easy = {0.03200000, 0.03200000 / 20.0, 0.03200000 / 50.0, -2.00000, -2.00000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.hard.gd == doctest::Approx(kCurrentHardGd));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.normal.gd == doctest::Approx(kCurrentNormalGd));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(config.gauge.easy.gd == doctest::Approx(kCurrentEasyGd));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
}

TEST_CASE("runtime migration upgrades the immediate previous gauge defaults to the latest penalties") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.16000000, 0.09000000, 0.01000000, -10.00000, -10.00000};
    config.gauge.normal = {0.19000000, 0.15000000, 0.01000000, -6.00000, -6.00000};
    config.gauge.easy = {0.25000000, 0.20000000, 0.01000000, -4.00000, -4.00000};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the immediate prior normal and easy penalties to the latest table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.03666667, 0.02444444, 0.00000000, -15.48800, -15.48800};
    config.gauge.normal = {0.05238095, 0.03492063, 0.00000000, -7.74400, -7.74400};
    config.gauge.easy = {0.10000000, 0.06666667, 0.00000000, -5.80800, -5.80800};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.hard.pr == doctest::Approx(kCurrentHardPr));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.normal.pr == doctest::Approx(kCurrentNormalPr));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
    CHECK(config.gauge.easy.pr == doctest::Approx(kCurrentEasyPr));
}

TEST_CASE("runtime migration upgrades the immediate prior great recovery table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.03666667, 0.02444444, 0.00000000, -15.48800, -15.48800};
    config.gauge.normal = {0.05238095, 0.03492063, 0.00000000, -9.68000, -9.68000};
    config.gauge.easy = {0.10000000, 0.06666667, 0.00000000, -6.82440, -6.82440};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
    CHECK(config.gauge.hard.bd == doctest::Approx(kCurrentHardBd));
    CHECK(config.gauge.normal.bd == doctest::Approx(kCurrentNormalBd));
    CHECK(config.gauge.easy.bd == doctest::Approx(kCurrentEasyBd));
}

TEST_CASE("runtime migration upgrades the immediate prior pg recovery table") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.gauge.hard = {0.03666667, 0.01333333, 0.00000000, -15.48800, -15.48800};
    config.gauge.normal = {0.05238095, 0.02000000, 0.00000000, -9.68000, -9.68000};
    config.gauge.easy = {0.10000000, 0.03333333, 0.00000000, -6.82440, -6.82440};

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.gauge.hard.pg == doctest::Approx(kCurrentHardPg));
    CHECK(config.gauge.normal.pg == doctest::Approx(kCurrentNormalPg));
    CHECK(config.gauge.easy.pg == doctest::Approx(kCurrentEasyPg));
    CHECK(config.gauge.hard.gr == doctest::Approx(kCurrentHardGr));
    CHECK(config.gauge.normal.gr == doctest::Approx(kCurrentNormalGr));
    CHECK(config.gauge.easy.gr == doctest::Approx(kCurrentEasyGr));
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
    CHECK(config.judge.hold_grace_ms == doctest::Approx(80.0));
    CHECK(config.judge.hold_break_ms == doctest::Approx(200.0));
    CHECK(config.input.debounce_ms == doctest::Approx(8.0));
}

TEST_CASE("config load keeps polling cadence and defaults judgement cadence when the new field is missing") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::filesystem::create_directories(temp.path / "config");
    std::filesystem::create_directories(temp.path / "profiles" / "test");
    write_file(temp.path / "config" / "config.json",
               "{\n"
               "  \"input\": {\n"
               "    \"polling_hz\": 1000,\n"
               "    \"debounce_ms\": 8.0\n"
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
    CHECK(result.config.input.polling_hz == 1000);
    CHECK(result.config.input.judgement_hz == 4000);
}

TEST_CASE("runtime migration upgrades the old default note height scale to 180 percent") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.note_height_scale = 1.0;
    config.skin.note_height_scales["10k"] = 1.0;
    config.skin.note_height_scales["16k"] = 1.0;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.skin.note_height_scale == doctest::Approx(1.80));
    CHECK(config.skin.note_height_scales["10k"] == doctest::Approx(1.80));
    CHECK(config.skin.note_height_scales["16k"] == doctest::Approx(1.80));
}

TEST_CASE("runtime migration aligns legacy long-note body width and brightness with notes") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.skin.hold_body_width_scale = 0.60;
    config.skin.hold_body_opacity = 0.55;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.skin.hold_body_width_scale == doctest::Approx(1.00));
    CHECK(config.skin.hold_body_opacity == doctest::Approx(1.00));

    auto minimal = loader.defaults();
    minimal.skin.visual_preset = "minimal";
    minimal.skin.hold_body_opacity = 0.15;
    CHECK(tenriff::app::migrate_bms_first_runtime_config(minimal));
    CHECK(minimal.skin.hold_body_opacity == doctest::Approx(1.00));
}

TEST_CASE("runtime migration upgrades the old result tail default to three seconds") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.ui.result_tail_ms = 500.0;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.ui.result_tail_ms == doctest::Approx(3000.0));
}

TEST_CASE("runtime migration flips the old default vsync preset to off") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.graphics.display_mode = "borderless";
    config.graphics.resolution = "native";
    config.graphics.vsync = true;
    config.graphics.refresh_hz = 1050;
    config.graphics.performance_overlay = false;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK_FALSE(config.graphics.vsync);
}

TEST_CASE("runtime migration replaces the old fixed cap with match display") {
    ConfigLoader loader;
    auto config = loader.defaults();
    config.graphics.display_mode = "borderless";
    config.graphics.resolution = "native";
    config.graphics.vsync = false;
    config.graphics.refresh_hz = 1050;
    config.graphics.performance_overlay = false;

    const bool changed = tenriff::app::migrate_bms_first_runtime_config(config);

    CHECK(changed);
    CHECK(config.graphics.refresh_hz == -1);
}
