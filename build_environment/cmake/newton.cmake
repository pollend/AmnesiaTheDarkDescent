ExternalProject_Add(external_newton
        URL file://${DEPENDENCY_ROOT}/deps/Newton
        PREFIX ${BUILD_DIR}/newton
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        INSTALL_DIR ${LIBDIR}/newton
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/newton
        )
