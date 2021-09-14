cmake_minimum_required(VERSION 3.0)
project(HPL2)

set(HPL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(LIBDIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/linux")
file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})

set(WITH_STATIC_LIBS ON)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

OPTION(USE_SDL2 "Use SDL2 instead of SDL1.2" ON)
add_subdirectory(OALWrapper)
add_subdirectory(core)

