#pragma once

#include "Common_3/Utilities/Math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include <cstdint>

// resources defined for the deferred renderer pipeline
namespace hpl::material {

    enum TextureConfigFlags {
        EnableDiffuse = 1 << 0,
        EnableNormal = 1 << 1,
        EnableSpecular = 1 << 2,
        EnableAlpha = 1 << 3,
        EnableHeight = 1 << 4,
        EnableIllumination = 1 << 5,
        EnableCubeMap = 1 << 6,
        EnableDissolveAlpha = 1 << 7,
        EnableCubeMapAlpha = 1 << 8,

        // additional flags
        IsHeightMapSingleChannel = 1 << 9,
        IsAlphaSingleChannel = 1 << 10,

        // Solid Diffuse
        UseDissolveFilter = 1 << 14,

        // Translucent
        UseRefractionNormals = 1 << 14,
        UseRefractionEdgeCheck = 1 << 15,
    };







    struct MaterialBlockOptions {
        bool m_enableRefration = false;
        bool m_enableReflection = false;
    };
    UnifomMaterialBlock resolveMaterialBlock(cMaterial& mat, const MaterialBlockOptions& options);


} // namespace hpl::material
