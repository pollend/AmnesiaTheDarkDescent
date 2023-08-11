#  Graphics

find_package(PkgConfig REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/lua.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/cpu_features.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/eastl.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/Bstr.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/astc-encoder.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/cgltf.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/meshoptimizer.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/tinydds.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/tinyimageformat.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/tinyktx.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/basis_universal.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/imgui.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/ga.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/nvapi.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/ags.cmake)

file(GLOB THE_FORGE_SOURCES
    ${THE_FORGE_DIR}/Common_3/Graphics/*.cpp
    ${THE_FORGE_DIR}/Common_3/Application/*.cpp
    ${THE_FORGE_DIR}/Common_3/Application/Profiler/*.cpp
    ${THE_FORGE_DIR}/Common_3/Application/Fonts/FontSystem.cpp
    ${THE_FORGE_DIR}/Common_3/Application/UI/UI.cpp
    ${FORGE_APP_DIR}/Common_3/Application/Profiler/*.cpp
    ${THE_FORGE_DIR}/Common_3/OS/WindowSystem/*.cpp
    ${THE_FORGE_DIR}/Common_3/OS/*.cpp

    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/*.cpp

    ${THE_FORGE_DIR}/Common_3/Utilities/FileSystem/FileSystem.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/FileSystem/SystemRun.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/Log/*.c
    ${THE_FORGE_DIR}/Common_3/Utilities/Math/*.c
    ${THE_FORGE_DIR}/Common_3/Utilities/MemoryTracking/*.c
    ${THE_FORGE_DIR}/Common_3/Utilities/Threading/*.c
    ${THE_FORGE_DIR}/Common_3/Utilities/*.c
)

add_library(TheForge STATIC ${THE_FORGE_SOURCES})
target_link_libraries(TheForge PUBLIC cpu_features Eastl imgui astc-encoder cgltf Bstr basis_universal meshoptimizer tinydds tinyimageformat tinyktx)
target_include_directories(TheForge PUBLIC ${THE_FORGE_DIR})

IF(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    file(GLOB THE_FORGE_OS_DARWIN_SOURCES
        ${THE_FORGE_DIR}/Common_3/OS/Darwin/*.cpp
        ${THE_FORGE_DIR}/Common_3/OS/Darwin/*.c)
    target_sources(TheForge PRIVATE ${THE_FORGE_OS_DARWIN_SOURCES})
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    file(GLOB THE_FORGE_OS_WINDOWS_SOURCES
        ${THE_FORGE_DIR}/Common_3/OS/Windows/*.cpp
        ${THE_FORGE_DIR}/Common_3/OS/Windows/*.c)
        
    target_sources(TheForge PRIVATE ${THE_FORGE_OS_WINDOWS_SOURCES})
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")

    file(GLOB THE_FORGE_OS_LINUX_SOURCES
        ${THE_FORGE_DIR}/Common_3/OS/Linux/*.cpp
        ${THE_FORGE_DIR}/Common_3/OS/Linux/*.c
        ${THE_FORGE_DIR}/Common_3/Utilities/FileSystem/UnixFileSystem.cpp)
    target_sources(TheForge PRIVATE ${THE_FORGE_OS_LINUX_SOURCES})

    pkg_check_modules(GTK REQUIRED gtk+-3.0)

    target_link_libraries(TheForge PRIVATE ${GTK_LIBRARIES})
    target_include_directories(TheForge PRIVATE ${GTK_INCLUDE_DIRS})
endif()

target_sources(TheForge PRIVATE 
    ${THE_FORGE_DIR}/Common_3/Application/InputSystem.cpp)
target_link_libraries( TheForge PRIVATE ga)


file(GLOB THE_FORGE_LUA_SCRIPTING_SOURCES
        "${THE_FORGE_DIR}/Common_3/Game/Scripting/*.cpp"
    )
target_sources(TheForge PRIVATE 
    ${THE_FORGE_LUA_SCRIPTING_SOURCES})
target_link_libraries( TheForge PRIVATE lua )

# Ozz Animation relies on the Resource Loader
if(THE_FORGE_OZZ_ANIMATION) 
    if(THE_FORGE_OZZ_ANIMATION)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/ozz-animation.cmake)
        file(GLOB THE_FORGE_OZZ_LOADER_SOURCES
            ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/Animation/*.cpp)
        target_sources(TheForge PRIVATE 
            ${THE_FORGE_OZZ_LOADER_SOURCES})
    endif()
endif()

if(THE_FORGE_METAL)
    file(GLOB GRAPHICS_METAL_SOURCE
        "${THE_FORGE_DIR}/Common_3/Graphics/Metal/*.mm")
    target_sources(TheForge PRIVATE ${GRAPHICS_METAL_SOURCE})
endif()

if(THE_FORGE_VULKAN)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/volk.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/SPIRV_Cross.cmake)

    file(GLOB GRAPHICS_VULKAN_SOURCE
        "${THE_FORGE_DIR}/Common_3/Graphics/Vulkan/*.cpp")
    target_sources(TheForge PRIVATE ${GRAPHICS_VULKAN_SOURCE})
    target_link_libraries(TheForge PRIVATE SPIRV_Cross volk)
endif()

# if(THE_FORGE_D3D12)
     file(GLOB GRAPHICS_D3D12_SOURCE
         "${THE_FORGE_DIR}/Common_3/Graphics/Direct3D12/*.cpp")
     target_sources(TheForge PRIVATE ${GRAPHICS_D3D12_SOURCE})
     target_include_directories(TheForge PRIVATE ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/D3D12MemoryAllocator)
     target_link_libraries(TheForge PRIVATE Nvapi ags)
# endif()

# if(THE_FORGE_D3D11)
    add_library(DirectXShaderCompiler STATIC IMPORTED)
    set_property(TARGET DirectXShaderCompiler PROPERTY IMPORTED_LOCATION
            ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/lib/x64/dxcompiler.lib
        )     
    file(GLOB GRAPHICS_D3D11_SOURCE
         "${THE_FORGE_DIR}/Common_3/Graphics/Direct3D11/*.cpp")
     target_sources(TheForge PRIVATE ${GRAPHICS_D3D11_SOURCE})
     target_link_libraries( TheForge PRIVATE 
           DirectXShaderCompiler
            "d3d11.lib")
# endif()
#IF(THE_FORGE_D3D12 OR THE_FORGE_D3D11)
     include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/3rdParty/WinPixEventRuntime.cmake)    
     target_link_libraries( TheForge PRIVATE WinPixEventRuntime  )
# #ENDIF()

if(THE_FORGE_OPENGLES)
    file(GLOB GRAPHICS_OPENGLES_SOURCE
        "${THE_FORGE_DIR}/Common_3/Graphics/OpenGLES/*.cpp")
    target_sources(TheForge PRIVATE ${GRAPHICS_OPENGLES_SOURCE})
    target_compile_definitions( TheForge PRIVATE "GLES" )
endif()


# Tools -------------------------------------------
SET(FSL_COMPILER  "${THE_FORGE_DIR}/Common_3/Tools/ForgeShadingLanguage/fsl.py" CACHE INTERNAL "FSL Compiler")
