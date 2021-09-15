cmake_minimum_required(VERSION 3.1)
project(HPL2)

set(HPL2_VERSION_MAJOR 2)
set(HPL2_VERSION_MINOR 0)

set(HPL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(LIBDIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/linux")
file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})

set(WITH_STATIC_LIBS ON)

find_package(PkgConfig REQUIRED) 

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

pkg_search_module(THEORA REQUIRED IMPORTED_TARGET theora)
include_directories(${THEORA_INCLUDE_DIRS})

pkg_search_module(DevIL REQUIRED IMPORTED_TARGET IL)
include_directories(${DevIL_INCLUDE_DIRS})

set(ANGLESCRIPT_DIR "${LIBDIR}/angelscript")
set(ANGLESCRIPT_INCLUDE_DIRS "${ANGLESCRIPT_DIR}/include")
set(ANGLESCRIPT_LIBRARY "${ANGLESCRIPT_DIR}/lib/libangelscript.a")

include_directories(${ANGLESCRIPT_INCLUDE_DIRS})

set(NEWTON_DIR "${LIBDIR}/newton")
set(NEWTON_INCLUDE_DIRS "${NEWTON_DIR}/include")
set(NEWTON_LIBRARY "${NEWTON_DIR}/lib/libNewton.a")
include_directories(${NEWTON_INCLUDE_DIRS})

OPTION(USE_SDL2 "Use SDL2 instead of SDL1.2" ON)
add_subdirectory(OALWrapper)
# add_subdirectory(core)

