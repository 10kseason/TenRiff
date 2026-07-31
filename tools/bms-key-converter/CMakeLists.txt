cmake_minimum_required(VERSION 3.20)
project(BmsKeyConverter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options("/utf-8")
endif()

set(TENRIFF_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Path to the TenRiff source root")
add_library(bms_key_converter_nk2
    "${TENRIFF_SOURCE_ROOT}/nk2/intent_graph.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/layout_model.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/nk2_convert.cpp"
    "${TENRIFF_SOURCE_ROOT}/nk2/nk2_report.cpp"
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
)

target_include_directories(bms_key_converter_core
    PUBLIC
        "${TENRIFF_SOURCE_ROOT}/src"
)

target_compile_features(bms_key_converter_core PUBLIC cxx_std_17)
target_link_libraries(bms_key_converter_core PUBLIC bms_key_converter_nk2)


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
