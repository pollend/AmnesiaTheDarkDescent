#pragma once

#include "graphics/GraphicsTypes.h"
#include "graphics/TextureDescriptorPool.h"
#include "scene/LightPoint.h"
#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>
#include <variant>

namespace hpl::resource {
    class iLight;
    struct PointLightResource {
        mat4 m_mvp;
        float3 m_lightPos;
        uint m_config;
        float4 m_lightColor;
        float m_radius;
    };
    using LightResourceVariants = std::variant<PointLightResource, std::monostate>;
    static LightResourceVariants CreateFromLight(iLight& light);

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
