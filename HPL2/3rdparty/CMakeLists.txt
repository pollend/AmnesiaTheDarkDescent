cmake_minimum_required(VERSION 3.9.6)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")


include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(ExternalProject)

set(LIBDIR ${CMAKE_CURRENT_BINARY_DIR}/Debug)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)

set (libvorbis_Version 1.3.7)
set (libogg_Version 1.3.5)
set (libSDL2_Version 2.0.16)
set (libGLEW_Version glew-2.2.0)

include(cmake/ogg.cmake)
include(cmake/SDL2.cmake)
include(cmake/vorbis.cmake)
include(cmake/OALWrapper.cmake)
include(cmake/glew.cmake)
include(cmake/theora.cmake)
