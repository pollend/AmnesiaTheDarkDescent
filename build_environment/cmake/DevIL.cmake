ExternalProject_Add(external_DevIL
    GIT_REPOSITORY https://github.com/DentonW/DevIL.git
    PREFIX ${BUILD_DIR}/DevIL
    GIT_TAG        origin/master
    SOURCE_SUBDIR DevIL
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/DevIL
)