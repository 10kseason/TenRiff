cmake_minimum_required(VERSION 3.20)
project(BmsKeyConverter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options("/utf-8" "/EHsc")
endif()

set(TENRIFF_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Path to the TenRiff source root")
option(TENRIFF_ENABLE_NK3_ONNX "Enable the NK3 P64 + generalized MLP converter" ON)
option(TENRIFF_ENABLE_NK3_NCNN "Enable the ncnn Vulkan backend for NK3" ON)
set(TENRIFF_NK3_MODEL "${TENRIFF_SOURCE_ROOT}/models/NK3-P64-hybrid.onnx")
set(TENRIFF_NK3_PATTERN_MODELS)
set(TENRIFF_NK3_NCNN_MODELS)
set(TENRIFF_NK3_MODELS_COMPLETE TRUE)
set(TENRIFF_NK3_NCNN_MODELS_COMPLETE TRUE)
foreach(target_keys RANGE 2 18)
    set(pattern_model
        "${TENRIFF_SOURCE_ROOT}/models/NK3-general-pattern-${target_keys}K.onnx")
    list(APPEND TENRIFF_NK3_PATTERN_MODELS "${pattern_model}")
    if(NOT EXISTS "${pattern_model}")
        set(TENRIFF_NK3_MODELS_COMPLETE FALSE)
    endif()
    foreach(extension param bin)
        set(ncnn_model
            "${TENRIFF_SOURCE_ROOT}/models/ncnn/NK3-general-pattern-${target_keys}K.ncnn.${extension}")
        list(APPEND TENRIFF_NK3_NCNN_MODELS "${ncnn_model}")
        if(NOT EXISTS "${ncnn_model}")
            set(TENRIFF_NK3_NCNN_MODELS_COMPLETE FALSE)
        endif()
    endforeach()
endforeach()
set(TENRIFF_NK3_ONNX_AVAILABLE OFF)
set(TENRIFF_NK3_OPENVINO_AVAILABLE OFF)
set(TENRIFF_NK3_NCNN_AVAILABLE OFF)
if(TENRIFF_ENABLE_NK3_ONNX)
    find_package(OpenVINO QUIET COMPONENTS Runtime)
    if(OpenVINO_FOUND AND EXISTS "${TENRIFF_NK3_MODEL}" AND TENRIFF_NK3_MODELS_COMPLETE)
        set(TENRIFF_NK3_OPENVINO_AVAILABLE ON)
        message(STATUS "NK3 P64 + generalized 2K-18K MLP enabled via OpenVINO")
    elseif(OpenVINO_FOUND)
        message(WARNING "One or more NK3 P64/generalized MLP models were not found")
    endif()

    if(TENRIFF_ENABLE_NK3_NCNN)
        if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT DEFINED ncnn_DIR)
            set(ncnn_DIR
                "${TENRIFF_SOURCE_ROOT}/third_party/ncnn-20260526/x64/lib/cmake/ncnn")
        endif()
        find_package(ncnn QUIET CONFIG)
        set(TENRIFF_NK3_NCNN_P64_PARAM
            "${TENRIFF_SOURCE_ROOT}/models/ncnn/NK3-P64-hybrid.ncnn.param")
        set(TENRIFF_NK3_NCNN_P64_BIN
            "${TENRIFF_SOURCE_ROOT}/models/ncnn/NK3-P64-hybrid.ncnn.bin")
        if(ncnn_FOUND AND EXISTS "${TENRIFF_NK3_NCNN_P64_PARAM}"
           AND EXISTS "${TENRIFF_NK3_NCNN_P64_BIN}"
           AND TENRIFF_NK3_NCNN_MODELS_COMPLETE)
            set(TENRIFF_NK3_NCNN_AVAILABLE ON)
            message(STATUS "NK3 P64 + generalized 2K-18K MLP enabled via ncnn Vulkan")
        elseif(ncnn_FOUND)
            message(WARNING "One or more converted NK3 ncnn models were not found")
        endif()
    endif()

    if(TENRIFF_NK3_OPENVINO_AVAILABLE OR TENRIFF_NK3_NCNN_AVAILABLE)
        set(TENRIFF_NK3_ONNX_AVAILABLE ON)
    else()
        message(WARNING "No NK3 inference backend was found; NK3 will report unavailable")
    endif()
endif()

add_library(bms_key_converter_nk2
    "${TENRIFF_SOURCE_ROOT}/nk2/intent_graph.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/layout_model.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/nk2_convert.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/nk2_report.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/nk3_relational.cpp"
)

target_include_directories(bms_key_converter_nk2
    PUBLIC
        "${TENRIFF_SOURCE_ROOT}"
)

target_compile_features(bms_key_converter_nk2 PUBLIC cxx_std_17)

add_library(bms_key_converter_core
    "${TENRIFF_SOURCE_ROOT}/src/app/BmsKeyConverter.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/app/BmsGameplayBuilder.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/chart/BmsParser.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/chart/BmsChartNorm.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/chart/BmsTimeline.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/chart/NoteLaneMapping.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/gameplay/GameplayChart.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/gameplay/KeyModeConverter.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/gameplay/Nk2KeyModeAdapter.cpp"
    "${TENRIFF_SOURCE_ROOT}/src/gameplay/Nk3OnnxKeyModeAdapter.cpp"
)

target_include_directories(bms_key_converter_core
    PUBLIC
        "${TENRIFF_SOURCE_ROOT}/src"
)

target_compile_features(bms_key_converter_core PUBLIC cxx_std_17)
target_link_libraries(bms_key_converter_core PUBLIC bms_key_converter_nk2)

if(TENRIFF_NK3_ONNX_AVAILABLE)
    target_compile_definitions(bms_key_converter_core PRIVATE TENRIFF_ENABLE_NK3_ONNX=1)
endif()
if(TENRIFF_NK3_OPENVINO_AVAILABLE)
    target_compile_definitions(bms_key_converter_core PRIVATE TENRIFF_ENABLE_NK3_OPENVINO=1)
    target_link_libraries(bms_key_converter_core PUBLIC openvino::runtime)
endif()
if(TENRIFF_NK3_NCNN_AVAILABLE)
    target_compile_definitions(bms_key_converter_core PRIVATE TENRIFF_ENABLE_NK3_NCNN=1)
    target_link_libraries(bms_key_converter_core PRIVATE ncnn)
endif()


add_executable(bms_key_converter
    "${TENRIFF_SOURCE_ROOT}/src/bms_key_converter_main.cpp"
)

target_link_libraries(bms_key_converter PRIVATE bms_key_converter_core)

if(WIN32)
    add_executable(bms_key_converter_gui WIN32
        "${TENRIFF_SOURCE_ROOT}/src/bms_key_converter_gui_main.cpp"
    )

    target_link_libraries(bms_key_converter_gui
        PRIVATE
            bms_key_converter_core
            comdlg32
            shell32
    )
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
        foreach(runtime_name
                openvino_intel_npu_plugin.dll
                openvino_intel_npu_compiler_loader.dll
                openvino_intel_npu_compiler.dll
                cache.json)
            set(runtime_path "${TENRIFF_OPENVINO_RUNTIME_DIR}/${runtime_name}")
            if(EXISTS "${runtime_path}")
                list(APPEND TENRIFF_OPENVINO_RUNTIME_DLLS "${runtime_path}")
            endif()
        endforeach()
    endif()
endif()

if(TENRIFF_NK3_ONNX_AVAILABLE)
    function(copy_nk3_runtime target_name)
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
            if(WIN32)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${TENRIFF_OPENVINO_RUNTIME_DLLS}
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying the NK3 OpenVINO runtime for ${target_name}"
                    COMMAND_EXPAND_LISTS)
            endif()
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

    foreach(target_name bms_key_converter bms_key_converter_gui)
        if(TARGET ${target_name})
            copy_nk3_runtime(${target_name})
        endif()
    endforeach()
endif()
