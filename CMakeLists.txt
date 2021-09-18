cmake_minimum_required(VERSION 3.9.6)


set(LIBDIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/linux")
file(GLOB LIB_SUBDIRS ${LIBDIR}/*)
set(CMAKE_PREFIX_PATH ${LIB_SUBDIRS})

OPTION(USE_SDL2 "Use SDL2 instead of SDL1.2" ON)

find_package(PkgConfig REQUIRED)
find_package(GLEW REQUIRED)
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenAL REQUIRED)
find_package(jpeg)

SET(FLTK_FLUID_EXECUTABLE "${LIBDIR}/FLTK/bin/fluid")
find_package(FLTK REQUIRED NO_MODULE)

pkg_check_modules(DevIL REQUIRED IMPORTED_TARGET IL)
pkg_check_modules(THEORA REQUIRED IMPORTED_TARGET theora)

add_subdirectory(HPL2)
add_subdirectory(amnesia/src)