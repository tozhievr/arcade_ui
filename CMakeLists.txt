
cmake_minimum_required(VERSION 4.5.0)

project(
    ModernProject
    VERSION 0.1.0
    DESCRIPTION "Native C++2d modular application"
    LANGUAGES CXX
)

# ------------------------------------------------------------
# Toolchain
# ------------------------------------------------------------

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Clang is required")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 23)
    message(FATAL_ERROR "Clang 23+ is required")
endif()

# CMake understands C++23 for dependency scanning.
# The actual language mode is explicitly C++2d.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_MODULE_STD ON)

# ------------------------------------------------------------
# Build configuration
# ------------------------------------------------------------

if(NOT CMAKE_CONFIGURATION_TYPES
   AND NOT CMAKE_BUILD_TYPE)
    set(
        CMAKE_BUILD_TYPE Release
        CACHE STRING "Build configuration" FORCE
    )
endif()

option(PROJECT_ENABLE_LTO "Enable IPO/LTO" ON)
option(PROJECT_ENABLE_TESTS "Enable testing" ON)
option(PROJECT_ENABLE_NATIVE "Optimize for host CPU" OFF)

# ------------------------------------------------------------
# Target
# ------------------------------------------------------------

add_executable(${PROJECT_NAME})

add_executable(
    ${PROJECT_NAME}::App
    ALIAS ${PROJECT_NAME}
)

target_sources(
    ${PROJECT_NAME}
    PRIVATE
        src/main.cpp

    PRIVATE
        FILE_SET application_modules
        TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES
            src/app.cppm
            src/contracts.cppm
)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        CXX_SCAN_FOR_MODULES ON
        INTERPROCEDURAL_OPTIMIZATION FALSE
        UNITY_BUILD OFF
        POSITION_INDEPENDENT_CODE ON
)

# ------------------------------------------------------------
# C++29 / Clang
# ------------------------------------------------------------

target_compile_options(
    ${PROJECT_NAME}::App
    PRIVATE
        -std=c++2d

        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wformat=2

        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g3>

        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
)

if(PROJECT_ENABLE_NATIVE)
    target_compile_options(
        ${PROJECT_NAME}::App
        PRIVATE
            -march=native
    )
endif()

# ------------------------------------------------------------
# Link-time optimization
# ------------------------------------------------------------

if(PROJECT_ENABLE_LTO)
    include(CheckIPOSupported)

    check_ipo_supported(
        RESULT ipo_supported
        OUTPUT ipo_error
        LANGUAGES CXX
    )

    if(ipo_supported)
        set_property(
            TARGET ${PROJECT_NAME}::App
            PROPERTY
                INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    else()
        message(
            WARNING
            "IPO is unavailable: ${ipo_error}"
        )
    endif()
endif()

# ------------------------------------------------------------
# Testing
# ------------------------------------------------------------

if(PROJECT_ENABLE_TESTS)
    include(CTest)

    if(BUILD_TESTING AND EXISTS
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

# ------------------------------------------------------------
# Installation
# ------------------------------------------------------------

include(GNUInstallDirs)

install(
    TARGETS ${PROJECT_NAME}::App
    RUNTIME DESTINATION
        "${CMAKE_INSTALL_BINDIR}"
)

# ------------------------------------------------------------
# Summary
# ------------------------------------------------------------

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Language mode: C++2d")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Configuration: ${CMAKE_BUILD_TYPE}")
message(STATUS "Modules: enabled")
message(STATUS "IPO requested: ${PROJECT_ENABLE_LTO}")
