cmake_minimum_required(VERSION 3.20)
project(TenRiff VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TENRIFF_ENABLE_TESTS "Build TenRiff unit tests" ON)

add_library(tenriff_chart
    src/chart/BmsParser.cpp
    src/chart/BmsChartNorm.cpp
    src/chart/BmsTimeline.cpp
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

add_executable(tenriff
    src/main.cpp
)

target_link_libraries(tenriff PRIVATE tenriff_chart tenriff_game)

if(TENRIFF_ENABLE_TESTS)
    enable_testing()
    add_executable(bms_parser_tests
        tests/unit/test_bms_parser.cpp
        tests/unit/test_bms_normalizer.cpp
        tests/unit/test_note_lane_mapping.cpp
        tests/unit/test_bms_timeline.cpp
        tests/unit/test_speed_manager.cpp
        tests/unit/test_gauge_manager.cpp
    )
    target_include_directories(bms_parser_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/external
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bms_parser_tests PRIVATE tenriff_chart tenriff_game)
    add_test(NAME bms_parser_tests COMMAND bms_parser_tests)
endif()
