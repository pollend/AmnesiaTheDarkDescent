cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(INSTALL_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_INSTALL_PREFIX ${INSTALL_OUTPUT_PATH} CACHE PATH "Install path prefix, prepended onto install directories." FORCE)

enable_language(ASM)

if(MSVC)
    add_definitions(/MP /std:c++20)
endif()

if(UNIX)
    set(VCPKG_CMAKE_SYSTEM_NAME Linux)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

add_definitions(
    # -DUSE_FORWARD_PLUS_BACKEND # prototype forward renderer
)

set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# hack to get to build on windwos
# HACK to correctly set Linux property for newer cmake
if(NOT APPLE AND UNIX)
    set(LINUX TRUE)
ENDIF()

execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE AMNESIA_TDD_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

# Hack to correctly compile on windows ...
if(WIN32)
    add_compile_definitions("NOMINMAX=1")
endif()

option(COPY_GAME_ASSETS "copy games assets into output directory" OFF)
option(WITH_TOOLS "build HPL2 with tools" ON)

add_subdirectory(external)
add_subdirectory(HPL2)
add_subdirectory(tools)
add_subdirectory(amnesia)

