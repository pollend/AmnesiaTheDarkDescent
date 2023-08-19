if(TARGET volk)
    return()
endif()

add_library(volk STATIC 
    ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/volk/volk.c)

target_include_directories(volk PUBLIC ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/volk)