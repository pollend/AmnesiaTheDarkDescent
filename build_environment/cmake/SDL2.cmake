ExternalProject_Add(external_SDL2
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        URL https://www.libsdl.org/release/SDL2-${libSDL2_Version}.tar.gz
        PREFIX ${BUILD_DIR}/SDL2
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/SDL2
        INSTALL_DIR ${LIBDIR}/SDL2
        )