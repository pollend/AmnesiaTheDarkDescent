ExternalProject_Add(external_theora
        URL http://downloads.xiph.org/releases/theora/libtheora-${libtheora_Version}.tar.bz2
        PREFIX ${BUILD_DIR}/theora
        BUILD_IN_SOURCE TRUE
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/downloads
        CONFIGURE_COMMAND ./configure 
                --with-vorbis="${BUILD_DIR}/vorbis"
                --with-ogg="${BUILD_DIR}/ogg"
                --disable-examples 
                --disable-shared 
                --disable-sdltest   
                --disable-vorbistest
                --disable-spec 
                --prefix ${LIBDIR}/theora 
        BUILD_COMMAND make
        INSTALL_COMMAND ${MAKE_EXECUTABLE} install
        INSTALL_DIR ${LIBDIR}/theora
        )

add_dependencies(external_theora external_ogg external_vorbis)