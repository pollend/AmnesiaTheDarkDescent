#pragma once

#include "graphics/GraphicsTypes.h"
#include "graphics/BindlessDescriptorPool.h"
#include "graphics/Material.h"

#include "Common_3/Utilities/Math/MathTypes.h"
#include "math/cFrustum.h"
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

    static constexpr uint32_t MaxReflectionBuffers = 4;
    static constexpr uint32_t MaxObjectUniforms = 4096;

    static constexpr uint32_t MaxSolidDiffuseMaterials = 2048;
    static constexpr uint32_t MaxWaterMaterials = 16;
    static constexpr uint32_t MaxTranslucenctMaterials = 512;

    static constexpr uint32_t MaxLightUniforms = 1024;
    static constexpr uint32_t MaxDecalUniforms = 1024;
    static constexpr uint32_t MaxParticleUniform = 1024;
    static constexpr uint32_t MaxIndirectDrawElements = 4096;

    static constexpr uint32_t LightClusterLightCount = 128;

    static constexpr uint32_t InvalidSceneTexture = std::numeric_limits<uint32_t>::max();

    enum LightType {
       PointLight = 0,
       SpotLight = 1
    };

    uint32_t constexpr IsAlphaSingleChannel = 0x1;
    uint32_t constexpr IsHeightSingleChannel = 0x2;
    uint32_t constexpr UseAlphaDissolveFilter = 0x4;
    uint32_t constexpr UseReflection = 0x8;
    uint32_t constexpr UseRefraction = 0x16;

    struct ViewportInfo {
        static constexpr uint32_t PrmaryViewportIndex = 0;
        static ViewportInfo create(cFrustum* frustum, float4 rect);

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

    static constexpr uint32_t WorldFogEnabled = 0x2;
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

        float4 m_normalizeShadow;

        mat4 m_viewProjection;
    };

    struct SceneParticle {
       uint32_t diffuseTextureIndex;
       uint32_t m_pad0;
       float sceneAlpha;
       float lightLevel;
       mat4 modelMat;
       mat4 uvMat;
    };


    struct SceneDecal {
       uint32_t diffuseTextureIndex;
       uint32_t m_pad0;
       uint32_t m_pad1;
       uint32_t m_pad2;
       mat4 modelMat;
       mat4 uvMat;
    };

    struct DiffuseMaterial {
        uint m_diffuseTextureIndex;
        uint m_normalTextureIndex;
        uint m_alphaTextureIndex;
        uint m_specularTextureIndex;
        uint m_heightTextureIndex;
        uint m_illuminationTextureIndex;
        uint m_dissolveAlphaTextureIndex;
        uint m_materialConfig;
        float m_heightMapScale;
        float m_heigtMapBias;
        float m_frenselBias;
        float m_frenselPow;
    };

    struct TranslucentMaterial {
        uint32_t m_diffuseTextureIndex;
        uint32_t m_normalTextureIndex;
        uint32_t m_alphaTextureIndex;
        uint32_t m_specularTextureIndex;
        uint32_t m_heightTextureIndex;
        uint32_t m_illuminationTextureIndex;
        uint32_t m_dissolveAlphaTextureIndex;
        uint32_t m_cubeMapTextureIndex;
        uint32_t m_cubeMapAlphaTextureIndex;

        uint32_t m_config;
        float m_refractionScale;
        float m_frenselBias;
        float m_frenselPow;

        float m_rimMulLight;
        float m_rimMulPower;
	    float m_pad0;
    };

    struct WaterMaterial {
        uint32_t m_diffuseTextureIndex;
        uint32_t m_normalTextureIndex;
        uint32_t m_cubemapTextureIndex;
        uint32_t m_config;

        float m_refractionScale;
        float m_frenselBias;
        float m_frenselPow;
        float m_reflectionFadeStart;

        float m_reflectionFadeEnd;
        float m_waveSpeed;
        float m_waveAmplitude;
        float m_waveFreq;
    };
}
