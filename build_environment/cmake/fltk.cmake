ExternalProject_Add(external_fltk
        URL https://www.fltk.org/pub/fltk/${libFLTK_Version}/fltk-${libFLTK_Version}-source.tar.gz
        PREFIX ${BUILD_DIR}/FLTK
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        INSTALL_DIR ${LIBDIR}/FLTK
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/FLTK -DFLTK_BUILD_EXAMPLES=OFF -DFLTK_BUILD_TEST=OFF 
        )
