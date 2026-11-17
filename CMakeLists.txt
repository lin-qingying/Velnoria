cmake_minimum_required(VERSION 3.24)

project(Lumenvox
    VERSION 0.1.0
    DESCRIPTION "Clean-room compatible client core"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(LUMENVOX_BUILD_TESTS "Build unit and property tests" ON)
option(LUMENVOX_BUILD_TOOLS "Build command line tools" ON)
option(LUMENVOX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Shared warning baseline. Kept in one place so every target inherits the same set.
add_library(lumenvox_warnings INTERFACE)
if(MSVC)
    target_compile_options(lumenvox_warnings INTERFACE
        /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus
        $<$<BOOL:${LUMENVOX_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(lumenvox_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
        $<$<BOOL:${LUMENVOX_WARNINGS_AS_ERRORS}>:-Werror>)
endif()
add_library(Lumenvox::Warnings ALIAS lumenvox_warnings)

add_subdirectory(core)

if(LUMENVOX_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(LUMENVOX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
