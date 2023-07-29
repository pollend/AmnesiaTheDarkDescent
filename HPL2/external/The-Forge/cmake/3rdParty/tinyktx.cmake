if(TARGET tinyktx)
    return()
endif()

add_library(tinyktx INTERFACE)
target_include_directories(tinyktx INTERFACE 
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyktx)
