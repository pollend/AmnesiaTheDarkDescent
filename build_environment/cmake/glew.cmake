ExternalProject_Add(external_GLEW
        GIT_REPOSITORY https://github.com/nigels-com/glew.git
        PREFIX ${BUILD_DIR}/GLEW
        GIT_TAG        ${libGLEW_Version}
        BUILD_IN_SOURCE TRUE
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${MAKE_EXECUTABLE} extensions; ${MAKE_EXECUTABLE}
        INSTALL_COMMAND ${MAKE_EXECUTABLE} install DESTDIR=${LIBDIR}/GLEW GLEW_DEST=/
        INSTALL_DIR ${LIBDIR}/GLEW
        )