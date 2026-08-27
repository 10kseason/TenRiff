cmake_minimum_required(VERSION 3.20)
project(TenRiff VERSION 1.5.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC UTF-8 support
if(MSVC)
    add_compile_options("/utf-8" "/EHsc")
endif()

option(TENRIFF_ENABLE_TESTS "Build TenRiff unit tests" ON)
option(TENRIFF_ENABLE_ONNX_UPSCALER "Enable the optional user-selected WinML ONNX background upscaler" ON)
option(TENRIFF_ENABLE_NK3_ONNX "Enable NK3 P64 with a non-10K to 10K MLP prior" ON)
option(TENRIFF_ENABLE_NK3_NCNN "Enable the ncnn Vulkan backend for NK3 ONNX" ON)
option(TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER "Build the standalone BMS key-converter CLI/GUI (not shipped)" OFF)
option(TENRIFF_ENABLE_ASAN "Build TenRiff with AddressSanitizer" OFF)

set(TENRIFF_NK3_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/models/NK3-P64-hybrid.onnx")
set(TENRIFF_NK3_PATTERN_MODELS)
set(TENRIFF_NK3_PATTERN_MODELS_AVAILABLE ON)
set(TENRIFF_NK3_NCNN_MODELS)
set(TENRIFF_NK3_NCNN_MODELS_AVAILABLE ON)
foreach(target_keys RANGE 2 18)
    set(pattern_model
        "${CMAKE_CURRENT_SOURCE_DIR}/models/NK3-general-pattern-${target_keys}K.onnx")
    list(APPEND TENRIFF_NK3_PATTERN_MODELS "${pattern_model}")
    if(NOT EXISTS "${pattern_model}")
        set(TENRIFF_NK3_PATTERN_MODELS_AVAILABLE OFF)
    endif()
    foreach(extension param bin)
        set(ncnn_pattern_model
            "${CMAKE_CURRENT_SOURCE_DIR}/models/ncnn/NK3-general-pattern-${target_keys}K.ncnn.${extension}")
        list(APPEND TENRIFF_NK3_NCNN_MODELS "${ncnn_pattern_model}")
        if(NOT EXISTS "${ncnn_pattern_model}")
            set(TENRIFF_NK3_NCNN_MODELS_AVAILABLE OFF)
        endif()
    endforeach()
endforeach()
set(TENRIFF_NK3_ONNX_AVAILABLE OFF)
set(TENRIFF_NK3_OPENVINO_AVAILABLE OFF)
set(TENRIFF_NK3_NCNN_AVAILABLE OFF)
if(TENRIFF_ENABLE_NK3_ONNX)
    find_package(OpenVINO QUIET COMPONENTS Runtime)
    if(OpenVINO_FOUND AND EXISTS "${TENRIFF_NK3_MODEL}"
       AND TENRIFF_NK3_PATTERN_MODELS_AVAILABLE)
        set(TENRIFF_NK3_OPENVINO_AVAILABLE ON)
        message(STATUS "NK3 P64 + non-10K to 10K generalized MLP enabled via OpenVINO")
    elseif(OpenVINO_FOUND)
        message(WARNING "One or more NK3 P64/generalized MLP models are missing")
    endif()

    if(TENRIFF_ENABLE_NK3_NCNN)
        if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT DEFINED ncnn_DIR)
            set(ncnn_DIR
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ncnn-20260526/x64/lib/cmake/ncnn")
        endif()
        find_package(ncnn QUIET CONFIG)
        set(TENRIFF_NK3_NCNN_P64_PARAM
            "${CMAKE_CURRENT_SOURCE_DIR}/models/ncnn/NK3-P64-hybrid.ncnn.param")
        set(TENRIFF_NK3_NCNN_P64_BIN
            "${CMAKE_CURRENT_SOURCE_DIR}/models/ncnn/NK3-P64-hybrid.ncnn.bin")
        if(ncnn_FOUND AND EXISTS "${TENRIFF_NK3_NCNN_P64_PARAM}"
           AND EXISTS "${TENRIFF_NK3_NCNN_P64_BIN}"
           AND TENRIFF_NK3_NCNN_MODELS_AVAILABLE)
            set(TENRIFF_NK3_NCNN_AVAILABLE ON)
            message(STATUS "NK3 P64 + generalized MLP enabled via ncnn Vulkan")
        elseif(ncnn_FOUND)
            message(WARNING "One or more converted NK3 ncnn models are missing")
        endif()
    endif()

    if(TENRIFF_NK3_OPENVINO_AVAILABLE OR TENRIFF_NK3_NCNN_AVAILABLE)
        set(TENRIFF_NK3_ONNX_AVAILABLE ON)
    else()
        message(WARNING "No NK3 inference backend was found; NK3 ONNX will report unavailable at runtime")
    endif()
endif()

if(TENRIFF_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address /Zi /FS)
        add_link_options(/INCREMENTAL:NO)
        get_filename_component(TENRIFF_ASAN_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(TENRIFF_ASAN_RUNTIME_DLL
            "${TENRIFF_ASAN_RUNTIME_DIR}/clang_rt.asan_dynamic-x86_64.dll")
        if(NOT EXISTS "${TENRIFF_ASAN_RUNTIME_DLL}")
            message(FATAL_ERROR "MSVC AddressSanitizer runtime DLL was not found")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    else()
        message(FATAL_ERROR "TENRIFF_ENABLE_ASAN is unsupported by this compiler")
    endif()
endif()

add_library(tenriff_chart
    src/chart/BmsParser.cpp
    src/chart/BmsChartNorm.cpp
    src/chart/BmsTimeline.cpp
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
add_library(tenriff_nk2
    nk2/intent_graph.cpp
    nk2/layout_model.cpp
    nk2/nk2_convert.cpp
    nk2/nk2_report.cpp
    nk2/nk3_relational.cpp
)

target_include_directories(tenriff_nk2
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tenriff_nk2 PUBLIC cxx_std_17)

add_library(tenriff_core
    src/config/SimpleJson.cpp
    src/config/Config.cpp
    src/config/Keymap.cpp
    src/config/KeycodeMap.cpp
    src/input/KeyStateTracker.cpp
    src/gameplay/GameplayChart.cpp
    src/gameplay/GameplayEngine.cpp
    src/gameplay/KeyModeConverter.cpp
    src/gameplay/Nk2KeyModeAdapter.cpp
    src/gameplay/Nk3OnnxKeyModeAdapter.cpp
    src/gameplay/ModeApplier.cpp
    src/gameplay/ModeSettings.cpp
    src/gameplay/OsuManiaScore.cpp
    src/gameplay/ResultStats.cpp
    src/gameplay/Replay.cpp
    src/network/LanDiscovery.cpp
    src/network/PeerProtocol.cpp
    src/network/PeerSession.cpp
    src/app/BmsGameplayBuilder.cpp
    src/app/BmsKeyConverter.cpp
    src/app/ChartFileHash.cpp
    src/app/ChartLoader.cpp
    src/app/CommandLine.cpp
    src/app/CrashLogger.cpp
    src/app/DifficultyTable.cpp
    src/app/DifficultyTableLink.cpp
    src/app/GraphicsTiming.cpp
    src/app/ImportedGameplaySkin.cpp
    src/app/Lr2Course.cpp
    src/app/Lr2Skin.cpp
    src/app/TenRiffSkin.cpp
    src/app/MenuRecordUtils.cpp
    src/app/MenuSongUtils.cpp
    src/app/MultiplayerChartSearch.cpp
    src/app/ModeManager.cpp
    src/app/ModeResolver.cpp
    src/app/OnlineRecordsClient.cpp
    src/app/RankedRecordsClient.cpp
    src/app/GlobalChatService.cpp
    src/app/PersistedRuntimeConfig.cpp
    src/app/ReplayVerifier.cpp
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
        tenriff_nk2
        tenriff_chart
        tenriff_game
        tenriff_runtime
)

if(TENRIFF_NK3_ONNX_AVAILABLE)
    target_compile_definitions(tenriff_core PRIVATE TENRIFF_ENABLE_NK3_ONNX=1)
endif()
if(TENRIFF_NK3_OPENVINO_AVAILABLE)
    target_compile_definitions(tenriff_core PRIVATE TENRIFF_ENABLE_NK3_OPENVINO=1)
    target_link_libraries(tenriff_core PUBLIC openvino::runtime)
endif()
if(TENRIFF_NK3_NCNN_AVAILABLE)
    target_compile_definitions(tenriff_core PRIVATE TENRIFF_ENABLE_NK3_NCNN=1)
    target_link_libraries(tenriff_core PRIVATE ncnn)
endif()

if(WIN32)
    target_link_libraries(tenriff_core PUBLIC ws2_32 winhttp bcrypt crypt32)
endif()

if(TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER)
    add_library(tenriff_bms_key_converter_core
        src/app/BmsKeyConverter.cpp
        src/app/BmsGameplayBuilder.cpp
        src/chart/BmsParser.cpp
        src/chart/BmsChartNorm.cpp
        src/chart/BmsTimeline.cpp
        src/chart/NoteLaneMapping.cpp
        src/gameplay/GameplayChart.cpp
        src/gameplay/KeyModeConverter.cpp
        src/gameplay/Nk2KeyModeAdapter.cpp
        src/gameplay/Nk3OnnxKeyModeAdapter.cpp
    )

    target_include_directories(tenriff_bms_key_converter_core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(tenriff_bms_key_converter_core PUBLIC cxx_std_17)
    target_link_libraries(tenriff_bms_key_converter_core PUBLIC tenriff_nk2)

    if(TENRIFF_NK3_ONNX_AVAILABLE)
        target_compile_definitions(tenriff_bms_key_converter_core PRIVATE TENRIFF_ENABLE_NK3_ONNX=1)
    endif()
    if(TENRIFF_NK3_OPENVINO_AVAILABLE)
        target_compile_definitions(tenriff_bms_key_converter_core PRIVATE TENRIFF_ENABLE_NK3_OPENVINO=1)
        target_link_libraries(tenriff_bms_key_converter_core PUBLIC openvino::runtime)
    endif()
    if(TENRIFF_NK3_NCNN_AVAILABLE)
        target_compile_definitions(tenriff_bms_key_converter_core PRIVATE TENRIFF_ENABLE_NK3_NCNN=1)
        target_link_libraries(tenriff_bms_key_converter_core PRIVATE ncnn)
    endif()
endif()


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
        src/app/SongPreviewBuilder.cpp
        src/app/SongSelectScreen.cpp
        src/app/GameSession.cpp
        src/app/MenuApp.cpp
        src/app/MenuAppAccount.cpp
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
        src/render/BgaImageLoader.cpp
        src/render/BgaVideoDecoder.cpp
        src/render/BgaVideoFfmpeg.cpp
        src/render/OnnxBackgroundUpscaler.cpp
        src/render/ResultPresentation.cpp
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
            windowscodecs
    )
    add_executable(bga_video_smoke
        tools/bga_video_smoke.cpp
        src/render/BgaVideoDecoder.cpp
        src/render/BgaVideoFfmpeg.cpp
    )
    target_include_directories(bga_video_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(bga_video_smoke PRIVATE cxx_std_17)
    target_link_libraries(bga_video_smoke PRIVATE ole32 mfplat mfreadwrite mfuuid)

    if(TENRIFF_ENABLE_ONNX_UPSCALER AND MSVC)
        target_compile_definitions(tenriff_app PRIVATE
            TENRIFF_ENABLE_ONNX_UPSCALER=1
            _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
        )
        target_link_libraries(tenriff_app PRIVATE windowsapp)

        add_executable(onnx_upscaler_winml_smoke
            tools/onnx_upscaler/winml_smoke.cpp
            src/render/OnnxBackgroundUpscaler.cpp
        )
        target_include_directories(onnx_upscaler_winml_smoke PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_compile_definitions(onnx_upscaler_winml_smoke PRIVATE
            TENRIFF_ENABLE_ONNX_UPSCALER=1
            _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
        )
        target_compile_features(onnx_upscaler_winml_smoke PRIVATE cxx_std_17)
        target_link_libraries(onnx_upscaler_winml_smoke PRIVATE windowsapp ole32 windowscodecs)
    endif()
endif()

add_executable(tenriff
    src/main.cpp
)

add_executable(tenriff_replay_verifier
    src/tenriff_replay_verifier_main.cpp
)
set_target_properties(tenriff_replay_verifier PROPERTIES
    OUTPUT_NAME "tenriff-replay-verifier")
target_link_libraries(tenriff_replay_verifier PRIVATE tenriff_core)

set_target_properties(tenriff PROPERTIES OUTPUT_NAME "TenRiff")

# Keep menu music beside the runnable binary. Copying the directory as a
# post-build step also makes newly added numbered variants available without
# maintaining an asset list in CMake.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Mainmusic")
    add_custom_command(TARGET tenriff POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Mainmusic"
            "$<TARGET_FILE_DIR:tenriff>/Mainmusic"
        COMMENT "Copying Mainmusic assets"
    )
endif()

if(WIN32)
    target_link_libraries(tenriff PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core tenriff_audio tenriff_app)
else()
    target_link_libraries(tenriff PRIVATE tenriff_chart tenriff_game tenriff_runtime tenriff_core)
endif()


if(TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER)
    add_executable(bms_key_converter
        src/bms_key_converter_main.cpp
    )

    target_include_directories(bms_key_converter
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bms_key_converter PRIVATE tenriff_bms_key_converter_core)
endif()

if(TENRIFF_NK3_OPENVINO_AVAILABLE)
    set(TENRIFF_OPENVINO_RUNTIME_DLLS)
    if(WIN32)
        set(TENRIFF_OPENVINO_RUNTIME_DIR "${OpenVINO_DIR}/../libs")
        set(TENRIFF_OPENVINO_REQUIRED_DLL_NAMES
            openvino.dll
            openvino_intel_cpu_plugin.dll
            openvino_intel_gpu_plugin.dll
            openvino_onnx_frontend.dll
        )
        foreach(dll_name IN LISTS TENRIFF_OPENVINO_REQUIRED_DLL_NAMES)
            set(dll_path "${TENRIFF_OPENVINO_RUNTIME_DIR}/${dll_name}")
            if(NOT EXISTS "${dll_path}")
                message(FATAL_ERROR "Required NK3 OpenVINO runtime DLL was not found: ${dll_path}")
            endif()
            list(APPEND TENRIFF_OPENVINO_RUNTIME_DLLS "${dll_path}")
        endforeach()
        foreach(dll_name tbb12.dll tbbbind_2_5.dll tbbmalloc.dll tbbmalloc_proxy.dll)
            set(dll_path "${TENRIFF_OPENVINO_RUNTIME_DIR}/${dll_name}")
            if(EXISTS "${dll_path}")
                list(APPEND TENRIFF_OPENVINO_RUNTIME_DLLS "${dll_path}")
            endif()
        endforeach()
        foreach(dll_name
                openvino_intel_npu_plugin.dll
                openvino_intel_npu_compiler_loader.dll
                openvino_intel_npu_compiler.dll
                cache.json)
            set(dll_path "${TENRIFF_OPENVINO_RUNTIME_DIR}/${dll_name}")
            if(EXISTS "${dll_path}")
                list(APPEND TENRIFF_OPENVINO_RUNTIME_DLLS "${dll_path}")
            endif()
        endforeach()
    endif()

endif()

# Ship completed TenRiff skin.json skins beside the executable. The runtime
# merges this read-mostly catalog behind profile-local imports.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/skins")
    add_custom_command(TARGET tenriff POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/skins"
            "$<TARGET_FILE_DIR:tenriff>/skins"
        COMMENT "Copying bundled TenRiff skins"
    )
endif()

if(TENRIFF_NK3_ONNX_AVAILABLE)
    function(tenriff_copy_nk3_runtime target_name)
        if(TENRIFF_NK3_OPENVINO_AVAILABLE)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${target_name}>/models"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TENRIFF_NK3_MODEL}"
                    "$<TARGET_FILE_DIR:${target_name}>/models/NK3-P64-hybrid.onnx"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${TENRIFF_NK3_PATTERN_MODELS}
                    "$<TARGET_FILE_DIR:${target_name}>/models"
                COMMENT "Copying NK3 ONNX models for ${target_name}"
                COMMAND_EXPAND_LISTS)
        endif()
        if(WIN32 AND TENRIFF_NK3_OPENVINO_AVAILABLE)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${TENRIFF_OPENVINO_RUNTIME_DLLS}
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying the NK3 OpenVINO runtime for ${target_name}"
                COMMAND_EXPAND_LISTS)
        endif()
        if(TENRIFF_NK3_NCNN_AVAILABLE)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${target_name}>/models/ncnn"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TENRIFF_NK3_NCNN_P64_PARAM}"
                    "${TENRIFF_NK3_NCNN_P64_BIN}"
                    ${TENRIFF_NK3_NCNN_MODELS}
                    "$<TARGET_FILE_DIR:${target_name}>/models/ncnn"
                COMMENT "Copying NK3 ncnn Vulkan models for ${target_name}"
                COMMAND_EXPAND_LISTS)
            if(WIN32)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:ncnn>"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying the ncnn Vulkan runtime for ${target_name}")
            endif()
        endif()
    endfunction()

    tenriff_copy_nk3_runtime(tenriff)
    if(TARGET bms_key_converter)
        tenriff_copy_nk3_runtime(bms_key_converter)
    endif()
endif()

# KeyWeaver NK2 - osu!mania front-end for the shared nK2 relaning engine.
add_library(tenriff_keyweaver_osu_core
    src/keyweaver/osu_file.cpp
    src/keyweaver/osu_pipeline.cpp
)

target_include_directories(tenriff_keyweaver_osu_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tenriff_keyweaver_osu_core PUBLIC cxx_std_17)
target_link_libraries(tenriff_keyweaver_osu_core PUBLIC tenriff_nk2)

add_executable(keyweaver_osu
    src/keyweaver_osu_main.cpp
)

target_link_libraries(keyweaver_osu PRIVATE tenriff_keyweaver_osu_core)

if(MSVC)
    target_compile_options(tenriff_keyweaver_osu_core PRIVATE /utf-8)
    target_compile_options(keyweaver_osu PRIVATE /utf-8)
    # wmain() keeps dropped paths intact instead of losing them to the ANSI
    # code page.
    set_target_properties(keyweaver_osu PROPERTIES LINK_FLAGS "/ENTRY:wmainCRTStartup")
endif()

if(WIN32 AND TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER)
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
    if(TENRIFF_NK3_ONNX_AVAILABLE)
        tenriff_copy_nk3_runtime(bms_key_converter_gui)
    endif()
endif()

if(TENRIFF_ENABLE_TESTS)
    enable_testing()
    if(TENRIFF_NK3_ONNX_AVAILABLE)
        add_executable(nk3_onnx_smoke
            tests/smoke/nk3_onnx_smoke.cpp
        )
        target_include_directories(nk3_onnx_smoke
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(nk3_onnx_smoke PRIVATE tenriff_core)
        tenriff_copy_nk3_runtime(nk3_onnx_smoke)
        add_test(NAME nk3_onnx_smoke COMMAND nk3_onnx_smoke)
        set_tests_properties(nk3_onnx_smoke PROPERTIES TIMEOUT 600)
    endif()

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
    add_test(
        NAME gameplay_judgement_benchmark
        COMMAND gameplay_judgement_benchmark)
    set_tests_properties(gameplay_judgement_benchmark PROPERTIES TIMEOUT 120)

    add_executable(online_records_smoke
        tests/smoke/online_records_smoke.cpp
    )
    target_include_directories(online_records_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(online_records_smoke PRIVATE tenriff_core)

    add_executable(ranked_records_smoke
        tests/smoke/ranked_records_smoke.cpp
    )
    target_include_directories(ranked_records_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(ranked_records_smoke PRIVATE tenriff_core)

    add_executable(bms_parser_tests
        tests/unit/test_bms_parser.cpp
        tests/unit/test_bms_normalizer.cpp
        tests/unit/test_note_lane_mapping.cpp
        tests/unit/test_bms_timeline.cpp
        tests/unit/test_speed_manager.cpp
        tests/unit/test_gauge_manager.cpp
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
        tests/unit/test_osu_mania_score.cpp
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
        tests/unit/test_song_select_screen.cpp
        tests/unit/test_session_mix.cpp
        tests/unit/test_lr2_course.cpp
        tests/unit/test_menu_settings_utils.cpp
        tests/unit/test_config.cpp
        tests/unit/test_menu_music_controller.cpp
        tests/unit/test_input_backend_status.cpp
        tests/unit/test_profile_setup_flow.cpp
        tests/unit/test_lr2_skin.cpp
        tests/unit/test_tenriff_skin.cpp
        tests/unit/test_text_fit.cpp
        tests/unit/test_difficulty_table.cpp
        tests/unit/test_game_session_hud.cpp
        tests/unit/test_graphics_timing.cpp
        tests/unit/test_render_performance.cpp
        tests/unit/test_bga_image_loader.cpp
        tests/unit/test_targa_image.cpp
        tests/unit/test_result_presentation.cpp
        tests/unit/test_lan_discovery.cpp
        tests/unit/test_peer_protocol.cpp
        tests/unit/test_peer_session.cpp
        tests/unit/test_multiplayer_menu_state.cpp
        tests/unit/test_chat_interaction.cpp
        tests/unit/test_online_records_client.cpp
        tests/unit/test_bms_key_converter.cpp
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
    if(TENRIFF_NK3_ONNX_AVAILABLE)
        tenriff_copy_nk3_runtime(bms_parser_tests)
    endif()
    target_compile_definitions(bms_parser_tests PRIVATE
        TENRIFF_TEST_REPLAY_VERIFIER_PATH="$<TARGET_FILE:tenriff_replay_verifier>")
    add_dependencies(bms_parser_tests tenriff_replay_verifier)
    if(TENRIFF_ENABLE_ASAN AND MSVC)
        add_custom_command(TARGET bms_parser_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TENRIFF_ASAN_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:bms_parser_tests>"
            COMMENT "Copying the MSVC AddressSanitizer runtime")
    endif()
    if(TENRIFF_ENABLE_ASAN)
        add_test(
            NAME bms_parser_tests_asan
            COMMAND bms_parser_tests
                    --skip-windows-runtime-integration
                    --skip-nk3-openvino-integration)
        set_tests_properties(bms_parser_tests_asan PROPERTIES TIMEOUT 600)
    else()
        add_test(NAME bms_parser_tests COMMAND bms_parser_tests)
        set_tests_properties(bms_parser_tests PROPERTIES TIMEOUT 600)
    endif()
endif()
