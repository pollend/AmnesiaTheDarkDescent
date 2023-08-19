if(TARGET D3D12MemoryAllocator)
    return()
endif()

add_library(D3D12MemoryAllocator INTERFACE)
target_include_directories(D3D12MemoryAllocator INTERFACE
    ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/D3D12MemoryAllocator)