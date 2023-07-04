/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "engine/RTTI.h"

#include "scene/Viewport.h"
#include "windowing/NativeWindow.h"

#include <graphics/ForgeHandles.h>
#include <graphics/ForgeRenderer.h>
#include <graphics/Image.h>
#include <graphics/Material.h>
#include <graphics/PassHBAOPlus.h>
#include <graphics/RenderList.h>
#include <graphics/RenderTarget.h>
#include <graphics/Renderable.h>
#include <graphics/Renderer.h>
#include <math/MathTypes.h>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>

#include <folly/small_vector.h>

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace hpl {

    class iFrameBuffer;
    class iDepthStencilBuffer;
    class iTexture;
    class iLight;
    class cSubMeshEntity;

    enum eDeferredShapeQuality {
        eDeferredShapeQuality_Low,
        eDeferredShapeQuality_Medium,
        eDeferredShapeQuality_High,
        eDeferredShapeQuality_LastEnum,
    };

    enum eDeferredSSAO {
        eDeferredSSAO_InBoxLight,
        eDeferredSSAO_OnColorBuffer,
        eDeferredSSAO_LastEnum,
    };

    class cRendererDeferred : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{A3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:
        static constexpr TinyImageFormat DepthBufferFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
        static constexpr TinyImageFormat NormalBufferFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        static constexpr TinyImageFormat PositionBufferFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        static constexpr TinyImageFormat SpecularBufferFormat = TinyImageFormat_R8G8_UNORM;
        static constexpr TinyImageFormat ColorBufferFormat = TinyImageFormat_R8G8B8A8_UNORM;
        static constexpr TinyImageFormat ShadowDepthBufferFormat = TinyImageFormat_D32_SFLOAT;
        static constexpr uint32_t MaxObjectUniforms = 4096;
        static constexpr uint32_t MaxLightUniforms = 1024;
        static constexpr uint32_t MaxHiZMipLevels = 32;
        static constexpr uint32_t MaxMaterialFrameDescriptors = 1024;


        enum LightConfiguration { HasGoboMap = 0x1, HasShadowMap = 0x2 };

        enum LightPipelineVariants {
            LightPipelineVariant_CW = 0x0,
            LightPipelineVariant_CCW = 0x1,
            LightPipelineVariant_StencilTest = 0x2,
            LightPipelineVariant_Size = 4,
        };
        union UniformLightData {
            struct LightUniformCommon {
                uint32_t m_config;
                mat4 m_mvp;
            } m_common;
            struct {
                LightUniformCommon m_common;
                float3 m_lightPos;
                float m_radius;
                float4 m_lightColor;
                mat4 m_invViewRotation;
            } m_pointLight;
            struct {
                LightUniformCommon m_common;
                mat4 m_spotViewProj;
                float4 m_color;
                float3 m_forward;
                float m_radius;
                float3 m_pos;
                float m_oneMinusCosHalfSpotFOV;
            } m_spotLight;
            struct {
                LightUniformCommon m_common;
                float4 m_lightColor;
            } m_boxLight;
        };

        struct UniformObject {
            float4 m_dissolveAmount;
            mat4 m_modelMat;
            mat4 m_invModelMat;
            mat4 m_uvMat;
        };

        struct UniformPerFrameData {
            mat4 m_invViewRotation;
            mat4 m_viewMatrix;
            mat4 m_invViewMatrix;
            mat4 m_projectionMatrix;
            mat4 m_viewProjectionMatrix;

            float worldFogStart;
            float worldFogLength;
            float oneMinusFogAlpha;
            float fogFalloffExp;
            float4 fogColor;

            float2 viewTexel;
            float2 viewportSize;
        };

        class ShadowMapData {
        public:
            ForgeRenderTarget m_target;
            iLight* m_light;
            uint32_t m_transformCount = 0;
            uint32_t m_frameCount = 0;
            float m_radius = 0.0f;
            float m_fov = 0.0f;
            float m_aspect = 0.0f;
        };

        struct FogRendererData {
            cFogArea* m_fogArea;
            bool m_insideNearFrustum;
            cVector3f m_boxSpaceFrustumOrigin;
            cMatrixf m_mtxInvBoxSpace;
        };

        struct GBuffer {
        public:
            GBuffer() = default;
            GBuffer(const GBuffer&) = delete;
            GBuffer(GBuffer&& buffer)
                : m_colorBuffer(std::move(buffer.m_colorBuffer))
                , m_normalBuffer(std::move(buffer.m_normalBuffer))
                , m_positionBuffer(std::move(buffer.m_positionBuffer))
                , m_specularBuffer(std::move(buffer.m_specularBuffer))
                , m_depthBuffer(std::move(buffer.m_depthBuffer))
                , m_outputBuffer(std::move(buffer.m_outputBuffer))
                , m_refractionImage(std::move(buffer.m_refractionImage))
                , m_hizDepthBuffer(std::move(buffer.m_hizDepthBuffer))
                , m_hiZMipCount(buffer.m_hiZMipCount) {
            }
            void operator=(GBuffer&& buffer) {
                m_colorBuffer = std::move(buffer.m_colorBuffer);
                m_normalBuffer = std::move(buffer.m_normalBuffer);
                m_positionBuffer = std::move(buffer.m_positionBuffer);
                m_specularBuffer = std::move(buffer.m_specularBuffer);
                m_depthBuffer = std::move(buffer.m_depthBuffer);
                m_outputBuffer = std::move(buffer.m_outputBuffer);
                m_refractionImage = std::move(buffer.m_refractionImage);
                m_hizDepthBuffer = std::move(buffer.m_hizDepthBuffer);
                m_hiZMipCount = buffer.m_hiZMipCount;
            }
            ForgeTextureHandle m_refractionImage;
            ForgeRenderTarget m_hizDepthBuffer;
            uint8_t m_hiZMipCount;

            ForgeRenderTarget m_colorBuffer;
            ForgeRenderTarget m_normalBuffer;
            ForgeRenderTarget m_positionBuffer;
            ForgeRenderTarget m_specularBuffer;
            ForgeRenderTarget m_depthBuffer;
            ForgeRenderTarget m_outputBuffer;
        };

        struct ViewportData {
        public:
            ViewportData() = default;
            ViewportData(const ViewportData&) = delete;
            ViewportData(ViewportData&& buffer)
                : m_size(buffer.m_size)
                , m_refractionImage(std::move(buffer.m_refractionImage))
                , m_gBuffer(std::move(buffer.m_gBuffer))
                , m_gBufferReflection(std::move(buffer.m_gBufferReflection)) {
            }

            ViewportData& operator=(const ViewportData&) = delete;

            void operator=(ViewportData&& buffer) {
                m_size = buffer.m_size;
                m_refractionImage = std::move(buffer.m_refractionImage);
                m_gBuffer = std::move(buffer.m_gBuffer);
                m_gBufferReflection = std::move(buffer.m_gBufferReflection);
            }

            cVector2l m_size = cVector2l(0, 0);
            std::array<GBuffer, ForgeRenderer::SwapChainLength> m_gBuffer;
            std::shared_ptr<Image> m_refractionImage;
            GBuffer m_gBufferReflection;
        };

        cRendererDeferred(cGraphics* apGraphics, cResources* apResources);
        ~cRendererDeferred();

        inline ViewportData* GetSharedData(cViewport& viewport) {
            return m_boundViewportData.resolve(viewport);
        }
        virtual ForgeRenderTarget GetOutputImage(uint32_t frameIndex, cViewport& viewport) override {
            auto sharedData = m_boundViewportData.resolve(viewport);
            if (!sharedData) {
                return ForgeRenderTarget();
            }
            return sharedData->m_gBuffer[frameIndex].m_outputBuffer;
        }

        virtual bool LoadData() override;
        virtual void DestroyData() override;

        virtual void Draw(
            const ForgeRenderer::Frame& frame,
            cViewport& viewport,
            float afFrameTime,
            cFrustum* apFrustum,
            cWorld* apWorld,
            cRenderSettings* apSettings,
            bool abSendFrameBufferToPostEffects) override;

        static void SetDepthCullLights(bool abX) {
            mbDepthCullLights = abX;
        }
        static int GetDepthCullLights() {
            return mbDepthCullLights;
        }

        static void SetSSAOLoaded(bool abX) {
            mbSSAOLoaded = abX;
        }
        static void SetSSAONumOfSamples(int alX) {
            mlSSAONumOfSamples = alX;
        }
        static void SetSSAOBufferSizeDiv(int alX) {
            mlSSAOBufferSizeDiv = alX;
        }
        static void SetSSAOScatterLengthMul(float afX) {
            mfSSAOScatterLengthMul = afX;
        }
        static void SetSSAOScatterLengthMin(float afX) {
            mfSSAOScatterLengthMin = afX;
        }
        static void SetSSAOScatterLengthMax(float afX) {
            mfSSAOScatterLengthMax = afX;
        }
        static void SetSSAOType(eDeferredSSAO aType) {
            mSSAOType = aType;
        }
        static void SetSSAODepthDiffMul(float afX) {
            mfSSAODepthDiffMul = afX;
        }
        static void SetSSAOSkipEdgeLimit(float afX) {
            mfSSAOSkipEdgeLimit = afX;
        }

        static bool GetSSAOLoaded() {
            return mbSSAOLoaded;
        }
        static int GetSSAONumOfSamples() {
            return mlSSAONumOfSamples;
        }
        static int GetSSAOBufferSizeDiv() {
            return mlSSAOBufferSizeDiv;
        }
        static float GetSSAOScatterLengthMul() {
            return mfSSAOScatterLengthMul;
        }
        static float GetSSAOScatterLengthMin() {
            return mfSSAOScatterLengthMin;
        }
        static float GetSSAOScatterLengthMax() {
            return mfSSAOScatterLengthMax;
        }
        static eDeferredSSAO GetSSAOType() {
            return mSSAOType;
        }
        static float GetSSAODepthDiffMul() {
            return mfSSAODepthDiffMul;
        }
        static float GetSSAOSkipEdgeLimit() {
            return mfSSAOSkipEdgeLimit;
        }

        static void SetEdgeSmoothLoaded(bool abX) {
            mbEdgeSmoothLoaded = abX;
        }
        static bool GetEdgeSmoothLoaded() {
            return mbEdgeSmoothLoaded;
        }

    private:
        LegacyRenderTarget& resolveRenderTarget(std::array<LegacyRenderTarget, 2>& rt);
        std::shared_ptr<Image>& resolveRenderImage(std::array<std::shared_ptr<Image>, 2>& img);
        iVertexBuffer* GetLightShape(iLight* apLight, eDeferredShapeQuality aQuality) const;

        struct PerObjectOption {
            cMatrixf m_viewMat;
            cMatrixf m_projectionMat;
            std::optional<cMatrixf> m_modelMatrix = std::nullopt;
        };
        struct PerFrameOption {
            float2 m_size;
            cMatrixf m_viewMat;
            cMatrixf m_projectionMat;
        };

        uint32_t updateFrameDescriptor(const ForgeRenderer::Frame& frame,Cmd* cmd, cWorld* apWorld,const PerFrameOption& options);
        void cmdBindMaterialDescriptor(Cmd* cmd, const ForgeRenderer::Frame& frame, cMaterial* apMaterial);
        void cmdBindObjectDescriptor(Cmd* cmd, const ForgeRenderer::Frame& frame, cMaterial* apMaterial, iRenderable* apObject, const PerObjectOption& option);

        std::array<std::unique_ptr<iVertexBuffer>, eDeferredShapeQuality_LastEnum> m_shapeSphere;
        std::unique_ptr<iVertexBuffer> m_shapePyramid;
        std::unique_ptr<iVertexBuffer> m_box;
        std::array<folly::small_vector<ShadowMapData, 32>, eShadowMapResolution_LastEnum> m_shadowMapData;

        int m_maxBatchLights;
        int mlMaxBatchVertices;
        int mlMaxBatchIndices;

        float m_farPlane;
        float m_farBottom;
        float m_farTop;
        float m_farLeft;
        float m_farRight;
        float mfMinLargeLightNormalizedArea;

        float m_shadowDistanceMedium;
        float m_shadowDistanceLow;
        float m_shadowDistanceNone;

        UniqueViewportData<ViewportData> m_boundViewportData;

        ForgeTextureHandle m_shadowJitterTexture;
        ForgeTextureHandle m_ssaoScatterDiskTexture;

        Image* m_dissolveImage;

        ForgeBufferHandle m_perFrameBuffer;

        // decal pass
        std::array<Pipeline*, eMaterialBlendMode_LastEnum> m_decalPipeline;
        Shader* m_decalShader;

        struct Fog {
            static constexpr uint32_t MaxFogCount = 128;

            enum FogVariant {
                EmptyVariant = 0x0,
                UseBackSide = 0x1,
                UseOutsideBox = 0x2,
            };

            enum PipelineVariant {
                PipelineVariantEmpty = 0x0,
                PipelineUseBackSide = 0x1,
                PipelineUseOutsideBox = 0x2,
                PipelineInsideNearFrustum = 0x4,
                PipelineNumVariants = 8
            };

            struct UniformFogData {
                mat4 m_mvp;
                mat4 m_mv;
                mat4 m_invModelRotation;
                float4 m_color;
                float4 m_rayCastStart;
                float4 m_fogNegPlaneDistNeg;
                float4 m_fogNegPlaneDistPos;
                float m_start;
                float m_length;
                float m_falloffExp;
            };

            struct UniformFullscreenFogData {
                float4 m_color;
                float m_fogStart;
                float m_fogLength;
                float m_fogFalloffExp;
            };
            std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_perFrameSet{};
            std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_perObjectSet{};

            GPURingBuffer m_fogUniformBuffer;
            RootSignature* m_fogRootSignature = nullptr;
            std::array<Shader*, 4> m_shader{};
            std::array<Pipeline*, PipelineVariant::PipelineNumVariants> m_pipeline{};

            Shader* m_fullScreenShader = nullptr;
            Pipeline* m_fullScreenPipeline = nullptr;
        } m_fogPass;

        RootSignature* m_materialRootSignature;
        // diffuse solid
        struct MaterialSolid {
            Shader* m_solidDiffuseShader;
            Shader* m_solidDiffuseParallaxShader;
            Pipeline* m_solidDiffusePipeline;
            Pipeline* m_solidDiffuseParallaxPipeline;
        } m_materialSolidPass;

        // illumination pass
        Shader* m_solidIlluminationShader;
        Pipeline* m_solidIlluminationPipeline;

        // translucency pass
        struct TranslucencyPipeline {
            enum TranslucencyShaderVariant {
                TranslucencyShaderVariantEmpty = 0x0,
                TranslucencyShaderVariantFog = 0x1,
                TranslucencyRefraction = 0x2,
                TranslucencyVariantCount = 4
            };

            enum TranslucencyWaterShaderVariant {
                TranslucencyWaterShaderVariantEmpty = 0x0,
                TranslucencyWaterShaderVariantFog = 0x1,
                TranslucencyWaterRefraction = 0x2,
                TranslucencyWaterReflection = 0x4,
                TranslucencyWaterVariantCount = 8
            };

            enum TranslucencyParticleShaderVariant {
                TranslucencyParticleShaderVariantEmpty = 0x0,
                TranslucencyParticleShaderVariantFog = 0x1,
                TranslucencyParticleVariantCount = 2
            };
            enum TranslucencyBlend : uint8_t { BlendAdd, BlendMul, BlendMulX2, BlendAlpha, BlendPremulAlpha, BlendModeCount };

            struct TranslucencyWaterConstant {
                float m_afT;
            };

            struct TranslucencyConstant {
                uint32_t m_blendMode;
                int m_textureMask;
                float m_sceneAlpha;
                float m_lightLevel;
            };
            // 3 bit key for pipeline variant
            union TranslucencyKey {
                uint8_t m_id;
                struct {
                    uint8_t m_hasDepthTest : 1;
                    uint8_t m_hasFog : 1;
                } m_field;
                static constexpr size_t NumOfVariants = 4;
            };
            union TranslucencyWaterKey {
                uint8_t m_id;
                struct {
                    uint8_t m_hasDepthTest : 1;
                    uint8_t m_hasFog : 1;
                    uint8_t m_hasReflection : 1;
                    uint8_t m_hasRefraction : 1;
                } m_field;
                static constexpr size_t NumOfVariants = 16;
            };
            RootSignature* m_refractionCopyRootSignature;
            std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_refractionPerFrameSet;
            Pipeline* m_refractionCopyPipeline;
            Shader* m_copyRefraction;

            std::array<Shader*, TranslucencyVariantCount> m_shaders{};
            std::array<Shader*, TranslucencyParticleVariantCount> m_particleShader{};
            std::array<Shader*, TranslucencyWaterVariantCount> m_waterShader{};

            std::array<std::array<Pipeline*, TranslucencyKey::NumOfVariants>, TranslucencyBlend::BlendModeCount> m_pipelines;
            std::array<Pipeline*, TranslucencyWaterKey::NumOfVariants> m_waterPipeline;
            std::array<Pipeline*, TranslucencyKey::NumOfVariants> m_refractionPipeline;
            std::array<std::array<Pipeline*, TranslucencyKey::NumOfVariants>, TranslucencyBlend::BlendModeCount> m_particlePipelines;

        } m_materialTranslucencyPass;

        // post processing
        struct ObjectSamplerKey {
            union {
                uint8_t m_id;
                struct {
                    AddressMode m_addressMode : 2;
                } m_field;
            };
            static constexpr size_t NumOfVariants = 4;
        };
        folly::F14VectorMap<SamplerDesc, Sampler*> m_objectSamplerMap;
        std::array<Sampler*, ObjectSamplerKey::NumOfVariants> m_objectSamplers{};
        struct LightResourceEntry {
            ForgeTextureHandle m_goboCubeMap;
            ForgeTextureHandle m_goboMap;
            ForgeTextureHandle m_falloffMap;
            ForgeTextureHandle m_attenuationLightMap;
        };
        std::array<std::array<LightResourceEntry, MaxLightUniforms>, ForgeRenderer::SwapChainLength> m_lightResources{};
        // z pass
        Shader* m_zPassShader;
        Pipeline* m_zPassPipeline;
        Pipeline* m_zPassShadowPipelineCW;
        Pipeline* m_zPassShadowPipelineCCW;
        DescriptorSet* m_zPassConstSet;
        std::set<iRenderable*> m_preZPassRenderables;

        GPURingBuffer m_objectUniformBuffer;
        struct MaterialPassDescriptorSet {
            std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_frameSet;
            std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_perObjectSet;
            std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_materialSet;
            folly::F14ValueMap<iRenderable*, uint32_t> m_objectDescriptorLookup;
            uint32_t m_frameIndex = 0;
            uint32_t m_objectIndex = 0;

            // Material
            struct MaterialInfo {
                struct MaterialDescInfo {
                    void* m_material = nullptr; // void* to avoid accessing the material
                    uint32_t m_version = 0; // version of the material
                    std::array<ForgeTextureHandle, eMaterialTexture_LastEnum>
                        m_textureHandles{}; // handles to keep textures alive for the descriptor
                } m_materialDescInfo[ForgeRenderer::SwapChainLength];
            };
            std::array<MaterialInfo, cMaterial::MaxMaterialID> m_materialInfo;
            ForgeBufferHandle m_materialUniformBuffer;
        } m_materialSet;

        uint32_t m_activeFrame = 0; // tracks the active frame if differnt then we need to reset some state
        CmdPool* m_prePassPool = nullptr;
        Cmd* m_prePassCmd = nullptr;
        Fence* m_prePassFence = nullptr;

        RootSignature* m_rootSignatureHIZOcclusion;
        Shader* m_ShaderHIZGenerate;
        Shader* m_shaderTestOcclusion;
        DescriptorSet* m_descriptorSetHIZGenerate;
        Pipeline* m_pipelineHIZGenerate;

        RootSignature* m_rootSignatureCopyDepth;
        ForgeDescriptorSet m_descriptorCopyDepth;
        ForgeDescriptorSet m_descriptorAABBOcclusionTest;
        ForgePipelineHandle m_pipelineCopyDepth;
        ForgePipelineHandle m_pipelineAABBOcclusionTest;
        Shader* m_copyDepthShader;
        ForgeBufferHandle m_hiZOcclusionUniformBuffer;
        ForgeBufferHandle m_occlusionTestBuffer;

        struct OcclusionQueryAlpha {
            iRenderable* m_renderable = nullptr;
            uint32_t m_maxQueryIndex = 0;
            uint32_t m_queryIndex = 0;
        };
        static constexpr uint32_t MaxQueryPoolSize = 4096 * 2;
        static constexpr uint32_t MaxOcclusionDescSize = 4096;
        QueryPool* m_occlusionQuery = nullptr;
        GPURingBuffer m_occlusionUniformBuffer;
        RootSignature* m_rootSignatureOcclusuion = nullptr;
        ForgeDescriptorSet m_descriptorOcclusionFrameSet;
        ForgeBufferHandle m_occlusionReadBackBuffer;
        ForgeShaderHandle m_shaderOcclusionQuery;
        ForgePipelineHandle m_pipelineMaxOcclusionQuery;
        ForgePipelineHandle m_pipelineOcclusionQuery;

        struct UniformTest {
            bool m_preZPass = false;
            iRenderable* m_renderable = nullptr;
        };
        struct UniformPropBlock {
            static constexpr uint32_t MaxObjectTest = 32768;
            mat4 viewProjeciton;
            uint32_t numObjects;
            uint32_t maxMipLevel;
            uint32_t pad2;
            uint32_t pad3;
            float4 inputColliders[MaxObjectTest];
        };

        // light pass
        struct UniformLightPerFrameSet {
            float gamma;
            float3 pad;
        };
        GPURingBuffer m_lightPassRingBuffer;
        RootSignature* m_lightPassRootSignature;
        ForgePipelineHandle m_lightStencilPipeline;
        std::array<Pipeline*, LightPipelineVariant_Size> m_pointLightPipeline;
        std::array<Pipeline*, LightPipelineVariant_Size> m_boxLightPipeline;
        std::array<Pipeline*, LightPipelineVariant_Size> m_spotLightPipeline;
        ForgeShaderHandle m_pointLightShader;
        ForgeShaderHandle m_spotLightShader;
        ForgeShaderHandle m_stencilLightShader;
        ForgeShaderHandle m_boxLightShader;
        std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_lightPerLightSet;
        std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_lightPerFrameSet;
        ForgeSamplerHandle m_shadowCmpSampler;

        Sampler* m_samplerPointClampToBorder;
        Sampler* m_pointSampler;
        Sampler* m_goboSampler;

        cRenderList m_reflectionRenderList;
        std::unique_ptr<renderer::PassHBAOPlus> m_hbaoPlusPipeline;

        static bool mbDepthCullLights;
        static bool mbSSAOLoaded;
        static int mlSSAONumOfSamples;
        static float mfSSAOScatterLengthMul;
        static float mfSSAOScatterLengthMin;
        static float mfSSAOScatterLengthMax;
        static float mfSSAODepthDiffMul;
        static float mfSSAOSkipEdgeLimit;
        static eDeferredSSAO mSSAOType;
        static int mlSSAOBufferSizeDiv;

        static bool mbEdgeSmoothLoaded;
        static bool mbEnableParallax;
    };
}; // namespace hpl
