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

namespace hpl {

    class iFrameBuffer;
    class iDepthStencilBuffer;
    class iTexture;
    class iLight;
    class cSubMeshEntity;

    //---------------------------------------------

    enum eDeferredLightList {
        eDeferredLightList_StencilBack_ScreenQuad, // First draw back to stencil, then draw light as full screen quad
        eDeferredLightList_StencilFront_RenderBack, // First draw front to stencil, then draw back facing as light.
        eDeferredLightList_RenderBack, // Draw back facing as light.
        eDeferredLightList_Batches, // Draw many lights as batch. Spotlights not allowed!

        eDeferredLightList_Box_StencilFront_RenderBack,
        eDeferredLightList_Box_RenderBack,

        eDeferredLightList_LastEnum
    };

    //---------------------------------------------

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
    class cDeferredLight final {
    public:
        cDeferredLight() {
        }

        cRect2l mClipRect;
        cMatrixf m_mtxViewSpaceRender;
        cMatrixf m_mtxViewSpaceTransform;

        int mlArea = 0;
        bool mbInsideNearPlane = false;
        bool mbCastShadows = false;
        eShadowMapResolution mShadowResolution = eShadowMapResolution_Low;
        iLight* mpLight = nullptr;

        cMatrixf GetLightMtx();
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
        HPL_RTTI_IMPL_CLASS(cRendererDeferred, iRenderer, "{A3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:
        struct SharedViewportData {
        public:
            SharedViewportData() = default;
            SharedViewportData(const SharedViewportData&) = delete;
            SharedViewportData(SharedViewportData&& buffer)
                : m_size(buffer.m_size)
                , m_refractionImage(std::move(buffer.m_refractionImage))
                , m_gBufferColor(std::move(buffer.m_gBufferColor))
                , m_gBufferNormalImage(std::move(buffer.m_gBufferNormalImage))
                , m_gBufferPositionImage(std::move(buffer.m_gBufferPositionImage))
                , m_gBufferSpecular(std::move(buffer.m_gBufferSpecular))
                , m_gBufferDepthStencil(std::move(buffer.m_gBufferDepthStencil))
                , m_outputImage(std::move(buffer.m_outputImage))
                , m_gBuffer_full(std::move(buffer.m_gBuffer_full))
                , m_gBuffer_colorAndDepth(std::move(buffer.m_gBuffer_colorAndDepth))
                , m_gBuffer_color(std::move(buffer.m_gBuffer_color))
                , m_gBuffer_depth(std::move(buffer.m_gBuffer_depth))
                , m_gBuffer_normals(std::move(buffer.m_gBuffer_normals))
                , m_gBuffer_linearDepth(std::move(buffer.m_gBuffer_linearDepth))
                , m_output_target(std::move(buffer.m_output_target))
                , m_gBufferReflectionColor(std::move(buffer.m_gBufferReflectionColor))
                , m_gBufferReflectionNormalImage(std::move(buffer.m_gBufferReflectionNormalImage))
                , m_gBufferReflectionPositionImage(std::move(buffer.m_gBufferReflectionPositionImage))
                , m_gBufferReflectionSpecular(std::move(buffer.m_gBufferReflectionSpecular))
                , m_gBufferReflectionDepthStencil(std::move(buffer.m_gBufferReflectionDepthStencil))
                , m_outputReflectionImage(std::move(buffer.m_outputReflectionImage))
                , m_gBufferReflection_full(std::move(buffer.m_gBufferReflection_full))
                , m_gBufferReflection_colorAndDepth(std::move(buffer.m_gBufferReflection_colorAndDepth))
                , m_gBufferReflection_color(std::move(buffer.m_gBufferReflection_color))
                , m_gBufferReflection_depth(std::move(buffer.m_gBufferReflection_depth))
                , m_gBufferReflection_normals(std::move(buffer.m_gBufferReflection_normals))
                , m_gBufferReflection_linearDepth(std::move(buffer.m_gBufferReflection_linearDepth))
                , m_outputReflection_target(std::move(buffer.m_outputReflection_target)) {
            }

            SharedViewportData& operator=(const SharedViewportData&) = delete;

            void operator=(SharedViewportData&& buffer) {
                m_size = buffer.m_size;
                m_refractionImage = std::move(buffer.m_refractionImage);
                m_gBufferColor = std::move(buffer.m_gBufferColor);
                m_gBufferNormalImage = std::move(buffer.m_gBufferNormalImage);
                m_gBufferPositionImage = std::move(buffer.m_gBufferPositionImage);
                m_gBufferSpecular = std::move(buffer.m_gBufferSpecular);
                m_gBufferDepthStencil = std::move(buffer.m_gBufferDepthStencil);

                m_outputImage = std::move(buffer.m_outputImage);
                m_gBuffer_full = std::move(buffer.m_gBuffer_full);
                m_gBuffer_colorAndDepth = std::move(buffer.m_gBuffer_colorAndDepth);
                m_gBuffer_color = std::move(buffer.m_gBuffer_color);
                m_gBuffer_depth = std::move(buffer.m_gBuffer_depth);
                m_gBuffer_normals = std::move(buffer.m_gBuffer_normals);
                m_gBuffer_linearDepth = std::move(buffer.m_gBuffer_linearDepth);
                m_output_target = std::move(buffer.m_output_target);

                m_gBufferReflectionColor = std::move(buffer.m_gBufferReflectionColor);
                m_gBufferReflectionNormalImage = std::move(buffer.m_gBufferReflectionNormalImage);
                m_gBufferReflectionPositionImage = std::move(buffer.m_gBufferReflectionPositionImage);
                m_gBufferReflectionSpecular = std::move(buffer.m_gBufferReflectionSpecular);
                m_gBufferReflectionDepthStencil = std::move(buffer.m_gBufferReflectionDepthStencil);
                m_outputReflectionImage = std::move(buffer.m_outputReflectionImage);

                m_gBufferReflection_full = std::move(buffer.m_gBufferReflection_full);
                m_gBufferReflection_colorAndDepth = std::move(buffer.m_gBufferReflection_colorAndDepth);
                m_gBufferReflection_color = std::move(buffer.m_gBufferReflection_color);
                m_gBufferReflection_depth = std::move(buffer.m_gBufferReflection_depth);
                m_gBufferReflection_normals = std::move(buffer.m_gBufferReflection_normals);
                m_gBufferReflection_linearDepth = std::move(buffer.m_gBufferReflection_linearDepth);
                m_outputReflection_target = std::move(buffer.m_outputReflection_target);
            }

            cVector2l m_size = cVector2l(0, 0);

            std::shared_ptr<Image> m_refractionImage;

            std::shared_ptr<Image> m_gBufferColor;
            std::shared_ptr<Image> m_gBufferNormalImage;
            std::shared_ptr<Image> m_gBufferPositionImage;
            std::shared_ptr<Image> m_gBufferSpecular;
            std::shared_ptr<Image> m_gBufferDepthStencil;
            std::shared_ptr<Image> m_outputImage;

            RenderTarget m_gBuffer_full;
            RenderTarget m_gBuffer_colorAndDepth;
            RenderTarget m_gBuffer_color;
            RenderTarget m_gBuffer_depth;
            RenderTarget m_gBuffer_normals;
            RenderTarget m_gBuffer_linearDepth;
            RenderTarget m_output_target; // used for rendering to the screen

            std::shared_ptr<Image> m_gBufferReflectionColor;
            std::shared_ptr<Image> m_gBufferReflectionNormalImage;
            std::shared_ptr<Image> m_gBufferReflectionPositionImage;
            std::shared_ptr<Image> m_gBufferReflectionSpecular;
            std::shared_ptr<Image> m_gBufferReflectionDepthStencil;
            std::shared_ptr<Image> m_outputReflectionImage;

            RenderTarget m_gBufferReflection_full;
            RenderTarget m_gBufferReflection_colorAndDepth;
            RenderTarget m_gBufferReflection_color;
            RenderTarget m_gBufferReflection_depth;
            RenderTarget m_gBufferReflection_normals;
            RenderTarget m_gBufferReflection_linearDepth;
            RenderTarget m_outputReflection_target; // used for rendering to the screen
        };

        cRendererDeferred(cGraphics* apGraphics, cResources* apResources);
        ~cRendererDeferred();

        inline SharedViewportData& GetSharedData(cViewport& viewport) {
            return m_boundViewportData.resolve(viewport);
        }
        // virtual std::shared_ptr<Image> GetDepthStencilImage(cViewport& viewport) override;
        virtual std::shared_ptr<Image> GetOutputImage(cViewport& viewport) override;

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
        RenderTarget& resolveRenderTarget(std::array<RenderTarget, 2>& rt);
        std::shared_ptr<Image>& resolveRenderImage(std::array<std::shared_ptr<Image>, 2>& img);

        void RenderEdgeSmoothPass(GraphicsContext& context, cViewport& viewport, RenderTarget& rt);

        iVertexBuffer* GetLightShape(iLight* apLight, eDeferredShapeQuality aQuality) const;

        std::array<std::unique_ptr<iVertexBuffer>, eDeferredShapeQuality_LastEnum> m_shapeSphere;
        std::unique_ptr<iVertexBuffer> m_shapePyramid;

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

        RenderTarget m_edgeSmooth_LinearDepth;
        UniqueViewportData<SharedViewportData> m_boundViewportData;

        iTexture* mpShadowJitterTexture;
        std::shared_ptr<Image> m_shadowJitterImage;
        int m_shadowJitterSize;
        int m_shadowJitterSamples;

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
        
        bgfx::ProgramHandle m_copyRegionProgram;
        bgfx::ProgramHandle m_edgeSmooth_UnpackDepthProgram;
        bgfx::ProgramHandle m_lightBoxProgram;

        ShaderVariantCollection<rendering::detail::FogVariant_UseOutsideBox | rendering::detail::FogVariant_UseBackSide> m_fogVariant;
        ShaderVariantCollection<rendering::detail::SpotlightVariant_UseGoboMap | rendering::detail::SpotlightVariant_UseShadowMap>
            m_spotlightVariants;
        ShaderVariantCollection<rendering::detail::PointlightVariant_UseGoboMap> m_pointLightVariants;

        std::vector<cDeferredLight*> mvTempDeferredLights;
        std::array<std::vector<cDeferredLight*>, eDeferredLightList_LastEnum> mvSortedLights;
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
