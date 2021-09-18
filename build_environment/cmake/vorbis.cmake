ExternalProject_Add(external_vorbis
        URL http://downloads.xiph.org/releases/vorbis/libvorbis-${libvorbis_Version}.tar.gz
        PREFIX ${BUILD_DIR}/vorbis
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        INSTALL_DIR ${LIBDIR}/vorbis
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/vorbis
        )
