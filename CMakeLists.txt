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

# VML
FetchContent_Declare(
    vml
    GIT_REPOSITORY https://github.com/chrysante/vml.git
    GIT_TAG v0.0.4
)

FetchContent_MakeAvailable(vml)

# ImGui
FetchContent_Declare(
    ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 4212de7d5b651895ccfe728f4eef9595e1d8d38a
)
FetchContent_MakeAvailable(ImGui)
add_library(ImGui STATIC)

target_sources(ImGui PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)

if(MOSAIC_PLATFORM_MACOS)
target_sources(ImGui PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_osx.mm
    ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
)
target_compile_options(ImGui PRIVATE
    $<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>
)
endif()

target_include_directories(ImGui 
PUBLIC
    ${imgui_SOURCE_DIR}
PRIVATE 
    ${imgui_SOURCE_DIR}/backends
)

# Library
add_library(mosaic ${MOSAIC_LIBRARY_TYPE})

mosaic_enable_warnings(mosaic)

target_link_libraries(mosaic
    PUBLIC
        vml
        ImGui
)

target_sources(mosaic PRIVATE
    source/application/application.cpp
    source/application/window.cpp
)

if(MOSAIC_PLATFORM_MACOS)
target_link_libraries(mosaic PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework GameController"
    "-framework QuartzCore"
    "-framework CoreVideo"
)
target_sources(mosaic PRIVATE
    source/application/application_macos.mm
    source/application/window_macos.mm
)
target_compile_options(mosaic PRIVATE
    $<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>
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