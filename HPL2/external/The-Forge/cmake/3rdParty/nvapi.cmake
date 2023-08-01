add_library(Nvapi STATIC IMPORTED)
set_property(TARGET Nvapi PROPERTY IMPORTED_LOCATION
        ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/nvapi/amd64/nvapi64.lib
        )
target_include_directories(Nvapi INTERFACE ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/nvapi)
