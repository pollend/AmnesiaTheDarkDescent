#pragma once

#include "Common_3/Utilities/Math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/TextureDescriptorPool.h"
#include <FixPreprocessor.h>
#include <functional>
#include <variant>

namespace hpl::resource {
    struct ViewportInfo {
         mat4 m_invViewMath;
         mat4 m_viewMat;
         mat4 m_projMat;
    };

    struct WorldInfo {
        float m_worldFogStart;
        float m_worldFogLength;
        float m_oneMinusFogAlpha;
        float m_fogFalloffExp;
        float4 m_fogColor;
    };

    struct SceneInfoResource {
        WorldInfo m_worldInfo;
        ViewportInfo m_viewports[64];
    };

    struct SceneObject {
        float m_dissolveAmount;
        uint32_t m_materialType;
        uint32_t m_materailIndex;
        float m_lightLevel;
        mat4 m_modelMat;
        mat4 m_invModelMat;
        mat4 m_uvMat;
    };

    struct DiffuseMaterial {
        union {
            uint m_texture[4];
            struct {
                uint16_t m_diffues;
                uint16_t m_normal;

                uint16_t m_alpha;
                uint16_t m_specular;

                uint16_t m_height;
                uint16_t m_illuminiation;

                uint16_t m_dissolveAlpha;
                uint16_t m_cubeMapAlpha;
            } m_tex;
        };
        uint m_materialConfig;
        float m_heightMapScale;
        float m_heigtMapBias;
        float m_frenselBias;
        float m_frenselPow;
    };

    using MaterialTypes = std::variant<DiffuseMaterial, std::monostate>;
    void visitTextures(MaterialTypes& material, std::function<void(eMaterialTexture, uint32_t slot)> handler);
    MaterialTypes createMaterial(TextureDescriptorPool& pool, cMaterial* material);
}
