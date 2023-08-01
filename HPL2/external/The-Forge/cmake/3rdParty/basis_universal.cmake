if(TARGET basis_universal)
    return()
endif()

file(GLOB BASIS_UNVERSAL_SOURCES 
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/*.c
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/transcoder/*.cpp
    #${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/*.cpp\

    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_astc_decomp.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_backend.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_basis_file.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_comp.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_enc.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_etc.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_frontend.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_global_selector_palette_helpers.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_gpu_texture.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_pvrtc1_4.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_resampler.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_resample_filters.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/basisu_ssim.cpp
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal/lodepng.cpp

)
add_library(basis_universal STATIC ${BASIS_UNVERSAL_SOURCES})
target_include_directories(basis_universal PUBLIC ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/basis_universal)