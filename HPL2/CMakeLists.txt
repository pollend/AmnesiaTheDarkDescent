project(HPL2)
set(HPL2_VERSION_MAJOR 2)
set(HPL2_VERSION_MINOR 0)

set(HPL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(extern)
add_subdirectory(core)
add_subdirectory(tools)

