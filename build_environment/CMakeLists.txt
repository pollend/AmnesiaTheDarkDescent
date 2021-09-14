cmake_minimum_required(VERSION 3.9.6)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(ExternalProject)

set(GLEW_BUILD "make extensions")
find_program(MAKE_EXECUTABLE
        NAMES gmake mingw32-make make
        NAMES_PER_DIR
        DOC "GNU Make")

set(LIB_INSTALL_PATH ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "Install path for the build dependencies")

set(LIBDIR ${LIB_INSTALL_PATH}/linux)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)
set(DEPENDENCY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set (libtheora_Version 1.1.1)
set (libvorbis_Version 1.3.7)
set (libogg_Version 1.3.5)
set (libSDL2_Version 2.0.16)
set (libGLEW_Version glew-2.2.0)
set (libAngleScript_Version 2.19.2)

include(cmake/ogg.cmake)
include(cmake/SDL2.cmake)
include(cmake/vorbis.cmake)
include(cmake/OALWrapper.cmake)
include(cmake/glew.cmake)
#include(cmake/theora.cmake)
include(cmake/DevIL.cmake)
include(cmake/angelscript.cmake)
include(cmake/newton.cmake)
