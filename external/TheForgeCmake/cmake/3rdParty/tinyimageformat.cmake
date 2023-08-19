if(TARGET tinyimageformat)
    return()
endif()

add_library(tinyimageformat INTERFACE)
target_include_directories(tinyimageformat INTERFACE 
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat)
