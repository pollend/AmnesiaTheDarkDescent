if(TARGET astc-encoder)
    return()
endif()

file(GLOB ASTC_ENCOUDER_SOURCE 
    #${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/*.c
    #${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/*.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenccli_error_metrics.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenccli_image.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenccli_image_external.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenccli_image_load_store.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenccli_platform_dependents.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_averages_and_directions.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_block_sizes.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_color_quantize.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_color_unquantize.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_compress_symbolic.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_compute_variance.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_decompress_symbolic.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_diagnostic_trace.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_entry.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_find_best_partitioning.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_ideal_endpoints_and_weights.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_image.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_integer_sequence.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_mathlib.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_mathlib_softfloat.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_partition_tables.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_percentile_tables.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_pick_best_endpoint_format.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_platform_isa_detection.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_quantization.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_symbolic_physical.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_weight_align.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/astcenc_weight_quant_xfer_tables.cpp
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/astc-encoder/Source/wuffs-v0.3.c
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp)

add_library(astc-encoder STATIC ${ASTC_ENCOUDER_SOURCE})
target_include_directories(astc-encoder PRIVATE 
    ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/TinyEXR)
#target_link_libraries(astc-encoder PRIVATE tinyEXR)


