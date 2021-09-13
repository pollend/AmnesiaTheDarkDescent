ExternalProject_Add(external_OALWrapper
        GIT_REPOSITORY https://github.com/pollend/OALWrapper
        PREFIX ${BUILD_DIR}/OALWrapper
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        GIT_TAG        origin/master
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/OALWrapper -DUSE_SDL2=True
            -DSDL_LIBRARY=${LIBDIR}/SDL2/lib
            -DSDL_INCLUDE_DIR=${LIBDIR}/SDL2/include/SDL2/
        INSTALL_DIR ${LIBDIR}/OALWrapper
        )

add_dependencies(
        external_OALWrapper
        external_SDL2
)