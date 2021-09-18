project(Launcher)

add_executable(Launcher
    launcher.cpp
    Main.cpp
    QualityChooser.cpp
    LauncherHelper.cpp)

target_link_libraries(Launcher
        HPL2
        fltk
        X11
        Xext
        Xft
        jpeg
        fltk_images
        fontconfig ${JPEG_LIBRARY})
target_include_directories(Launcher PRIVATE ${FLTK_INCLUDE_DIR})

