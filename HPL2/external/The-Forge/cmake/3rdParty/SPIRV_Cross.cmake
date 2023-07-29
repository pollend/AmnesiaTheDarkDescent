if(TARGET SPIRV_Cross)
    return()
endif()

file(GLOB SPIRV_CROSS_SOURCES ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/SPIRV_Cross/*.cpp)

add_library(SPIRV_Cross STATIC ${SPIRV_CROSS_SOURCES})
set_property(TARGET SPIRV_Cross PROPERTY CXX_STANDARD 17)
