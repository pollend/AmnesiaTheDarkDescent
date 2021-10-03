Include(FetchContent)
SET(VULKAN ON)

if( NOT THE_FORGE_DIR )
    set( THE_FORGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/The-Forge" CACHE STRING "Location of The-Forge." )
elseif( NOT IS_ABSOLUTE "${THE_FORGE_DIR}")
    get_filename_component(THE_FORGE_DIR "${THE_FORGE_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

include(cmake/The-Forge.cmake)
add_subdirectory(abseil-cpp)
add_subdirectory(OALWrapper)
add_subdirectory(Newton)
add_subdirectory(AngelScript)


FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.0.0-preview3
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)