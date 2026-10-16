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

# Keep assertions enabled in RelWithDebInfo.
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-UNDEBUG>
    )
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

# Sanitizers
set(MOSAIC_SANITIZER "" CACHE STRING
    "Sanitizer to use: none, asan, tsan")

set_property(CACHE MOSAIC_SANITIZER PROPERTY STRINGS
    none asan tsan)

add_library(mosaic_sanitizer INTERFACE)
if(NOT MSVC)
    if(MOSAIC_SANITIZER STREQUAL "asan")
        target_compile_options(mosaic_sanitizer INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(mosaic_sanitizer INTERFACE
            -fsanitize=address,undefined
        )
    elseif(MOSAIC_SANITIZER STREQUAL "tsan")
        target_compile_options(mosaic_sanitizer INTERFACE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
        target_link_options(mosaic_sanitizer INTERFACE
            -fsanitize=thread
        )
    endif()
endif()


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
    GIT_TAG 53a15c7fff7665d528904d76508693984279e3be
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(vml)

# utility
FetchContent_Declare(
    utility
    GIT_REPOSITORY https://github.com/chrysante/utility.git
    GIT_TAG v1.1.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(utility)

# utility
FetchContent_Declare(
    csp
    GIT_REPOSITORY https://github.com/chrysante/csp.git
    GIT_TAG v0.0.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(csp)

# termfmt
FetchContent_Declare(
    termfmt
    GIT_REPOSITORY https://github.com/chrysante/termfmt.git
    GIT_TAG 0.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(termfmt)

# json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/catch2.git
    GIT_TAG v3.16.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# magic_enum
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(magic_enum)

# yaml-cpp
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.9.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

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
        mosaic_sanitizer
        vml
        utility
        csp
        ImGui
        magic_enum
        yaml-cpp::yaml-cpp
    PRIVATE
        nlohmann_json::nlohmann_json
        termfmt
)

target_sources(mosaic PRIVATE
    source/application/application.cpp
    source/application/debug_tools.cpp
    source/application/input.cpp
    source/application/input_event.cpp
    source/application/window.cpp
    source/gui/appearance.cpp
    source/gui/command_palette.cpp
    source/gui/dockspace.cpp
    source/gui/font.cpp
    source/gui/ui_context.cpp
    source/gui/toolbar.cpp
    source/gui/toolbar_renderer.cpp
    source/gui/view.cpp
    source/gui/view_demo.cpp
    source/gui/view_manager.cpp
    source/gui/view_registry.cpp
    source/gui/view_renderer.cpp
    source/service/completion.cpp
    source/service/dispatcher.cpp
    source/service/event_emitter.cpp
    source/service/event_receiver.cpp
    source/service/event_system.cpp
    source/service/font_manager.cpp
    source/service/service_context.cpp
    source/service/thread_pool.cpp
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
    source/application/debug_tools_macos.mm
    source/application/directories_macos.mm
    source/application/input_macos.mm
    source/application/resource_util_macos.mm
    source/application/window_macos.mm
    source/gui/appearance_macos.mm
    source/imgui/imgui_impl_mosaic_macos.mm
    source/imgui/imgui_impl_mosaic_metal.mm
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

# Set this once in the top-level project.
set(MOSAIC_BUNDLE_IDENTIFIER_PREFIX "com.mosaic"
    CACHE STRING "Default macOS bundle identifier prefix")

function(mosaic_configure_app target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR
            "mosaic_configure_app: '${target_name}' is not a target")
    endif()
    cmake_parse_arguments(
        ARG
        ""
        "BUNDLE_IDENTIFIER"
        ""
        ${ARGN}
    )
    if(APPLE)
        if(ARG_BUNDLE_IDENTIFIER)
            set(bundle_identifier "${ARG_BUNDLE_IDENTIFIER}")
        else()
            set(bundle_identifier
                "${MOSAIC_BUNDLE_IDENTIFIER_PREFIX}.${target_name}")
        endif()
        set_target_properties(${target_name} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${target_name}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${bundle_identifier}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
            MACOSX_BUNDLE_BUNDLE_VERSION "1"
        )
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/resource"
                    "$<TARGET_BUNDLE_CONTENT_DIR:${target_name}>/Resources"
                COMMENT "Copying resources for ${target_name}"
            )
        endif()
    else()
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/resource"
                    "$<TARGET_FILE_DIR:${target_name}>/resource"
                COMMENT "Copying resources for ${target_name}"
            )
        endif()
    endif()
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