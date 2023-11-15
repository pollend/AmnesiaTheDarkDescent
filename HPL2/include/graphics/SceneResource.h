#pragma once


#include "graphics/GraphicsTypes.h"
#include "graphics/TextureDescriptorPool.h"
#include "graphics/Material.h"


#include "Common_3/Utilities/Math/MathTypes.h"
#include <FixPreprocessor.h>

#include <functional>
#include <variant>

class Renderer;

// this is the resource model for the renderer
namespace hpl::resource {
    static constexpr uint32_t MaterialSceneSamplersCount = static_cast<uint32_t>(eTextureWrap_LastEnum) * static_cast<uint32_t>(eTextureFilter_LastEnum) * static_cast<uint32_t>(TextureAntistropy::Antistropy_Count);
    uint32_t textureFilterIdx(TextureAntistropy anisotropy, eTextureWrap wrap, eTextureFilter filter);
    std::array<SharedSampler, resource::MaterialSceneSamplersCount> createSceneSamplers(Renderer* renderer);

    static constexpr uint32_t MaterailSamplerNonAntistropyCount = static_cast<uint32_t>(eTextureWrap_LastEnum) * static_cast<uint32_t>(eTextureFilter_LastEnum);
    uint32_t textureFilterNonAnistropyIdx(eTextureWrap wrap, eTextureFilter filter);

    static constexpr uint32_t MaxSceneTextureCount = 4096;

    static constexpr uint32_t MaterialIdBit =  0;
    static constexpr uint32_t MaterialIndexBit = 8;

    static constexpr uint32_t MaterialIDMask =         0xff; // 0000 0000 0000 0000 0000 0000 1111 1111
    static constexpr uint32_t MaterialIndexMask =  0xffff00; // 0000 0000 1111 1111 1111 1111 0000 0000
    uint32_t encodeMaterialID(hpl::MaterialID id, uint32_t handle);

    struct ViewportInfo {
        static constexpr uint32_t PrmaryViewportIndex = 0;
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
        uint m_indirectOffset;
        float m_dissolveAmount;
        uint32_t m_materialId;
        float m_lightLevel;
        float m_illuminationAmount;
        uint32_t m_pad0;
        uint32_t m_pad1;
        uint32_t m_pad2;
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
        uint m_samplerIndex;
        uint m_materialConfig;
        float m_heightMapScale;
        float m_heigtMapBias;
        float m_frenselBias;
        float m_frenselPow;
        uint m_pad0;
        uint m_pad1;
        uint m_pad2;
    };

    using MaterialTypes = std::variant<DiffuseMaterial, std::monostate>;
    void visitTextures(MaterialTypes& material, std::function<void(eMaterialTexture, uint32_t slot)> handler);
    MaterialTypes createMaterial(TextureDescriptorPool& pool, cMaterial* material);
}
