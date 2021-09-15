cmake_minimum_required (VERSION 2.8.11)
project(Launcher)

find_package(fltk)
find_package(fltk_images)
find_package(jpeg)

add_executable(Launcher
    launcher.cpp
    Main.cpp
    QualityChooser.cpp
    LauncherHelper.cpp)
target_link_libraries(Launcher HPL2 ${FLTK_IMAGES_LIBRARY} ${FLTK_LIBRARY} X11 Xext Xft fontconfig ${JPEG_LIBRARY})

# vim: et ts=4
