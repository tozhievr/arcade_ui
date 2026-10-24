cmake_minimum_required(VERSION 4.5.0)
project(ArcadeUI VERSION 0.8.0 LANGUAGES CXX)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Clang required")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_library(arcade_ui STATIC)
target_sources(arcade_ui PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.cppm)
target_compile_options(arcade_ui PRIVATE -std=c++2d -Wall -Wextra -Wpedantic)
add_executable(arcadeui src/main.cpp)
target_compile_options(arcadeui PRIVATE -std=c++2d -Wall -Wextra -Wpedantic)
target_link_libraries(arcadeui PRIVATE arcade_ui)
set(UI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/examples/dashboard.html")
set(UI_BINARY "${CMAKE_CURRENT_BINARY_DIR}/dashboard.arcuxi")
add_custom_command(OUTPUT "${UI_BINARY}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py" "${UI_SOURCE}" -o "${UI_BINARY}"
    DEPENDS "${UI_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
    VERBATIM)
add_custom_target(arcuxi ALL DEPENDS "${UI_BINARY}")
add_dependencies(arcadeui arcuxi)
include(CTest)
if(BUILD_TESTING)
    add_test(NAME compiler_unit COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -v)
    add_test(NAME runtime_smoke COMMAND arcadeui "${UI_BINARY}")
    add_executable(arcadeui_incremental_tests tests/incremental.cpp)
    target_compile_options(arcadeui_incremental_tests PRIVATE -std=c++2d -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_incremental_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_incremental_tests arcuxi)
    add_test(NAME runtime_incremental COMMAND arcadeui_incremental_tests "${UI_BINARY}")
endif()

# v0.8: geometry integration tests for the packed Grid track stream.
if(BUILD_TESTING)
    set(GRID_BINARY "${CMAKE_CURRENT_BINARY_DIR}/layout_grid_v3.arcuxi")
    set(FLEX_BINARY "${CMAKE_CURRENT_BINARY_DIR}/layout_flex_v3.arcuxi")
    add_custom_command(OUTPUT "${GRID_BINARY}" "${FLEX_BINARY}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/layout_grid_v3.html" -o "${GRID_BINARY}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/layout_flex_v3.html" -o "${FLEX_BINARY}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/layout_grid_v3.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/layout_flex_v3.html"
        VERBATIM)
    add_custom_target(arcuxi_layout_v3 DEPENDS "${GRID_BINARY}" "${FLEX_BINARY}")
    add_executable(arcadeui_layout_v3_tests tests/layout_v3.cpp)
    target_compile_options(arcadeui_layout_v3_tests PRIVATE -std=c++2d -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_layout_v3_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_layout_v3_tests arcuxi_layout_v3)
    add_test(NAME runtime_layout_v3 COMMAND arcadeui_layout_v3_tests "${GRID_BINARY}" "${FLEX_BINARY}")
endif()
