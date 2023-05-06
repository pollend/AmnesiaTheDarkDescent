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
#include <graphics/Material.h>
#include <graphics/ShaderVariantCollection.h>
#include <math/MathTypes.h>
#include <memory>
#include <vector>

#include <graphics/Pipeline.h>

#include "Common_3/Utilities/RingBuffer.h"
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
        static void InitializeDeferred(ForgeRenderer& pipeline);

        static constexpr TinyImageFormat DepthBufferFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
        static constexpr uint32_t FrameBuffer = 2;
        static constexpr uint32_t MaxObjectUniforms = 4096;
		
        struct CBObjectData {
			mat4 m_modelMatrix;
			mat4 m_normalMatrix;
			mat4 m_uvMatrix;
		};

        struct PerFrameData {
            mat4 m_invViewRotation;
            mat4 m_viewMatrix;
            mat4 m_projectionMatrix;
            mat4 m_viewProjectionMatrix;
        };

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
                : m_colorBuffer(std::move(buffer.m_colorBuffer)),
                  m_normalBuffer(std::move(buffer.m_normalBuffer)),
                  m_positionBuffer(std::move(buffer.m_positionBuffer)),
                  m_depthBuffer(std::move(buffer.m_depthBuffer)),
                  m_outputBuffer(std::move(buffer.m_outputBuffer)) {
            }
            void operator=(GBuffer&& buffer) {
                m_colorBuffer = std::move(buffer.m_colorBuffer);
                m_normalBuffer = std::move(buffer.m_normalBuffer); 
                m_positionBuffer = std::move(buffer.m_positionBuffer); 
                m_depthBuffer = std::move(buffer.m_depthBuffer); 
                m_outputBuffer = std::move(buffer.m_outputBuffer); 
            }

            ForgeRenderTarget m_colorBuffer;
            ForgeRenderTarget m_normalBuffer;
            ForgeRenderTarget m_positionBuffer;
            ForgeRenderTarget m_depthBuffer;
            ForgeRenderTarget m_outputBuffer;
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
            std::array<GBuffer, ForgeRenderer::SwapChainLength> m_gBuffer;
            std::shared_ptr<Image> m_refractionImage;
            GBuffer m_gBufferReflection;

        };

        cRendererDeferred( cGraphics* apGraphics, cResources* apResources);
        ~cRendererDeferred();

        inline SharedViewportData& GetSharedData(cViewport& viewport) {
            return m_boundViewportData.resolve(viewport);
        }
        // virtual std::shared_ptr<Image> GetDepthStencilImage(cViewport& viewport) override;
        virtual ForgeTextureHandle* GetOutputImage(cViewport& viewport) override { return nullptr; }

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
        void RenderEdgeSmoothPass(GraphicsContext& context, cViewport& viewport, LegacyRenderTarget& rt);
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
        Image* m_dissolveImage;
        
        ForgeBufferHandle m_perFrameBuffer;

        // diffuse material
        Shader* m_zPassShader;
        Pipeline* m_zPassPipeline;
        RootSignature* m_zPassRootSignature;
        DescriptorSet* m_zPassConstantDescriptorSet;
        DescriptorSet* m_zPassPerFrameDescriptorSet;
        DescriptorSet* m_zPassPerObjectDescriptorSet;
        DescriptorSet* m_zPassPerMaterialDescriptorSet;
        uint32_t m_zObjectIndex = 0;

        Shader* m_ZPassDiffuse;
        Pipeline* m_diffuseZPass;

        Buffer* m_indirectDiffusePositionBuffer;
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
