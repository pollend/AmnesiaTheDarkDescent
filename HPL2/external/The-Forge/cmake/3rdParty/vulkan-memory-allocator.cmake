if(TARGET VulkanMemoryAllocator)
    return()
endif()

add_library(VulkanMemoryAllocator INTERFACE)
target_include_directories(VulkanMemoryAllocator INTERFACE
    ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/VulkanMemoryAllocator)