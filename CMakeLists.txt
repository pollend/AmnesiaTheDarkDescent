cmake_minimum_required(VERSION 3.21)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(INSTALL_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)

# HACK to correctly set Linux property for newer cmake
if(NOT APPLE AND UNIX)
    set(LINUX TRUE)
ENDIF()

# Hack to correctly compile on windows ...
if(WIN32)
    add_compile_definitions("NOMINMAX=1")
    list(APPEND CMAKE_PREFIX_PATH "C:/workspace/vcpkg/packages/glew_x64-windows")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

option(COPY_GAME_ASSETS "copy games assets into output directory" ON)
option(WITH_TOOLS "build HPL2 with tools" ON)


add_subdirectory(HPL2)
add_subdirectory(amnesia/src)
