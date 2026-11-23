cmake_minimum_required(VERSION 3.20)
project(Morpheus VERSION 0.1.0 LANGUAGES C CXX)
# Morpheus: Atelier's motion graphics and compositing app (After Effects' kind).
# SDL2 (window, input), OpenGL 3, Dear ImGui (the interface), stb (images, fonts).
# Only SDL2 has to be installed; Dear ImGui and stb are fetched here (or taken from external/ when present).
#
#   Morpheus                     the app window
#   Morpheus --script NAME --capture shot.bmp   scripted test + screenshot

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# Dear ImGui 1.90.4
if(EXISTS ${CMAKE_SOURCE_DIR}/external/imgui-1.90.4/imgui.h)
    set(FETCHCONTENT_SOURCE_DIR_IMGUI ${CMAKE_SOURCE_DIR}/external/imgui-1.90.4)
endif()
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.4.tar.gz)
FetchContent_MakeAvailable(imgui)

# stb single-file headers
set(STB_DIR ${CMAKE_SOURCE_DIR}/external/stb)
foreach(f stb_image.h stb_truetype.h)
    if(NOT EXISTS ${STB_DIR}/${f})
        file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/master/${f} ${STB_DIR}/${f})
    endif()
endforeach()

# SDL2: use the VC development package on Windows
if(WIN32 AND NOT SDL2_DIR AND EXISTS ${CMAKE_SOURCE_DIR}/external/SDL2-2.30.9/cmake)
    set(SDL2_DIR ${CMAKE_SOURCE_DIR}/external/SDL2-2.30.9/cmake)
endif()
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# Dear ImGui as a static library
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)

set(SOURCES
    src/main.cpp
    src/app.cpp
    src/splash.cpp
    src/lang.cpp
    src/panels.cpp
    src/project.cpp
    src/time_.cpp
    src/keyframe.cpp
    src/props.cpp
    src/raster.cpp
    src/render.cpp
    src/timeline.cpp
    src/icons.cpp
    src/selftest.cpp)
if(WIN32)
    list(APPEND SOURCES src/app.rc)
endif()

add_executable(Morpheus WIN32 ${SOURCES})
target_include_directories(Morpheus PRIVATE ${STB_DIR} src)
target_link_libraries(Morpheus PRIVATE imgui)
target_compile_definitions(Morpheus PRIVATE MORPHEUS_VERSION="${PROJECT_VERSION}")
if(TARGET SDL2::SDL2main)
    target_link_libraries(Morpheus PRIVATE SDL2::SDL2main)
endif()
if(MSVC)
    target_compile_options(Morpheus PRIVATE /W3 /utf-8 /O2 /fp:fast /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX)
else()
    target_link_libraries(Morpheus PRIVATE dl)
    target_compile_options(Morpheus PRIVATE -DNOMINMAX)
endif()

# Copy assets and lang next to the exe; SDL2.dll on Windows
add_custom_command(TARGET Morpheus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Morpheus>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lang $<TARGET_FILE_DIR:Morpheus>/lang)
if(WIN32)
    add_custom_command(TARGET Morpheus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:Morpheus>)
endif()
