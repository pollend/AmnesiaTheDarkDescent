if(TARGET cgltf)
    return()
endif()

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE 
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/cgltf)
