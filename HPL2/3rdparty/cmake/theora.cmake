ExternalProject_Add(external_theora
        URL http://downloads.xiph.org/releases/theora/libtheora-${libtheora_Version}.tar.bz2
        PREFIX ${BUILD_DIR}/theora
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        INSTALL_DIR ${LIBDIR}/theora
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/theora
        )