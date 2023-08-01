add_library(ags SHARED IMPORTED)
set_property(TARGET ags PROPERTY IMPORTED_LOCATION
         ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/ags/ags_lib/lib/amd_ags_x64.dll
        )
set_property(TARGET ags PROPERTY IMPORTED_IMPLIB
         ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/ags/ags_lib/lib/amd_ags_x64.lib
        )
target_include_directories(ags INTERFACE  ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/ags)