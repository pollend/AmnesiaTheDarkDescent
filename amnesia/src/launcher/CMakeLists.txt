cmake_minimum_required (VERSION 2.8.11)
project(Launcher)

SET(FLTK_FLUID_EXECUTABLE "${LIBDIR}/FLTK/bin/fluid")

FIND_PACKAGE(FLTK REQUIRED)
find_package(fltk_images)
find_package(jpeg)

include_directories(${FLTK_INCLUDE_DIR})

add_executable(Launcher
    launcher.cpp
    Main.cpp
    QualityChooser.cpp
    LauncherHelper.cpp)
target_link_libraries(Launcher HPL2 ${FLTK_IMAGES_LIBRARY} ${FLTK_LIBRARY} X11 Xext Xft fontconfig ${JPEG_LIBRARY})

# vim: et ts=4
