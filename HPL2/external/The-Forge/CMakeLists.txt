cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)

project(The-Forge)


option( THE_FORGE_VULKAN                "Build Vulkan"                             ON)
option( THE_FORGE_D3D12                 "Build D3D12"                              OFF)
option( THE_FORGE_D3D11                 "Build D3D11"                              OFF)
option( THE_FORGE_METAL                 "Build Metal"                              OFF)
option( THE_FORGE_BUILD_EXAMPLES        "Build examples"                           OFF)

option( THE_FORGE_SCRIPTING_LUA        "Build Lua (IScripting.h)"                  OFF)
option( THE_FORGE_INPUT_SYSTEM         "Build Input System"                        OFF)
option( THE_FORGE_OZZ_ANIMATION        "Build OZZ Animation System"                OFF)
option( THE_FORGE_NOESIS_GUI           "Build Noesis GUI"                          ON)

option(THE_FORGE_UNIT_TEST            "Build Unit Test"                            OFF)

set(THE_FORGE_EXTERNAL_CONFIG_FILE          "" CACHE STRING "Location of The-Forge config file"       )

if(NOT THE_FORGE_DIR)
	set(THE_FORGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/The-Forge" CACHE STRING "Location of the forge.")
elseif(NOT IS_ABSOLUTE "${THE_FORGE_DIR}")
	get_filename_component(BX_DIR "${THE_FORGE_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

IF(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -stdlib=libc++ -x objective-c++")
    target_compile_options(The-Forge PRIVATE "-fobjc-arc")
endif()

include( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/The-Forge.cmake )