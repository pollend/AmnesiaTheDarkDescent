

set(FORGE_FILES
        ${RENDERER_SOURCE_FILES}
        ${RENDERER_INCLUDE_FILES}
        ${RENDERER_FILES}
        ${FORGE_APP_FILES}
        ${FORGE_OS_CORE_FILES}
        ${FORGE_OS_SPECIFIC_FILES}

        ${FORGE_FILESYSTEM_FILES}
        ${FORGE_MEMORY_TRACKING_FILES}
        ${FORGE_THREADING_FILES}
        ${FORGE_MATH_FILES}
        ${FORGE_GAME_FILES}
        ${FORGE_LOG_FILES}
        ${FORGE_OS_INTERFACE_FILES}
        ${OS_MIDDLEWARE_PARALLEL_PRIMS_FILES}
        )


add_library(The-Forge STATIC
        ${FORGE_FILES}
        )

target_include_directories(The-Forge PUBLIC
        ../The-Forge/Common_3/
        ${RENDER_INCLUDES}
        )

target_link_libraries(The-Forge PUBLIC ${RENDER_LIBRARIES} ${THIRD_PARTY_DEPS})

target_link_directories(The-Forge PUBLIC ${RENDER_LIBRARY_PATHS})

target_compile_definitions(The-Forge PUBLIC ${RENDER_DEFINES})

if (${APPLE_PLATFORM} MATCHES ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -stdlib=libc++ -x objective-c++")
    target_compile_options(The-Forge PRIVATE "-fobjc-arc")
endif()