cmake_minimum_required(VERSION 3.0)
project(HPL2)

set(HPL2_VERSION_MAJOR 2)
set(HPL2_VERSION_MINOR 0)

set(HPL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# include(UtilityFunctions)
# include(BoilerPlate)

set(LIBDIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/linux")
file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})

set(WITH_STATIC_LIBS ON)

find_package(PkgConfig REQUIRED) 

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

pkg_check_modules(DevIL REQUIRED IL)

set(ANGLESCRIPT_DIR "${LIBDIR}/angelscript")
set(ANGLESCRIPT_INCLUDE_DIRS "${ANGLESCRIPT_DIR}/include")
set(ANGLESCRIPT_LIBRARY "${ANGLESCRIPT_DIR}/lib/libangelscript.a")

set(NEWTON_DIR "${LIBDIR}/newton")
set(NEWTON_INCLUDE_DIRS "${NEWTON_DIR}/include")
set(NEWTON_LIBRARY "${NEWTON_DIR}/lib/libNewton.a")

OPTION(USE_SDL2 "Use SDL2 instead of SDL1.2" ON)
add_subdirectory(OALWrapper)
# add_subdirectory(core)

