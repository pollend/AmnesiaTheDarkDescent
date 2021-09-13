ExternalProject_Add(external_GLEW
        GIT_REPOSITORY https://github.com/nigels-com/glew.git
        PREFIX ${BUILD_DIR}/GLEW
        GIT_TAG        ${libGLEW_Version}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/GLEW
        SOURCE_DIR build/cmake
        INSTALL_DIR ${LIBDIR}/GLEW
        )