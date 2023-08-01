if(TARGET minizip)
    return()
endif()

file(GLOB MINIZIP_SOURCES 
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/minizip/*.c
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/lib/brg/*.c)

add_library(minizip STATIC ${MINIZIP_SOURCES})



