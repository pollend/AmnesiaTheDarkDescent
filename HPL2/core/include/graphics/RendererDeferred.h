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

#include "bgfx/bgfx.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Image.h"
#include "graphics/Renderer.h"
#include "graphics/ShaderVariantCollection.h"
#include "math/MathTypes.h"
#include <array>
#include <graphics/RenderTarget.h>
#include <memory>

namespace hpl {

	//---------------------------------------------

	//////////////////////////////////////////////////////////////////////////////
	// GENERAL RENDERER INFO - DEFERRRED RENDERER
	//
	// Important to note is that this renderer does not require depth or stencil buffer
	// for the render target, but always use internal stuff for rendering.
	//
	//
	//////////////////////////////////////////////////////////////////////////////

    //---------------------------------------------

	class iFrameBuffer;
	class iDepthStencilBuffer;
	class iTexture;
	class iLight;
	class cSubMeshEntity;

	//---------------------------------------------

	enum eDeferredLightList
	{
		eDeferredLightList_StencilBack_ScreenQuad,		//First draw back to stencil, then draw light as full screen quad
		eDeferredLightList_StencilFront_RenderBack,		//First draw front to stencil, then draw back facing as light.
		eDeferredLightList_RenderBack,					//Draw back facing as light.
		eDeferredLightList_Batches,						//Draw many lights as batch. Spotlights not allowed!

		eDeferredLightList_Box_StencilFront_RenderBack,
		eDeferredLightList_Box_RenderBack,

		eDeferredLightList_LastEnum
	};

	//---------------------------------------------

	enum eDeferredShapeQuality
	{
		eDeferredShapeQuality_Low,
		eDeferredShapeQuality_Medium,
		eDeferredShapeQuality_High,
		eDeferredShapeQuality_LastEnum,
	};

	//---------------------------------------------

	enum eDeferredGBuffer
	{
		eDeferredGBuffer_32Bit,
		eDeferredGBuffer_64Bit,
		eDeferredGBuffer_LastEnum,
	};

	//---------------------------------------------

	enum eDeferredSSAO
	{
		eDeferredSSAO_InBoxLight,
		eDeferredSSAO_OnColorBuffer,
		eDeferredSSAO_LastEnum,
	};

	//---------------------------------------------

	enum eGBufferComponents
	{
		eGBufferComponents_Full,
		eGBufferComponents_ColorAndDepth,
		eGBufferComponents_Color,
		eGBufferComponents_Depth,
		eGBufferComponents_Normals,
		eGBufferComponents_LinearDepth,
		eGBufferComponents_LastEnum,
	};

	//---------------------------------------------

	class cDeferredLight
	{
	public:
		cDeferredLight() : m_occlusionQuery(BGFX_INVALID_HANDLE), mbCastShadows(false){}

		iLight *mpLight;
		cRect2l mClipRect;
		int mlArea;
		cMatrixf m_mtxViewSpaceRender;
		cMatrixf m_mtxViewSpaceTransform;
		bool mbInsideNearPlane;
		bgfx::OcclusionQueryHandle m_occlusionQuery;

		// iTexture *mpShadowTexture;
		bool mbCastShadows;
		eShadowMapResolution mShadowResolution;

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

		struct ZPassInput {
			float m_width = 0;
			float m_height = 0;
			
			Cull m_cull = Cull::CounterClockwise;
            cMatrixf m_view = cMatrixf(cMatrixf::Identity);
            cMatrixf m_projection = cMatrixf(cMatrixf::Identity);
		};

		void RenderZPassObject(bgfx::ViewId view,const ZPassInput& input, GraphicsContext& context, iRenderer* renderer, iRenderable* object, RenderTarget& rt);
	};

	class cRendererDeferred : public  iRenderer
	{
	public:
		cRendererDeferred(cGraphics *apGraphics,cResources* apResources);
		~cRendererDeferred();

		bool LoadData();
		void DestroyData();

		virtual std::shared_ptr<Image> GetDepthStencilImage() override;
		virtual std::shared_ptr<Image> GetOutputImage() override;


		virtual void Draw(GraphicsContext& context, float afFrameTime, cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, RenderViewport& apRenderTarget,
					bool abSendFrameBufferToPostEffects, tRendererCallbackList *apCallbackList) override;



		Image* GetRefractionImage(){ return m_refractionImage;}
		Image* GetReflectionImage(){ return m_reflectionImage;}

		//Static properties. Must be set before renderer data load.
		static void SetGBufferType(eDeferredGBuffer aType){ mGBufferType = aType; }
		static eDeferredGBuffer GetGBufferType(){ return mGBufferType; }

		static void SetNumOfGBufferTextures(int alNum){ mlNumOfGBufferTextures = alNum;}
		static int GetNumOfGBufferTextures(){ return mlNumOfGBufferTextures;}

		static void SetDepthCullLights(bool abX){ mbDepthCullLights = abX;}
		static int GetDepthCullLights(){ return mbDepthCullLights;}

		static void SetSSAOLoaded(bool abX){ mbSSAOLoaded = abX;}
		static void SetSSAONumOfSamples(int alX){ mlSSAONumOfSamples = alX;}
		static void SetSSAOBufferSizeDiv(int alX){ mlSSAOBufferSizeDiv = alX;}
		static void SetSSAOScatterLengthMul(float afX){ mfSSAOScatterLengthMul = afX;}
		static void SetSSAOScatterLengthMin(float afX){ mfSSAOScatterLengthMin = afX;}
		static void SetSSAOScatterLengthMax(float afX){ mfSSAOScatterLengthMax = afX;}
		static void SetSSAOType(eDeferredSSAO aType) {mSSAOType = aType;}
		static void SetSSAODepthDiffMul(float afX){ mfSSAODepthDiffMul = afX;}
		static void SetSSAOSkipEdgeLimit(float afX){mfSSAOSkipEdgeLimit = afX;}

		static bool GetSSAOLoaded(){ return mbSSAOLoaded;}
		static int GetSSAONumOfSamples(){ return mlSSAONumOfSamples;}
		static int GetSSAOBufferSizeDiv(){ return mlSSAOBufferSizeDiv;}
		static float GetSSAOScatterLengthMul(){ return mfSSAOScatterLengthMul;}
		static float GetSSAOScatterLengthMin(){ return mfSSAOScatterLengthMin;}
		static float GetSSAOScatterLengthMax(){ return mfSSAOScatterLengthMax;}
		static eDeferredSSAO GetSSAOType() {return mSSAOType;}
		static float GetSSAODepthDiffMul(){ return mfSSAODepthDiffMul;}
		static float GetSSAOSkipEdgeLimit(){ return mfSSAOSkipEdgeLimit;}

		static void SetEdgeSmoothLoaded(bool abX){ mbEdgeSmoothLoaded = abX;}
		static bool GetEdgeSmoothLoaded(){ return mbEdgeSmoothLoaded;}

		static void SetOcclusionTestLargeLights(bool abX){ mbOcclusionTestLargeLights = abX;}
		static bool GetOcclusionTestLargeLights(){ return mbOcclusionTestLargeLights;}

		static void SetDebugRenderFrameBuffers(bool abX){ mbDebugRenderFrameBuffers = abX;}
		static bool GetDebugRenderFrameBuffers(){ return mbDebugRenderFrameBuffers;}

	private:
		RenderTarget& resolveRenderTarget(std::array<RenderTarget, 2>& rt);
		std::shared_ptr<Image>& resolveRenderImage(std::array<std::shared_ptr<Image>, 2>& img);

		// takes the contents of the gbuffer and renders the lights
		void RenderLightPass(GraphicsContext& context, RenderTarget& rt);  
		void RenderDiffusePass(GraphicsContext& context, RenderTarget& rt);
		void RenderDecalPass(GraphicsContext& context, RenderTarget& rt);
		void RenderIlluminationPass(GraphicsContext& context, RenderTarget& rt);
		void RenderFogPass(GraphicsContext& context, RenderTarget& rt);
		void RenderFullScreenFogPass(GraphicsContext& context, RenderTarget& rt);
		void RenderEdgeSmoothPass(GraphicsContext& context, RenderTarget& rt);
		void RenderTranslucentPass(GraphicsContext& context, RenderTarget& rt);
		void RenderZPass(GraphicsContext& context, RenderTarget& rt);
		void RenderShadowPass(GraphicsContext& context, const cDeferredLight& apLightData, RenderTarget& rt);
		
		void RenderShadowLight(GraphicsContext& context, GraphicsContext::ShaderProgram& shaderProgram, RenderTarget& rt);

		void SetupLightsAndRenderQueries(GraphicsContext& context, RenderTarget& rt);
		void InitLightRendering();

		void RenderReflection(iRenderable *apObject);
		void RenderSubMeshEntityReflection(cSubMeshEntity *pReflectionObject);

		iVertexBuffer* GetLightShape(iLight *apLight, eDeferredShapeQuality aQuality);

		iVertexBuffer *mpShapeSphere[eDeferredShapeQuality_LastEnum];
		iVertexBuffer *mpShapePyramid;

		int mlMaxBatchLights;
		int mlMaxBatchVertices;
		int mlMaxBatchIndices;

		float mfLastFrustumFOV;
		float mfLastFrustumFarPlane;

		float mfFarPlane;
		float mfFarBottom;
		float mfFarTop;
		float mfFarLeft;
		float mfFarRight;

		cMatrixf m_mtxInvView;

		float mfMinLargeLightNormalizedArea;
		int mlMinLargeLightArea;

		float mfMinRenderReflectionNormilzedLength;

		float mfShadowDistanceMedium;
		float mfShadowDistanceLow;
		float mfShadowDistanceNone;

		bool mbStencilNeedClearing;
		cRect2l mStencilDirtyRect;

		std::array<RenderTarget, 2> m_gBuffer_full;
		std::array<RenderTarget, 2> m_gBuffer_colorAndDepth;
		std::array<RenderTarget, 2> m_gBuffer_color;
		std::array<RenderTarget, 2> m_gBuffer_depth;
		std::array<RenderTarget, 2> m_gBuffer_normals;
		std::array<RenderTarget, 2> m_gBuffer_linearDepth;
		std::array<RenderTarget, 2> m_output_target; // used for rendering to the screen 
		
		std::array<std::shared_ptr<Image>, 2> m_gBufferColor;
		std::array<std::shared_ptr<Image>, 2> m_gBufferNormalImage;
		std::array<std::shared_ptr<Image>, 2> m_gBufferPositionImage;
		std::array<std::shared_ptr<Image>, 2> m_gBufferSpecular;
		std::array<std::shared_ptr<Image>, 2> m_gBufferDepthStencil;
		std::array<std::shared_ptr<Image>, 2> m_outputImage;

		RenderTarget m_edgeSmooth_LinearDepth;

		iFrameBuffer *mpAccumBuffer;
		iFrameBuffer *mpReflectionBuffer;

		iTexture *mpRefractionTexture;
		iTexture *mpReflectionTexture;

		Image *m_refractionImage;
		Image *m_reflectionImage;

		bool mbReflectionTextureCleared;

		iTexture *mpShadowJitterTexture;
		int mlShadowJitterSize;
		int mlShadowJitterSamples;

		bgfx::UniformHandle m_u_param;
		bgfx::UniformHandle m_u_boxInvViewModelRotation;
		bgfx::UniformHandle m_u_lightPos;
		bgfx::UniformHandle m_u_fogColor;
		bgfx::UniformHandle m_u_lightColor;
		bgfx::UniformHandle m_u_spotViewProj;
		bgfx::UniformHandle m_u_overrideColor;
		bgfx::UniformHandle m_u_mtxInvViewRotation;

		bgfx::UniformHandle m_s_depthMap;
		bgfx::UniformHandle m_s_positionMap;
		bgfx::UniformHandle m_s_deferredColorMap; // TODO: combined with diffuseMap
		bgfx::UniformHandle m_s_diffuseMap;
		bgfx::UniformHandle m_s_normalMap;
		bgfx::UniformHandle m_s_specularMap;
		bgfx::UniformHandle m_s_attenuationLightMap;
		bgfx::UniformHandle m_s_spotFalloffMap;
		bgfx::UniformHandle m_s_shadowMap;
		bgfx::UniformHandle m_s_goboMap;
		
		bgfx::ProgramHandle m_deferredFog;
		bgfx::ProgramHandle m_fullscreenFog;
		bgfx::ProgramHandle m_edgeSmooth_UnpackDepthProgram;
		bgfx::ProgramHandle m_lightBoxProgram;
		ShaderVariantCollection<
			rendering::detail::SpotlightVariant_UseGoboMap |
			rendering::detail::SpotlightVariant_UseShadowMap> m_spotlightVariants; 
		ShaderVariantCollection<
			rendering::detail::PointlightVariant_UseGoboMap> m_pointLightVariants; 


		std::vector<cDeferredLight*> mvTempDeferredLights;
		std::vector<cDeferredLight*> mvSortedLights[eDeferredLightList_LastEnum];

		iGpuProgram *mpLightStencilProgram;

		cMatrixf m_mtxTempLight;

		//Static setting variables
		static eDeferredGBuffer mGBufferType;
		static int mlNumOfGBufferTextures;
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

		static bool mbDebugRenderFrameBuffers;
		static bool mbOcclusionTestLargeLights;

	};

	//---------------------------------------------

};
