ExternalProject_Add(external_ogg
        URL http://downloads.xiph.org/releases/ogg/libogg-${libogg_Version}.tar.gz
        PREFIX ${BUILD_DIR}/ogg
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        INSTALL_DIR ${LIBDIR}/ogg
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ogg
        )
add_dependencies(external_ogg external_vorbis)