cmake_minimum_required(VERSION 3.20)
project(BmsKeyConverter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options("/utf-8")
endif()

set(TENRIFF_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Path to the TenRiff source root")
option(TENRIFF_ENABLE_NK3_ONNX "Enable the OpenVINO NK3 P64 + generalized MLP converter" ON)
set(TENRIFF_NK3_MODEL "${TENRIFF_SOURCE_ROOT}/models/NK3-P64-hybrid.onnx")
set(TENRIFF_NK3_PATTERN_MODELS)
foreach(target_keys RANGE 2 18)
    list(APPEND TENRIFF_NK3_PATTERN_MODELS
        "${TENRIFF_SOURCE_ROOT}/models/NK3-general-pattern-${target_keys}K.onnx")
endforeach()
set(TENRIFF_NK3_ONNX_AVAILABLE OFF)
if(TENRIFF_ENABLE_NK3_ONNX)
    find_package(OpenVINO QUIET COMPONENTS Runtime)
    set(TENRIFF_NK3_MODELS_COMPLETE TRUE)
    foreach(model_path IN LISTS TENRIFF_NK3_PATTERN_MODELS)
        if(NOT EXISTS "${model_path}")
            set(TENRIFF_NK3_MODELS_COMPLETE FALSE)
        endif()
    endforeach()
    if(OpenVINO_FOUND AND EXISTS "${TENRIFF_NK3_MODEL}" AND TENRIFF_NK3_MODELS_COMPLETE)
        set(TENRIFF_NK3_ONNX_AVAILABLE ON)
        message(STATUS "NK3 P64 + generalized 2K-18K MLP enabled via OpenVINO")
    elseif(NOT OpenVINO_FOUND)
        message(WARNING "OpenVINO was not found; NK3 ONNX will report unavailable")
    else()
        message(WARNING "One or more NK3 P64/generalized MLP models were not found")
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
    target_link_libraries(bms_key_converter_core PUBLIC openvino::runtime)
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

if(TENRIFF_NK3_ONNX_AVAILABLE)
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
    foreach(target_name bms_key_converter bms_key_converter_gui)
        if(TARGET ${target_name})
            if(WIN32)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${target_name}>/models"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${TENRIFF_NK3_MODEL}"
                        "$<TARGET_FILE_DIR:${target_name}>/models/NK3-P64-hybrid.onnx"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${TENRIFF_NK3_PATTERN_MODELS}
                        "$<TARGET_FILE_DIR:${target_name}>/models"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${TENRIFF_OPENVINO_RUNTIME_DLLS}
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying NK3 models and OpenVINO GPU/CPU/NPU runtime for ${target_name}"
                    COMMAND_EXPAND_LISTS)
            else()
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${target_name}>/models"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${TENRIFF_NK3_MODEL}"
                        "$<TARGET_FILE_DIR:${target_name}>/models/NK3-P64-hybrid.onnx"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${TENRIFF_NK3_PATTERN_MODELS}
                        "$<TARGET_FILE_DIR:${target_name}>/models"
                    COMMENT "Copying the NK3 models for ${target_name}"
                    COMMAND_EXPAND_LISTS)
            endif()
        endif()
    endforeach()
endif()
