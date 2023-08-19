if(TARGET tinyEXR)
    return()
endif()

add_library(tinyEXR STATIC ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp)
#target_include_directories(tinyEXR PRIVATE 
#    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/TinyEXR)

  

