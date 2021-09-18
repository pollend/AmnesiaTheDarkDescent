ExternalProject_Add(external_angelscript
    URL https://www.angelcode.com/angelscript/sdk/files/angelscript_${libAngleScript_Version}.zip
    PREFIX ${BUILD_DIR}/angelscript
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND mkdir -p ${LIBDIR}/angelscript/include; mkdir -p ${LIBDIR}/angelscript/lib
    PATCH_COMMAND patch -p0 -l --binary  < ${DEPENDENCY_ROOT}/patches/angelscript.patch
    BUILD_COMMAND  ${MAKE_EXECUTABLE} -C angelscript/projects/gnuc
    INSTALL_COMMAND ${MAKE_EXECUTABLE} install LOCAL=${LIBDIR}/angelscript -C angelscript/projects/gnuc
    INSTALL_DIR ${LIBDIR}/angelscript
)