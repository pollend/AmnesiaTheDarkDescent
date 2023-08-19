if(TARGET tinydds)
    return()
endif()

add_library(tinydds INTERFACE)
target_include_directories(tinydds INTERFACE 
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinydds)



