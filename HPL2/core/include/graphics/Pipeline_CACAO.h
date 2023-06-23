#pragma once

#include <cstdint>

namespace hpl::renderer {

class Pipeline_CACAO {

    static constexpr uint32_t FFX_CACAO_PREPARE_DEPTHS_AND_MIPS_WIDTH = 8;
    static constexpr uint32_t FFX_CACAO_PREPARE_DEPTHS_AND_MIPS_HEIGHT = 8;

    static constexpr uint32_t FFX_CACAO_PREPARE_DEPTHS_WIDTH = 8;
    static constexpr uint32_t FX_CACAO_PREPARE_DEPTHS_HEIGHT = 8;

    static constexpr uint32_t FFX_CACAO_PREPARE_DEPTHS_HALF_WIDTH = 8;
    static constexpr uint32_t FFX_CACAO_PREPARE_DEPTHS_HALF_HEIGHT = 8;

    static constexpr uint32_t FFX_CACAO_PREPARE_NORMALS_WIDTH = 8;
    static constexpr uint32_t FFX_CACAO_PREPARE_NORMALS_HEIGHT = 8;

    static constexpr uint32_t PREPARE_NORMALS_FROM_INPUT_NORMALS_WIDTH  = 8;
    static constexpr uint32_t PREPARE_NORMALS_FROM_INPUT_NORMALS_HEIGHT = 8;

    // ============================================================================
    // SSAO Generation

    static constexpr uint32_t FFX_CACAO_GENERATE_SPARSE_WIDTH = 4;
    static constexpr uint32_t FFX_CACAO_GENERATE_SPARSE_HEIGHT = 16;

    static constexpr uint32_t FFX_CACAO_GENERATE_WIDTH  = 8;
    static constexpr uint32_t FFX_CACAO_GENERATE_HEIGHT = 8;

    // ============================================================================
    // Importance Map

    static constexpr uint32_t IMPORTANCE_MAP_WIDTH  = 8;
    static constexpr uint32_t IMPORTANCE_MAP_HEIGHT = 8;

    static constexpr uint32_t IMPORTANCE_MAP_A_WIDTH  = 8;
    static constexpr uint32_t IMPORTANCE_MAP_A_HEIGHT = 8;

    static constexpr uint32_t IMPORTANCE_MAP_B_WIDTH  = 8;
    static constexpr uint32_t IMPORTANCE_MAP_B_HEIGHT = 8;

    // ============================================================================
    // Edge Sensitive Blur

    static constexpr uint32_t FFX_CACAO_BLUR_WIDTH  = 16;
    static constexpr uint32_t FFX_CACAO_BLUR_HEIGHT = 16;

    // ============================================================================
    // Apply

    static constexpr uint32_t FFX_CACAO_APPLY_WIDTH  = 8;
    static constexpr uint32_t FFX_CACAO_APPLY_HEIGHT = 8;

    // ============================================================================
    // Bilateral Upscale

    static constexpr uint32_t FFX_CACAO_BILATERAL_UPSCALE_WIDTH  = 8;
    static constexpr uint32_t FFX_CACAO_BILATERAL_UPSCALE_HEIGHT = 8;

    enum class FFX_CACAO_Quality {
        FFX_CACAO_QUALITY_LOWEST  = 0,
        FFX_CACAO_QUALITY_LOW     = 1,
//        FFX_CACAO_QUALITY_MEDIUM  = 2,
//        FFX_CACAO_QUALITY_HIGH    = 3,
//        FFX_CACAO_QUALITY_HIGHEST = 4,
    };

    FFX_CACAO_Quality m_quality;
};

}
