cmake_minimum_required(VERSION 3.50)

project(mosaic
    VERSION 0.1.0
    LANGUAGES CXX OBJCXX
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

function(mosaic_enable_tsan TARGET)
    if(not MSVC)
        target_compile_options(${TARGET} PRIVATE
            -fsanitize=thread
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

# utility
FetchContent_Declare(
    utility
    GIT_REPOSITORY https://github.com/chrysante/utility.git
    GIT_TAG v1.1.1
)
FetchContent_MakeAvailable(utility)

# termfmt
FetchContent_Declare(
    termfmt
    GIT_REPOSITORY https://github.com/chrysante/termfmt.git
    GIT_TAG 0.0.1
)
FetchContent_MakeAvailable(termfmt)

# json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/catch2.git
    GIT_TAG v3.16.0
)
FetchContent_MakeAvailable(Catch2)

# magic_enum
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.8
)
FetchContent_MakeAvailable(magic_enum)

# ImGui
add_library(ImGui STATIC)

set(imgui_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

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

generate_export_header(mosaic
    EXPORT_FILE_NAME
        ${PROJECT_BINARY_DIR}/generated/mosaic/support/export.h
)

target_link_libraries(mosaic
    PUBLIC
        vml
        utility
        ImGui
        magic_enum
    PRIVATE
        nlohmann_json::nlohmann_json
        termfmt
)

target_sources(mosaic PRIVATE
    source/application/application.cpp
    source/application/input.cpp
    source/application/input_event.cpp
    source/application/window.cpp
    source/service/completion.cpp
    source/service/dispatcher.cpp
    source/service/event_emitter.cpp
    source/service/event_receiver.cpp
    source/service/event_system.cpp
    source/service/font_manager.cpp
    source/service/thread_pool.cpp
    source/gui/command_palette.cpp
    source/gui/font.cpp
    source/gui/ui_context.cpp
    source/gui/toolbar.cpp
    source/support/logger.cpp
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
    source/application/input_macos.mm
    source/application/resource_util_macos.mm
    source/application/window_macos.mm
)
target_compile_options(mosaic PRIVATE
    $<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>
)
endif()

target_include_directories(mosaic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include/mosaic
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

function(mosaic_configure_app target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "mosaic_configure_app: '${target_name}' is not a target")
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resource"
            "$<TARGET_FILE_DIR:${target_name}>/resource"
        COMMENT "Copying resources for ${target_name}"
    )
endfunction()

# Tests
add_executable(mosaic_test)
mosaic_enable_warnings(mosaic_test)

target_sources(mosaic_test PRIVATE
    test/mosaic/service/event_system.t.cpp
    test/mosaic/service/thread_pool.t.cpp
    test/mosaic/service/dispatcher.t.cpp
)

target_link_libraries(mosaic_test PRIVATE mosaic Catch2::Catch2WithMain)

# Example application
option(MOSAIC_BUILD_EXAMPLES "Build examples" ON)

if(MOSAIC_BUILD_EXAMPLES)

add_executable(mosaic_example)
mosaic_enable_warnings(mosaic_example)
mosaic_configure_app(mosaic_example)

target_sources(mosaic_example PRIVATE
    examples/mosaic_example.cpp
)

target_link_libraries(mosaic_example
    PRIVATE mosaic
)

endif()