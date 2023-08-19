if(TARGET WinPixEventRuntime)
    return()
endif()

add_library(WinPixEventRuntime SHARED IMPORTED)
set_property(TARGET WinPixEventRuntime PROPERTY IMPORTED_LOCATION
        ${THE_FORGE_DIR}/Common_3/OS/ThirdParty/OpenSource/winpixeventruntime/bin/WinPixEventRuntime.dll
        )
set_property(TARGET WinPixEventRuntime PROPERTY IMPORTED_IMPLIB
        ${THE_FORGE_DIR}/Common_3/OS/ThirdParty/OpenSource/winpixeventruntime/bin/WinPixEventRuntime.lib
        )