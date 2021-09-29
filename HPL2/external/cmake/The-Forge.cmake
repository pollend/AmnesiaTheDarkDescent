#OS
file(GLOB FORGE_OS_INTERFACES "${THE_FORGE_DIR}/Common_3/OS/Interfaces/*.*")
file(GLOB FORGE_OS_CORE "${THE_FORGE_DIR}/Common_3/OS/Core/*.*")
file(GLOB FORGE_OS_IMAGE "${THE_FORGE_DIR}/Common_3/OS/Image/*.*")
file(GLOB FORGE_OS_LOGGING "${THE_FORGE_DIR}/Common_3/OS/Logging/*.*")
file(GLOB FORGE_OS_MATH "${THE_FORGE_DIR}/Common_3/OS/Math/*.*")
file(GLOB FORGE_OS_MEMORYTRACKING "${THE_FORGE_DIR}/Common_3/OS/MemoryTracking/*.*")
file(GLOB FORGE_OS_PROFILER "${THE_FORGE_DIR}/Common_3/OS/Profiler/*.*")

#OS filesystem
set(FORGE_OS_FILESYSTEM
        ${THE_FORGE_DIR}/Common_3/OS/FileSystem/FileSystem.cpp
        ${THE_FORGE_DIR}/Common_3/OS/FileSystem/SystemRun.cpp
        ${THE_FORGE_DIR}/Common_3/OS/FileSystem/ZipFileSystem.c
        )

if(UNIX AND NOT APPLE)
    set(FORGE_OS_FILESYSTEM ${FORGE_OS_FILESYSTEM} ${THE_FORGE_DIR}/Common_3/OS/FileSystem/UnixFileSystem.cpp)
endif()


if(UNIX AND NOT APPLE)
    file(GLOB FORGE_OS_INTERFACES_LINUX ${THE_FORGE_DIR}/Common_3/OS/Linux/*.*)
    list (APPEND FORGE_OS_INTERFACES ${FORGE_OS_INTERFACES_LINUX})
endif()

#Renderer
set(FORGE_RENDERER
        ${THE_FORGE_DIR}/Common_3/Renderer/CommonShaderReflection.cpp
        ${THE_FORGE_DIR}/Common_3/Renderer/IRay.h
        ${THE_FORGE_DIR}/Common_3/Renderer/IRenderer.h
        ${THE_FORGE_DIR}/Common_3/Renderer/IResourceLoader.h
        ${THE_FORGE_DIR}/Common_3/Renderer/IShaderReflection.h
        ${THE_FORGE_DIR}/Common_3/Renderer/Renderer.cpp
        ${THE_FORGE_DIR}/Common_3/Renderer/ResourceLoader.cpp
        )

#eastl
file(GLOB FORGE_EASTL ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/EASTL/*.*)

#basis transcoder
file(GLOB FORGE_BASIS_TRANSCODER "${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/basis_universal/transcoder/*.*")

#zip
file(GLOB FORGE_ZIP "${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/zip/*.*")

#spirv tools
set(FORGE_SPIRVTOOLS
        ${THE_FORGE_DIR}/Common_3/Tools/SpirvTools/SpirvTools.h
        ${THE_FORGE_DIR}/Common_3/Tools/SpirvTools/SpirvTools.cpp
        )

#spirv-cross
file(GLOB FORGE_SPIRVCROSS "${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/SPIRV_Cross/*.*")

#rmem
set(FORGE_RMEM
        ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/rmem/src/rmem_get_module_info.cpp
        ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/rmem/src/rmem_hook.cpp
        ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/rmem/src/rmem_lib.cpp
        )

#lua lib
file(GLOB FORGE_LUA "${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/lua-5.3.5/src/*.*")
#remove luac.c
list(REMOVE_ITEM FORGE_LUA "${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/lua-5.3.5/src/luac.c")

#lua middleware
file(GLOB FORGE_LUA_MIDDLEWARE "${THE_FORGE_DIR}/Middleware_3/LUA/*.*")


file(GLOB FORGE_TEXT "${THE_FORGE_DIR}/Middleware_3/Text/*.*")
file(GLOB FORGE_UI_IMGUI "${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/imgui/*.*")

if(D3D12)
    set(FORGE_DEFS ${FORGE_DEFS} DIRECT3D12)
    file(GLOB FORGE_RENDERER_D3D12 "${THE_FORGE_DIR}/Common_3/Renderer/Direct3D12/*.*")
    set(FORGE_RENDERER ${FORGE_RENDERER} ${FORGE_RENDERER_D3D12})
endif()

if(VULKAN)
    find_package(Vulkan REQUIRED)
    set(FORGE_DEFS ${FORGE_DEFS} VULKAN)
    set(FORGE_INCLUDES ${FORGE_INCLUDES} ${Vulkan_INCLUDE_DIRS})
    file(GLOB FORGE_RENDERER_VK "${THE_FORGE_DIR}/Common_3/Renderer/Vulkan/*.*")
    set(FORGE_RENDERER ${FORGE_RENDERER} ${FORGE_RENDERER_VK})
endif()

#includes
set(FORGE_INCLUDES ${FORGE_INCLUDES}
        ${THE_FORGE_DIR}
        ${THE_FORGE_DIR}/Common_3/OS
        ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource
        )

#definitions
set(FORGE_DEFS ${FORGE_DEFS} USE_LOGGING )

if(UNIX)
    set(FORGE_DEFS ${FORGE_DEFS} LUA_USE_LINUX)
endif()

set(SOURCE_LIST
        ${FORGE_OS_FILESYSTEM}
        ${FORGE_OS_INTERFACES}
        ${FORGE_OS_CORE}
        ${FORGE_OS_IMAGE}
        ${FORGE_OS_LOGGING}
        ${FORGE_OS_MATH}
        ${FORGE_OS_MEMORYTRACKING}
        ${FORGE_OS_PROFILER}
        ${FORGE_RENDERER}
        ${FORGE_EASTL}
        ${FORGE_SPIRVTOOLS}
        ${FORGE_SPIRVCROSS}
        ${FORGE_BASIS_TRANSCODER}
        ${FORGE_TEXT}
        ${FORGE_UI_IMGUI}
        ${FORGE_RMEM}
        ${FORGE_LUA}
        ${FORGE_LUA_MIDDLEWARE})


find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

#add the lib
add_library(the-forge STATIC ${SOURCE_LIST})

target_link_libraries(the-forge ${GTK3_LIBRARIES})
target_include_directories(the-forge PRIVATE ${GTK3_INCLUDE_DIRS})

target_include_directories(the-forge PUBLIC ${FORGE_INCLUDES})
target_compile_definitions(the-forge PUBLIC ${FORGE_DEFS})

target_compile_definitions(the-forge PUBLIC $<$<CONFIG:Debug>:_DEBUG>)
target_compile_definitions(the-forge PUBLIC $<$<CONFIG:Debug>:USE_MEMORY_TRACKING>)

#eastl needs this enabled
if(MSVC)
    set_target_properties(the-forge PROPERTIES COMPILE_FLAGS "/Zc:wchar_t")
endif()

#Project solution folders
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST})