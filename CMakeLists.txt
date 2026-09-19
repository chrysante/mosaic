cmake_minimum_required(VERSION 3.50)

project(mosaic
    VERSION 0.1.0
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Config 
option(MOSAIC_BUILD_SHARED
    "Build Mosaic as a shared library"
    ON
)
if(MOSAIC_BUILD_SHARED)
    set(MOSAIC_LIBRARY_TYPE SHARED)
else()
    set(MOSAIC_LIBRARY_TYPE STATIC)
endif()

function(mosaic_enable_warnings TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE
            /W4
        )
    else()
        target_compile_options(${TARGET} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endfunction()

# Platform
set(MOSAIC_PLATFORM_WINDOWS 0)
set(MOSAIC_PLATFORM_LINUX   0)
set(MOSAIC_PLATFORM_MACOS   0)

if(WIN32)
    set(MOSAIC_PLATFORM_WINDOWS 1)
elseif(APPLE)
    set(MOSAIC_PLATFORM_MACOS 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MOSAIC_PLATFORM_LINUX 1)
endif()

configure_file(
    ${PROJECT_SOURCE_DIR}/include/mosaic/config/platform.h.in
    ${PROJECT_BINARY_DIR}/generated/mosaic/config/platform.h
)

# Dependencies
include(FetchContent)

FetchContent_Declare(
    vml
    GIT_REPOSITORY https://github.com/chrysante/vml.git
    GIT_TAG v0.0.4
)

FetchContent_MakeAvailable(vml)

# Library
add_library(mosaic ${MOSAIC_LIBRARY_TYPE})

mosaic_enable_warnings(mosaic)

target_link_libraries(mosaic
    PUBLIC
        vml
)

target_sources(mosaic PRIVATE
    source/application/application.cpp
    source/application/window.cpp
)

if(MOSAIC_PLATFORM_MACOS)
target_link_libraries(mosaic PRIVATE
    "-framework AppKit"
)
target_sources(mosaic PRIVATE
    source/application/application_macos.mm
    source/application/window_macos.mm
)
endif()

target_include_directories(mosaic
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/generated
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/include/mosaic
        ${PROJECT_SOURCE_DIR}/source
)

# Example application
option(MOSAIC_BUILD_EXAMPLES "Build examples" ON)

if(MOSAIC_BUILD_EXAMPLES)

add_executable(mosaic_example)

mosaic_enable_warnings(mosaic_example)

target_sources(mosaic_example PRIVATE
    examples/mosaic_example.cpp
)

target_link_libraries(mosaic_example
    PRIVATE mosaic
)

endif()