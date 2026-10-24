cmake_minimum_required(VERSION 4.4.0)
project(ArcadeUI VERSION 1.9.0 LANGUAGES CXX)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Clang required")
endif()
# Prefer the experimental C++29 spelling on compilers that implement it.
# Current released Clang may support -std=c++2c but not -std=c++2d.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++2d" ARCADE_UI_HAS_CXX2D)
if(ARCADE_UI_HAS_CXX2D)
    set(ARCADE_UI_CXX_FLAG "-std=c++2d")
else()
    check_cxx_compiler_flag("-std=c++2c" ARCADE_UI_HAS_CXX2C)
    if(NOT ARCADE_UI_HAS_CXX2C)
        message(FATAL_ERROR "Clang with -std=c++2c or -std=c++2d support is required")
    endif()
    set(ARCADE_UI_CXX_FLAG "-std=c++2c")
endif()
message(STATUS "Arcade UI language mode: ${ARCADE_UI_CXX_FLAG}")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_library(arcade_ui STATIC)
target_sources(arcade_ui PUBLIC FILE_SET CXX_MODULES FILES
    src/arcade.ui.cppm
    src/contracts.cppm
    src/app.cppm
    src/arcade.ui.mock.cppm)
target_compile_options(arcade_ui PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
add_executable(arcadeui src/main.cpp)
target_compile_options(arcadeui PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
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
    target_compile_options(arcadeui_incremental_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
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
    target_compile_options(arcadeui_layout_v3_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_layout_v3_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_layout_v3_tests arcuxi_layout_v3)
    add_test(NAME runtime_layout_v3 COMMAND arcadeui_layout_v3_tests "${GRID_BINARY}" "${FLEX_BINARY}")
endif()

if(BUILD_TESTING)
    set(FLEX_FREEZE_GROW "${CMAKE_CURRENT_BINARY_DIR}/flex_freeze.arcuxi")
    set(FLEX_FREEZE_SHRINK "${CMAKE_CURRENT_BINARY_DIR}/flex_shrink.arcuxi")
    add_custom_command(OUTPUT "${FLEX_FREEZE_GROW}" "${FLEX_FREEZE_SHRINK}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py" "${CMAKE_CURRENT_SOURCE_DIR}/examples/flex_freeze.html" -o "${FLEX_FREEZE_GROW}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py" "${CMAKE_CURRENT_SOURCE_DIR}/examples/flex_shrink.html" -o "${FLEX_FREEZE_SHRINK}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py" "${CMAKE_CURRENT_SOURCE_DIR}/examples/flex_freeze.html" "${CMAKE_CURRENT_SOURCE_DIR}/examples/flex_shrink.html"
        VERBATIM)
    add_custom_target(arcuxi_flex_freeze DEPENDS "${FLEX_FREEZE_GROW}" "${FLEX_FREEZE_SHRINK}")
    add_executable(arcadeui_flex_freeze_tests tests/flex_freeze.cpp)
    target_compile_options(arcadeui_flex_freeze_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_flex_freeze_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_flex_freeze_tests arcuxi_flex_freeze)
    add_test(NAME runtime_flex_freeze COMMAND arcadeui_flex_freeze_tests "${FLEX_FREEZE_GROW}" "${FLEX_FREEZE_SHRINK}")
endif()

if(BUILD_TESTING)
    add_executable(arcadeui_text_intrinsics_tests tests/text_intrinsics.cpp)
    target_compile_options(arcadeui_text_intrinsics_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_text_intrinsics_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_text_intrinsics_tests arcuxi)
    set(TEXT_INTRINSIC_BINARY "${CMAKE_CURRENT_BINARY_DIR}/text_intrinsic.arcuxi")
    add_custom_command(OUTPUT "${TEXT_INTRINSIC_BINARY}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/examples/text_intrinsic.html" -o "${TEXT_INTRINSIC_BINARY}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/examples/text_intrinsic.html"
        VERBATIM)
    add_custom_target(arcuxi_text_intrinsics DEPENDS "${TEXT_INTRINSIC_BINARY}")
    add_dependencies(arcadeui_text_intrinsics_tests arcuxi_text_intrinsics)
    add_test(NAME runtime_text_intrinsics COMMAND arcadeui_text_intrinsics_tests "${UI_BINARY}" "${TEXT_INTRINSIC_BINARY}")
endif()

if(BUILD_TESTING)
    add_executable(arcadeui_text_line_tests tests/text_line_layout.cpp)
    target_compile_options(arcadeui_text_line_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_text_line_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_text_line_tests arcuxi_text_intrinsics)
    add_test(NAME runtime_text_lines COMMAND arcadeui_text_line_tests "${TEXT_INTRINSIC_BINARY}")
endif()

# v1.2: tests that the computed grid-track width affects text row height.
if(BUILD_TESTING)
    set(TEXT_ROWS_BINARY "${CMAKE_CURRENT_BINARY_DIR}/text_layout_rows.arcuxi")
    add_custom_command(OUTPUT "${TEXT_ROWS_BINARY}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/text_layout_rows.html" -o "${TEXT_ROWS_BINARY}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/arcuxc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/text_layout_rows.html"
        VERBATIM)
    add_custom_target(arcuxi_text_rows DEPENDS "${TEXT_ROWS_BINARY}")
    add_executable(arcadeui_text_geometry_tests tests/text_constrained_geometry.cpp)
    target_compile_options(arcadeui_text_geometry_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_text_geometry_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_text_geometry_tests arcuxi_text_rows)
    add_test(NAME runtime_text_geometry COMMAND arcadeui_text_geometry_tests "${TEXT_ROWS_BINARY}")
endif()

# Optional native OpenType text backend. Header-only dependencies stay inside
# the private global module fragment, never in the exported module interface.
option(ARCADE_UI_ENABLE_FONT_BACKEND "Build HarfBuzz/FreeType text backend" ON)
if(ARCADE_UI_ENABLE_FONT_BACKEND)
    find_package(Freetype QUIET)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(HARFBUZZ QUIET IMPORTED_TARGET harfbuzz)
    endif()
    if(Freetype_FOUND AND HARFBUZZ_FOUND)
        add_library(arcade_ui_font STATIC)
        target_sources(arcade_ui_font PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.font.cppm)
        target_compile_options(arcade_ui_font PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
        target_link_libraries(arcade_ui_font PRIVATE arcade_ui Freetype::Freetype PkgConfig::HARFBUZZ)
        add_executable(arcadeui_font_backend_tests tests/font_backend.cpp)
        target_compile_options(arcadeui_font_backend_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
        target_link_libraries(arcadeui_font_backend_tests PRIVATE arcade_ui_font)
        add_test(NAME runtime_font_backend COMMAND arcadeui_font_backend_tests)
        message(STATUS "Arcade UI: HarfBuzz/FreeType backend enabled")
    else()
        message(STATUS "Arcade UI: HarfBuzz/FreeType unavailable; basic UI remains enabled")
    endif()
endif()

# ICU UAX #14 / UAX #9 backend; no ICU headers are exported by the module.
option(ARCADE_UI_ENABLE_UNICODE "Enable ICU Unicode text segmentation" ON)
if(ARCADE_UI_ENABLE_UNICODE)
    find_package(ICU QUIET COMPONENTS uc i18n)
    if(ICU_FOUND)
        add_library(arcade_ui_unicode STATIC)
        target_sources(arcade_ui_unicode PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.unicode.cppm)
        target_link_libraries(arcade_ui_unicode PRIVATE ICU::uc ICU::i18n)
        target_compile_options(arcade_ui_unicode PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
        if(BUILD_TESTING)
            add_executable(arcadeui_unicode_tests tests/unicode.cpp)
            target_link_libraries(arcadeui_unicode_tests PRIVATE arcade_ui_unicode)
            target_compile_options(arcadeui_unicode_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
            add_test(NAME runtime_unicode COMMAND arcadeui_unicode_tests)
        endif()
        if(TARGET arcade_ui_font)
            target_link_libraries(arcade_ui_font PRIVATE arcade_ui_unicode)
            target_compile_definitions(arcade_ui_font PRIVATE ARCADE_UI_HAS_UNICODE=1)
        endif()
        message(STATUS "Arcade UI: ICU Unicode analysis enabled")
    else()
        message(STATUS "Arcade UI: ICU unavailable; Unicode module disabled")
    endif()
endif()

# v1.5: per-line UAX #9 visual resolution using ICU ubidi_setLine.
if(BUILD_TESTING AND TARGET arcade_ui_unicode)
    add_executable(arcadeui_unicode_lines_tests tests/unicode_lines.cpp)
    target_compile_options(arcadeui_unicode_lines_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
    target_link_libraries(arcadeui_unicode_lines_tests PRIVATE arcade_ui_unicode)
    add_test(NAME runtime_unicode_lines COMMAND arcadeui_unicode_lines_tests)
endif()

# v1.6: CSS Text line-fitting proof-of-concept, backed by ICU Unicode boundaries.
if(TARGET arcade_ui_unicode)
    add_library(arcade_ui_text STATIC)
    target_sources(arcade_ui_text PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.text.cppm)
    target_compile_options(arcade_ui_text PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcade_ui_text PUBLIC arcade_ui_unicode)
    if(BUILD_TESTING)
        add_executable(arcadeui_text_pipeline_tests tests/text_pipeline.cpp)
        target_compile_options(arcadeui_text_pipeline_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
        target_link_libraries(arcadeui_text_pipeline_tests PRIVATE arcade_ui_text)
        add_test(NAME runtime_text_pipeline COMMAND arcadeui_text_pipeline_tests)
    endif()
endif()

# v1.7: optional bridge from the CSS Text line fitter into Document geometry.
if(TARGET arcade_ui_text)
    add_library(arcade_ui_text_document STATIC)
    target_sources(arcade_ui_text_document PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.text.document.cppm)
    target_compile_options(arcade_ui_text_document PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcade_ui_text_document PUBLIC arcade_ui_text arcade_ui)
    if(BUILD_TESTING)
        add_executable(arcadeui_grapheme_tests tests/text_graphemes.cpp)
        target_compile_options(arcadeui_grapheme_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
        target_link_libraries(arcadeui_grapheme_tests PRIVATE arcade_ui_text_document)
        add_test(NAME runtime_text_graphemes COMMAND arcadeui_grapheme_tests)
        add_executable(arcadeui_text_document_tests tests/text_document_bridge.cpp)
        target_compile_options(arcadeui_text_document_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
        target_link_libraries(arcadeui_text_document_tests PRIVATE arcade_ui_text_document)
        add_dependencies(arcadeui_text_document_tests arcuxi_text_rows)
        add_test(NAME runtime_text_document_bridge COMMAND arcadeui_text_document_tests "${TEXT_ROWS_BINARY}")
    endif()
endif()

# v1.8: Unicode script / per-line bidi segmentation for future shaped runs.
if(TARGET arcade_ui_unicode)
    add_library(arcade_ui_text_runs STATIC)
    target_sources(arcade_ui_text_runs PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.text.runs.cppm)
    target_compile_options(arcade_ui_text_runs PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcade_ui_text_runs PUBLIC arcade_ui_unicode PRIVATE ICU::uc)
    if(BUILD_TESTING)
        add_executable(arcadeui_text_runs_tests tests/text_runs.cpp)
        target_compile_options(arcadeui_text_runs_tests PRIVATE ${ARCADE_UI_CXX_FLAG})
        target_link_libraries(arcadeui_text_runs_tests PRIVATE arcade_ui_text_runs)
        add_test(NAME runtime_text_runs COMMAND arcadeui_text_runs_tests)
    endif()
endif()

# v1.9: independent SoA shaped-glyph cache, no external headers required.
add_library(arcade_ui_text_cache STATIC)
target_sources(arcade_ui_text_cache PUBLIC FILE_SET CXX_MODULES FILES src/arcade.ui.text.cache.cppm)
target_compile_options(arcade_ui_text_cache PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
if(BUILD_TESTING)
    add_executable(arcadeui_shaped_cache_tests tests/shaped_cache.cpp)
    target_compile_options(arcadeui_shaped_cache_tests PRIVATE ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_shaped_cache_tests PRIVATE arcade_ui_text_cache)
    add_test(NAME runtime_shaped_cache COMMAND arcadeui_shaped_cache_tests)
endif()

# v2.0: connect OpenType shaping with the shared SoA cache module.
# Public linkage is necessary because arcade.ui.font exports the cache API.
if(TARGET arcade_ui_font)
    target_link_libraries(arcade_ui_font PUBLIC arcade_ui_text_cache)
    if(TARGET arcadeui_font_backend_tests)
        set_tests_properties(runtime_font_backend PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

# Native rendering contract / AppBuilder integration, no GPU required.
if(BUILD_TESTING)
    add_executable(arcadeui_app_contracts_tests)
    target_sources(arcadeui_app_contracts_tests PRIVATE
        tests/app_contracts_main.cpp)
    target_sources(arcadeui_app_contracts_tests PRIVATE
        FILE_SET CXX_MODULES FILES tests/app_contracts.cppm)
    target_compile_options(arcadeui_app_contracts_tests PRIVATE
        ${ARCADE_UI_CXX_FLAG} -Wall -Wextra -Wpedantic)
    target_link_libraries(arcadeui_app_contracts_tests PRIVATE arcade_ui)
    add_dependencies(arcadeui_app_contracts_tests arcuxi)
    add_test(NAME runtime_app_contracts
        COMMAND arcadeui_app_contracts_tests "${UI_BINARY}")
endif()
