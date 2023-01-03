include(FetchContent)

# maybeeeee use folly
# FetchContent_Declare(
#     folly
#     URL "https://github.com/facebook/folly/archive/v2019.09.30.00.zip"
#     URL_HASH SHA512=2edb1e0b50a75604530d8bdcc35cacef500cb8b1466d546b3a201882da69cfba1b91493225b71d5bcf99f610d5b1668b115edebcfb25f9e36dc75e8632afaa58)
# FetchContent_GetProperties(folly)
# if(NOT folly_POPULATED)
#     message(STATUS "Downloading folly...")
#     FetchContent_Populate(folly)
# endif()

FetchContent_Declare(  
  bgfx_cmake
  GIT_REPOSITORY git@github.com:bkaradzic/bgfx.cmake.git
)
set(BGFX_OPENGL_VERSION 33)
set(BGFX_INSTALL OFF)
set(BGFX_INSTALL_EXAMPLES OFF)
set(BGFX_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(bgfx_cmake) 

add_subdirectory(abseil-cpp)
add_subdirectory(OALWrapper)
add_subdirectory(Newton)
add_subdirectory(AngelScript)