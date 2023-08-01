if(TARGET cpu_features)
    return()
endif()


set(CPU_FEATURES_SOURCES
        ${THE_FORGE_DIR}/Common_3/OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_macos.c
        ${THE_FORGE_DIR}/Common_3/OS/ThirdParty/OpenSource/cpu_features/src/impl_aarch64_iOS.c
        ${THE_FORGE_DIR}/Common_3/OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_windows.c
        )

add_library(cpu_features STATIC ${CPU_FEATURES_SOURCES})
