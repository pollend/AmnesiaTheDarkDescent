#pragma once

#include "graphics/GraphicsTypes.h"
#include "graphics/BindlessDescriptorPool.h"
#include "graphics/Material.h"

#include "Common_3/Utilities/Math/MathTypes.h"
#include <FixPreprocessor.h>

#include <functional>
#include <variant>

struct Renderer;

// this is the resource model for the renderer
namespace hpl::resource {
    static constexpr uint32_t MaterialSceneSamplersCount = static_cast<uint32_t>(eTextureWrap_LastEnum) * static_cast<uint32_t>(eTextureFilter_LastEnum) * static_cast<uint32_t>(TextureAntistropy::Antistropy_Count);
    uint32_t textureFilterIdx(TextureAntistropy anisotropy, eTextureWrap wrap, eTextureFilter filter);
    std::array<SharedSampler, resource::MaterialSceneSamplersCount> createSceneSamplers(Renderer* renderer);

    static constexpr uint32_t MaterailSamplerNonAntistropyCount = static_cast<uint32_t>(eTextureWrap_LastEnum) * static_cast<uint32_t>(eTextureFilter_LastEnum);
    uint32_t textureFilterNonAnistropyIdx(eTextureWrap wrap, eTextureFilter filter);

    static constexpr uint32_t MaxScene2DTextureCount = 10000;
    static constexpr uint32_t MaxSceneCubeTextureCount = 5000;

    static constexpr uint32_t MaterialIdBit =  0;
    static constexpr uint32_t MaterialIndexBit = 8;

    static constexpr uint32_t MaterialIDMask =         0xff; // 0000 0000 0000 0000 0000 0000 1111 1111
    static constexpr uint32_t MaterialIndexMask =  0xffff00; // 0000 0000 1111 1111 1111 1111 0000 0000
    static constexpr uint32_t InvalidSceneTexture = 0xffff;

    enum LightType {
       PointLight = 0,
       SpotLight = 1
    };

    uint32_t constexpr IsAlphaSingleChannel = 0x1;
    uint32_t constexpr IsHeightSingleChannel = 0x2;
    uint32_t constexpr UseAlphaDissolveFilter = 0x4;

    struct ViewportInfo {
        static constexpr uint32_t PrmaryViewportIndex = 0;
        mat4 m_invViewMat;
        mat4 m_invProjMat;
        mat4 m_invViewProj;
        mat4 m_viewMat;
        mat4 m_projMat;
        float4 m_rect;
        float3 m_cameraPosition;
        uint m_pad0;
        float m_zNear;
        float m_zFar;
        uint m_pad1;
    };

    static constexpr uint32_t WorldFogEnabled = 2;
    struct WorldInfo {
        uint m_flags;
        float m_worldFogStart;
        float m_worldFogLength;
        float m_oneMinusFogAlpha;
        float m_fogFalloffExp;
        uint32_t m_pad0;
        uint32_t m_pad1;
        uint32_t m_pad2;
        float4 m_fogColor;
    };

    struct SceneInfoResource {
        WorldInfo m_worldInfo;
        ViewportInfo m_viewports[64];
    };

    struct SceneObject {
        uint32_t m_indexOffset;
        uint32_t m_vertexOffset;
        uint32_t m_materialType;
        uint32_t m_materialIndex;

        float m_dissolveAmount;
        float m_lightLevel;
        float m_illuminationAmount;
        float m_alphaAmount;

        mat4 m_modelMat;
        mat4 m_invModelMat;
        mat4 m_uvMat;
    };

    struct SceneLight {
        float4 m_color;

        float3 m_direction;
        uint m_lightType;

        float3 m_position;
        float m_radius;

        uint m_attenuationTexture;
        float m_angle;
        uint m_goboTexture;
        uint m_pad2;

        mat4 m_viewProjection;
    };

    struct SceneParticle {
       uint32_t diffuseTextureIndex;
       uint32_t sampleIndex;
       float sceneAlpha;
       float lightLevel;
       mat4 modelMat;
       mat4 uvMat;
    };


    struct SceneDecal {
       uint32_t diffuseTextureIndex;
       uint32_t sampleIndex;
       uint m_pad0;
       uint m_pad1;
       mat4 modelMat;
       mat4 uvMat;
    };

    struct DiffuseMaterial {
        uint m_diffuseTextureIndex;
        uint m_normalTextureIndex;
        uint m_alphaTextureIndex;
        uint m_specularTextureIndex;
        uint m_heightTextureIndex;
        uint m_illuminiationTextureIndex;
        uint m_dissolveAlphaTextureIndex;
        uint m_materialConfig;
        float m_heightMapScale;
        float m_heigtMapBias;
        float m_frenselBias;
        float m_frenselPow;
    };

    struct TranslucentMaterial {

    };

    struct WaterMaterial {

    };
}
