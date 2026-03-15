cmake_minimum_required(VERSION 3.20)
project(TenRiff VERSION 0.7.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC UTF-8 support
if(MSVC)
    add_compile_options("/utf-8")
endif()

option(TENRIFF_ENABLE_TESTS "Build TenRiff unit tests" ON)

add_library(tenriff_chart
    src/chart/BmsParser.cpp
    src/chart/BmsChartNorm.cpp
    src/chart/BmsTimeline.cpp
    src/chart/OsuManiaLoader.cpp
    src/chart/OsuDifficulty.cpp
    src/chart/NoteLaneMapping.cpp
)

target_include_directories(tenriff_chart
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tenriff_chart PUBLIC cxx_std_17)

add_library(tenriff_game
    src/game/GaugeManager.cpp
    src/game/SpeedManager.cpp
)

target_include_directories(tenriff_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tenriff_game PUBLIC cxx_std_17)

add_library(tenriff_runtime
    src/timing/ClockSync.cpp
    src/timing/HighResClock.cpp
    src/render/RenderThread.cpp
)

target_include_directories(tenriff_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tenriff_runtime PUBLIC cxx_std_17)

add_library(tenriff_core
    src/config/SimpleJson.cpp
    src/config/Config.cpp
    src/config/Keymap.cpp
    src/config/KeycodeMap.cpp
    src/input/KeyStateTracker.cpp
    src/gameplay/GameplayChart.cpp
    src/gameplay/GameplayEngine.cpp
    src/gameplay/ModeApplier.cpp
    src/gameplay/ModeSettings.cpp
    src/gameplay/ResultStats.cpp
    src/gameplay/Replay.cpp
    src/app/BmsGameplayBuilder.cpp
    src/app/ChartLoader.cpp
    src/app/CommandLine.cpp
    src/app/GraphicsTiming.cpp
    src/app/ModeResolver.cpp
    src/app/RuntimeConfigMigration.cpp
    src/app/SongIndex.cpp
    src/app/SongIndexerThread.cpp
)

target_include_directories(tenriff_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tenriff_core PUBLIC cxx_std_17)

target_link_libraries(tenriff_core
    PUBLIC
        tenriff_chart
        tenriff_game
        tenriff_runtime
)

# Audio and input subsystem (Windows-specific for now).
if(WIN32)
    add_library(tenriff_audio
        src/audio/WasapiBackend.cpp
        src/audio/AudioThread.cpp
        src/input/RawInputHandler.cpp
        src/input/InputThread.cpp
    )

    target_include_directories(tenriff_audio
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(tenriff_audio PUBLIC cxx_std_17)

    # Link Windows audio libraries.
    target_link_libraries(tenriff_audio
        PUBLIC
            tenriff_core
            ole32
            avrt
    )

    add_library(tenriff_app
        src/app/GameSession.cpp
        src/app/MenuApp.cpp
        src/render/MenuWindow.cpp
    )

    target_include_directories(tenriff_app
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(tenriff_app PUBLIC cxx_std_17)

    target_link_libraries(tenriff_app
        PUBLIC
            tenriff_core
            tenriff_audio
            d3d11
            dxgi
            d2d1
            dwrite
            dxguid
            mfplat
            mfreadwrite
            mfuuid
    )
endif()

add_executable(tenriff
    src/main.cpp
)

set_target_properties(tenriff PROPERTIES OUTPUT_NAME "TenRiff")

if(WIN32)
    target_link_libraries(tenriff PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core tenriff_audio tenriff_app)
else()
    target_link_libraries(tenriff PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core)
endif()

if(TENRIFF_ENABLE_TESTS)
    enable_testing()
    add_executable(bms_realworld_smoke
        tests/smoke/bms_realworld_smoke.cpp
    )
    target_include_directories(bms_realworld_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bms_realworld_smoke PRIVATE tenriff_core)

    add_executable(bms_parser_tests
        tests/unit/test_bms_parser.cpp
        tests/unit/test_bms_normalizer.cpp
        tests/unit/test_note_lane_mapping.cpp
        tests/unit/test_bms_timeline.cpp
        tests/unit/test_speed_manager.cpp
        tests/unit/test_gauge_manager.cpp
        tests/unit/test_osu_mania_loader.cpp
        tests/unit/test_clock_sync.cpp
        tests/unit/test_key_state_tracker.cpp
        tests/unit/test_keycode_map.cpp
        tests/unit/test_keymap.cpp
        tests/unit/test_simple_json.cpp
        tests/unit/test_mode_applier.cpp
        tests/unit/test_gameplay_engine.cpp
        tests/unit/test_replay_export.cpp
        tests/unit/test_chart_loader.cpp
        tests/unit/test_chart_audio_streaming.cpp
        tests/unit/test_chart_audio_playback.cpp
        tests/unit/test_osu_difficulty.cpp
        tests/unit/test_song_index.cpp
        tests/unit/test_song_select_state.cpp
        tests/unit/test_config.cpp
        tests/unit/test_game_session_hud.cpp
        tests/unit/test_graphics_timing.cpp
        tests/unit/test_render_performance.cpp
        tests/unit/test_utf8_compat.cpp
    )
    target_include_directories(bms_parser_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/external
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    if(WIN32)
        target_link_libraries(bms_parser_tests PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core tenriff_audio tenriff_app)
    else()
        target_link_libraries(bms_parser_tests PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core)
    endif()
    add_test(NAME bms_parser_tests COMMAND bms_parser_tests)
endif()
