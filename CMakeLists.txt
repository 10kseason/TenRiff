cmake_minimum_required(VERSION 3.20)
project(TenRiff VERSION 1.1.7 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC UTF-8 support
if(MSVC)
    add_compile_options("/utf-8")
endif()

option(TENRIFF_ENABLE_TESTS "Build TenRiff unit tests" ON)

# Pinned vendored miniz is used only for local .osk/.osz ZIP package import.
add_library(tenriff_miniz STATIC
    external/miniz/miniz.c
    external/miniz/miniz_tdef.c
    external/miniz/miniz_tinfl.c
    external/miniz/miniz_zip.c
)

target_include_directories(tenriff_miniz
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/miniz
)

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
    src/gameplay/KeyModeConverter.cpp
    src/gameplay/ModeApplier.cpp
    src/gameplay/ModeSettings.cpp
    src/gameplay/ResultStats.cpp
    src/gameplay/Replay.cpp
    src/network/PeerProtocol.cpp
    src/network/PeerSession.cpp
    src/app/BmsGameplayBuilder.cpp
    src/app/BmsKeyConverter.cpp
    src/app/ChartLoader.cpp
    src/app/CommandLine.cpp
    src/app/CrashLogger.cpp
    src/app/GraphicsTiming.cpp
    src/app/ImportedGameplaySkin.cpp
    src/app/Lr2Skin.cpp
    src/app/MenuRecordUtils.cpp
    src/app/MenuSongUtils.cpp
    src/app/MultiplayerChartSearch.cpp
    src/app/ModeManager.cpp
    src/app/ModeResolver.cpp
    src/app/OsuArchiveImport.cpp
    src/app/OsuHitsoundResolver.cpp
    src/app/OsuSkin.cpp
    src/app/PersistedRuntimeConfig.cpp
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
    PRIVATE
        tenriff_miniz
)

if(WIN32)
    target_link_libraries(tenriff_core PUBLIC ws2_32)
endif()

add_library(tenriff_bms_key_converter_core
    src/app/BmsKeyConverter.cpp
    src/app/BmsGameplayBuilder.cpp
    src/chart/BmsParser.cpp
    src/chart/BmsChartNorm.cpp
    src/chart/BmsTimeline.cpp
    src/chart/NoteLaneMapping.cpp
    src/gameplay/GameplayChart.cpp
    src/gameplay/KeyModeConverter.cpp
)

target_include_directories(tenriff_bms_key_converter_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tenriff_bms_key_converter_core PUBLIC cxx_std_17)

# Audio and input subsystem (Windows-specific for now).
if(WIN32)
    target_link_libraries(tenriff_core
        PUBLIC
            dbghelp
    )

    add_library(tenriff_audio
        src/audio/WasapiBackend.cpp
        src/audio/AudioThread.cpp
        src/audio/OggVorbisDecoder.cpp
        src/input/RawInputHandler.cpp
        src/input/InputThread.cpp
    )

    target_include_directories(tenriff_audio
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/external
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
        src/app/MenuMusicController.cpp
        src/app/GameSession.cpp
        src/app/MenuApp.cpp
        src/app/MenuAppMultiplayer.cpp
        src/app/MenuAppDeviceSettings.cpp
        src/app/MenuAppKeymap.cpp
        src/app/MenuAppRecords.cpp
        src/app/MenuAppSongSelect.cpp
        src/app/MenuAppSongSelectRender.cpp
        src/app/MenuAppSongSelectState.cpp
        src/app/MenuAppSongSelectUtils.cpp
        src/app/MenuAppSettings.cpp
        src/app/MenuAppSkin.cpp
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
            d3dcompiler
            dxgi
            d2d1
            dwrite
            dxguid
            mfplat
            mfreadwrite
            mfuuid
            winmm
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

add_executable(bms_key_converter
    src/bms_key_converter_main.cpp
)

target_include_directories(bms_key_converter
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(bms_key_converter PRIVATE tenriff_bms_key_converter_core)
else()
    target_link_libraries(bms_key_converter PRIVATE tenriff_bms_key_converter_core)
endif()

if(WIN32)
    add_executable(bms_key_converter_gui WIN32
        src/bms_key_converter_gui_main.cpp
    )

    target_include_directories(bms_key_converter_gui
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(bms_key_converter_gui
        PRIVATE
            tenriff_bms_key_converter_core
            comdlg32
            shell32
    )
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

    add_executable(bms_mode_smoke
        tests/smoke/bms_mode_smoke.cpp
    )
    target_include_directories(bms_mode_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bms_mode_smoke PRIVATE tenriff_core)

    add_executable(bms_10k_compare_smoke
        tests/smoke/bms_10k_compare_smoke.cpp
    )
    target_include_directories(bms_10k_compare_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bms_10k_compare_smoke PRIVATE tenriff_core)

    add_executable(song_index_benchmark
        tests/smoke/song_index_benchmark.cpp
    )
    target_include_directories(song_index_benchmark
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(song_index_benchmark PRIVATE tenriff_core)

    add_executable(gameplay_judgement_benchmark
        tests/smoke/gameplay_judgement_benchmark.cpp
    )
    target_include_directories(gameplay_judgement_benchmark
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(gameplay_judgement_benchmark PRIVATE tenriff_core)

    add_executable(n2nc_compare_smoke
        tests/smoke/n2nc_compare_smoke.cpp
    )
    target_include_directories(n2nc_compare_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(n2nc_compare_smoke PRIVATE tenriff_core)

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
        tests/unit/test_input_thread.cpp
        tests/unit/test_raw_input_health_probe.cpp
        tests/unit/test_keycode_map.cpp
        tests/unit/test_keymap.cpp
        tests/unit/test_simple_json.cpp
        tests/unit/test_mode_applier.cpp
        tests/unit/test_mode_manager.cpp
        tests/unit/test_gameplay_engine.cpp
        tests/unit/test_session_result_status.cpp
        tests/unit/test_replay_export.cpp
        tests/unit/test_chart_loader.cpp
        tests/unit/test_chart_audio_streaming.cpp
        tests/unit/test_chart_audio_playback.cpp
        tests/unit/test_ogg_vorbis_decoder.cpp
        tests/unit/test_10k_calc_consistency.cpp
        tests/unit/test_osu_difficulty.cpp
        tests/unit/test_song_index.cpp
        tests/unit/test_song_select_state.cpp
        tests/unit/test_config.cpp
        tests/unit/test_input_backend_status.cpp
        tests/unit/test_profile_setup_flow.cpp
        tests/unit/test_lr2_skin.cpp
        tests/unit/test_osu_archive_import.cpp
        tests/unit/test_osu_skin.cpp
        tests/unit/test_game_session_hud.cpp
        tests/unit/test_graphics_timing.cpp
        tests/unit/test_render_performance.cpp
        tests/unit/test_peer_protocol.cpp
        tests/unit/test_peer_session.cpp
        tests/unit/test_multiplayer_menu_state.cpp
        tests/unit/test_bms_key_converter.cpp
        tests/unit/test_utf8_compat.cpp
    )
    target_include_directories(bms_parser_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/external
            ${CMAKE_CURRENT_SOURCE_DIR}/external/miniz
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    if(WIN32)
        target_link_libraries(bms_parser_tests PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core tenriff_audio tenriff_app)
    else()
        target_link_libraries(bms_parser_tests PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core)
    endif()
    add_test(NAME bms_parser_tests COMMAND bms_parser_tests)
endif()
