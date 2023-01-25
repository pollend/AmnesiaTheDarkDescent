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

#include "graphics/RendererDeferred.h"

#include "bgfx/bgfx.h"
#include <bx/debug.h>

#include "absl/types/span.h"
#include <cstdint>
#include <graphics/Enum.h>
#include "bx/math.h"
#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include <graphics/RenderViewport.h>
#include "impl/VertexBufferBGFX.h"
#include "math/Math.h"

#include "math/MathTypes.h"
#include "scene/SceneTypes.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/PreprocessParser.h"

#include "graphics/Graphics.h"
#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Renderable.h"
#include "graphics/RenderList.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/OcclusionQuery.h"
#include "graphics/TextureCreator.h"
#include "graphics/ShaderUtil.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/GpuShaderManager.h"
#include "resources/MeshManager.h"
#include "graphics/GPUShader.h"
#include "graphics/GPUProgram.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/RenderableContainer.h"
#include "scene/Light.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/FogArea.h"
#include "scene/MeshEntity.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// STATIC VARIABLES
	//////////////////////////////////////////////////////////////////////////

	eDeferredGBuffer cRendererDeferred::mGBufferType = eDeferredGBuffer_32Bit;
	int cRendererDeferred::mlNumOfGBufferTextures = 4;
	bool cRendererDeferred::mbDepthCullLights = true;

	bool cRendererDeferred::mbSSAOLoaded = false;
	int cRendererDeferred::mlSSAONumOfSamples = 8;
	int cRendererDeferred::mlSSAOBufferSizeDiv = 2;
	float cRendererDeferred::mfSSAOScatterLengthMul = 0.2f;
	float cRendererDeferred::mfSSAOScatterLengthMin = 0.015f;
	float cRendererDeferred::mfSSAOScatterLengthMax = 0.13f;
	float cRendererDeferred::mfSSAODepthDiffMul = 1.5f;
	float cRendererDeferred::mfSSAOSkipEdgeLimit = 3.0f;
	eDeferredSSAO cRendererDeferred::mSSAOType = eDeferredSSAO_OnColorBuffer;
	bool cRendererDeferred::mbEdgeSmoothLoaded = false;

	//debug
	bool cRendererDeferred::mbOcclusionTestLargeLights = true;
	bool cRendererDeferred::mbDebugRenderFrameBuffers = false;

	enum eDefferredProgramMode
	{
		eDefferredProgramMode_Lights,
		eDefferredProgramMode_Misc,
		eDefferredProgramMode_LastEnum
	};


	cRendererDeferred::cRendererDeferred(cGraphics *apGraphics,cResources* apResources)
		: iRenderer("Deferred",apGraphics, apResources,eDefferredProgramMode_LastEnum)
	{
		////////////////////////////////////
		// Set up render specific things
		mbSetFrameBufferAtBeginRendering = false;		//Not using the input frame buffer for any rendering. Only doing copy at the end!
		mbClearFrameBufferAtBeginRendering = false;
		mbSetupOcclusionPlaneForFog = true;

		////////////////////////////////////
		// Set up variables
		mfLastFrustumFOV = -1;
		mfLastFrustumFarPlane = -1;

		mfMinLargeLightNormalizedArea = 0.2f*0.2f;
		mfMinRenderReflectionNormilzedLength = 0.15f;

		mfShadowDistanceMedium = 10;
		mfShadowDistanceLow = 20;
		mfShadowDistanceNone = 40;

		mlMaxBatchLights = 100;

		mbReflectionTextureCleared = false;
	}

	//-----------------------------------------------------------------------
	RenderTarget& cRendererDeferred::resolveRenderTarget(std::array<RenderTarget, 2>& rt) {
		return rt[ mpCurrentSettings->mbIsReflection ? 1 : 0];
	}

	std::shared_ptr<Image>& cRendererDeferred::resolveRenderImage(std::array<std::shared_ptr<Image>, 2>& img) {
		return img[ mpCurrentSettings->mbIsReflection ? 1 : 0];
	}

	cRendererDeferred::~cRendererDeferred()
	{
		for(auto& it: mvTempDeferredLights) {
			delete it;
		}
	}

	bool cRendererDeferred::LoadData()
	{
		cVector2l vRelfectionSize = cVector2l(mvScreenSize.x/mlReflectionSizeDiv, mvScreenSize.y/mlReflectionSizeDiv);

		Log("Setting up G-Bugger: type: %d texturenum: %d\n", mGBufferType, mlNumOfGBufferTextures);
		
		// initialize frame buffers;
		{
			auto colorImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(mvScreenSize.x, mvScreenSize.y, false, bgfx::TextureFormat::Enum::RGBA8);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			auto positionImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(mvScreenSize.x, mvScreenSize.y, false, bgfx::TextureFormat::Enum::RGBA32F);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			auto normalImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(mvScreenSize.x, mvScreenSize.y, false, bgfx::TextureFormat::Enum::RGBA16F);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			auto depthImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(mvScreenSize.x, mvScreenSize.y, false, bgfx::TextureFormat::Enum::D24S8);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			m_gBufferColor = 
				{ colorImage(),colorImage()};
			m_gBufferNormalImage = 
				{ normalImage(), normalImage()};
			m_gBufferPositionImage = 
				{ positionImage(),positionImage()};
			m_gBufferSpecular = 
				{ colorImage(),colorImage()};
			m_outputImage = {
				colorImage(), colorImage()
			};

			m_gBufferDepthStencil = { depthImage(),depthImage()};
			{
				std::array<std::shared_ptr<Image>, 5> images = {m_gBufferColor[0], m_gBufferNormalImage[0], m_gBufferPositionImage[0], m_gBufferSpecular[0], m_gBufferDepthStencil[0]};
				std::array<std::shared_ptr<Image>, 5> reflectionImages = {m_gBufferColor[1], m_gBufferNormalImage[1], m_gBufferPositionImage[1], m_gBufferSpecular[1], m_gBufferDepthStencil[1]};
				m_gBuffer_full = {RenderTarget(absl::MakeSpan(images)), RenderTarget(absl::MakeSpan(reflectionImages)) };
			}
			{
				std::array<std::shared_ptr<Image>, 2> images = {m_gBufferColor[0], m_gBufferDepthStencil[0]};
				std::array<std::shared_ptr<Image>, 2> reflectionImages = {m_gBufferColor[1], m_gBufferDepthStencil[1]};
				m_gBuffer_colorAndDepth = {RenderTarget(absl::MakeSpan(images)), RenderTarget(absl::MakeSpan(reflectionImages))};
			}
			{
				std::array<std::shared_ptr<Image>, 2> images = {m_outputImage[0], m_gBufferDepthStencil[0] };
				std::array<std::shared_ptr<Image>, 2> reflectionImages = {m_outputImage[1], m_gBufferDepthStencil[0]};
				m_output_target = {RenderTarget(absl::MakeSpan(images)), RenderTarget(absl::MakeSpan(reflectionImages))};
			}

			m_gBuffer_color = {RenderTarget(m_gBufferColor[0]), RenderTarget(m_gBufferColor[1])};
			m_gBuffer_depth = {RenderTarget(m_gBufferDepthStencil[0]), RenderTarget(m_gBufferDepthStencil[1])};
			m_gBuffer_normals = {RenderTarget(m_gBufferNormalImage[0]), RenderTarget(m_gBufferNormalImage[1])};
			m_gBuffer_linearDepth = {RenderTarget(m_gBufferPositionImage[0]), RenderTarget(m_gBufferPositionImage[1])};

		}

		// ////////////////////////////////////
		// //Create Accumulation texture
		// mpAccumBufferTexture = mpGraphics->CreateTexture("AccumBiffer",eTextureType_Rect,eTextureUsage_RenderTarget);
		// mpAccumBufferTexture->CreateFromRawData(cVector3l(mvScreenSize.x, mvScreenSize.y,0),ePixelFormat_RGBA, NULL);
		// mpAccumBufferTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

		// ////////////////////////////////////
		// //Create Accumulation buffer
		// mpAccumBuffer = mpGraphics->CreateFrameBuffer("Deferred_Accumulation");
		// mpAccumBuffer->SetTexture2D(0,mpAccumBufferTexture);
		// mpAccumBuffer->SetDepthStencilBuffer(mpDepthStencil[0]);

		// mpAccumBuffer->CompileAndValidate();

		// ////////////////////////////////////
		// //Create Refraction texture
		// mpRefractionTexture = mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA,0)->GetColorBuffer(0)->ToTexture();
		// mpRefractionTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

		// ////////////////////////////////////
		// //Create Reflection texture
		// mpReflectionTexture = CreateRenderTexture("ReflectionTexture",vRelfectionSize,ePixelFormat_RGBA);

		// ////////////////////////////////////
		// //Create Reflection buffer
		// mpReflectionBuffer = mpGraphics->CreateFrameBuffer("Deferred_Reflection");
		// mpReflectionBuffer->SetTexture2D(0,mpReflectionTexture);
		// mpReflectionBuffer->SetDepthStencilBuffer(mpDepthStencil[1]);
		// mpReflectionBuffer->CompileAndValidate();


		////////////////////////////////////
	//Create Shadow Textures
		cVector3l vShadowSize[] = {
				cVector3l(2*128, 2*128,1),
				cVector3l(2*256, 2*256,1),
				cVector3l(2*256, 2*256,1),
				cVector3l(2*512, 2*512,1),
				cVector3l(2*1024, 2*1024,1)
		};
		int lStartSize = 2;
		if(mShadowMapResolution == eShadowMapResolution_Medium) {
			lStartSize = 1;
		} else if(mShadowMapResolution == eShadowMapResolution_Low) {
			lStartSize = 0;
		}

		auto createShadowMap = [](const cVector3l &avSize) -> cShadowMapData {
			auto desc = ImageDescriptor::CreateTexture2D(avSize.x, avSize.y, false, bgfx::TextureFormat::D16F);
			desc.m_configuration.m_rt = RTType::RT_Write;
			auto image = std::make_shared<Image>();
			image->Initialize(desc);
			return {-1,
				RenderTarget(image),
				{}
			};
		};

		for (size_t i = 0; i < 1; ++i)
		{
			m_shadowMapData[eShadowMapResolution_High].emplace_back(
				createShadowMap(vShadowSize[lStartSize + eShadowMapResolution_High]));
		}
		for (size_t i = 0; i < 4; ++i)
		{
			m_shadowMapData[eShadowMapResolution_Medium].emplace_back(
				createShadowMap(vShadowSize[lStartSize + eShadowMapResolution_Medium]));
		}
		for (size_t i = 0; i < 6; ++i)
		{
			m_shadowMapData[eShadowMapResolution_Low].emplace_back(
				createShadowMap(vShadowSize[lStartSize + eShadowMapResolution_Low]));
		}

		// Select samples depending quality and shader model (if dynamic branching is supported)
        if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_4))
		{
			//High
			if(mShadowMapQuality == eShadowMapQuality_High)	{
				mlShadowJitterSize = 32;
				mlShadowJitterSamples = 32;	//64 here instead? I mean, ATI has to deal with medium has max? or different max for ATI?
			}
			//Medium
			else if(mShadowMapQuality == eShadowMapQuality_Medium) {
				mlShadowJitterSize = 32;
				mlShadowJitterSamples = 16;
			}
			//Low
			else {
				mlShadowJitterSize = 0;
				mlShadowJitterSamples = 0;
			}
		}
		else
		{
			//High
			if(mShadowMapQuality == eShadowMapQuality_High)	{
				mlShadowJitterSize = 32;
				mlShadowJitterSamples = 16;
			}
			//Medium
			else if(mShadowMapQuality == eShadowMapQuality_Medium)	{
				mlShadowJitterSize = 32;
				mlShadowJitterSamples = 4;
			}
			//Low
			else {
				mlShadowJitterSize = 0;
				mlShadowJitterSamples = 0;
			}
		}

		if(mShadowMapQuality != eShadowMapQuality_Low)
		{
			m_shadowJitterImage = std::make_shared<Image>();
			TextureCreator::GenerateScatterDiskMap2D(*m_shadowJitterImage, mlShadowJitterSize,mlShadowJitterSamples, true);
		}

		m_lightBoxProgram = hpl::loadProgram("vs_light_box", "fs_light_box");
		m_forVariant.Initialize(
			ShaderHelper::LoadProgramHandlerDefault("vs_deferred_fog", "fs_deferred_fog", false, true));
		m_spotlightVariants.Initialize(
			ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight", false, true));
		m_pointLightVariants.Initialize(
			ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_pointlight", false, true));
		// uniforms
		m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
		m_u_boxInvViewModelRotation  = bgfx::createUniform("u_boxInvViewModelRotation", bgfx::UniformType::Mat4);
		m_u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
		m_u_lightPos = bgfx::createUniform("u_lightPos", bgfx::UniformType::Vec4);
		m_u_fogColor = bgfx::createUniform("u_fogColor", bgfx::UniformType::Vec4);
		m_u_spotViewProj = bgfx::createUniform("u_spotViewProj", bgfx::UniformType::Mat4);
		m_u_mtxInvViewRotation = bgfx::createUniform("u_mtxInvViewRotation", bgfx::UniformType::Mat4);

		// samplers
		m_s_depthMap = bgfx::createUniform("s_depthMap", bgfx::UniformType::Sampler);
		m_s_deferredColorMap = bgfx::createUniform("s_deferredColorMap", bgfx::UniformType::Sampler);
		m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
		m_s_normalMap = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
		m_s_specularMap = bgfx::createUniform("s_specularMap", bgfx::UniformType::Sampler);
		m_s_positionMap = bgfx::createUniform("s_positionMap", bgfx::UniformType::Sampler);
		m_s_attenuationLightMap = bgfx::createUniform("s_attenuationLightMap", bgfx::UniformType::Sampler);
		m_s_spotFalloffMap = bgfx::createUniform("s_spotFalloffMap", bgfx::UniformType::Sampler);
		m_s_shadowMap = bgfx::createUniform("s_shadowMap", bgfx::UniformType::Sampler);
		m_s_goboMap = bgfx::createUniform("s_goboMap", bgfx::UniformType::Sampler);
		m_u_overrideColor = bgfx::createUniform("u_overrideColor", bgfx::UniformType::Vec4);

		////////////////////////////////////
		//Create SSAO programs and textures
		if(mbSSAOLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
		{
			mbSSAOLoaded = false;
			Warning("System does not support float textures! SSAO is disabled.\n");
		}
		if(mbSSAOLoaded)
		{
			cVector2l vSSAOSize = mvScreenSize / mlSSAOBufferSizeDiv;

		}

		////////////////////////////////////
		//Create Smooth Edge and textures
		if(mbEdgeSmoothLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
		{
			mbEdgeSmoothLoaded = false;
			Warning("System does not support float textures! Edge smooth is disabled.\n");
		}
		if(mbEdgeSmoothLoaded)
		{
			
		}

		////////////////////////////////////
		//Create light shapes
		tFlag lVtxFlag = eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0;
		mpShapeSphere[eDeferredShapeQuality_High] = LoadVertexBufferFromMesh("core_12_12_sphere.dae",lVtxFlag);
		mpShapeSphere[eDeferredShapeQuality_Medium] = LoadVertexBufferFromMesh("core_7_7_sphere.dae",lVtxFlag);
		mpShapeSphere[eDeferredShapeQuality_Low] = LoadVertexBufferFromMesh("core_5_5_sphere.dae",lVtxFlag);

		mpShapePyramid = LoadVertexBufferFromMesh("core_pyramid.dae",lVtxFlag);

		////////////////////////////////////
		//Batch vertex buffer
		mlMaxBatchVertices = mpShapeSphere[eDeferredShapeQuality_Low]->GetVertexNum() * mlMaxBatchLights;
		mlMaxBatchIndices = mpShapeSphere[eDeferredShapeQuality_Low]->GetIndexNum() * mlMaxBatchLights;

		return true;
	}

	//-----------------------------------------------------------------------


	void cRendererDeferred::DestroyData()
	{
		for(auto& shape: mpShapeSphere) {
			if(shape) {
				delete shape;
			}
		}
		if(mpShapePyramid) hplDelete(mpShapePyramid);


		// mpGraphics->DestroyTexture(mpReflectionTexture);


		/////////////////////////
		//Shadow textures
		DestroyShadowMaps();

		// if(mpShadowJitterTexture) mpGraphics->DestroyTexture(mpShadowJitterTexture);

		/////////////////////////
		//Fog stuff
		// hplDelete(mpFogProgramManager);

		/////////////////////////
		//SSAO textures and programs
		if(mbSSAOLoaded)
		{
			// mpGraphics->DestroyTexture(mpSSAOTexture);
			// mpGraphics->DestroyTexture(mpSSAOBlurTexture);
			// mpGraphics->DestroyTexture(mpSSAOScatterDisk);

			// mpGraphics->DestroyFrameBuffer(mpSSAOBuffer);
			// mpGraphics->DestroyFrameBuffer(mpSSAOBlurBuffer);

			// mpGraphics->DestroyGpuProgram(mpUnpackDepthProgram);
			// for(int i=0;i<2; ++i)
			// 	mpGraphics->DestroyGpuProgram(mpSSAOBlurProgram[i]);
			// mpGraphics->DestroyGpuProgram(mpSSAORenderProgram);
		}

		/////////////////////////////
		// Edge smooth
		if(mbEdgeSmoothLoaded)
		{
			// mpGraphics->DestroyFrameBuffer(mpEdgeSmooth_LinearDepthBuffer);

			// mpGraphics->DestroyGpuProgram(mpEdgeSmooth_UnpackDepthProgram);
			// mpGraphics->DestroyGpuProgram(mpEdgeSmooth_RenderProgram);
		}

	}

	std::shared_ptr<Image> cRendererDeferred::GetDepthStencilImage() {
		return m_gBuffer_depth[0].GetImage();
	}

	std::shared_ptr<Image> cRendererDeferred::GetOutputImage() {
		return m_output_target[0].GetImage();
	}

	// NOTE: this logic is incomplete
	void cRendererDeferred::RenderFogPass(GraphicsContext& context, RenderTarget& rt) {
		if(mpCurrentRenderList->GetFogAreaNum() == 0)
		{
			mpCurrentSettings->mvFogRenderData.resize(0); //Make sure render data array is empty!
			return;
		}
		const auto view = context.StartPass("Fog Pass");
		for(auto& fogData: mpCurrentSettings->mvFogRenderData)
		{
			cFogArea* pFogArea = fogData.mpFogArea;

			struct {
				float u_fogStart;
				float u_fogLength;
				float u_fogFalloffExp;

				float u_fogRayCastStart[3];
				float u_fogNegPlaneDistNeg[3];
				float u_fogNegPlaneDistPos[3];
			} uniforms = {0};

			//Outside of box setup
			cMatrixf rotationMatrix = cMatrixf::Identity;
			uint32_t flags =  rendering::detail::FogVariant_None;
			if(!fogData.mbInsideNearFrustum)
			{
				cMatrixf mtxInvModelView = cMath::MatrixInverse( cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), *pFogArea->GetModelMatrixPtr()) );
				cVector3f vRayCastStart = cMath::MatrixMul(mtxInvModelView, cVector3f(0));

				uniforms.u_fogRayCastStart[0] = vRayCastStart.x;
				uniforms.u_fogRayCastStart[1] = vRayCastStart.y;
				uniforms.u_fogRayCastStart[2] = vRayCastStart.z;

				rotationMatrix = mtxInvModelView.GetRotation().GetTranspose();

				cVector3f vNegPlaneDistNeg( cMath::PlaneToPointDist(cPlanef(-1,0,0,0.5f),vRayCastStart), cMath::PlaneToPointDist(cPlanef(0,-1,0,0.5f),vRayCastStart),
											cMath::PlaneToPointDist(cPlanef(0,0,-1,0.5f),vRayCastStart));
				cVector3f vNegPlaneDistPos( cMath::PlaneToPointDist(cPlanef(1,0,0,0.5f),vRayCastStart), cMath::PlaneToPointDist(cPlanef(0,1,0,0.5f),vRayCastStart),
											cMath::PlaneToPointDist(cPlanef(0,0,1,0.5f),vRayCastStart));
				
				uniforms.u_fogNegPlaneDistNeg[0] = vNegPlaneDistNeg.x * -1;
				uniforms.u_fogNegPlaneDistNeg[1] = vNegPlaneDistNeg.y * -1;
				uniforms.u_fogNegPlaneDistNeg[2] = vNegPlaneDistNeg.z * -1;

				uniforms.u_fogNegPlaneDistPos[0] = vNegPlaneDistPos.x * -1;
				uniforms.u_fogNegPlaneDistPos[1] = vNegPlaneDistPos.y * -1;
				uniforms.u_fogNegPlaneDistPos[2] = vNegPlaneDistPos.z * -1;
			}
			if(fogData.mbInsideNearFrustum)
			{
				flags |=  pFogArea->GetShowBacksideWhenInside() ? rendering::detail::FogVariant_UseBackSide : rendering::detail::FogVariant_None;
			} 
			else 
			{
				flags |= rendering::detail::FogVariant_UseOutsideBox;
				flags |=  pFogArea->GetShowBacksideWhenOutside() ? rendering::detail::FogVariant_UseBackSide : rendering::detail::FogVariant_None;
			}
			
			const auto fogColor = mpCurrentWorld->GetFogColor();
			float uniformFogColor[4] = {fogColor.r, fogColor.g, fogColor.b, fogColor.a};

			GraphicsContext::LayoutStream layoutStream;
			GraphicsContext::ShaderProgram shaderProgram;
			shaderProgram.m_handle = m_forVariant.GetVariant(flags);
			shaderProgram.m_uniforms.push_back(
				{m_u_param, &uniforms, 3});
			shaderProgram.m_uniforms.push_back(
				{m_u_boxInvViewModelRotation, rotationMatrix.v});
			shaderProgram.m_uniforms.push_back(
				{m_u_fogColor, uniformFogColor, 1});
			
			shaderProgram.m_configuration.m_rgbBlendFunc = 
				CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
			shaderProgram.m_configuration.m_write = Write::RGB;
			
			shaderProgram.m_projection = mpCurrentProjectionMatrix->GetTranspose();
			shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderProgram.m_modelTransform = pFogArea->GetModelMatrixPtr() ? cMatrixf::Identity : pFogArea->GetModelMatrixPtr()->GetTranspose();

			mpShapeBox->GetLayoutStream(layoutStream);

			GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
			drawRequest.m_width = mvScreenSize.x;
			drawRequest.m_height = mvScreenSize.y;
			context.Submit(view, drawRequest);
		}
	}


	void cRendererDeferred::RenderFullScreenFogPass(GraphicsContext& context, RenderTarget& rt) {
		const auto view = context.StartPass("Full Screen Fog");
		
		struct {
			float u_fogStart;
			float u_fogLength;
			float u_fogFalloffExp;
		} uniforms = {0};

		uniforms.u_fogStart = mpCurrentWorld->GetFogStart();
		uniforms.u_fogLength = mpCurrentWorld->GetFogEnd() - mpCurrentWorld->GetFogStart();
		uniforms.u_fogFalloffExp = mpCurrentWorld->GetFogFalloffExp();
		
		GraphicsContext::LayoutStream layout;
		cMatrixf projMtx;
		context.ScreenSpaceQuad(layout, projMtx, mvScreenSize.x, mvScreenSize.y);
		
		GraphicsContext::ShaderProgram shaderProgram;
		shaderProgram.m_configuration.m_write = Write::RGBA;
		shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
		shaderProgram.m_projection = projMtx;

		shaderProgram.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
		shaderProgram.m_configuration.m_write = Write::RGB;

		cMatrixf rotationMatrix = cMatrixf::Identity;
		const auto fogColor = mpCurrentWorld->GetFogColor();
		float uniformFogColor[4] = {fogColor.r, fogColor.g, fogColor.b, fogColor.a};

		shaderProgram.m_handle = m_forVariant.GetVariant(rendering::detail::FogVariant_None);
		shaderProgram.m_textures.push_back(
			{m_s_depthMap, resolveRenderImage(m_gBufferPositionImage)->GetHandle(), 0});
		shaderProgram.m_uniforms.push_back(
			{m_u_param, &uniforms, 3});
		shaderProgram.m_uniforms.push_back(
				{m_u_boxInvViewModelRotation, rotationMatrix.v});
		shaderProgram.m_uniforms.push_back(
				{m_u_fogColor, uniformFogColor, 1});
		
		GraphicsContext::DrawRequest drawRequest {rt, layout, shaderProgram};
		drawRequest.m_width = mvScreenSize.x;
		drawRequest.m_height = mvScreenSize.y;
		context.Submit(view, drawRequest);
	}

	void cRendererDeferred::RenderIlluminationPass(GraphicsContext& context, RenderTarget& rt) {
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Illumination)) {
			return;
		}

		bgfx::ViewId view = context.StartPass("RenderIllumination");
		RenderableHelper(eRenderListType_Illumination, eMaterialRenderMode_Illumination, [&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
			shaderInput.m_configuration.m_depthTest = DepthTest::Equal;
			shaderInput.m_configuration.m_write = Write::RGBA;

			shaderInput.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
			shaderInput.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

			shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ?  obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();

			GraphicsContext::DrawRequest drawRequest {rt, layoutInput, shaderInput};
			drawRequest.m_width = mvScreenSize.x;
			drawRequest.m_height = mvScreenSize.y;
			context.Submit(view, drawRequest);
		});
	}

	void cRendererDeferred::RenderEdgeSmoothPass(GraphicsContext& context, RenderTarget& rt) {

		auto edgeSmoothView = context.StartPass("EdgeSmooth");
		cVector3f vQuadPos = cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane);
		cVector2f vQuadSize = cVector2f(mfFarRight*2,mfFarTop*2);

		GraphicsContext::ShaderProgram shaderProgram;
		shaderProgram.m_handle = m_edgeSmooth_UnpackDepthProgram;
		shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_gBufferPositionImage)->GetHandle(), 0});
		shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_gBufferNormalImage)->GetHandle(), 0});

		GraphicsContext::LayoutStream layout;
		context.Quad(layout, vQuadPos, cVector2f(1,1), cVector2f(0,0), cVector2f(1,1));

		GraphicsContext::DrawRequest drawRequest {m_edgeSmooth_LinearDepth, layout, shaderProgram};
		context.Submit(edgeSmoothView, drawRequest);
	}


	void cRendererDeferred::RenderDiffusePass(GraphicsContext& context, RenderTarget& rt) {
		BX_ASSERT(rt.GetImages().size() >= 4, "expected 4 images Diffuse(0), Normal(1), Position(2), Specular(3), Depth(4)");
		BX_ASSERT(rt.IsValid(), "Invalid render target");
		
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Diffuse)) {
			return;
		}

		auto& target = resolveRenderTarget(m_gBuffer_full);
		auto view = context.StartPass("Diffuse");
		RenderableHelper(eRenderListType_Diffuse, eMaterialRenderMode_Diffuse, [&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			shaderInput.m_configuration.m_write = Write::RGBA;
			shaderInput.m_configuration.m_cull = Cull::CounterClockwise;

			shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ?  obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();
			shaderInput.m_normalMtx = cMath::MatrixInverse(cMath::MatrixMul(shaderInput.m_modelTransform, shaderInput.m_view)); // matrix is already transposed

			GraphicsContext::DrawRequest drawRequest {target, layoutInput, shaderInput};
			drawRequest.m_width = mvScreenSize.x;
			drawRequest.m_height = mvScreenSize.y;
			context.Submit(view, drawRequest);
		});
	}

	void cRendererDeferred::RenderDecalPass(GraphicsContext& context, RenderTarget& rt) {
		BX_ASSERT(rt.IsValid(), "Invalid render target");
		BX_ASSERT(rt.GetImages().size() >= 1, "expected atleast 1 image Color(0)");
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Decal)) {
			return;
		}

		auto& target = resolveRenderTarget(m_gBuffer_colorAndDepth);
		auto view = context.StartPass("RenderDecals");
		RenderableHelper(eRenderListType_Decal, eMaterialRenderMode_Diffuse, [&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			shaderInput.m_configuration.m_write = Write::RGBA;

			cMaterial *pMaterial = obj->GetMaterial();
			shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
			shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());

			shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ?  obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();

			// shaderInput.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
			
			GraphicsContext::DrawRequest drawRequest {target, layoutInput, shaderInput};
			drawRequest.m_width = mvScreenSize.x;
			drawRequest.m_height = mvScreenSize.y;

			context.Submit(view, drawRequest);
		});
	}


	void rendering::detail::RenderZPassObject(bgfx::ViewId view, const ZPassInput& input, GraphicsContext& context, iRenderer* renderer, iRenderable* object, RenderTarget& rt) {
		eMaterialRenderMode renderMode = object->GetCoverageAmount() >= 1 ? eMaterialRenderMode_Z : eMaterialRenderMode_Z_Dissolve;
		cMaterial *pMaterial = object->GetMaterial();
		iMaterialType* materialType = pMaterial->GetType();
		iVertexBuffer* vertexBuffer = object->GetVertexBuffer();
		if(vertexBuffer == nullptr || materialType == nullptr) {
			return;
		}

		GraphicsContext::LayoutStream layoutInput;
		// GraphicsContext::ShaderProgram shaderInput;
		vertexBuffer->GetLayoutStream(layoutInput);
		materialType->ResolveShaderProgram(renderMode, pMaterial, object, renderer, [&](GraphicsContext::ShaderProgram& shaderInput) {
			shaderInput.m_configuration.m_write = Write::Depth;
			shaderInput.m_configuration.m_cull = input.m_cull;
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			
			shaderInput.m_modelTransform = object->GetModelMatrixPtr() ? object->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity ;
			shaderInput.m_view = input.m_view;
			shaderInput.m_projection = input.m_projection;

			GraphicsContext::DrawRequest drawRequest {rt, layoutInput, shaderInput};
			drawRequest.m_width = input.m_width;
			drawRequest.m_height = input.m_height;
			context.Submit(view, drawRequest);
		});
		
	}

	void cRendererDeferred::RenderZPass(GraphicsContext& context, RenderTarget& rt) {
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Z)) {
			return;
		}

		auto view = context.StartPass("RenderZ");
		for(auto& obj: mpCurrentRenderList->GetRenderableItems(eRenderListType_Z)) {
			rendering::detail::ZPassInput options;
			options.m_height = mvScreenSize.y;
			options.m_width = mvScreenSize.x;
			
			options.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			options.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			rendering::detail::RenderZPassObject(view, options, context, this, obj, rt);
		}
	}


	void cRendererDeferred::RenderTranslucentPass(GraphicsContext& context, RenderTarget& target) {
		auto view = context.StartPass("Translucent");
		for(auto& obj: mpCurrentRenderList->GetRenderableItems(eRenderListType_Translucent))
		{
			
			auto* pMaterial = obj->GetMaterial();
			auto* pMaterialType = pMaterial->GetType();
			auto* vertexBuffer = obj->GetVertexBuffer();
			// const bool hasColorAttribute = vertexBuffer->GetElement(eVertexBufferElement_Color0) != nullptr; // hack 
			
			cMatrixf *pMatrix = obj->GetModelMatrix(mpCurrentFrustum);

			eMaterialRenderMode renderMode = mpCurrentWorld->GetFogActive() ? eMaterialRenderMode_DiffuseFog : eMaterialRenderMode_Diffuse;
			if(!pMaterial->GetAffectedByFog()) {
				renderMode = eMaterialRenderMode_Diffuse;
			}

			////////////////////////////////////////
			// Check the fog area alpha
			mfTempAlpha = 1;
			if(pMaterial->GetAffectedByFog())
			{
				for(size_t i=0; i<mpCurrentSettings->mvFogRenderData.size(); ++i)
				{
					mfTempAlpha *= GetFogAreaVisibilityForObject(&mpCurrentSettings->mvFogRenderData[i], obj);
				}
			}

			if(!obj->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime)) 
			{
				continue;
			}

			if(obj->RetrieveOcculsionQuery(this)==false)
			{
				continue;
			}
			
			if(pMaterial->HasRefraction())
			{
				// skipping refraction for now
			}

			if(pMaterial->HasWorldReflection() && obj->GetRenderType() == eRenderableType_SubMesh)
			{
			}

			pMaterialType->ResolveShaderProgram(renderMode, pMaterial, obj, this,[&](GraphicsContext::ShaderProgram& shaderInput) {
				GraphicsContext::LayoutStream layoutInput;
				vertexBuffer->GetLayoutStream(layoutInput);
				
				shaderInput.m_configuration.m_depthTest = pMaterial->GetDepthTest() ? DepthTest::LessEqual: DepthTest::None;
				shaderInput.m_configuration.m_write = Write::RGB;
				shaderInput.m_configuration.m_cull = Cull::None;
				
				// shaderInput.m_uniforms.push_back({m_u_overrideColor, overrideColor });

				shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
				shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
				shaderInput.m_modelTransform = pMatrix ?  pMatrix->GetTranspose() : cMatrixf::Identity;
				
				if(!pMaterial->HasRefraction()) {
					shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
					shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
				}

				GraphicsContext::DrawRequest drawRequest {target, layoutInput, shaderInput};
				drawRequest.m_width = mvScreenSize.x;
				drawRequest.m_height = mvScreenSize.y;
				context.Submit(view, drawRequest);
			});

			if(pMaterial->HasTranslucentIllumination())
			{
				pMaterialType->ResolveShaderProgram(
					(renderMode == eMaterialRenderMode_Diffuse ? eMaterialRenderMode_Illumination : eMaterialRenderMode_IlluminationFog), pMaterial, obj, this,[&](GraphicsContext::ShaderProgram& shaderInput) {
					GraphicsContext::LayoutStream layoutInput;
					vertexBuffer->GetLayoutStream(layoutInput);

					shaderInput.m_configuration.m_depthTest = pMaterial->GetDepthTest() ? DepthTest::LessEqual: DepthTest::None;
					shaderInput.m_configuration.m_write = Write::RGB;
					shaderInput.m_configuration.m_cull = Cull::None;

					shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);
					shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);

					shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderInput.m_modelTransform = pMatrix ?  pMatrix->GetTranspose() : cMatrixf::Identity;
					
					GraphicsContext::DrawRequest drawRequest {target, layoutInput, shaderInput};
					drawRequest.m_width = mvScreenSize.x;
					drawRequest.m_height = mvScreenSize.y;
					context.Submit(view, drawRequest);
				});
			}
		}
	}

	void cRendererDeferred::Draw(GraphicsContext& context, float afFrameTime, cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, RenderViewport& apRenderTarget,
					bool abSendFrameBufferToPostEffects, tRendererCallbackList *apCallbackList)  {
		// keep around for the moment ...
		BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, apRenderTarget, abSendFrameBufferToPostEffects, apCallbackList);
		
		mpCurrentRenderList->Setup(mfCurrentFrameTime,mpCurrentFrustum);
		
		//Setup far plane coordinates
		mfFarPlane = mpCurrentFrustum->GetFarPlane();
		mfFarTop = -tan(mpCurrentFrustum->GetFOV()*0.5f) * mfFarPlane;
		mfFarBottom = -mfFarTop;
		mfFarRight = mfFarBottom * mpCurrentFrustum->GetAspect();
		mfFarLeft = -mfFarRight;

		cRendererCallbackFunctions handler(context, this);

		[&]{
			auto& target = resolveRenderTarget(m_gBuffer_full);
			auto view = context.StartPass("Clear Depth");
			GraphicsContext::DrawClear clear {
				target,
				{0, 1, 0, ClearOp::Depth | ClearOp::Stencil | ClearOp::Color},
				0,
				0,
				static_cast<uint16_t>(mvScreenSize.x),
 				static_cast<uint16_t>(mvScreenSize.y)
			};
			context.ClearTarget(view, clear);	
		}();

		tRenderableFlag lVisibleFlags= (mpCurrentSettings->mbIsReflection) ?
			eRenderableFlag_VisibleInReflection : eRenderableFlag_VisibleInNonReflection;

		///////////////////////////
		//Occlusion testing
		if(mpCurrentSettings->mbUseOcclusionCulling)
		{ 
			auto view = context.StartPass("Render Z");
			RenderZPassWithVisibilityCheck(	
				context, 
				mpCurrentSettings->mpVisibleNodeTracker,
				eObjectVariabilityFlag_All,
				lVisibleFlags,
				resolveRenderTarget(m_gBuffer_depth),
			[&](iRenderable* object) {
				cMaterial *pMaterial = object->GetMaterial();

				mpCurrentRenderList->AddObject(object);

				if(!pMaterial || pMaterial->GetType()->IsTranslucent()) {
					return false;
				}
				rendering::detail::ZPassInput options;
				options.m_width = mvScreenSize.x;
				options.m_height = mvScreenSize.y;

				options.m_view =  mpCurrentFrustum->GetViewMatrix().GetTranspose();
				options.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
				rendering::detail::RenderZPassObject(view, options, context, this, object, resolveRenderTarget(m_gBuffer_depth));

				return true;
			});

			// this occlusion query logic is used for reflections just cut this for now
			AssignAndRenderOcclusionQueryObjects(
				context.StartPass("Render Occlusion"), 
				context, 
				false, 
				true,
				resolveRenderTarget(m_gBuffer_depth));

			SetupLightsAndRenderQueries(context, resolveRenderTarget(m_gBuffer_depth));

			mpCurrentRenderList->Compile(	eRenderListCompileFlag_Diffuse |
											eRenderListCompileFlag_Translucent |
											eRenderListCompileFlag_Decal |
											eRenderListCompileFlag_Illumination);
		}
		///////////////////////////
		//Brute force
		else
		{
			CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), lVisibleFlags);
			CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), lVisibleFlags);

			mpCurrentRenderList->Compile(	eRenderListCompileFlag_Z |
											eRenderListCompileFlag_Diffuse |
											eRenderListCompileFlag_Translucent |
											eRenderListCompileFlag_Decal |
											eRenderListCompileFlag_Illumination);
			RenderZPass(context, resolveRenderTarget(m_gBuffer_depth) );

			// this occlusion query logic is used for reflections just cut this for now
			AssignAndRenderOcclusionQueryObjects(
				context.StartPass("Render Occlusion Pass"), 
				context, 
				false, 
				true,
				resolveRenderTarget(m_gBuffer_depth));

			SetupLightsAndRenderQueries(context, resolveRenderTarget(m_gBuffer_depth));
		}


		// Render GBuffer to m_gBuffer_full old method is RenderGbuffer(context);
		RenderDiffusePass(context, resolveRenderTarget(m_gBuffer_full));
		RenderDecalPass(context, resolveRenderTarget(m_gBuffer_colorAndDepth));

		RunCallback(eRendererMessage_PostGBuffer, handler);

		RenderLightPass(context, resolveRenderTarget(m_output_target));

		// render illumination into gbuffer color RenderIllumination
		RenderIlluminationPass(context, resolveRenderTarget(m_output_target));
		// TODO: MP need to implement this
		RenderFogPass(context, resolveRenderTarget(m_output_target));
		if(mpCurrentWorld->GetFogActive()) {
			RenderFullScreenFogPass(context, resolveRenderTarget(m_output_target));
		}

		// TODO: MP need to implement this
		//  RenderEdgeSmooth();
		// if(mbEdgeSmoothLoaded && mpCurrentSettings->mbUseEdgeSmooth) {
		// 	RenderEdgeSmoothPass(context, m_edgeSmooth_LinearDepth);
		// }
		RunCallback(eRendererMessage_PostSolid, handler);

		// not going to even try.
		// calls back through RendererDeffered need to untangle all the complicated state ...
		RenderTranslucentPass(context, resolveRenderTarget(m_output_target));

		RunCallback(eRendererMessage_PostTranslucent, handler);

	}


	void cRendererDeferred::RenderShadowPass(GraphicsContext& context, const cDeferredLight& apLightData, RenderTarget& rt) {
 		BX_ASSERT(apLightData.mpLight->GetLightType() == eLightType_Spot, "Only spot lights are supported for shadow rendering")
		cVector2l size = rt.GetImage()->GetImageSize();
		cLightSpot *pSpotLight = static_cast<cLightSpot*>(apLightData.mpLight);
		cFrustum *pLightFrustum = pSpotLight->GetFrustum();

		GraphicsContext::DrawClear depthClear {
			rt,
			{0, 1.0, 0, ClearOp::Depth},
			0,
			0,
			static_cast<uint16_t>(size.x),
			static_cast<uint16_t>(size.y)
		};
		context.ClearTarget(context.StartPass("Shadow Pass Clear"), depthClear);
		
		bgfx::ViewId view = context.StartPass("Shadow Pass");
		for(size_t i=0; i<mvShadowCasters.size(); ++i)
		{
			rendering::detail::ZPassInput options;
			options.m_height = size.y;
			options.m_width = size.x;
			options.m_cull = pLightFrustum->GetInvertsCullMode() ? Cull::Clockwise : Cull::CounterClockwise;
			options.m_view = pLightFrustum->GetViewMatrix().GetTranspose();
			options.m_projection = pLightFrustum->GetProjectionMatrix().GetTranspose();
			rendering::detail::RenderZPassObject(view, options, context, this, mvShadowCasters[i], rt);
		}
	}

	void cRendererDeferred::RenderLightPass(GraphicsContext& context, RenderTarget& rt) {
		InitLightRendering();
		
		{
			const auto pass = context.StartPass("Clear Backbuffer");
			GraphicsContext::DrawClear clear {
				rt,
				{0, 1, 0, ClearOp::Color},
				0,
				0,
				static_cast<uint16_t>(mvScreenSize.x),
				static_cast<uint16_t>(mvScreenSize.y)
			};
			context.ClearTarget(pass, clear);	
		}


		// Drawing Box Lights
		{
			auto drawBoxLight = [&](bgfx::ViewId view, GraphicsContext::ShaderProgram& shaderProgram, cDeferredLight* light) {
				GraphicsContext::LayoutStream layoutStream;
				mpShapeBox->GetLayoutStream(layoutStream);
				cLightBox *pLightBox = static_cast<cLightBox*>(light->mpLight);

				const auto& color = light->mpLight->GetDiffuseColor();
				float lightColor[4] = {color.r, color.g, color.b, color.a};
				shaderProgram.m_handle = m_lightBoxProgram;
				shaderProgram.m_textures.push_back({m_s_deferredColorMap, resolveRenderImage(m_gBufferColor)->GetHandle(), 0});
				shaderProgram.m_uniforms.push_back({m_u_lightColor, lightColor});

				shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
				shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
				shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(), light->GetLightMtx()).GetTranspose();

				switch(pLightBox->GetBlendFunc())
				{
				case eLightBoxBlendFunc_Add:
					shaderProgram.m_configuration.m_rgbBlendFunc = 
						CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					shaderProgram.m_configuration.m_alphaBlendFunc = 
						CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					break;
				case eLightBoxBlendFunc_Replace:
					break;
				default:
					BX_ASSERT(false, "Unknown blend func %d", pLightBox->GetBlendFunc());
					break;
				}

				GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
				drawRequest.m_width = mvScreenSize.x;
				drawRequest.m_height = mvScreenSize.y;
				// if(bgfx::isValid(light->m_occlusionQuery)) {
				// 	bgfx::setCondition(light->m_occlusionQuery, true);
				// }
				context.Submit(view, drawRequest);
			};


			const auto boxStencilPass = context.StartPass("eDeferredLightList_Box_StencilFront_RenderBack");
			bgfx::setViewMode(boxStencilPass, bgfx::ViewMode::Sequential);

			// auto& renderStencilFront = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack];
			// auto lightIt = renderStencilFront.begin();
			for(auto& light : mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack]) {
				{
					GraphicsContext::ShaderProgram shaderProgram;
					GraphicsContext::LayoutStream layoutStream;
					mpShapeBox->GetLayoutStream(layoutStream);

					shaderProgram.m_handle = m_nullShader;
					shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
						StencilFunction::Always,
						StencilFail::Keep, 
						StencilDepthFail::Replace, 
						StencilDepthPass::Keep,
						0xff, 0xff);
				
					shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(), light->GetLightMtx()).GetTranspose();

					GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
					drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
					drawRequest.m_width = mvScreenSize.x;
					drawRequest.m_height = mvScreenSize.y;
					// if(bgfx::isValid(light->m_occlusionQuery)) {
					// 	bgfx::setCondition(light->m_occlusionQuery, true);
					// }
					context.Submit(boxStencilPass, drawRequest);
				}
				
				{

					GraphicsContext::ShaderProgram shaderProgram;
					shaderProgram.m_configuration.m_cull = Cull::Clockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					shaderProgram.m_configuration.m_write = Write::RGBA;
					shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
						StencilFunction::Equal,
						StencilFail::Zero, 
						StencilDepthFail::Zero, 
						StencilDepthPass::Zero,
						0xff, 0xff);
					drawBoxLight(boxStencilPass, shaderProgram, light);
				}
			}

			const auto boxLightBackPass = context.StartPass("eDeferredLightList_Box_RenderBack");
			for(auto& light: mvSortedLights[eDeferredLightList_Box_RenderBack])
			{
				GraphicsContext::ShaderProgram shaderProgram;
				shaderProgram.m_configuration.m_cull = Cull::Clockwise;
				shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
				shaderProgram.m_configuration.m_write = Write::RGBA;
				drawBoxLight(boxLightBackPass, shaderProgram, light);
			}
		
		}

		// note: skipping implementing RenderLights_StencilBack_ScreenQuad

		// render light
		{
			auto drawLight = [&](bgfx::ViewId pass, GraphicsContext::ShaderProgram& shaderProgram, cDeferredLight* apLightData) {
				// if(bgfx::isValid(apLightData->m_occlusionQuery)) {
				// 	bgfx::setCondition(apLightData->m_occlusionQuery, true);
				// }
				
				GraphicsContext::LayoutStream layoutStream;
				GetLightShape(apLightData->mpLight, eDeferredShapeQuality_High)->GetLayoutStream(layoutStream);
				GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
				drawRequest.m_width = mvScreenSize.x;
				drawRequest.m_height = mvScreenSize.y;
				switch(apLightData->mpLight->GetLightType()) {
					case eLightType_Point: {
						struct {
							float lightRadius;
							float pad[3];
						} param = {0};
						param.lightRadius = apLightData->mpLight->GetRadius();
						auto attenuationImage = apLightData->mpLight->GetFalloffMap();

						const auto modelViewMtx = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), apLightData->mpLight->GetWorldMatrix());
						const auto color = apLightData->mpLight->GetDiffuseColor();
						cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, apLightData->GetLightMtx()).GetTranslation();
						float lightPosition[4] = {lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f};
						float lightColor[4] = {color.r, color.g, color.b, color.a};
						cMatrixf mtxInvViewRotation = cMath::MatrixMul(apLightData->mpLight->GetWorldMatrix(),m_mtxInvView).GetTranspose();

						shaderProgram.m_uniforms.push_back({m_u_lightPos, lightPosition});
						shaderProgram.m_uniforms.push_back({m_u_lightColor, lightColor});
						shaderProgram.m_uniforms.push_back({m_u_param, &param});
						shaderProgram.m_uniforms.push_back({m_u_mtxInvViewRotation, &mtxInvViewRotation.v});

						shaderProgram.m_textures.push_back({m_s_diffuseMap, resolveRenderImage(m_gBufferColor)->GetHandle(), 1});
						shaderProgram.m_textures.push_back({m_s_normalMap, resolveRenderImage(m_gBufferNormalImage)->GetHandle(), 2});
						shaderProgram.m_textures.push_back({m_s_positionMap, resolveRenderImage(m_gBufferPositionImage)->GetHandle(), 3});
						shaderProgram.m_textures.push_back({m_s_specularMap, resolveRenderImage(m_gBufferSpecular)->GetHandle(), 4});
						shaderProgram.m_textures.push_back({m_s_attenuationLightMap, attenuationImage->GetHandle(), 5});

						uint32_t flags = 0;
						if(apLightData->mpLight->GetGoboTexture()) {
							flags |= rendering::detail::PointlightVariant_UseGoboMap;
							shaderProgram.m_textures.push_back({m_s_goboMap, apLightData->mpLight->GetGoboTexture()->GetHandle(), 0});
						}
						shaderProgram.m_handle = m_pointLightVariants.GetVariant(flags);
						context.Submit(pass, drawRequest);
						break;
					}
					case eLightType_Spot: {
						cLightSpot *pLightSpot = static_cast<cLightSpot*>(apLightData->mpLight);
						//Calculate and set the forward vector
						cVector3f vForward = cVector3f(0,0,1);
						vForward = cMath::MatrixMul3x3(apLightData->m_mtxViewSpaceTransform, vForward);

						struct {
							float lightRadius;
							float lightForward[3];

							float oneMinusCosHalfSpotFOV;
							float pad[3];
						} uParam = {
							apLightData->mpLight->GetRadius(),
							{vForward.x, vForward.y, vForward.z},
							1 - pLightSpot->GetCosHalfFOV(),
							{0,0,0}

						};
						auto goboImage = apLightData->mpLight->GetGoboTexture();
						auto spotFallOffImage = pLightSpot->GetSpotFalloffMap();
						auto spotAttenuationImage = pLightSpot->GetFalloffMap();
						
						uint32_t flags = 0;
						const auto modelViewMtx = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), apLightData->mpLight->GetWorldMatrix());
						const auto color = apLightData->mpLight->GetDiffuseColor();
						cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, apLightData->GetLightMtx()).GetTranslation();
						float lightPosition[4] = {lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f};
						float lightColor[4] = {color.r, color.g, color.b, color.a};
						cMatrixf spotViewProj = cMath::MatrixMul(pLightSpot->GetViewProjMatrix(),m_mtxInvView).GetTranspose();
				
						shaderProgram.m_uniforms.push_back({m_u_lightPos, lightPosition});
						shaderProgram.m_uniforms.push_back({m_u_lightColor, lightColor});
						shaderProgram.m_uniforms.push_back({m_u_spotViewProj, spotViewProj.v});

						shaderProgram.m_uniforms.push_back({m_u_param, &uParam, 1});

						shaderProgram.m_textures.push_back({m_s_diffuseMap, resolveRenderImage(m_gBufferColor)->GetHandle(), 0});
						shaderProgram.m_textures.push_back({m_s_normalMap, resolveRenderImage(m_gBufferNormalImage)->GetHandle(), 1});
						shaderProgram.m_textures.push_back({m_s_positionMap, resolveRenderImage(m_gBufferPositionImage)->GetHandle(), 2});
						shaderProgram.m_textures.push_back({m_s_specularMap, resolveRenderImage(m_gBufferSpecular)->GetHandle(), 3});
					
						shaderProgram.m_textures.push_back({m_s_attenuationLightMap, spotAttenuationImage->GetHandle(), 4});
						if(goboImage) {
							shaderProgram.m_textures.push_back({m_s_goboMap, goboImage->GetHandle(), 5});
							flags |= rendering::detail::SpotlightVariant_UseGoboMap;
						} else {
							shaderProgram.m_textures.push_back({m_s_spotFalloffMap, spotFallOffImage->GetHandle(), 5});
						}

						if(apLightData->mbCastShadows && SetupShadowMapRendering(pLightSpot))
						{
							flags |= rendering::detail::SpotlightVariant_UseShadowMap;
							eShadowMapResolution shadowMapRes = apLightData->mShadowResolution;
							cShadowMapData *pShadowData = GetShadowMapData(shadowMapRes, apLightData->mpLight);
							if(ShadowMapNeedsUpdate(apLightData->mpLight, pShadowData))
							{
								RenderShadowPass(context, *apLightData, pShadowData->m_target);
							}
							shaderProgram.m_textures.push_back({m_s_shadowMap, pShadowData->m_target.GetImage()->GetHandle(), 6});
						}
						shaderProgram.m_handle = m_spotlightVariants.GetVariant(flags);
						
						context.Submit(pass, drawRequest);
						break;
					}
					default:
						break;
				}
			};
			
			// auto& renderStencilFrontRenderBack = mvSortedLights[eDeferredLightList_StencilFront_RenderBack];
			// auto lightIt = renderStencilFrontRenderBack.begin();\

			const auto lightStencilBackPass = context.StartPass("eDeferredLightList_StencilFront_RenderBack");
			bgfx::setViewMode(lightStencilBackPass, bgfx::ViewMode::Sequential);
			for(auto& light: mvSortedLights[eDeferredLightList_StencilFront_RenderBack]) 
			{
				{
					GraphicsContext::ShaderProgram shaderProgram;
					GraphicsContext::LayoutStream layoutStream;
					
					GetLightShape(light->mpLight, eDeferredShapeQuality_Medium)->GetLayoutStream(layoutStream);

					shaderProgram.m_handle = m_nullShader;
					shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					
					shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(), light->GetLightMtx()).GetTranspose();
					
					shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
						StencilFunction::Always,
						StencilFail::Keep, 
						StencilDepthFail::Replace, 
						StencilDepthPass::Keep,
						0xff, 0xff);
				
					GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
					drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
					drawRequest.m_width = mvScreenSize.x;
					drawRequest.m_height = mvScreenSize.y;
					// if(bgfx::isValid(light->m_occlusionQuery)) {
					// 	bgfx::setCondition(light->m_occlusionQuery, true);
					// }
					context.Submit(lightStencilBackPass, drawRequest);
				}
				{
					GraphicsContext::ShaderProgram shaderProgram;
					shaderProgram.m_configuration.m_cull = Cull::Clockwise;
					shaderProgram.m_configuration.m_write = Write::RGBA;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
						StencilFunction::Equal,
						StencilFail::Zero, 
						StencilDepthFail::Zero, 
						StencilDepthPass::Zero,
						0xff, 0xff);
					shaderProgram.m_configuration.m_rgbBlendFunc = 
						CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					shaderProgram.m_configuration.m_alphaBlendFunc = 
						CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					
					shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(),light->GetLightMtx()).GetTranspose();
					
					drawLight(lightStencilBackPass, shaderProgram, light);
				}
			}

			const auto lightBackPass = context.StartPass("eDeferredLightList_RenderBack");
			for(auto& light: mvSortedLights[eDeferredLightList_RenderBack])
			{
				GraphicsContext::ShaderProgram shaderProgram;
				shaderProgram.m_configuration.m_cull = Cull::Clockwise;
				shaderProgram.m_configuration.m_write = Write::RGBA;
				shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
				shaderProgram.m_configuration.m_rgbBlendFunc = 
					CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
				shaderProgram.m_configuration.m_alphaBlendFunc = 
					CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
				
				shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
				shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
				shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(), light->GetLightMtx()).GetTranspose();

				drawLight(lightBackPass, shaderProgram, light);
			}
		}
	}

	void cRendererDeferred::RenderShadowLight(GraphicsContext& context, GraphicsContext::ShaderProgram& shaderProgram, RenderTarget& rt) {

	}
	
	//Definitions used when rendering lights
	#define kLightRadiusMul_High		1.08f
	#define kLightRadiusMul_Medium		1.12f
	#define kLightRadiusMul_Low			1.2f
	#define kMaxStencilBitsUsed			8
	#define kStartStencilBit			0


	/**
	 * Calculates matrices for both rendering shape and the transformation
	 * \param a_mtxDestRender This has a scale based on radius and radius mul. The mul is to make sure that the shape covers the while light.
	 * \param a_mtxDestTransform A simple view space transform for the light.
	 * \param afRadiusMul
	 */
	static inline void SetupLightMatrix(cMatrixf& a_mtxDestRender,cMatrixf& a_mtxDestTransform, iLight *apLight, cFrustum *apFrustum, float afRadiusMul)
	{
		////////////////////////////
		// Point Light
		if(apLight->GetLightType() == eLightType_Point)
		{
			a_mtxDestRender = cMath::MatrixScale(apLight->GetRadius() * afRadiusMul); //kLightRadiusMul = make sure it encapsulates the light.
			a_mtxDestTransform = cMath::MatrixMul(apFrustum->GetViewMatrix(), apLight->GetWorldMatrix());
			a_mtxDestRender = cMath::MatrixMul(a_mtxDestTransform,a_mtxDestRender);
		}
		////////////////////////////
		// Spot Light
		else if(apLight->GetLightType() == eLightType_Spot)
		{
			cLightSpot *pLightSpot = static_cast<cLightSpot*>(apLight);

			float fFarHeight = pLightSpot->GetTanHalfFOV() * pLightSpot->GetRadius() * 2.0f;
			//Note: Aspect might be wonky if there is no gobo.
			float fFarWidth = fFarHeight * pLightSpot->GetAspect();

			a_mtxDestRender = cMath::MatrixScale(cVector3f(fFarWidth,fFarHeight,apLight->GetRadius()) );//x and y = "far plane", z = radius
			a_mtxDestTransform = cMath::MatrixMul(apFrustum->GetViewMatrix(), apLight->GetWorldMatrix());
			a_mtxDestRender = cMath::MatrixMul(a_mtxDestTransform,a_mtxDestRender);
		}
	}

	cMatrixf cDeferredLight::GetLightMtx() {
		if(!mpLight) {
			return cMatrixf::Identity;
		}
		switch(mpLight->GetLightType()) {
			case eLightType_Point:
				return cMath::MatrixScale(mpLight->GetRadius() * kLightRadiusMul_Medium);
			case eLightType_Spot: {
				cLightSpot *pLightSpot = static_cast<cLightSpot*>(mpLight);

				float fFarHeight = pLightSpot->GetTanHalfFOV() * pLightSpot->GetRadius() * 2.0f;
				//Note: Aspect might be wonky if there is no gobo.
				float fFarWidth = fFarHeight * pLightSpot->GetAspect();

				return cMath::MatrixScale(cVector3f(fFarWidth,fFarHeight,mpLight->GetRadius()) );//x and y = "far plane", z = radius
			}
			case eLightType_Box: {
				cLightBox *pLightBox = static_cast<cLightBox*>(mpLight);

				return cMath::MatrixScale(pLightBox->GetSize());
			}
			default:
				break;
		}
		
		return cMatrixf::Identity;
	}

	iVertexBuffer* cRendererDeferred::GetLightShape(iLight *apLight, eDeferredShapeQuality aQuality)
	{
		///////////////////
		//Point Light
		if(apLight->GetLightType() == eLightType_Point)
		{
			return mpShapeSphere[aQuality];
		}
		///////////////////
		// Spot Light
		else if(apLight->GetLightType() == eLightType_Spot)
		{
			return mpShapePyramid;
		}

		return NULL;
	}

	static bool SortFunc_Box(const cDeferredLight* apLightDataA, const cDeferredLight* apLightDataB)
	{
		iLight *pLightA = apLightDataA->mpLight;
		iLight *pLightB = apLightDataB->mpLight;

		cLightBox *pBoxLightA = static_cast<cLightBox*>(pLightA);
		cLightBox *pBoxLightB = static_cast<cLightBox*>(pLightB);

		if(pBoxLightA->GetBoxLightPrio() != pBoxLightB->GetBoxLightPrio())
			return pBoxLightA->GetBoxLightPrio() < pBoxLightB->GetBoxLightPrio();

		//////////////////////////
		//Pointer
		return pLightA < pLightB;
	}

	//-----------------------------------------------------------------------

	static bool SortFunc_Default(const cDeferredLight* apLightDataA, const cDeferredLight* apLightDataB)
	{
		iLight *pLightA = apLightDataA->mpLight;
		iLight *pLightB = apLightDataB->mpLight;

		//////////////////////////
		//Type
		if(pLightA->GetLightType() != pLightB->GetLightType())
		{
			return pLightA->GetLightType() < pLightB->GetLightType();
		}

		//////////////////////////
		//Specular
		int lHasSpecularA = pLightA->GetDiffuseColor().a > 0 ? 1 : 0;
		int lHasSpecularB = pLightB->GetDiffuseColor().a > 0 ? 1 : 0;
		if(lHasSpecularA != lHasSpecularB)
		{
			return lHasSpecularA < lHasSpecularB;
		}

		////////////////////////////////
		// Point inside near plane
		if(pLightA->GetLightType() == eLightType_Point)
		{
			return apLightDataA->mbInsideNearPlane < apLightDataB->mbInsideNearPlane;
		}

		//////////////////////////
		//Gobo
		if(pLightA->GetGoboTexture() != pLightB->GetGoboTexture())
		{
			return pLightA->GetGoboTexture() < pLightB->GetGoboTexture();
		}

		//////////////////////////
		//Attenuation
		if(pLightA->GetFalloffMap() != pLightB->GetFalloffMap())
		{
			return pLightA->GetFalloffMap() < pLightB->GetFalloffMap();
		}

		//////////////////////////
		//Spot falloff
		if(pLightA->GetLightType() == eLightType_Spot)
		{
			cLightSpot *pLightSpotA = static_cast<cLightSpot*>(pLightA);
			cLightSpot *pLightSpotB = static_cast<cLightSpot*>(pLightB);

			if(pLightSpotA->GetSpotFalloffMap() != pLightSpotB->GetSpotFalloffMap())
			{
				return pLightSpotA->GetSpotFalloffMap() < pLightSpotB->GetSpotFalloffMap();
			}
		}

		//////////////////////////
		//Pointer
		return pLightA < pLightB;
	}



	//-----------------------------------------------------------------------

	typedef bool (*tSortDeferredLightFunc)(const cDeferredLight* apLightDataA, const cDeferredLight* apLightDataB);

	static tSortDeferredLightFunc vLightSortFunctions[eDeferredLightList_LastEnum] = {	SortFunc_Default,
																						SortFunc_Default,
																						SortFunc_Default,

																						SortFunc_Default, //<- Batches, not used!

																						SortFunc_Box,
																						SortFunc_Box};


	void cRendererDeferred::SetupLightsAndRenderQueries(GraphicsContext& context, RenderTarget& rt)
	{

		//////////////////////////
		// Clear light list
		for(auto& light: mvTempDeferredLights) {
			// if(bgfx::isValid(light->m_occlusionQuery)) {
			// 	bgfx::destroy(light->m_occlusionQuery);
			// }
			delete light;
		}
		mvTempDeferredLights.resize(0);

		////////////////////////////////
		// Set up variables
		float fScreenArea = (float)(mvRenderTargetSize.x*mvRenderTargetSize.y);

		mlMinLargeLightArea =	(int)(mfMinLargeLightNormalizedArea * fScreenArea);
		auto view = context.StartPass("Light Render Queries");

		///////////////////////////////
		//Iterate all lights in render list
		for(int i=0; i<mpCurrentRenderList->GetLightNum(); ++i)
		{
			cDeferredLight* pLightData = hplNew( cDeferredLight, () );
			iLight* pLight = mpCurrentRenderList->GetLight(i);
			eLightType lightType = pLight->GetLightType();

			pLightData->mpLight = pLight;

			////////////////////////////////
			//Add to light list used later when rendering.
			mvTempDeferredLights.push_back(pLightData);

			////////////////////////////
			// Skip box lights
			if(lightType == eLightType_Box) {
				continue;
			}

			//Point
			if(lightType == eLightType_Point)
			{
				pLightData->mbInsideNearPlane = mpCurrentFrustum->CheckSphereNearPlaneIntersection(	pLight->GetWorldPosition(),
																								pLight->GetRadius()*kLightRadiusMul_Low);
			}
			//Spot
			else if(lightType == eLightType_Spot)
			{
				cLightSpot *pLightSpot = static_cast<cLightSpot*>(pLight);
				pLightData->mbInsideNearPlane = mpCurrentFrustum->CheckFrustumNearPlaneIntersection(pLightSpot->GetFrustum());
			}

			////////////////////////////////////
			// Setup Light matrix
			SetupLightMatrix(	pLightData->m_mtxViewSpaceRender, pLightData->m_mtxViewSpaceTransform,
								pLight,mpCurrentFrustum,kLightRadiusMul_Medium);


			////////////////////////
			//Calculate screen clip rect and area

			//Point
			if(lightType == eLightType_Point)
			{
				pLightData->mClipRect = cMath::GetClipRectFromSphere(pLightData->m_mtxViewSpaceRender.GetTranslation(), pLight->GetRadius(),
																	mpCurrentFrustum,mvRenderTargetSize,
																	true,mfScissorLastTanHalfFov);
			}
			//Spot
			else if(lightType == eLightType_Spot)
			{
				cMath::GetClipRectFromBV(	pLightData->mClipRect, *pLight->GetBoundingVolume(), mpCurrentFrustum,
											mvScreenSize, mfScissorLastTanHalfFov);
			}
			pLightData->mlArea = pLightData->mClipRect.w * pLightData->mClipRect.h;

			///////////////////////////
			// Setup Shadow casting variables
			if(lightType == eLightType_Spot && pLight->GetCastShadows()  && mpCurrentSettings->mbRenderShadows)
			{
				cLightSpot *pLightSpot = static_cast<cLightSpot*>(pLight);

				////////////////////////
				//Inside near plane, use max resolution
				if(pLightData->mbInsideNearPlane)
				{
					pLightData->mbCastShadows = true;
					
					pLightData->mShadowResolution = rendering::detail::GetShadowMapResolution(pLight->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);
				}
				else
				{
					cVector3f vIntersection = pLightSpot->GetFrustum()->GetOrigin();
					pLightSpot->GetFrustum()->CheckLineIntersection(mpCurrentFrustum->GetOrigin(), pLight->GetBoundingVolume()->GetWorldCenter(),vIntersection);

					float fDistToLight = cMath::Vector3Dist(mpCurrentFrustum->GetOrigin(), vIntersection);

					pLightData->mbCastShadows = true;
					pLightData->mShadowResolution = rendering::detail::GetShadowMapResolution(pLight->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);

					///////////////////////
					//Skip shadow
					if(fDistToLight > mfShadowDistanceNone)
					{
						pLightData->mbCastShadows = false;
					}
					///////////////////////
					//Use Low
					else if(fDistToLight > mfShadowDistanceLow)
					{
						if(pLightData->mShadowResolution == eShadowMapResolution_Low)
							pLightData->mbCastShadows = false;
						pLightData->mShadowResolution = eShadowMapResolution_Low;
					}
					///////////////////////
					//Use Medium
					else if(fDistToLight > mfShadowDistanceMedium)
					{
						if(pLightData->mShadowResolution == eShadowMapResolution_High)
							pLightData->mShadowResolution = eShadowMapResolution_Medium;
						else
							pLightData->mShadowResolution = eShadowMapResolution_Low;
					}
				}

			}

			// If inside near plane or too small on screen skip queries
			if(pLightData->mbInsideNearPlane || pLightData->mlArea < mlMinLargeLightArea) {
				continue;
			}

			auto lightVertex = GetLightShape(pLight, eDeferredShapeQuality_Medium);
			if(!lightVertex) {
				continue;
			}

			GraphicsContext::ShaderProgram shaderProgram;
			GraphicsContext::LayoutStream layoutStream;
		
			// ////////////////////////////////
			// //Render light shape and make a query
			// BX_ASSERT(!bgfx::isValid(pLightData->m_occlusionQuery), "Occlusion query already exists!");
			// pLightData->m_occlusionQuery = bgfx::createOcclusionQuery();

			// lightVertex->GetLayoutStream(layoutStream);

			// shaderProgram.m_handle = m_nullShader;
			// shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
			// shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;

			// shaderProgram.m_modelTransform = pLightData->m_mtxViewSpaceRender.GetTranspose();
			// shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();


			// GraphicsContext::DrawRequest drawRequest = {rt, layoutStream, shaderProgram};
			// drawRequest.m_width = mvScreenSize.x;
			// drawRequest.m_height = mvScreenSize.y;
			// context.Submit(view, drawRequest, pLightData->m_occlusionQuery);
			
		}
	}

	void cRendererDeferred::InitLightRendering()
	{
		/////////////////////////////////
		// Get the inverse view matrix
		m_mtxInvView = cMath::MatrixInverse(mpCurrentFrustum->GetViewMatrix());

		//////////////////////////////
		//Setup misc variables
		mbStencilNeedClearing = false;

		//////////////////////////////
		//Clear lists
		for(auto& lights: mvSortedLights) {
			lights.clear();
		}

		//////////////////////////////
		//Fill lists
		mpCurrentSettings->mlNumberOfLightsRendered =0;
		for(size_t i=0; i<mvTempDeferredLights.size(); ++i)
		{
			cDeferredLight* pLightData =  mvTempDeferredLights[i];
			iLight* pLight = pLightData->mpLight;
			eLightType lightType = pLight->GetLightType();

			////////////////////////
			//If box, we have special case...
			if(lightType == eLightType_Box)
			{
				cLightBox *pLightBox = static_cast<cLightBox*>(pLight);

				//Set up matrix
				pLightData->m_mtxViewSpaceRender = cMath::MatrixScale(pLightBox->GetSize());
				pLightData->m_mtxViewSpaceRender.SetTranslation(pLightBox->GetWorldPosition());
				pLightData->m_mtxViewSpaceRender = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), pLightData->m_mtxViewSpaceRender);

				mpCurrentSettings->mlNumberOfLightsRendered++;

				//Check if near plane is inside box. If so only render back
				if( mpCurrentFrustum->CheckBVNearPlaneIntersection(pLight->GetBoundingVolume()) )
				{
					mvSortedLights[eDeferredLightList_Box_RenderBack].push_back(pLightData);
				}
				else
				{
					mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack].push_back(pLightData);
				}

				continue;
			}

			// ////////////////////////
			// // Test if query has any samples.
			// //  Only check if the query is done, else skip so we do not have a stop-and-wait.
			// auto& prevResult = pLightData->m_occlusionResult;
			// bool bLightVisible = true;
			// if(prevResult.second == bgfx::OcclusionQueryResult::Enum::Visible ||
			// 	prevResult.second == bgfx::OcclusionQueryResult::Enum::Invisible)
			// {
			// 	if(prevResult.first <= mpCurrentSettings->mlSampleVisiblilityLimit)
			// 	{
			// 		bLightVisible = false;
			// 	}

			// 	// bx::debugPrintf("Fetching query for light '%s'/%d. Have %d samples. Visible: %d\n", pLight->GetName().c_str(), pLight, numSamples, !bLightInvisible);
			// }

			// if(!bLightVisible)
			// {
			// 	continue;
			// }
		

			mpCurrentSettings->mlNumberOfLightsRendered++;

			////////////////////////
			//Check what list to put light in
			if(pLightData->mbInsideNearPlane)
			{
				if(lightType == eLightType_Point)
				{
					//mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].push_back(lightData);
					mvSortedLights[eDeferredLightList_RenderBack].push_back(pLightData);
				}
				else
				{
					mvSortedLights[eDeferredLightList_RenderBack].push_back(pLightData);
				}
			}
			else
			{
				if(lightType == eLightType_Point)
				{
					if(pLightData->mlArea >= mlMinLargeLightArea) {
						mvSortedLights[eDeferredLightList_StencilFront_RenderBack].push_back(pLightData);
					} else {
						mvSortedLights[eDeferredLightList_RenderBack].push_back(pLightData);
					}
				}
				//Always do double passes for spotlights as they need to will get artefacts otherwise...
				//(At least with gobos)l
				else if(lightType == eLightType_Spot)
				{
					//mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].push_back(pLightData);
					mvSortedLights[eDeferredLightList_StencilFront_RenderBack].push_back(pLightData);
				}



				//Skip batching for now, only speed boosts when having many small lights
				//Add later when proper test scenes exist.
				//mvSortedLights[eDeferredLightList_Batches].push_back(lightData);
			}
		}

		//Log("Near lights: %d\n",mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].size());
		//Log("Large lights: %d\n",mvSortedLights[eDeferredLightList_StencilFront_RenderBack].size());
		//Log("Default lights: %d\n",mvSortedLights[eDeferredLightList_RenderBack].size());

		//////////////////////////////
		//Sort lists
		for(int i=0; i<eDeferredLightList_LastEnum; ++i)
		{
			if(mvSortedLights[i].size() > 0)
			{
				std::sort(mvSortedLights[i].begin(), mvSortedLights[i].end(), vLightSortFunctions[i]);
			}
		}

	}

}