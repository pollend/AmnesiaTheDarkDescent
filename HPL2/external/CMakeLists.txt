include(FetchContent)

FetchContent_Declare(  
  bgfx_cmake
  GIT_REPOSITORY git@github.com:bkaradzic/bgfx.cmake.git
)
set(BGFX_INSTALL OFF)
set(BGFX_INSTALL_EXAMPLES OFF)
set(BGFX_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(bgfx_cmake) 

add_subdirectory(abseil-cpp)
add_subdirectory(OALWrapper)
add_subdirectory(Newton)
add_subdirectory(AngelScript)