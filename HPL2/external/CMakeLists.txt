SET(VULKAN ON)


if( NOT THE_FORGE_DIR )
    set( THE_FORGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/The-Forge" CACHE STRING "Location of The-Forge." )
elseif( NOT IS_ABSOLUTE "${THE_FORGE_DIR}")
    get_filename_component(THE_FORGE_DIR "${THE_FORGE_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

include(cmake/The-Forge.cmake)

add_subdirectory(OALWrapper)
add_subdirectory(Newton)
add_subdirectory(AngelScript)