#The Forge Graphics Folder

set(GRAPHICS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../The-Forge/Common_3/Graphics)

set(GRAPHICS_SOURCE_FILES
        ${GRAPHICS_DIR}/CommonShaderReflection.cpp
        ${GRAPHICS_DIR}/DependencyTracking.cpp
        ${GRAPHICS_DIR}/GPUConfig.cpp
        ${GRAPHICS_DIR}/Graphics.cpp
        ../The-Forge/Common_3/Resources/ResourceLoader/ResourceLoader.cpp
        )

set(GRAPHICS_INCLUDE_FILES
        ${GRAPHICS_DIR}/GraphicsConfig.h
        ${GRAPHICS_DIR}/GPUConfig.h
        ${GRAPHICS_DIR}/Interfaces/IGraphics.h
        ${GRAPHICS_DIR}/Interfaces/IRay.h
        ${GRAPHICS_DIR}/Interfaces/IShaderReflection.h
        ../The-Forge/Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h
        ../The-Forge/Common_3/Resources/ResourceLoader/TextureContainers.h)

set(METAL_FILES
        ${GRAPHICS_DIR}/Metal/MetalAvailabilityMacros.h
        ${GRAPHICS_DIR}/Metal/MetalCapBuilder.h
        ${GRAPHICS_DIR}/Metal/MetalConfig.h
        ${GRAPHICS_DIR}/Metal/MetalMemoryAllocatorImpl.h
        ${GRAPHICS_DIR}/Metal/MetalRaytracing.mm
        ${GRAPHICS_DIR}/Metal/MetalRenderer.mm
        ${GRAPHICS_DIR}/Metal/MetalShaderReflection.mm
        )

set(DX11_FILES
        ${GRAPHICS_DIR}/Direct3D11/Direct3D11.cpp
        ${GRAPHICS_DIR}/Direct3D11/Direct3D11CapBuilder.h
        ${GRAPHICS_DIR}/Direct3D11/Direct3D11Config.h
        ${GRAPHICS_DIR}/Direct3D11/Direct3D11Raytracing.cpp
        ${GRAPHICS_DIR}/Direct3D11/Direct3D11ShaderReflection.cpp
        )

set(DX12_FILES
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12.cpp
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12CapBuilder.h
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12Config.h
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12Hooks.cpp
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12Hooks.h
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12Raytracing.cpp
        ${GRAPHICS_DIR}/Direct3D12/Direct3D12ShaderReflection.cpp
        )

set(VULKAN_FILES
        ${GRAPHICS_DIR}/Vulkan/Vulkan.cpp
        ${GRAPHICS_DIR}/Vulkan/VulkanCapsBuilder.h
        ${GRAPHICS_DIR}/Vulkan/VulkanConfig.h
        ${GRAPHICS_DIR}/Vulkan/VulkanRaytracing.cpp
        ${GRAPHICS_DIR}/Vulkan/VulkanShaderReflection.cpp
        )

set(RENDERER_FILES ${GRAPHICS_SOURCE_FILES} ${GRAPHICS_INCLUDE_FILES})

if(${APPLE_PLATFORM} MATCHES ON)
    find_library(APPLE_METAL Metal)
    find_library(APPLE_METALKIT MetalKit)
    find_library(APPLE_METALPS MetalPerformanceShaders)

    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            ${APPLE_METAL}
            ${APPLE_METALKIT}
            ${APPLE_METALPS}
            )

    set(RENDERER_FILES ${RENDERER_FILES} ${METAL_FILES})

    find_library(APPLE_APPKIT AppKit)
    find_library(APPLE_QUARTZCORE QuartzCore)
    find_library(APPLE_IOKIT IOKit)

    set(RENDER_LIBRARIES
            ${RENDER_LIBRARIES}
            ${APPLE_QUARTZCORE}
            ${APPLE_APPKIT}
            ${APPLE_IOKIT}
            )
endif()

if(${VULKAN} MATCHES ON)
    find_package(Vulkan REQUIRED)
    if (Vulkan_FOUND MATCHES TRUE)
        message("Vulkan SDK found.")
        set(RENDER_LIBRARIES ${RENDER_LIBRARIES} Vulkan::Vulkan)
    else()
        message("Vulkan SDK not found.  Please make sure it is installed and added to your path.")
    endif()

    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            VulkanMemoryAllocator
            SpirvTools
            #VOLK
            )
    set(RENDERER_FILES ${RENDERER_FILES} ${VULKAN_FILES})

    set(RENDER_LIBRARY_PATHS ${Vulkan_INCLUDE_DIRS})

endif()

if(${DX11} MATCHES ON)
    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            DirectXShaderCompiler
            "d3d11.lib"
            )
    set(RENDERER_FILES ${RENDERER_FILES} ${DX11_FILES})
endif()

if(${DX12} MATCHES ON)
    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            D3D12MemoryAllocator
            )

    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            "d3d12.lib"
            )

    set(RENDERER_FILES ${RENDERER_FILES} ${DX12_FILES})
endif()

if(${WINDOWS} MATCHES ON)
    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            WinPixEventRuntime
            AGS
            Nvapi

            )

    set(RENDER_LIBRARIES ${RENDER_LIBRARIES}
            "Xinput9_1_0.lib"
            "ws2_32.lib"
            )

    set(RENDER_DEFINES ${RENDER_DEFINES}
            "_WINDOWS"
            )

endif()