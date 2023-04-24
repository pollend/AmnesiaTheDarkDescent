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
#include "graphics/RenderList.h"
#include "scene/Viewport.h"
#include "windowing/NativeWindow.h"
#include <array>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <graphics/GraphicsContext.h>
#include <graphics/Image.h>
#include <graphics/RenderTarget.h>
#include <graphics/Renderer.h>
#include <graphics/ShaderVariantCollection.h>
#include <math/MathTypes.h>
#include <memory>
#include <vector>

#include <graphics/Pipeline.h>


#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <FixPreprocessor.h>


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

    //---------------------------------------------
    namespace rendering::detail {
        enum SpotlightVariant { 
            SpotlightVariant_None = 0, 
            SpotlightVariant_UseGoboMap = 0x1, 
            SpotlightVariant_UseShadowMap = 0x2 
        };

        enum PointlightVariant { 
            PointlightVariant_None = 0, 
            PointlightVariant_UseGoboMap = 0x1 
        };

        enum FogVariant { 
            FogVariant_None = 0, 
            FogVariant_UseOutsideBox = 0x1, 
            FogVariant_UseBackSide = 0x2 
        };

        void RenderZPassObject(
            bgfx::ViewId view,
            GraphicsContext& context,
            cViewport& viewport,
            iRenderer* renderer,
            iRenderable* object,
            Cull cull = Cull::CounterClockwise);
    }; // namespace rendering::detail

    class cRendererDeferred : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{A3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:

        static constexpr uint32_t FrameBuffer = 2;

        class ShadowMapData {
        public:
            LegacyRenderTarget m_target;
            iLight *m_light;
            int m_transformCount;
            int m_frameCount;
            float m_radius;
            float m_fov;
            float m_aspect;
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
                : m_colorBuffer(buffer.m_colorBuffer),
                  m_normalBuffer(buffer.m_normalBuffer),
                  m_positionBuffer(buffer.m_positionBuffer),
                  m_depthBuffer(buffer.m_depthBuffer),
                  m_outputBuffer(buffer.m_outputBuffer) {
                buffer.m_colorBuffer = nullptr;
                buffer.m_normalBuffer = nullptr;
                buffer.m_positionBuffer = nullptr;
                buffer.m_depthBuffer = nullptr;
                buffer.m_outputBuffer = nullptr;
            }
            void operator=(GBuffer&& buffer) {
                m_colorBuffer = buffer.m_colorBuffer;
                m_normalBuffer = buffer.m_normalBuffer; 
                m_positionBuffer = buffer.m_positionBuffer; 
                m_depthBuffer = buffer.m_depthBuffer; 
                m_outputBuffer = buffer.m_outputBuffer; 

                buffer.m_colorBuffer = nullptr;
                buffer.m_normalBuffer = nullptr;
                buffer.m_positionBuffer = nullptr;
                buffer.m_depthBuffer = nullptr;
                buffer.m_outputBuffer = nullptr;
            }

            RenderTarget* m_colorBuffer;
            RenderTarget* m_normalBuffer;
            RenderTarget* m_positionBuffer;
            RenderTarget* m_depthBuffer;
            RenderTarget* m_outputBuffer;

            // std::shared_ptr<Image> m_colorImage;
            // std::shared_ptr<Image> m_normalImage;
            // std::shared_ptr<Image> m_positionImage;
            // std::shared_ptr<Image> m_specularImage;
            // std::shared_ptr<Image> m_depthStencilImage;
            // std::shared_ptr<Image> m_outputImage;

            // std::shared_ptr<Image> m_SSAOImage;
            // std::shared_ptr<Image> m_SSAOBlurImage;

            // LegacyRenderTarget m_fullTarget;
            // LegacyRenderTarget m_colorAndDepthTarget;
            // LegacyRenderTarget m_colorTarget;
            // LegacyRenderTarget m_depthTarget;
            // LegacyRenderTarget m_normalTarget;
            // LegacyRenderTarget m_positionTarget;
            // LegacyRenderTarget m_outputTarget; // used for rendering to the screen

            // LegacyRenderTarget m_SSAOTarget;
            // LegacyRenderTarget m_SSAOBlurTarget;
            
        };

        struct SharedViewportData {
        public:
            SharedViewportData() = default;
            SharedViewportData(const SharedViewportData&) = delete;
            SharedViewportData(SharedViewportData&& buffer)
                : m_size(buffer.m_size)
                , m_refractionImage(std::move(buffer.m_refractionImage))
                , m_gBuffer(std::move(buffer.m_gBuffer))
                , m_gBufferReflection(std::move(buffer.m_gBufferReflection)) {
            }

            SharedViewportData& operator=(const SharedViewportData&) = delete;

            void operator=(SharedViewportData&& buffer) {
                m_size = buffer.m_size;
                m_refractionImage = std::move(buffer.m_refractionImage);
                m_gBuffer = std::move(buffer.m_gBuffer);
                m_gBufferReflection = std::move(buffer.m_gBufferReflection);

            }

            cVector2l m_size = cVector2l(0, 0);
            std::array<GBuffer, HPLPipeline::SwapChainLength> m_gBuffer;
            

            std::shared_ptr<Image> m_refractionImage;
            GBuffer m_gBufferReflection;

        };

        cRendererDeferred(HPLPipeline* pipeline, cGraphics* apGraphics, cResources* apResources);
        ~cRendererDeferred();

        inline SharedViewportData& GetSharedData(cViewport& viewport) {
            return m_boundViewportData.resolve(viewport);
        }
        // virtual std::shared_ptr<Image> GetDepthStencilImage(cViewport& viewport) override;
        virtual Texture* GetOutputImage(cViewport& viewport) override { return nullptr; }

        virtual bool LoadData() override;
        virtual void DestroyData() override;

        virtual void Draw(
            GraphicsContext& context,
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

        void RenderEdgeSmoothPass(GraphicsContext& context, cViewport& viewport, LegacyRenderTarget& rt);
        // cShadowMapData& iRenderer::GetShadowMapData(eShadowMapResolution aResolution, iLight* apLight);

        struct LightPassOptions {
            const cMatrixf& frustumProjection;
            const cMatrixf& frustumInvView;
            const cMatrixf& frustumView;

           cRendererDeferred::GBuffer& m_gBuffer;

        };
        void RenderLightPass(GraphicsContext& context, std::span<iLight*> lights, cWorld* apWorld, cViewport& viewport, cFrustum* apFrustum, LightPassOptions& options);
        struct FogPassOptions {
            const cMatrixf& frustumProjection;
            const cMatrixf& frustumInvView;
            const cMatrixf& frustumView;

           cRendererDeferred::GBuffer& m_gBuffer;
        };
        void RenderFogPass(GraphicsContext& context, std::span<cRendererDeferred::FogRendererData> fogRenderData, cWorld* apWorld, cViewport& viewport, cFrustum* apFrustum, FogPassOptions& options);
        struct FogPassFullscreenOptions {
            cRendererDeferred::GBuffer& m_gBuffer;
        };
        void RenderFullscreenFogPass(GraphicsContext& context, cWorld* apWorld, cViewport& viewport, cFrustum* apFrustum, FogPassFullscreenOptions& options);
        iVertexBuffer* GetLightShape(iLight* apLight, eDeferredShapeQuality aQuality) const;

        std::array<std::unique_ptr<iVertexBuffer>, eDeferredShapeQuality_LastEnum> m_shapeSphere;
        std::unique_ptr<iVertexBuffer> m_shapePyramid;
        std::array<absl::InlinedVector<ShadowMapData, 15>, eShadowMapResolution_LastEnum> m_shadowMapData;

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

        LegacyRenderTarget m_edgeSmooth_LinearDepth;
        UniqueViewportData<SharedViewportData> m_boundViewportData;

        std::shared_ptr<Image> m_shadowJitterImage;
        std::shared_ptr<Image> m_ssaoScatterDiskImage;

        UniformWrapper<StringLiteral("u_param"), bgfx::UniformType::Vec4> m_u_param;
        UniformWrapper<StringLiteral("u_lightPos"), bgfx::UniformType::Vec4>  m_u_lightPos;
        UniformWrapper<StringLiteral("u_fogColor"), bgfx::UniformType::Vec4> m_u_fogColor;
        UniformWrapper<StringLiteral("u_lightColor"), bgfx::UniformType::Vec4> m_u_lightColor;
        UniformWrapper<StringLiteral("u_overrideColor"), bgfx::UniformType::Vec4> m_u_overrideColor;
        UniformWrapper<StringLiteral("u_copyRegion"), bgfx::UniformType::Vec4> m_u_copyRegion;

        UniformWrapper<StringLiteral("u_spotViewProj"), bgfx::UniformType::Mat4> m_u_spotViewProj;
        UniformWrapper<StringLiteral("u_mtxInvRotation"), bgfx::UniformType::Mat4> m_u_mtxInvRotation;
        UniformWrapper<StringLiteral("u_mtxInvViewRotation"), bgfx::UniformType::Mat4> m_u_mtxInvViewRotation;

        UniformWrapper<StringLiteral("s_depthMap"), bgfx::UniformType::Sampler> m_s_depthMap;
        UniformWrapper<StringLiteral("s_positionMap"), bgfx::UniformType::Sampler> m_s_positionMap;
        UniformWrapper<StringLiteral("s_diffuseMap"), bgfx::UniformType::Sampler> m_s_diffuseMap;
        UniformWrapper<StringLiteral("s_normalMap"), bgfx::UniformType::Sampler> m_s_normalMap;
        UniformWrapper<StringLiteral("s_specularMap"), bgfx::UniformType::Sampler> m_s_specularMap;
        UniformWrapper<StringLiteral("s_attenuationLightMap"), bgfx::UniformType::Sampler> m_s_attenuationLightMap;
        UniformWrapper<StringLiteral("s_spotFalloffMap"), bgfx::UniformType::Sampler> m_s_spotFalloffMap;
        UniformWrapper<StringLiteral("s_shadowMap"), bgfx::UniformType::Sampler> m_s_shadowMap;
        UniformWrapper<StringLiteral("s_goboMap"), bgfx::UniformType::Sampler> m_s_goboMap;
        UniformWrapper<StringLiteral("s_shadowOffsetMap"), bgfx::UniformType::Sampler> m_s_shadowOffsetMap;

        UniformWrapper<StringLiteral("s_scatterDisk"), bgfx::UniformType::Sampler> m_s_scatterDisk;
        
        Buffer* m_indirectDiffusePositionBuffer;

        bgfx::ProgramHandle m_deferredSSAOProgram;
        bgfx::ProgramHandle m_deferredSSAOBlurHorizontalProgram;
        bgfx::ProgramHandle m_deferredSSAOBlurVerticalProgram;

        bgfx::ProgramHandle m_copyRegionProgram;
        bgfx::ProgramHandle m_edgeSmooth_UnpackDepthProgram;
        bgfx::ProgramHandle m_lightBoxProgram;
        bgfx::ProgramHandle m_nullShader;

        ShaderVariantCollection<rendering::detail::FogVariant_UseOutsideBox | rendering::detail::FogVariant_UseBackSide> m_fogVariant;
        ShaderVariantCollection<rendering::detail::SpotlightVariant_UseGoboMap | rendering::detail::SpotlightVariant_UseShadowMap>
            m_spotlightVariants;
        ShaderVariantCollection<rendering::detail::PointlightVariant_UseGoboMap> m_pointLightVariants;

        cRenderList m_reflectionRenderList;

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
