if(TARGET eastl)
    return()
endif()

file(GLOB EASTL_SOURCES
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/EASTL/*.cpp)

add_library(Eastl STATIC ${EASTL_SOURCES})
# target_include_directories(lua PUBLIC ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/EASTL)

