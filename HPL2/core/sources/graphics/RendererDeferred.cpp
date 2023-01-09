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

#include "absl/types/span.h"
#include "bgfx/bgfx.h"
#include <bx/debug.h>
#include <cstdint>
#include <graphics/Enum.h>
#include "bx/math.h"
#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include <graphics/RenderViewport.h>
#include "math/Math.h"

#include "math/MathTypes.h"
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
#include "graphics/ProgramComboManager.h"
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
#include <memory>

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

	#define eFeature_Light_LightShapes		eFlagBit_0
	#define eFeature_Light_Batching			eFlagBit_1
	#define eFeature_Light_Specular			eFlagBit_2
	#define eFeature_Light_SpotLight		eFlagBit_3
	#define eFeature_Light_Gobo				eFlagBit_4
	#define eFeature_Light_DivideInFrag		eFlagBit_5
	#define eFeature_Light_ShadowMap		eFlagBit_6

	#define kLightFeatureNum 7

	cProgramComboFeature gvLightFeatureVec[] =
	{
		cProgramComboFeature("UseDeferredLightShapes", kPC_VertexBit),
		cProgramComboFeature("UseBatching", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseSpecular", kPC_FragmentBit),
		cProgramComboFeature("LightType_Spot", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseGobo", kPC_FragmentBit),
		cProgramComboFeature("DivideInFrag", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseShadowMap", kPC_FragmentBit, eFeature_Light_SpotLight),
	};

	//////////////////////////////////////////////////////////////////////////
	// PROGRAM VARAIBLES
	//////////////////////////////////////////////////////////////////////////

	#define kVar_avLightPos							0
	#define kVar_avLightColor						1
	#define kVar_afInvLightRadius					2
	#define kVar_afNegFarPlane						3
	#define kVar_afOneMinusCosHalfSpotFOV			4
	#define kVar_avLightForward						5
	#define kVar_a_mtxSpotViewProj					6
	#define kVar_a_mtxInvViewRotation				7
	#define kVar_avShadowMapOffsetMul				8
	#define kVar_afNegInvFarPlane					9
	#define kVar_afFarPlane							10
	#define kVar_afScatterLengthMul					11
	#define kVar_avScreenSize						12
	#define kVar_avScatterLengthLimits				13
	#define kVar_avFogStartAndLength				14
	#define kVar_avFogColor							15
	#define kVar_avRayCastStart						16
	#define kVar_a_mtxBoxInvViewModelRotation		17
	#define kVar_avNegPlaneDistNeg					18
	#define kVar_avNegPlaneDistPos					19
	#define kVar_afFalloffExp						20
	#define kVar_afDepthDiffMul						21
	#define kVar_afSkipEdgeLimit					22


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

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
				auto desc = ImageDescriptor::CreateTexture2D(mvScreenSize.x, mvScreenSize.y, false, bgfx::TextureFormat::Enum::RGBA32F);
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

			m_color = 
				{ colorImage(),colorImage()};
			m_normal = 
				{ colorImage(), colorImage()};
			m_linearDepth = 
				{ colorImage(),colorImage()};
			m_specular = 
				{ colorImage(),colorImage()};
			m_outputImage = {
				colorImage(), colorImage()
			};

			m_depthStencil = { depthImage(),depthImage()};
			{
				std::array<std::shared_ptr<Image>, 5> images = {m_color[0], m_normal[0], m_linearDepth[0], m_specular[0], m_depthStencil[0]};
				std::array<std::shared_ptr<Image>, 5> reflectionImages = {m_color[1], m_normal[1], m_linearDepth[1], m_specular[1], m_depthStencil[1]};
				m_gBuffer_full = {RenderTarget(absl::MakeSpan(images)), RenderTarget(absl::MakeSpan(reflectionImages)) };
			}
			{
				std::array<std::shared_ptr<Image>, 2> images = {m_color[0], m_depthStencil[0]};
				std::array<std::shared_ptr<Image>, 2> reflectionImages = {m_color[1], m_depthStencil[1]};
				m_gBuffer_colorAndDepth = {RenderTarget(absl::MakeSpan(images)), RenderTarget(absl::MakeSpan(reflectionImages))};
			}
			{
				std::array<std::shared_ptr<Image>, 2> images = {m_outputImage[0], m_depthStencil[0] };
				std::array<std::shared_ptr<Image>, 2> reflectionImages = {m_outputImage[1], m_depthStencil[0]};
				m_output_target = {RenderTarget(absl::MakeSpan(images)), RenderTarget(absl::MakeSpan(reflectionImages))};
			}

			m_gBuffer_color = {RenderTarget(m_color[0]), RenderTarget(m_color[1])};
			m_gBuffer_depth = {RenderTarget(m_depthStencil[0]), RenderTarget(m_depthStencil[1])};
			m_gBuffer_normals = {RenderTarget(m_normal[0]), RenderTarget(m_normal[1])};
			m_gBuffer_linearDepth = {RenderTarget(m_linearDepth[0]), RenderTarget(m_linearDepth[1])};

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
									cVector3l(128, 128,1),
									cVector3l(256, 256,1),
									cVector3l(256, 256,1),
									cVector3l(512, 512,1),
									cVector3l(1024, 1024,1)
		};
		int lStartSize = 2;
		if(mShadowMapResolution == eShadowMapResolution_Medium)		lStartSize = 1;
		else if(mShadowMapResolution == eShadowMapResolution_Low)	lStartSize = 0;

		auto createShadowMap = [](const cVector3l &avSize) -> cShadowMapData {
			auto desc = ImageDescriptor::CreateTexture2D(avSize.x, avSize.y, false, bgfx::TextureFormat::D16F);
			desc.m_configuration.m_rt = RTType::RT_Write;
			auto image = std::make_shared<Image>();
			image->Initialize(desc);
			return {nullptr, nullptr, nullptr, -1,
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
			mpShadowJitterTexture = mpGraphics->CreateTexture("ShadowOffset", eTextureType_2D, eTextureUsage_Normal);
			mpGraphics->GetTextureCreator()->GenerateScatterDiskMap2D(mpShadowJitterTexture,mlShadowJitterSize,mlShadowJitterSamples, true);
		}
		else
		{
			mpShadowJitterTexture = NULL;
		}
		m_deferredFog = hpl::loadProgram("vs_deferred_fog", "fs_deferred_fog");
		m_lightBoxProgram = hpl::loadProgram("vs_light_box", "fs_light_box");
		m_pointLightProgram = hpl::loadProgram("vs_deferred_light", "fs_deferred_light_point");
		
		// uniforms
		m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
		m_u_boxInvViewModelRotation  = bgfx::createUniform("u_boxInvViewModelRotation", bgfx::UniformType::Mat4);
		m_u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);

		// samplers
		m_s_depthMap = bgfx::createUniform("s_depthMap", bgfx::UniformType::Sampler);
		m_s_deferredColorMap = bgfx::createUniform("s_deferredColorMap", bgfx::UniformType::Sampler);
		m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
		m_s_normalMap = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
		m_s_specularMap = bgfx::createUniform("s_spcularMap", bgfx::UniformType::Sampler);
		

		// ////////////////////////////////////
		// //Create Light programs
		// {
		// 	/////////////////////////////
		// 	//Misc
		// 	{
		// 		cParserVarContainer vars;

		// 		//////////////
		// 		//Light Stencil
		// 		mpLightStencilProgram = mpProgramManager->CreateProgramFromShaders("LightStencil",
		// 																			"deferred_base_vtx.glsl",
		// 																			"deferred_base_frag.glsl",
		// 																			&vars,true);

		// 		//////////////
		// 		//Light box
		// 		for(int i=0;i<2; ++i)
		// 		{
		// 			if(i==1) vars.Add("UseSSAO");
		// 			mpLightBoxProgram[i] = mpProgramManager->CreateProgramFromShaders("LightBoxNormal",
		// 																			"deferred_base_vtx.glsl",
		// 																			"deferred_light_box_frag.glsl",
		// 																			&vars,true);

		// 			vars.Clear();
		// 			if(mpLightBoxProgram[i])
		// 			{
		// 				mpLightBoxProgram[i]->GetVariableAsId("avLightColor",kVar_avLightColor);
		// 			}
		// 		}
		// 	}

		// 	/////////////////////////////
		// 	//Lights
		// 	{
		// 		cParserVarContainer defaultVars;

		// 		//Shadow variables
		// 		defaultVars.Add("ShadowJitterLookupMul",1.0f / (float)mlShadowJitterSize);
		// 		defaultVars.Add("ShadowJitterSamplesDiv2",mlShadowJitterSamples / 2);
		// 		defaultVars.Add("ShadowJitterSamples", mlShadowJitterSamples);

		// 		if(mShadowMapQuality == eShadowMapQuality_High)		defaultVars.Add("ShadowMapQuality_High");
		// 		if(mShadowMapQuality == eShadowMapQuality_Medium)	defaultVars.Add("ShadowMapQuality_Medium");
		// 		if(mShadowMapQuality == eShadowMapQuality_Low)		defaultVars.Add("ShadowMapQuality_Low");

		// 		//Vertex shader will handles deferred lights
		// 		defaultVars.Add("DeferredLight");

		// 		//Deferred renderer type
		// 		if(mGBufferType == eDeferredGBuffer_32Bit)	defaultVars.Add("Deferred_32bit","");
		// 		else										defaultVars.Add("Deferred_64bit","");

		// 		if(mlNumOfGBufferTextures == 4)				defaultVars.Add("RenderTargets_4","");
		// 		else										defaultVars.Add("RenderTargets_3","");

		// 		mpProgramManager->SetupGenerateProgramData(	eDefferredProgramMode_Lights,"Lights", "deferred_base_vtx.glsl", "deferred_light_frag.glsl",gvLightFeatureVec,
		// 											kLightFeatureNum,defaultVars);
		// 											//1, defaultVars);


		// 		mpProgramManager->AddGenerateProgramVariableId("avLightPos",	kVar_avLightPos, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("avLightColor",	kVar_avLightColor, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("afInvLightRadius",	kVar_afInvLightRadius, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("afNegFarPlane",	kVar_afNegFarPlane, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("afOneMinusCosHalfSpotFOV",	kVar_afOneMinusCosHalfSpotFOV, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("avLightForward", kVar_avLightForward, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("a_mtxSpotViewProj", kVar_a_mtxSpotViewProj, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("a_mtxInvViewRotation", kVar_a_mtxInvViewRotation, eDefferredProgramMode_Lights);
		// 		mpProgramManager->AddGenerateProgramVariableId("avShadowMapOffsetMul", kVar_avShadowMapOffsetMul, eDefferredProgramMode_Lights);
		// 	}

		// 	//////////////////////////////
		// 	// Generate some light programs
		// 	//for(i=0; i<128; ++i)
		// 	//	mpProgramManager->GenerateProgram(eDefferredProgramMode_Lights, i);
		// }

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

			// /////////////////////////////////////
			// // Textures and frame buffers

			// // Textures
			// mpSSAOTexture = CreateRenderTexture("SSAO", vSSAOSize,ePixelFormat_RGB);
			// mpSSAOBlurTexture = CreateRenderTexture("SSAOBlur", vSSAOSize,ePixelFormat_RGB);

			// // //Frame buffers

			// mpSSAOBuffer = mpGraphics->CreateFrameBuffer("SSAO");
			// mpSSAOBuffer->SetTexture2D(0,mpSSAOTexture);
			// mpSSAOBuffer->CompileAndValidate();

			// mpSSAOBlurBuffer = mpGraphics->CreateFrameBuffer("SSAOBlur");
			// mpSSAOBlurBuffer->SetTexture2D(0,mpSSAOBlurTexture);
			// mpSSAOBlurBuffer->CompileAndValidate();

			// //Scatter disk
			// mpSSAOScatterDisk = mpGraphics->CreateTexture("SSAOScatterDisk", eTextureType_2D,eTextureUsage_Normal);
			// mpGraphics->GetTextureCreator()->GenerateScatterDiskMap2D(mpSSAOScatterDisk,4, mlSSAONumOfSamples, false);


			// /////////////////////////////////////
			// // Programs
			// cParserVarContainer programVars;

			// //Program for unpacking depth to a lower resolution texture
			// if(mGBufferType == eDeferredGBuffer_32Bit)	programVars.Add("Deferred_32bit");
			// else										programVars.Add("Deferred_64bit");
			// programVars.Add("UseUv");
			// mpUnpackDepthProgram = mpGraphics->CreateGpuProgramFromShaders("UnpackDepth","deferred_base_vtx.glsl", "deferred_unpack_depth_frag.glsl",&programVars);
			// if(mpUnpackDepthProgram)
			// {
			// 	mpUnpackDepthProgram->GetVariableAsId("afNegInvFarPlane",kVar_afNegInvFarPlane);
			// }
			// programVars.Clear();

			// //SSAO Blur programs (vertical and horizontal)
			// programVars.Add("BlurHorisontal");
			// mpSSAOBlurProgram[0] = mpGraphics->CreateGpuProgramFromShaders("SSAOBlurHori","deferred_ssao_blur_vtx.glsl", "deferred_ssao_blur_frag.glsl",&programVars);
			// programVars.Clear();
			// mpSSAOBlurProgram[1] = mpGraphics->CreateGpuProgramFromShaders("SSAOBlurVert","deferred_ssao_blur_vtx.glsl", "deferred_ssao_blur_frag.glsl",&programVars);

			// for(int i=0; i<2; ++i)
			// {
			// 	if(mpSSAOBlurProgram[i])
			// 	{
			// 		mpSSAOBlurProgram[i]->GetVariableAsId("afFarPlane",kVar_afFarPlane);
			// 	}
			// }

			// //SSAO Rendering
			// programVars.Add("SampleNumDiv2", mlSSAONumOfSamples / 2);
			// mpSSAORenderProgram = mpGraphics->CreateGpuProgramFromShaders(	"SSAORender","deferred_ssao_render_vtx.glsl",
			// 																"deferred_ssao_render_frag.glsl",&programVars);
			// if(mpSSAORenderProgram)
			// {
			// 	mpSSAORenderProgram->GetVariableAsId("afFarPlane",kVar_afFarPlane);
			// 	mpSSAORenderProgram->GetVariableAsId("afScatterLengthMul", kVar_afScatterLengthMul);
			// 	mpSSAORenderProgram->GetVariableAsId("avScatterLengthLimits", kVar_avScatterLengthLimits);
			// 	mpSSAORenderProgram->GetVariableAsId("avScreenSize", kVar_avScreenSize);
			// 	mpSSAORenderProgram->GetVariableAsId("afDepthDiffMul", kVar_afDepthDiffMul);
			// 	mpSSAORenderProgram->GetVariableAsId("afSkipEdgeLimit", kVar_afSkipEdgeLimit);
			// }
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
			// /////////////////////////////////////
			// // Textures and frame buffers

			// // Textures
			// mpEdgeSmooth_LinearDepthTexture = CreateRenderTexture("EdgeSmoothLinearDepth", mvScreenSize,ePixelFormat_RGB16);
			// mpEdgeSmooth_TempAccum = mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA,0)->GetColorBuffer(0)->ToTexture();

			// //Frame buffers
			// mpEdgeSmooth_LinearDepthBuffer = mpGraphics->CreateFrameBuffer("EdgeSmoothLinearDepth");
			// mpEdgeSmooth_LinearDepthBuffer->SetTexture2D(0,mpEdgeSmooth_LinearDepthTexture);
			// mpEdgeSmooth_LinearDepthBuffer->CompileAndValidate();

			// /////////////////////////////////////
			// // Programs

			// cParserVarContainer programVars;

			// //Program for unpacking depth
			// if(mGBufferType == eDeferredGBuffer_32Bit)	programVars.Add("Deferred_32bit");
			// else										programVars.Add("Deferred_64bit");
			// programVars.Add("UseUv");
			// mpEdgeSmooth_UnpackDepthProgram = mpGraphics->CreateGpuProgramFromShaders("EdgeSmoothUnpackDepth","deferred_base_vtx.glsl", "deferred_unpack_depth_frag.glsl",&programVars);
			// if(mpEdgeSmooth_UnpackDepthProgram)
			// {
			// 	mpEdgeSmooth_UnpackDepthProgram->GetVariableAsId("afNegInvFarPlane",kVar_afNegInvFarPlane);
			// }
			// programVars.Clear();

			// //Program for edge smoothing
			// programVars.Add("UseUv");
			// mpEdgeSmooth_RenderProgram =  mpGraphics->CreateGpuProgramFromShaders("EdgeSmoothRender","deferred_base_vtx.glsl", "deferred_edge_smooth_frag.glsl",&programVars);
			// if(mpEdgeSmooth_RenderProgram)
			// {
			// 	mpEdgeSmooth_RenderProgram->GetVariableAsId("afFarPlane",kVar_afFarPlane);
			// }
		}

		////////////////////////////////////
		//Create light shapes
		tFlag lVtxFlag = eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0;
		mpShapeSphere[eDeferredShapeQuality_High] = LoadVertexBufferFromMesh("core_12_12_sphere.dae",lVtxFlag);
		mpShapeSphere[eDeferredShapeQuality_Medium] = LoadVertexBufferFromMesh("core_7_7_sphere.dae",lVtxFlag);
		mpShapeSphere[eDeferredShapeQuality_Low] = LoadVertexBufferFromMesh("core_5_5_sphere.dae",lVtxFlag);

		mpShapePyramid = LoadVertexBufferFromMesh("core_pyramid.dae",lVtxFlag);

		////////////////////////////////////
		//Quad used when rendering light.
		mpFullscreenLightQuad = CreateQuadVertexBuffer(eVertexBufferType_Software,0,1,0,mvScreenSizeFloat, true);

		////////////////////////////////////
		//Batch vertex buffer
		mlMaxBatchVertices = mpShapeSphere[eDeferredShapeQuality_Low]->GetVertexNum() * mlMaxBatchLights;
		mlMaxBatchIndices = mpShapeSphere[eDeferredShapeQuality_Low]->GetIndexNum() * mlMaxBatchLights;

		return true;
	}

	//-----------------------------------------------------------------------


	void cRendererDeferred::DestroyData()
	{
		/////////////////////////
		//Vertex buffers

		if(mpFullscreenLightQuad) hplDelete(mpFullscreenLightQuad);

		for(int i=0;i< eDeferredShapeQuality_LastEnum; ++i)
		{
			if(mpShapeSphere[i]) hplDelete(mpShapeSphere[i]);
		}
		if(mpShapePyramid) hplDelete(mpShapePyramid);


		/////////////////////////
		//G-Buffer and Accum buffer
		for(int type=0; type<2; ++type)
		{

			mpGraphics->DestoroyDepthStencilBuffer(mpDepthStencil[type]);
		}

		mpGraphics->DestroyFrameBuffer(mpAccumBuffer);
		mpGraphics->DestroyFrameBuffer(mpReflectionBuffer);

		mpGraphics->DestroyTexture(mpAccumBufferTexture);
		mpGraphics->DestroyTexture(mpReflectionTexture);


		/////////////////////////
		//Shadow textures
		DestroyShadowMaps();

		if(mpShadowJitterTexture) mpGraphics->DestroyTexture(mpShadowJitterTexture);

		/////////////////////////
		//Fog stuff
		// hplDelete(mpFogProgramManager);

		/////////////////////////
		//SSAO textures and programs
		if(mbSSAOLoaded)
		{
			mpGraphics->DestroyTexture(mpSSAOTexture);
			mpGraphics->DestroyTexture(mpSSAOBlurTexture);
			mpGraphics->DestroyTexture(mpSSAOScatterDisk);

			mpGraphics->DestroyFrameBuffer(mpSSAOBuffer);
			mpGraphics->DestroyFrameBuffer(mpSSAOBlurBuffer);

			mpGraphics->DestroyGpuProgram(mpUnpackDepthProgram);
			for(int i=0;i<2; ++i)
				mpGraphics->DestroyGpuProgram(mpSSAOBlurProgram[i]);
			mpGraphics->DestroyGpuProgram(mpSSAORenderProgram);
		}

		/////////////////////////////
		// Edge smooth
		if(mbEdgeSmoothLoaded)
		{
			mpGraphics->DestroyTexture(mpEdgeSmooth_LinearDepthTexture);
			mpGraphics->DestroyFrameBuffer(mpEdgeSmooth_LinearDepthBuffer);

			mpGraphics->DestroyGpuProgram(mpEdgeSmooth_UnpackDepthProgram);
			mpGraphics->DestroyGpuProgram(mpEdgeSmooth_RenderProgram);
		}

		// mpProgramManager->DestroyShadersAndPrograms();
	}

	//-----------------------------------------------------------------------

	Image& cRendererDeferred::FetchOutputFromRenderer() {
		// not going to bother with RenderViewport for the moment ...
		// if(m_currentRenderTarget.GetRenderTarget()) {
		// 	return *m_currentRenderTarget.GetRenderTarget()->GetImage();
		// }
		return *m_output_target[0].GetImage();
	}
		
	iTexture* cRendererDeferred::GetPostEffectTexture()
	{
		return mpAccumBufferTexture;
	}

	// NOTE: this logic is incomplete
	void cRendererDeferred::RenderFogPass(GraphicsContext& context, RenderTarget& rt) {
		if(mpCurrentRenderList->GetFogAreaNum() == 0)
		{
			mpCurrentSettings->mvFogRenderData.resize(0); //Make sure render data array is empty!
			return;
		}

		for(auto& fogData: mpCurrentSettings->mvFogRenderData)
		{
			cFogArea* pFogArea = fogData.mpFogArea;
			DeferredFogUniforms uniforms = {0};

			//Outside of box setup
			cMatrixf rotationMatrix = cMatrixf::Identity;
			if(fogData.mbInsideNearFrustum==false)
			{
				cMatrixf mtxInvModelView = cMath::MatrixInverse( cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), *pFogArea->GetModelMatrixPtr()) );
				cVector3f vRayCastStart = cMath::MatrixMul(mtxInvModelView, cVector3f(0));

				uniforms.u_fogRayCastStart[0] = vRayCastStart.x;
				uniforms.u_fogRayCastStart[1] = vRayCastStart.y;
				uniforms.u_fogRayCastStart[2] = vRayCastStart.z;

				rotationMatrix = mtxInvModelView.GetRotation();

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
				uniforms.u_useBackside = pFogArea->GetShowBacksideWhenInside()? 1.0 : 0.0;
			} 
			else 
			{
				uniforms.u_useBackside = pFogArea->GetShowBacksideWhenOutside() ? 1.0 : 0.0;
				uniforms.u_useOutsideBox = 1.0;
			}
			uniforms.u_negFarPlane = mpCurrentFrustum->GetFarPlane() * -1.0f;

			GraphicsContext::LayoutStream layoutStream;
			GraphicsContext::ShaderProgram shaderProgram;
			shaderProgram.m_handle = m_deferredFog;
			shaderProgram.m_uniforms.push_back(
				{m_u_param, &uniforms, 3});
			shaderProgram.m_uniforms.push_back(
				{m_u_boxInvViewModelRotation, rotationMatrix.v, 1});
			
			shaderProgram.m_projection = *mpCurrentProjectionMatrix;
			shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix();
			shaderProgram.m_modelTransform = *pFogArea->GetModelMatrixPtr();

			mpShapeBox->GetLayoutStream(layoutStream);
		}
	}


	void cRendererDeferred::RenderFullScreenFogPass(GraphicsContext& context, RenderTarget& rt) {
		const auto& view = context.StartPass("FullScreenFog");
		auto& image = resolveRenderImage(m_linearDepth);
		
		DeferredFogUniforms uniforms = {0};
		
		uniforms.u_fogStart = mpCurrentWorld->GetFogStart();
		uniforms.u_fogLength = mpCurrentWorld->GetFogEnd() - mpCurrentWorld->GetFogStart();
		uniforms.u_fogFalloffExp = mpCurrentWorld->GetFogFalloffExp();
		
		auto fogColor = mpCurrentWorld->GetFogColor();
		float u_color[4] = {fogColor.r, fogColor.g, fogColor.b, fogColor.a};

		GraphicsContext::LayoutStream layout;
		cMatrixf projMtx;
		context.ScreenSpaceQuad(layout, projMtx, mvScreenSize.x, mvScreenSize.y);
		
		GraphicsContext::ShaderProgram shaderProgram;
		shaderProgram.m_configuration.m_write = Write::RGBA;
		shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
		shaderProgram.m_projection = projMtx;
		
		shaderProgram.m_handle = m_deferredFog;
		shaderProgram.m_textures.push_back(
			{m_s_depthMap, image->GetHandle(), 0});
		shaderProgram.m_uniforms.push_back(
			{m_u_param, &uniforms, 3});
		// shaderProgram.m_uniforms.push_back(
		// 		{m_u_color, u_color, 1});

		GraphicsContext::DrawRequest drawRequest {m_edgeSmooth_LinearDepth, layout, shaderProgram};
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

			shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderInput.m_modelTransform = obj->GetModelMatrixPtr()->GetTranspose();

			GraphicsContext::DrawRequest drawRequest {rt, layoutInput, shaderInput};
			drawRequest.m_width = mvScreenSize.x;
			drawRequest.m_height = mvScreenSize.y;
			context.Submit(view, drawRequest);
		});
	}

	void cRendererDeferred::CopyToFrameBuffer()
	{
		BX_ASSERT(false, "Noop");
	}

	void cRendererDeferred::RenderEdgeSmoothPass(GraphicsContext& context, RenderTarget& rt) {

		auto edgeSmoothView = context.StartPass("EdgeSmooth");
		cVector3f vQuadPos = cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane);
		cVector2f vQuadSize = cVector2f(mfFarRight*2,mfFarTop*2);

		GraphicsContext::ShaderProgram shaderProgram;
		shaderProgram.m_handle = m_edgeSmooth_UnpackDepthProgram;
		shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_linearDepth)->GetHandle(), 0});
		shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_normal)->GetHandle(), 0});

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
			shaderInput.m_modelTransform = *obj->GetModelMatrixPtr();
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			shaderInput.m_configuration.m_write = Write::RGBA;
			shaderInput.m_configuration.m_cull = Cull::CounterClockwise;

			shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderInput.m_modelTransform = obj->GetModelMatrixPtr()->GetTranspose();
			
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
			shaderInput.m_modelTransform = *obj->GetModelMatrixPtr();
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			shaderInput.m_configuration.m_write = Write::RGBA;
			shaderInput.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);

			shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			shaderInput.m_modelTransform = obj->GetModelMatrixPtr()->GetTranspose();

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
		iGpuProgram* program = pMaterial->GetProgram(0, renderMode);
		iVertexBuffer* vertexBuffer = object->GetVertexBuffer();
		if(vertexBuffer == nullptr || program == nullptr || materialType == nullptr) {
			return;
		}

		GraphicsContext::LayoutStream layoutInput;
		GraphicsContext::ShaderProgram shaderInput;
		vertexBuffer->GetLayoutStream(layoutInput);
		materialType->GetShaderData(shaderInput, renderMode, program, pMaterial, object, renderer);
		shaderInput.m_configuration.m_write = Write::Depth;
		shaderInput.m_configuration.m_cull = Cull::CounterClockwise;
		shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
		
		shaderInput.m_modelTransform = object->GetModelMatrixPtr()->GetTranspose();
		shaderInput.m_view = input.m_view;
		shaderInput.m_projection = input.m_projection;

		GraphicsContext::DrawRequest drawRequest {rt, layoutInput, shaderInput};
		drawRequest.m_width = input.m_width;
		drawRequest.m_height = input.m_height;
		context.Submit(view, drawRequest);
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


	void cRendererDeferred::RenderTranslucentPass(GraphicsContext& context, RenderTarget& rt) {
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Translucent)) {
			return;
		}

		auto& target = resolveRenderTarget(m_gBuffer_color);
		auto view = context.StartPass("RenderTranslucent");
		for(auto& obj: mpCurrentRenderList->GetRenderableItems(eRenderListType_Translucent))
		{
			cMaterial *pMaterial = obj->GetMaterial();

			eMaterialRenderMode renderMode = mpCurrentWorld->GetFogActive() ? eMaterialRenderMode_DiffuseFog : eMaterialRenderMode_Diffuse;
			if(!pMaterial->GetAffectedByFog()) {
				renderMode = eMaterialRenderMode_Diffuse;
			}
			if((mpCurrentSettings->mbIsReflection && pMaterial->HasWorldReflection()) ||
				!obj->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime)) {
				continue;
			}

			if(pMaterial->HasWorldReflection() && obj->GetRenderType() == eRenderableType_SubMesh)
			{
				if(!CheckRenderablePlaneIsVisible(obj, mpCurrentFrustum)) {
					continue;
				}

				cSubMeshEntity *subMeshEntity = static_cast<cSubMeshEntity*>(obj);
				cMaterial *pRelfMaterial = subMeshEntity->GetMaterial();
				bool bReflectionIsInRange=true;
				if(pRelfMaterial->GetMaxReflectionDistance() > 0)
				{
					cVector3f vPoint = mpCurrentFrustum->GetOrigin() + mpCurrentFrustum->GetForward()*-1*pRelfMaterial->GetMaxReflectionDistance();
					cVector3f vNormal = mpCurrentFrustum->GetForward();
					cPlanef maxRelfctionDistPlane;
					maxRelfctionDistPlane.FromNormalPoint(vNormal, vPoint);

					if(cMath::CheckPlaneBVCollision(maxRelfctionDistPlane, *obj->GetBoundingVolume())==eCollision_Outside)
					{
						bReflectionIsInRange = false;
					}
				}

				if(mpCurrentSettings->mbRenderWorldReflection && bReflectionIsInRange && obj->GetIsOneSided())
				{
				}
				else
				{
					if(mbReflectionTextureCleared == false)
					{
						cRenderTarget renderTarget;
						renderTarget.mpFrameBuffer = mpReflectionBuffer;
						SetFrameBuffer(mpReflectionBuffer, false, false);
						ClearFrameBuffer(eClearFrameBufferFlag_Color, false);
						SetAccumulationBuffer();
						mbReflectionTextureCleared = true;
					}
				}
			}
			
			//TODO: need to implement occlusion query for translucent objects
			// if(obj->RetrieveOcculsionQuery(this)==false)
			// {
			// 	continue;
			// }
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

		RunCallback(eRendererMessage_PostGBuffer);

		RenderLightPass(context, resolveRenderTarget(m_output_target));

		// render illumination into gbuffer color RenderIllumination
		RenderIlluminationPass(context, resolveRenderTarget(m_output_target));
		RenderFogPass(context, resolveRenderTarget(m_gBuffer_color));
		if(mpCurrentWorld->GetFogActive()) {
			RenderFullScreenFogPass(context, resolveRenderTarget(m_gBuffer_color));
		}

		//  RenderEdgeSmooth();
		// if(mbEdgeSmoothLoaded && mpCurrentSettings->mbUseEdgeSmooth) {
		// 	RenderEdgeSmoothPass(context, m_edgeSmooth_LinearDepth);
		// }
		RunCallback(eRendererMessage_PostSolid);

		// not going to even try.
		// calls back through RendererDeffered need to untangle all the complicated state ...
		// RenderTranslucent()
		// RenderTranslucentPass(context, RenderTarget &rt)

		RunCallback(eRendererMessage_PostTranslucent);

		if(mbOcclusionTestLargeLights) {
			RetrieveAllLightOcclusionPair(false); //false = we do not stop and wait.
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

		auto clearStencilBuffer = [&](bgfx::ViewId view) {
			GraphicsContext::DrawClear clear {
				rt,
				{0, 1, 0, ClearOp::Stencil},
				0,
				0,
				static_cast<uint16_t>(mvScreenSize.x),
				static_cast<uint16_t>(mvScreenSize.y)
			};
			context.ClearTarget(view, clear);
		};

		// Drawing Box Lights
		{
			const auto pass = context.StartPass("Render Box Lights");
			auto drawBoxLight = [&](GraphicsContext::ShaderProgram& shaderProgram, cDeferredLight* light) {
				GraphicsContext::LayoutStream layoutStream;
				mpShapeBox->GetLayoutStream(layoutStream);
				cLightBox *pLightBox = static_cast<cLightBox*>(light->mpLight);

				const auto& color = light->mpLight->GetDiffuseColor();
				float lightColor[4] = {color.r, color.g, color.b, color.a};
				shaderProgram.m_handle = m_lightBoxProgram;
				shaderProgram.m_textures.push_back({m_s_deferredColorMap, resolveRenderImage(m_color)->GetHandle(), 0});
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
				context.Submit(pass, drawRequest);
			};
			auto& renderStencilFront = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack];
			auto lightIt = renderStencilFront.begin();
			while(lightIt != renderStencilFront.end()) {
				absl::InlinedVector<cDeferredLight*, 8> lights;
				for(uint8_t bit = 0; bit < 8 && lightIt != renderStencilFront.end(); ++bit, ++lightIt) {
					lights.push_back(*lightIt);
				}
				// stencil buffer is 8 bits wide, so we can only render 8 lights at a time
				// on the first pass we need to move the iterator
				for(size_t i = 0; i < lights.size(); ++i) {
					GraphicsContext::ShaderProgram shaderProgram;
					GraphicsContext::LayoutStream layoutStream;
					mpShapeBox->GetLayoutStream(layoutStream);

					shaderProgram.m_handle = m_nullShader;
					shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					shaderProgram.m_configuration.m_stencilTest = CreateStencilTest(
						StencilFunction::Always,
						StencilFail::Keep, 
						StencilDepthFail::Replace, 
						StencilDepthPass::Keep,
						0xff, 1 << i);
				
				
					shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(lights[i]->mpLight->GetWorldMatrix(), lights[i]->GetLightMtx()).GetTranspose();


					GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
					drawRequest.m_width = mvScreenSize.x;
					drawRequest.m_height = mvScreenSize.y;
					context.Submit(pass, drawRequest);
				}
				for(size_t i = 0; i < lights.size(); ++i) {
					GraphicsContext::ShaderProgram shaderProgram;
					shaderProgram.m_configuration.m_cull = Cull::Clockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					shaderProgram.m_configuration.m_write = Write::RGBA;
					shaderProgram.m_configuration.m_stencilTest = CreateStencilTest(
						StencilFunction::Equal,
						StencilFail::Keep, 
						StencilDepthFail::Keep, 
						StencilDepthPass::Keep,
						0xff, 1 << i);
					drawBoxLight(shaderProgram, lights[i]);
				}
				clearStencilBuffer(pass);
			}
		
			for(auto& light: mvSortedLights[eDeferredLightList_Box_RenderBack])
			{
				GraphicsContext::ShaderProgram shaderProgram;
				shaderProgram.m_configuration.m_cull = Cull::Clockwise;
				shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
				shaderProgram.m_configuration.m_write = Write::RGBA;
				drawBoxLight(shaderProgram, light);
			}
		
		}

		// note: skipping implementing RenderLights_StencilBack_ScreenQuad

		// render light
		{
			const auto pass = context.StartPass("Render Lights");
			auto& renderStencilFrontRenderBack = mvSortedLights[eDeferredLightList_StencilFront_RenderBack];
			auto lightIt = renderStencilFrontRenderBack.begin();
			while(lightIt != renderStencilFrontRenderBack.end()) {
				absl::InlinedVector<cDeferredLight*, 8> lights;
				for(uint8_t bit = 0; bit < 8 && lightIt != renderStencilFrontRenderBack.end(); ++bit, ++lightIt) {
					lights.push_back(*lightIt);
				}

				for(size_t i = 0; i < lights.size(); ++i) {
					GraphicsContext::ShaderProgram shaderProgram;
					GraphicsContext::LayoutStream layoutStream;
					mpShapeBox->GetLayoutStream(layoutStream);

					GetLightShape(lights[i]->mpLight, eDeferredShapeQuality_Medium)->GetLayoutStream(layoutStream);

					shaderProgram.m_handle = m_nullShader;
					shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					
					shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(lights[i]->mpLight->GetWorldMatrix(), lights[i]->GetLightMtx()).GetTranspose();
					
					
					shaderProgram.m_configuration.m_stencilTest = CreateStencilTest(
						StencilFunction::Always,
						StencilFail::Keep, 
						StencilDepthFail::Replace, 
						StencilDepthPass::Keep,
						0xff, 1 << i);
				

					GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
					drawRequest.m_width = mvScreenSize.x;
					drawRequest.m_height = mvScreenSize.y;
					context.Submit(pass, drawRequest);
				}

				for(size_t i = 0; i < lights.size(); ++i) {
					GraphicsContext::ShaderProgram shaderProgram;
					GraphicsContext::LayoutStream layoutStream;
					GetLightShape(lights[i]->mpLight, eDeferredShapeQuality_High)->GetLayoutStream(layoutStream);
					SetupLightProgram(shaderProgram, lights[i]);
					shaderProgram.m_configuration.m_cull = Cull::Clockwise;
					shaderProgram.m_configuration.m_write = Write::RGBA;
					shaderProgram.m_configuration.m_stencilTest = CreateStencilTest(
						StencilFunction::Equal,
						StencilFail::Keep, 
						StencilDepthFail::Keep, 
						StencilDepthPass::Keep,
						0xff, 1 << i);
					shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
					shaderProgram.m_configuration.m_rgbBlendFunc = 
						CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					shaderProgram.m_configuration.m_alphaBlendFunc = 
						CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					
					shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(lights[i]->mpLight->GetWorldMatrix(), lights[i]->GetLightMtx()).GetTranspose();

					GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
					drawRequest.m_width = mvScreenSize.x;
					drawRequest.m_height = mvScreenSize.y;
					context.Submit(pass, drawRequest);
				}
				clearStencilBuffer(pass);
			}

			for(auto& light: mvSortedLights[eDeferredLightList_RenderBack])
			{
				GraphicsContext::ShaderProgram shaderProgram;
				GraphicsContext::LayoutStream layoutStream;
				GetLightShape(light->mpLight, eDeferredShapeQuality_Medium)->GetLayoutStream(layoutStream);
				SetupLightProgram(shaderProgram, light);
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

				GraphicsContext::DrawRequest drawRequest {rt, layoutStream, shaderProgram};
				drawRequest.m_width = mvScreenSize.x;
				drawRequest.m_height = mvScreenSize.y;
				context.Submit(pass, drawRequest);
			}
		}
	}

	void cRendererDeferred::RenderShadowLight(GraphicsContext& context, GraphicsContext::ShaderProgram& shaderProgram, RenderTarget& rt) {

	}
	
	void cRendererDeferred::SetupLightProgram(GraphicsContext::ShaderProgram& shaderProgram, cDeferredLight* apLightData) {
		switch(apLightData->mpLight->GetLightType()) {
			case eLightType_Point: {
				struct {
					float hasGobo;
					float hasSpecular;
					float farPlaneDepth;
					float lightRadius;
				} param = {
					0.0,
					0.0,
					-mpCurrentFrustum->GetFarPlane(),
					apLightData->mpLight->GetRadius()

				};
				const auto modelViewMtx = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), apLightData->mpLight->GetWorldMatrix());
				const auto color = apLightData->mpLight->GetDiffuseColor();
				cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, apLightData->GetLightMtx()).GetTranslation();
				float lightPosition[4] = {lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f};
				float lightColor[4] = {color.r, color.g, color.b, color.a};
				shaderProgram.m_handle = m_pointLightProgram;
				shaderProgram.m_uniforms.push_back({m_u_lightPos, lightPosition});
				shaderProgram.m_uniforms.push_back({m_u_lightColor, lightColor});
				shaderProgram.m_uniforms.push_back({m_u_param, &param});

				shaderProgram.m_textures.push_back({m_s_normalMap, resolveRenderImage(m_normal)->GetHandle(), 1});
				shaderProgram.m_textures.push_back({m_s_diffuseMap, resolveRenderImage(m_color)->GetHandle(), 0});
				shaderProgram.m_textures.push_back({m_s_normalMap, resolveRenderImage(m_normal)->GetHandle(), 1});
				shaderProgram.m_textures.push_back({m_s_depthMap, resolveRenderImage(m_depthStencil)->GetHandle(), 2});
				break;
			}
		}
		
	} 
		

	void cRendererDeferred::SetupRenderList()
	{
		mpCurrentRenderList->Setup(mfCurrentFrameTime,mpCurrentFrustum);
	}

	void cRendererDeferred::RenderObjects()
	{
		
	}

	void cRendererDeferred::RenderZ(GraphicsContext& context)
	{
		START_RENDER_PASS(EarlyZ);

		SetDepthTest(true);
		SetDepthWrite(true);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_None);

		SetTextureRange(NULL,0);

		cRenderableVecIterator zIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Z);
		while(zIt.HasNext())
		{
			iRenderable *pObject = zIt.Next();
			RenderZObject(context, pObject, NULL);
		}

		END_RENDER_PASS();
	}

	void cRendererDeferred::RenderEdgeSmooth()
	{
		if(mbEdgeSmoothLoaded==false || mpCurrentSettings->mbUseEdgeSmooth==false) return;
		//if(mbEdgeSmoothLoaded==false) return;

		START_RENDER_PASS(EdgeSmooth);

		//////////////////////////////
		// Set up variables
		cVector3f vQuadPos = cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane);
		cVector2f vQuadSize = cVector2f(mfFarRight*2,mfFarTop*2);

		iTexture *pGBufferDepthTexture = GetBufferTexture(2);
		iTexture *pGBufferNormalTexture = GetBufferTexture(1);

		//////////////////////////////
		// Set up render states
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetTextureRange(NULL, 1);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetBlendMode(eMaterialBlendMode_None);
		SetDepthWrite(false);
		SetDepthTest(false);

		SetFlatProjectionMinMax(cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane*1.5f),cVector3f(mfFarRight,mfFarTop,mfFarPlane*1.5f));

		////////////////////////////////////////////
		// Render linear depth to texture
		SetFrameBuffer(mpEdgeSmooth_LinearDepthBuffer);


		if(mGBufferType == eDeferredGBuffer_64Bit)
		{
			mpEdgeSmooth_UnpackDepthProgram->SetFloat(kVar_afNegInvFarPlane, -1.0f / mfFarPlane);
		}
		SetProgram(mpEdgeSmooth_UnpackDepthProgram);
		SetTexture(0, pGBufferDepthTexture);	//Set G-buffer depth texture

		DrawQuad(vQuadPos, vQuadSize,0, mvScreenSizeFloat,true);

		////////////////////////////////////////////
		// Copy the screen to temp texture
		SetAccumulationBuffer();
		CopyFrameBufferToTexure(mpEdgeSmooth_TempAccum,0,mvScreenSize,0, true);

		//////////////////////////////
		// Render Edge smoothing
		SetTexture(0,mpEdgeSmooth_TempAccum);
		SetTexture(1,mpEdgeSmooth_LinearDepthTexture);
		SetTexture(2,pGBufferNormalTexture);

		SetProgram(mpEdgeSmooth_RenderProgram);
		if(mpEdgeSmooth_RenderProgram)
		{
			mpEdgeSmooth_RenderProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		}

		DrawQuad(vQuadPos, vQuadSize,0, mvScreenSizeFloat,true);

		/////////////////////////////
		// Set render states back to normal
		SetNormalFrustumProjection();

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	//Definitions used when rendering lights
	#define kLightRadiusMul_High		1.08f
	#define kLightRadiusMul_Medium		1.12f
	#define kLightRadiusMul_Low			1.2f
	#define kMaxStencilBitsUsed			8
	#define kStartStencilBit			0

	//-----------------------------------------------------------------------

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


	//-----------------------------------------------------------------------

	void cRendererDeferred::SetupLightProgramVariables(	iGpuProgram *apProgram,cDeferredLight* apLightData)
	{
		iLight *pLight = apLightData->mpLight;

		if(apProgram==NULL) return;

		///////////////////////
		// General variables
		apProgram->SetVec3f(kVar_avLightPos, apLightData->m_mtxViewSpaceRender.GetTranslation());
		apProgram->SetColor4f(kVar_avLightColor, pLight->GetDiffuseColor());
		apProgram->SetFloat(kVar_afInvLightRadius, 1.0f / pLight->GetRadius());

		////////////////////////
		// Point light specific
		if(pLight->GetLightType() == eLightType_Point)
		{
			if(pLight->GetGoboTexture())
			{
				cMatrixf mtxFinal = cMath::MatrixMul(pLight->GetWorldMatrix(),m_mtxInvView);
				apProgram->SetMatrixf(kVar_a_mtxInvViewRotation, mtxFinal.GetRotation());
			}
		}
		////////////////////////
		// Spot light specific
		else if(pLight->GetLightType() == eLightType_Spot)
		{
			cLightSpot *pLightSpot = static_cast<cLightSpot*>(pLight);

			//Calculate and set the forward vector
			cVector3f vForward = cVector3f(0,0,1);
			vForward = cMath::MatrixMul3x3(apLightData->m_mtxViewSpaceTransform, vForward);
			apProgram->SetVec3f(kVar_avLightForward, vForward);

			//Get the already calculated cosine of fov
			apProgram->SetFloat(kVar_afOneMinusCosHalfSpotFOV,  1 - pLightSpot->GetCosHalfFOV());

			//If a shadow, set up shadow variables and then clear shadow map
            if(apLightData->mpShadowTexture)
			{
				cVector2f vInvShadowMapSize(1.0f / (float)apLightData->mpShadowTexture->GetWidth(),
											1.0f / (float)apLightData->mpShadowTexture->GetHeight());

				if(mpShadowJitterTexture)
					apProgram->SetVec2f(kVar_avShadowMapOffsetMul, vInvShadowMapSize * pLight->GetShadowMapBlurAmount());

				apLightData->mpShadowTexture = NULL;
			}

			if(pLight->GetGoboTexture() || apLightData->mbCastShadows)
			{
				cMatrixf mtxFinal = cMath::MatrixMul(pLightSpot->GetViewProjMatrix(), m_mtxInvView);
				apProgram->SetMatrixf(kVar_a_mtxSpotViewProj, mtxFinal);
			}
		}
	}

	//-----------------------------------------------------------------------

	#ifdef kDebug_RenderLightData

		#define RenderSingleLightDebug() \
			SetStencilActive(false); SetProgram(NULL);SetTextureRange(NULL,0); mpLowLevelGraphics->SetColor(cColor(0.25,0,0,0));\
			DrawCurrent();\
			mpLowLevelGraphics->SetColor(cColor(1,1)); for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, mpGBufferTexture[i]); SetStencilActive(true);
	#else
		#define RenderSingleLightDebug()
	#endif

	//-----------------------------------------------------------------------

	// TODO: need to work out logic for setting up light
	iGpuProgram* cRendererDeferred::SetupProgramAndTextures(cDeferredLight* apLightData, tFlag alExtraFlags)
	{
		iLight *pLight = apLightData->mpLight;
		eLightType lightType = pLight->GetLightType();

		/////////////////////////
		//Flag setup
		tFlag lFlags = alExtraFlags;
		if(pLight->GetDiffuseColor().a > 0)	lFlags |= eFeature_Light_Specular;
		if(pLight->GetGoboTexture())		lFlags |= eFeature_Light_Gobo;

		//Spotlight specifics
		if(lightType == eLightType_Spot)
		{
			lFlags |= eFeature_Light_SpotLight;
			if(lFlags & eFeature_Light_LightShapes) lFlags |= eFeature_Light_DivideInFrag;

			//Only spot lights can cast shadows (for now)
			if(apLightData->mbCastShadows && apLightData->mpShadowTexture)
			{
				lFlags |= eFeature_Light_ShadowMap;
			}
		}

		/////////////////////////
		//Program
		// iGpuProgram *pProgram = mpProgramManager->GenerateProgram(eDefferredProgramMode_Lights, lFlags);
		// SetProgram(pProgram);
		// if(pProgram)
		// {
		// 	pProgram->SetFloat(kVar_afNegFarPlane, -mpCurrentFrustum->GetFarPlane());
		// }

		/////////////////////////
		//Textures
		// SetTexture(4,pLight->GetFalloffMap());

		// if(pLight->GetGoboTexture())		SetTexture(5, pLight->GetGoboTexture());

		////////////////////////
		// Point light specific
		if(lightType == eLightType_Point)
		{

		}
		////////////////////////
		// Spot light specific
		else if(lightType == eLightType_Spot)
		{
			cLightSpot *pLightSpot = static_cast<cLightSpot*>(pLight);

			//Shadow stuff
			if(apLightData->mpShadowTexture)
			{
				SetTexture(6, apLightData->mpShadowTexture);

				if(mpShadowJitterTexture) SetTexture(7, mpShadowJitterTexture);
			}

			//Spot fall off
			// if(pLight->GetGoboTexture()==NULL)
			// 	SetTexture(5,pLightSpot->GetSpotFalloffMap());

		}

		return nullptr;
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLightShadowMap(cDeferredLight* apLightData)
	{
		//Setup render states
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);

		//Render shadow map and set used shadow texture to data.
		iLight *pLight = apLightData->mpLight;
		eShadowMapResolution shadowMapRes = apLightData->mShadowResolution;

		cShadowMapData *pShadowData = GetShadowMapData(shadowMapRes, pLight);
		apLightData->mpShadowTexture = pShadowData->mpTexture;

        if(ShadowMapNeedsUpdate(pLight, pShadowData))
		{
			RenderShadowMap(pLight, pShadowData->mpBuffer);

			//Reset to previous frame buffer
			SetAccumulationBuffer();
		}

		//Set back G-buffer textures
		for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, GetBufferTexture(i));

		//Reset render states
		SetDepthTestFunc(eDepthTestFunc_GreaterOrEqual);

		SetDepthWrite(false);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetBlendMode(eMaterialBlendMode_Add);
		SetAlphaMode(eMaterialAlphaMode_Solid);

	}

	//-----------------------------------------------------------------------


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

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetupLightsAndRenderQueries(GraphicsContext& context, RenderTarget& rt)
	{
		//////////////////////////
		// Check query results from last frame and clear list.
		tLightSet setPrevVisibleLights;
		if(mbOcclusionTestLargeLights)
		{
			for(auto& occlusionPair: mpCurrentSettings->m_lightOcclusionPairs) {
				if(occlusionPair.mlSampleResults > mpCurrentSettings->mlSampleVisiblilityLimit) {
					setPrevVisibleLights.insert(occlusionPair.mpLight);
				}
				if(bgfx::isValid(occlusionPair.m_occlusionQuery)) {
					bgfx::destroy(occlusionPair.m_occlusionQuery);
				}
			}

			mpCurrentSettings->m_lightOcclusionPairs.resize(0);
		}


		//////////////////////////
		// Clear light list
		STLDeleteAll_NoClear(mvTempDeferredLights);
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
					pLightData->mShadowResolution = GetShadowMapResolution(pLight->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);
				}
				////////////////////////
				//Outside near plane, calculate distance and chose resolution
				else
				{
					cVector3f vIntersection = pLightSpot->GetFrustum()->GetOrigin();
					pLightSpot->GetFrustum()->CheckLineIntersection(mpCurrentFrustum->GetOrigin(), pLight->GetBoundingVolume()->GetWorldCenter(),vIntersection);

					float fDistToLight = cMath::Vector3Dist(mpCurrentFrustum->GetOrigin(), vIntersection);

					pLightData->mbCastShadows = true;
					pLightData->mShadowResolution = GetShadowMapResolution(pLight->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);

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


			///////////////////////////
			// Render Query

			//If not doing occlusion testing on large light, might as well just skip here
			if(mbOcclusionTestLargeLights==false || mpCurrentSettings->mbUseOcclusionCulling==false) {
				continue;
			}

			// If inside near plane or too small on screen skip queries
			if(pLightData->mbInsideNearPlane || pLightData->mlArea < mlMinLargeLightArea) {
				continue;
			}

			///////////////////////////
			// Only check if light was invisible last frame
			
			if(std::find(setPrevVisibleLights.begin(), setPrevVisibleLights.end(), pLight) != setPrevVisibleLights.end()) {
				continue;
			}

			auto lightVertex = GetLightShape(pLight, eDeferredShapeQuality_Medium);
			if(!lightVertex) {
				continue;
			}

			GraphicsContext::ShaderProgram shaderProgram;
			GraphicsContext::LayoutStream layoutStream;
		
			////////////////////////////////
			//Render light shape and make a query
			BX_ASSERT(!bgfx::isValid(pLightData->m_occlusionQuery), "Occlusion query already exists!");
			pLightData->m_occlusionQuery = bgfx::createOcclusionQuery();

			lightVertex->GetLayoutStream(layoutStream);

			shaderProgram.m_handle = m_nullShader;
			shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
			shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;

			shaderProgram.m_modelTransform = pLightData->m_mtxViewSpaceRender;

			GraphicsContext::DrawRequest drawRequest = {rt, layoutStream, shaderProgram};
			drawRequest.m_width = mvScreenSize.x;
			drawRequest.m_height = mvScreenSize.y;
			context.Submit(view, drawRequest, pLightData->m_occlusionQuery);
			
		}
	}


	//-----------------------------------------------------------------------

	void cRendererDeferred::InitLightRendering()
	{
		/////////////////////////////////
		// Get the inverse view matrix
		m_mtxInvView = cMath::MatrixInverse(mpCurrentFrustum->GetViewMatrix());


		//////////////////////////////
		//Setup quad vertex buffers
		if(mfLastFrustumFOV != mpCurrentFrustum->GetFOV() || mfLastFrustumFarPlane != mfFarPlane)
		{
			UpdateQuadVertexPostion(	mpFullscreenLightQuad,
										cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane),cVector2f(mfFarRight*2,mfFarTop*2),
										true);
		}

		//////////////////////////////
		//Setup misc variables
		mbStencilNeedClearing = false;

		//////////////////////////////
		//Clear lists
		for(int i=0;i<eDeferredLightList_LastEnum; ++i)
		{
			mvSortedLights[i].resize(0); //No clear, keep array size data, no need to delete, same pointer in temp list
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

			////////////////////////
			// Test if query has any samples.
			//  Only check if the query is done, else skip so we do not have a stop-and-wait.
			// iOcclusionQuery *pQuery = pLightData->mpQuery;
			// if(pQuery)
			// {
			if(bgfx::isValid(pLightData->m_occlusionQuery)) {
				int32_t numSamples = 0;
				const auto result = bgfx::getResult(pLightData->m_occlusionQuery, &numSamples);
				bool bLightInvisible = false;
				if(result != bgfx::OcclusionQueryResult::Enum::NoResult)
				{
					if(numSamples <= mpCurrentSettings->mlSampleVisiblilityLimit)
					{
						bLightInvisible = true;
					}

					bx::debugPrintf("Fetching query for light '%s'/%d. Have %d samples. Visible: %d\n", pLight->GetName().c_str(), pLight, numSamples, !bLightInvisible);
				}

				if(bLightInvisible)
				{
					continue;
				}
			}

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


	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLights_StencilBack_ScreenQuad()
	{
		if(mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].empty()) return;
		if(mbLog) Log("---\nRendering Lights StencilBack_ScreenQuad Begin\n");

		//Check if stencil is dirty
		if(mbStencilNeedClearing)
		{
			ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			mbStencilNeedClearing = false;
		}

		if(mbDepthCullLights) SetStencilActive(true);

		///////////////////////
		// Render Inside Near Plane Lights
		int lNumOfNearPlaneLights = (int)mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].size();
		int lStartLight = 0;
		while(lNumOfNearPlaneLights>0)
		{
			//cVector2l vMinClip(10000,10000);
			//cVector2l vMaxClip(-1000,-10000);

			int lIterations;
			if(lNumOfNearPlaneLights >kMaxStencilBitsUsed)	lIterations = kMaxStencilBitsUsed;
			else											lIterations = lNumOfNearPlaneLights;

			lNumOfNearPlaneLights -= lIterations;

			/////////////////////////////
			//Render stencil
			if(mbDepthCullLights)
				SetDepthTest(true);
			else
				SetDepthTest(false);
			SetChannelMode(eMaterialChannelMode_None);
			SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Keep,eStencilOp_Replace);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Stencil light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencilWriteMask(cMath::GetFlagBit(kStartStencilBit + i));

				//Set up textures
				SetTexture(5,NULL);

				//Set up matrix
				//SetupLightMatrix(m_mtxTempLight,pLight, mpCurrentFrustum);
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set vertex buffer
				SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_Medium));

				//Set program
				SetProgram(mpLightStencilProgram);

				DrawCurrent();
			}

			/////////////////////////////
			//Render lights as quads
			SetDepthTest(false);
			SetChannelMode(eMaterialChannelMode_RGBA);
			SetFlatProjectionMinMax(cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane*1.5f),cVector3f(mfFarRight,mfFarTop,mfFarPlane*1.5f));
			SetCullMode(eCullMode_CounterClockwise);

			SetStencilWriteMask(0xFF);

			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad][lStartLight + i];
				iLight *pLight = pLightData->mpLight;

				//////////////////
				// Render shadow (if light is caster)
				if(pLightData->mbCastShadows && SetupShadowMapRendering(pLight))
				{
					if(mbDepthCullLights)
					{
						SetStencilActive(false);
						SetDepthTest(true);
					}
					else
					{
						SetDepthTest(false);
					}

					//Render shadow map
					RenderLightShadowMap(pLightData);

					if(mbDepthCullLights)
					{
						SetDepthTest(false);
						SetStencilActive(true);
					}
					else
					{
						SetDepthTest(false);
					}

					SetFlatProjectionMinMax(cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane*1.5f),cVector3f(mfFarRight,mfFarTop,mfFarPlane*1.5f));
				}

				//////////////////
				// Render light
				if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencil(	eStencilFunc_Equal, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
												eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);

				//Set vertex buffer
				SetVertexBuffer(mpFullscreenLightQuad);

				// Set up clip
				SetScissorRect(pLightData->mClipRect, true);

				//Debug data
				RenderSingleLightDebug();

				//Set up program and textures
				iGpuProgram *pProgram = SetupProgramAndTextures(pLightData,0);

				//Set up Light specific variables
				SetupLightProgramVariables(pProgram, pLightData);

				//Draw the light
				DrawCurrent();

				/////////////////////////////
				// Update min max clip rect occupied by lights
				/*if(vMinClip.x > lightData.mClipRect.x) vMinClip.x = lightData.mClipRect.x;
				if(vMinClip.y > lightData.mClipRect.y) vMinClip.y = lightData.mClipRect.y;
				if(vMaxClip.x < lightData.mClipRect.x+lightData.mClipRect.w) vMaxClip.x = lightData.mClipRect.x+lightData.mClipRect.w;
				if(vMaxClip.y < lightData.mClipRect.y+lightData.mClipRect.h) vMaxClip.y = lightData.mClipRect.y+lightData.mClipRect.h;*/
			}

			/////////////////////////////
			//Go back to normal frustum projection
			SetNormalFrustumProjection();
			SetCullMode(eCullMode_Clockwise);
			SetScissorActive(false);

			/////////////////////////////
			//Prepare for next iteration (if any)
			if(lNumOfNearPlaneLights >0)
			{
				//According to test there was a slow down using this. Keeping code anyways.
				//cRect2l dirtyRect(vMinClip, vMaxClip - vMinClip);
				//SetupScissorRect(dirtyRect);

				lStartLight += kMaxStencilBitsUsed;
				ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			}
		}

		////////////////////////////////////////
		// Reset things needed
		mbStencilNeedClearing = true;
		if(mbLog) Log("Rendering Lights StencilBack_ScreenQuad End\n---\n");
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLights_StencilFront_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_StencilFront_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights StencilFront_RenderBack Begin\n");

		//Check if stencil is dirty
		if(mbStencilNeedClearing)
		{
			ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			mbStencilNeedClearing = false;
		}

		if(mbDepthCullLights)
		{
			SetStencilActive(true);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}

		///////////////////////
		// Render Large Lights
		int lNumOfNearPlaneLights = (int)mvSortedLights[eDeferredLightList_StencilFront_RenderBack].size();
		int lStartLight = 0;
		while(lNumOfNearPlaneLights>0)
		{


			int lIterations;
			if(lNumOfNearPlaneLights >kMaxStencilBitsUsed)	lIterations = kMaxStencilBitsUsed;
			else											lIterations = lNumOfNearPlaneLights;

			lNumOfNearPlaneLights -= lIterations;

			/////////////////////////////
			//Render stencil using the front (back?!), set bit at zfail (since test is inverted)
			SetChannelMode(eMaterialChannelMode_None);
			SetCullMode(eCullMode_CounterClockwise);
			SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Replace,eStencilOp_Keep);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Stencil light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencilWriteMask(cMath::GetFlagBit(kStartStencilBit + i));

				//Set up textures
				SetTexture(5,NULL);

				//Set up matrix
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set vertex buffer
				//SetVertexBuffer(mpShapeSphereMedRes);
				SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_Medium));

				//Set program
				SetProgram(mpLightStencilProgram);

				//Draw the light
				DrawCurrent();
			}

			/////////////////////////////
			//Render back (front?!) of light geometry, checking depth and stencil
			SetChannelMode(eMaterialChannelMode_RGBA);
			SetCullMode(eCullMode_Clockwise);
			SetStencilWriteMask(0xFF);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;

				//////////////////
				// Render shadow (if light is caster)
				if(pLightData->mbCastShadows && SetupShadowMapRendering(pLight))
				{
					//Setup render modes for shadow map rendering
					SetCullMode(eCullMode_CounterClockwise);
					SetStencilActive(false);

					//Render shadow map
					RenderLightShadowMap(pLightData);

					//Reset render settings to previous
					SetStencilActive(true);
					SetCullMode(eCullMode_Clockwise);
				}

				//////////////////
				// Render light
				if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencil(	eStencilFunc_Equal, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
												eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);

				//Set up matrix
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set vertex buffer
				//SetVertexBuffer(mpShapeSphereHighRes);
				SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_High));

				//Set up program and textures
				iGpuProgram *pProgram = SetupProgramAndTextures(pLightData,eFeature_Light_LightShapes | eFeature_Light_DivideInFrag);

				//Set up Light specific variables
				SetupLightProgramVariables(pProgram, pLightData);

				//Draw the light
				////////////////////////
				// If lights are occlusion tested, render a query with the front and push back into Light-Occlusion pair vector
				if(mbOcclusionTestLargeLights)
				{
					cLightOcclusionPair loPair;
					loPair.mpLight = pLight;
					loPair.m_occlusionQuery = bgfx::createOcclusionQuery();

					loPair.mpQuery->Begin();
					DrawCurrent();
					loPair.mpQuery->End();

					mpCurrentSettings->m_lightOcclusionPairs.push_back(loPair);
				}
				///////////////////////////////
				//Normal rendering of front
				else
				{
					DrawCurrent();
				}
			}

			/////////////////////////////
			//Prepare for next iteration (if any)
			if(lNumOfNearPlaneLights >0)
			{
				lStartLight += kMaxStencilBitsUsed;
				ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			}
		}

		////////////////////////////////////////
		// Reset things needed
		mbStencilNeedClearing = true;
		if(mbLog) Log("Rendering Lights StencilFront_RenderBack End\n---\n");
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLights_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights RenderBack Begin\n");

		if(mbDepthCullLights)
		{
			SetStencilActive(false);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}

		/////////////////////////////
		//Render back of light geometry, checking depth
		SetChannelMode(eMaterialChannelMode_RGBA);
		for(size_t i=0; i<mvSortedLights[eDeferredLightList_RenderBack].size(); ++i)
		{
			cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_RenderBack][i];
			iLight *pLight = pLightData->mpLight;

			//////////////////
			// Render shadow (if light is caster)
			if(pLightData->mbCastShadows && SetupShadowMapRendering(pLight))
			{
				//Setup render modes for shadow map rendering
				SetCullMode(eCullMode_CounterClockwise);

				//Render shadow map
				RenderLightShadowMap(pLightData);

				//Reset render settings to previous
				SetCullMode(eCullMode_Clockwise);
			}

			//////////////////
			// Render light
			if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

			//Set up matrix
			SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

			//Set vertex buffer
			//SetVertexBuffer(mpShapeSphereMedRes);
			SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_Medium));

			//Set up program and textures
			tFlag lExtraFlags = eFeature_Light_LightShapes;
			if(pLightData->mbInsideNearPlane) lExtraFlags |= eFeature_Light_DivideInFrag;
			iGpuProgram *pProgram = SetupProgramAndTextures(pLightData,lExtraFlags);

			//Set up Light specific variables
			SetupLightProgramVariables(pProgram, pLightData);

			//Draw the light
			DrawCurrent();
		}
		if(mbLog) Log("Rendering Lights RenderBack End\n---\n");
	}

	//------------------------------------------------------------------------------

	void cRendererDeferred::RenderBoxLight(cDeferredLight* apLightData)
	{
		iLight *pLight = apLightData->mpLight;
		if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

		cLightBox *pLightBox = static_cast<cLightBox*>(pLight);

		//Get correct program, depending if ssao is used or not.
		int lProgramNum =	(mbSSAOLoaded && mpCurrentSettings->mbSSAOActive && mSSAOType == eDeferredSSAO_InBoxLight) ? 1 :0;

		//Set up texture
		if(lProgramNum == 1) SetTexture(5, mpSSAOTexture);

		//Set up program
		SetProgram(mpLightBoxProgram[lProgramNum]);

		//Set up Light specific variables
		if(mpLightBoxProgram[lProgramNum])
		{
			mpLightBoxProgram[lProgramNum]->SetColor4f(kVar_avLightColor,pLight->GetDiffuseColor());
		}

		//Blend mode
		switch(pLightBox->GetBlendFunc())
		{
		case eLightBoxBlendFunc_Add:
			SetBlendMode(eMaterialBlendMode_Add); break;
		case eLightBoxBlendFunc_Replace:
			SetBlendMode(eMaterialBlendMode_None); break;
		}

		//Set up matrix
		SetModelViewMatrix(apLightData->m_mtxViewSpaceRender);

		//Draw the light
		DrawCurrent();
	}

	//------------------------------------------------------------------------------

	void cRendererDeferred::RenderLights_Box_StencilFront_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights Box_StencilFront_RenderBack Begin\n");

		//Check if stencil is dirty
		if(mbStencilNeedClearing)
		{
			ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			mbStencilNeedClearing = false;
		}

		if(mbDepthCullLights)
		{
			SetStencilActive(true);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}
		SetVertexBuffer(mpShapeBox);

		///////////////////////
		// Render Large Lights
		int lNumOfNearPlaneLights = (int)mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack].size();
		int lStartLight = 0;
		while(lNumOfNearPlaneLights>0)
		{
			int lIterations;
			if(lNumOfNearPlaneLights >kMaxStencilBitsUsed)	lIterations = kMaxStencilBitsUsed;
			else											lIterations = lNumOfNearPlaneLights;

			lNumOfNearPlaneLights -= lIterations;

			/////////////////////////////
			//Render stencil using the front, set bit at zfail (since test is inverted)
			SetChannelMode(eMaterialChannelMode_None);
			SetCullMode(eCullMode_CounterClockwise);
			SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Replace,eStencilOp_Keep);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Stencil light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencilWriteMask(cMath::GetFlagBit(kStartStencilBit + i));

				//Set up matrix
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set program
				SetProgram(mpLightStencilProgram);

				DrawCurrent();
			}

			/////////////////////////////
			//Render back of light geometry, checking depth and stencil
			SetChannelMode(eMaterialChannelMode_RGBA);
			SetCullMode(eCullMode_Clockwise);
			SetStencilWriteMask(0xFF);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencil(	eStencilFunc_Equal, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
												eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);
				//SetStencil(	eStencilFunc_Always, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
				//								eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);

				RenderBoxLight(pLightData);
			}

			/////////////////////////////
			//Prepare for next iteration (if any)
			if(lNumOfNearPlaneLights >0)
			{
				lStartLight += kMaxStencilBitsUsed;
				ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			}
		}

		////////////////////////////////////////
		// Reset things needed
		mbStencilNeedClearing = true;
		SetBlendMode(eMaterialBlendMode_Add);

		if(mbLog) Log("Rendering Lights Box_StencilFront_RenderBack End\n---\n");
	}

	//------------------------------------------------------------------------------

	void cRendererDeferred::RenderLights_Box_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_Box_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights Box_RenderBack Begin\n");

		if(mbDepthCullLights)
		{
			SetStencilActive(false);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}

		SetChannelMode(eMaterialChannelMode_RGBA);
		SetVertexBuffer(mpShapeBox);

		/////////////////////////////
		//Render back of light geometry, checking depth
		for(size_t i=0; i<mvSortedLights[eDeferredLightList_Box_RenderBack].size(); ++i)
		{
			cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_Box_RenderBack][i];

			RenderBoxLight(pLightData);
		}

		//Reset what is needed.
		SetBlendMode(eMaterialBlendMode_Add);

		if(mbLog) Log("Rendering Lights Box_RenderBack End\n---\n");
	}

	//------------------------------------------------------------------------------

	void cRendererDeferred::RenderLights()
	{
		START_RENDER_PASS(Lights);

		/////////////////////////////////////////
		// Sort the Lights and place them in the lists
		InitLightRendering();


		/////////////////////////////////////////
		// Render SSAO (used by box lights)
		// RenderSSAO();
		
		/////////////////////////////////////////
		// Set up general render states.
		SetDepthTest(true);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_Add);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, GetBufferTexture(i));
		SetTextureRange(NULL, mlNumOfGBufferTextures);

		////////////////////////////////
		//Set up frame buffer
		SetAccumulationBuffer();
		ClearFrameBuffer(eClearFrameBufferFlag_Color |eClearFrameBufferFlag_Stencil,true); //Perhaps there is some ambient light that can be added instead?

		///////////////////////
		// Set up culling and depth mode
		SetCullMode(eCullMode_Clockwise);
		SetDepthTestFunc(eDepthTestFunc_GreaterOrEqual);

		///////////////////////
		// Render box lights
		RenderLights_Box_StencilFront_RenderBack();
		RenderLights_Box_RenderBack();

		///////////////////////
		// Render lights that are inside near plane
		RenderLights_StencilBack_ScreenQuad();

		///////////////////////
		// Render lights with large screen size or with shadows.
		RenderLights_StencilFront_RenderBack();

		///////////////////////
		// Simple rendering with no stencil
		RenderLights_RenderBack();

		///////////////////////
		// Batch lights and renderer several at a time.
		// Skip for now...
		//RenderLights_Batches();

		////////////////////////////
		//Reset settings
		SetStencilActive(false);
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
		SetCullMode(eCullMode_CounterClockwise);

		////////////////////////////
		//Debug: Draw wire frame for all lights!
		#ifdef kDebug_RenderLightData
		{
			SetDepthTest(true);
			SetDepthWrite(false);
			SetBlendMode(eMaterialBlendMode_None);

			SetVertexBuffer(NULL);
			SetProgram(NULL);
			SetTextureRange(NULL,0);
			SetMatrix(NULL);

			for(int i=0; i<mpRenderList->GetLightNum(); ++i)
			{
				iLight* pLight = mpRenderList->GetLight(i);

				if(pLight->GetLightType() == eLightType_Point)
				{
					mpLowLevelGraphics->DrawSphere(pLight->GetWorldPosition(),pLight->GetRadius(), cColor(1,1));
					mpLowLevelGraphics->DrawSphere(pLight->GetWorldPosition(),0.2f, cColor(0.6f,1));
				}
				else if(pLight->GetLightType() == eLightType_Spot)
				{
					cLightSpot *pLightSpot = static_cast<cLightSpot*>(pLight);

					pLightSpot->GetFrustum()->Draw(mpLowLevelGraphics);
				}
			}
		}
		#endif


		END_RENDER_PASS();
	}

	void cRendererDeferred::RenderTranslucent()
	{
		if(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Translucent)==false) return;

		START_RENDER_PASS(Translucent);

		///////////////////////////////
		//Set up rendering
		SetDepthTest(true);
		SetDepthWrite(false);

		SetAlphaLimit(0.01f);
		SetAlphaMode(eMaterialAlphaMode_Trans);

		///////////////////////////////
		//Set up variables
		float fHalfFovTan=0;

		///////////////////////////////
		//Iterate transparent objects
		cRenderableVecIterator transIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Translucent);
		while(transIt.HasNext())
		{
			iRenderable *pObject = transIt.Next();
			cMaterial *pMaterial = pObject->GetMaterial();

			eMaterialRenderMode renderMode = mpCurrentWorld->GetFogActive() ? eMaterialRenderMode_DiffuseFog : eMaterialRenderMode_Diffuse;
			if(pMaterial->GetAffectedByFog()==false) renderMode = eMaterialRenderMode_Diffuse;

			//No world reflections in a reflection!
			if(mpCurrentSettings->mbIsReflection && pMaterial->HasWorldReflection()) continue;

			////////////////////////////////////////
			// Check the fog area alpha
			mfTempAlpha = 1;
			if(pMaterial->GetAffectedByFog())
			{
				for(size_t i=0; i<mpCurrentSettings->mvFogRenderData.size(); ++i)
				{
					mfTempAlpha *= GetFogAreaVisibilityForObject(&mpCurrentSettings->mvFogRenderData[i], pObject);
				}
			}


			////////////////////////////////////////
			// Update object, need to do this here since otherwise the reflection rendering might reset it!

			//Before viewport
			if(pObject->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime)==false)
			{
				continue;
			}

			if(pObject->RetrieveOcculsionQuery(this)==false)
			{
				continue;
			}

			cMatrixf *pMatrix = pObject->GetModelMatrix(mpCurrentFrustum);

			////////////////////////////////////////
			// World reflection
			if(pMaterial->HasWorldReflection() && pObject->GetRenderType() == eRenderableType_SubMesh)
			{
				if(CheckRenderablePlaneIsVisible(pObject, mpCurrentFrustum)==false) continue;

				///////////////////////////////////
				//Retrieve all occlusion queries before rendering new scene.
				//  Otherwise it will lead to problems on some cards.
				WaitAndRetrieveAllOcclusionQueries();	//Queires for halos and such
				if(mbOcclusionTestLargeLights)
					RetrieveAllLightOcclusionPair(false);	//Queries for light visibility (false = no stop and wait!)

				///////////////////////////////////
				//Render the reflection
				cSubMeshEntity *pReflectSubMeshEnt = static_cast<cSubMeshEntity*>(pObject);
				RenderReflection(pReflectSubMeshEnt);
			}

			////////////////////////////////////////
			// Refraction set up
			if(pMaterial->HasRefraction())
			{
				if(CheckRenderablePlaneIsVisible(pObject, mpCurrentFrustum)==false) continue;

				////////////////////////////////////
				// Get the clip rect needed by the refraction
				cBoundingVolume *pBV = pObject->GetBoundingVolume();

				if(fHalfFovTan ==0)
					fHalfFovTan = tan(mpCurrentFrustum->GetFOV()*0.5f);
				cRect2l clipRect = GetClipRectFromObject(pObject, 0.2f, mpCurrentFrustum, mvRenderTargetSize, fHalfFovTan);

				////////////////////////////////////
				// Add an extra check to make sure there is no bleeding. Draw outline of mesh to alpha!
				if(pMaterial->UseRefractionEdgeCheck())
				{
					////////////////////////////////////
					// Clear alpha
					SetFlatProjection(cVector2f((float)mvRenderTargetSize.x, (float)mvRenderTargetSize.y));

					//Set up new settings
					SetDepthTest(false);
					SetProgram(NULL);
					SetBlendMode(eMaterialBlendMode_None);
					SetAlphaMode(eMaterialAlphaMode_Solid);
					SetChannelMode(eMaterialChannelMode_A);
					SetTextureRange(NULL,0);

					DrawQuad(	cVector2f((float)clipRect.x, (float)clipRect.y),
								cVector2f((float)clipRect.w, (float)clipRect.h), 0, 1, false, cColor(1,0));

					//Set Normal projection and depth test
					SetNormalFrustumProjection();
					SetDepthTest(true);

					////////////////////////////////////
					// Render alpha of object
					SetMatrix(pMatrix);
					SetVertexBuffer(pObject->GetVertexBuffer());

					DrawCurrent();

					//Set backs settings to normal.
					SetChannelMode(eMaterialChannelMode_RGBA);
					SetAlphaMode(eMaterialAlphaMode_Trans);
				}

				////////////////////////////////////
				// Copy frame buffer to texture
				CopyFrameBufferToTexure(mpRefractionTexture,
										cVector2l(clipRect.x, clipRect.y),
										cVector2l(clipRect.w, clipRect.h),
										cVector2l(clipRect.x, clipRect.y),
										true);

			}

			////////////////////////////////////////
			// Set up and render
			if(pMaterial->HasRefraction())	SetBlendMode(eMaterialBlendMode_None); //Blending shall take place in shader!
			else							SetBlendMode(pMaterial->GetBlendMode());
			SetDepthTest(pMaterial->GetDepthTest());

			SetMaterialProgram(renderMode,pMaterial);
			SetMaterialTextures(renderMode, pMaterial);

			SetMatrix(pMatrix);

			SetVertexBuffer(pObject->GetVertexBuffer());

			DrawCurrentMaterial(renderMode, pObject);


			////////////////////////////////////////
			// Set up and render Illumination
			if(pMaterial->HasTranslucentIllumination())
			{
				renderMode = renderMode == eMaterialRenderMode_Diffuse ? eMaterialRenderMode_Illumination : eMaterialRenderMode_IlluminationFog;

				SetBlendMode(eMaterialBlendMode_Add);
				SetDepthTest(pMaterial->GetDepthTest());

				SetMaterialProgram(renderMode,pMaterial);
				SetMaterialTextures(renderMode, pMaterial);

				SetMatrix(pMatrix);

				SetVertexBuffer(pObject->GetVertexBuffer());

				DrawCurrentMaterial(renderMode, pObject);
			}
		}

		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetAlphaLimit(mfDefaultAlphaLimit);



		END_RENDER_PASS();
	}

    //-----------------------------------------------------------------------

	void cRendererDeferred::RenderReflection(iRenderable *apObject)
	{
		////////////////////////////////////
		//Set up variables
		cSubMeshEntity *pReflectionObject = static_cast<cSubMeshEntity*>(apObject);
		cMaterial *pRelfMaterial = pReflectionObject->GetMaterial();


		///////////////////////////
		//Check if surface is close enough for reflection!
		bool bReflectionIsInRange=true;
		if(pRelfMaterial->GetMaxReflectionDistance() > 0)
		{
			cVector3f vPoint = mpCurrentFrustum->GetOrigin() + mpCurrentFrustum->GetForward()*-1*pRelfMaterial->GetMaxReflectionDistance();
			cVector3f vNormal = mpCurrentFrustum->GetForward();

			cPlanef maxRelfctionDistPlane;
			maxRelfctionDistPlane.FromNormalPoint(vNormal, vPoint);

			if(cMath::CheckPlaneBVCollision(maxRelfctionDistPlane, *pReflectionObject->GetBoundingVolume())==eCollision_Outside)
			{
				bReflectionIsInRange = false;
			}
		}

		//////////////////////////////////////////////
		// Render the reflection!
		if(mpCurrentSettings->mbRenderWorldReflection && bReflectionIsInRange && pReflectionObject->GetIsOneSided())
		{
			RenderSubMeshEntityReflection(pReflectionObject);
		}
		//////////////////////////////////////////////
		// No reflection, just clear!
		else
		{
			if(mbReflectionTextureCleared == false)
			{
				if(mpCurrentSettings->mbLog) Log("- Clear reflection Begin!\n");

				cRenderTarget renderTarget;
				renderTarget.mpFrameBuffer = mpReflectionBuffer;

				SetFrameBuffer(mpReflectionBuffer, false, false);

				ClearFrameBuffer(eClearFrameBufferFlag_Color, false);

				SetAccumulationBuffer();

				mbReflectionTextureCleared = true;

				if(mpCurrentSettings->mbLog) Log("- Clear reflection End!\n");
			}
		}
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderSubMeshEntityReflection(cSubMeshEntity *pReflectionObject)
	{
		cMaterial *pRelfMaterial = pReflectionObject->GetMaterial();

		if(mbLog) Log("------------- Setting up Reflection rendering -----------\n");

		////////////////////////
		//Reset settings from normal rendering
		cFrustum *pSaved_Frustum = mpCurrentFrustum;
		cRenderSettings *pSaved_Settings = mpCurrentSettings;
		RenderViewport pSaved_RenderTarget = m_currentRenderTarget;
		bool bSaved_SendFrameBufferToPostEffects = mbSendFrameBufferToPostEffects;


		EndRendering(false);

		SetAlphaLimit(mfDefaultAlphaLimit);//Need to have the normal alpha limit!

		////////////////////////
		//Setup settings
		mpCurrentSettings->SetupReflectionSettings();

		mpCurrentSettings->mpReflectionSettings->mbUseOcclusionCulling = pRelfMaterial->GetWorldReflectionOcclusionTest();

		///////////////////////////////////
		//Reflection texture is not cleared!
		mbReflectionTextureCleared = false;

		///////////////////////////////////
		//Make render target
		RenderViewport renderTarget;
		// cVector2l vCurrentFrameBufferSize = mpCurrentRenderTarget->mpFrameBuffer ? mpCurrentRenderTarget->mpFrameBuffer->GetSize() : mvScreenSize;
		// renderTarget.mpFrameBuffer = mpReflectionBuffer;
		// renderTarget.mvPos = mpCurrentRenderTarget->mvPos / mlReflectionSizeDiv;
		// renderTarget.mvSize.x = mpCurrentRenderTarget->mvSize.x == -1 ? vCurrentFrameBufferSize.x/mlReflectionSizeDiv : mpCurrentRenderTarget->mvSize.x/mlReflectionSizeDiv;
		// renderTarget.mvSize.y = mpCurrentRenderTarget->mvSize.y == -1 ? vCurrentFrameBufferSize.y/mlReflectionSizeDiv : mpCurrentRenderTarget->mvSize.y/mlReflectionSizeDiv;

		///////////////////////////
		//Make a frustum, mirrored along the plane
		cSubMesh *pSubMesh = pReflectionObject->GetSubMesh();
		cVector3f vSurfaceNormal = cMath::Vector3Normalize(cMath::MatrixMul3x3(pReflectionObject->GetWorldMatrix(), pSubMesh->GetOneSidedNormal()));
		cVector3f vSurfacePos = cMath::MatrixMul(pReflectionObject->GetWorldMatrix(), pSubMesh->GetOneSidedPoint());

		cPlanef reflectPlane;
		reflectPlane.FromNormalPoint(vSurfaceNormal, vSurfacePos);

		cMatrixf mtxReflection = cMath::MatrixPlaneMirror(reflectPlane);
		cMatrixf mtxReflView = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), mtxReflection);
		cVector3f vReflOrigin = cMath::MatrixMul(mtxReflection, mpCurrentFrustum->GetOrigin());


		cMatrixf mtxProj = mpCurrentFrustum->GetProjectionMatrix();

		cPlanef cameraSpaceReflPlane = cMath::TransformPlane(mtxReflView, reflectPlane);
		cMatrixf mtxReflProj = cMath::ProjectionMatrixObliqueNearClipPlane(mtxProj, cameraSpaceReflPlane);

		cFrustum reflectFrustum;
		reflectFrustum.SetupPerspectiveProj(mtxReflProj, mtxReflView,
			mpCurrentFrustum->GetFarPlane(),mpCurrentFrustum->GetNearPlane(),
			mpCurrentFrustum->GetFOV(), mpCurrentFrustum->GetAspect(),
			vReflOrigin,false, &mtxProj, true);
		reflectFrustum.SetInvertsCullMode(true);

		///////////////////////////
		//Reset clip planes
		mpCurrentSettings->mpReflectionSettings->ResetOcclusionPlanes();

		///////////////////////////
		//End of reflection clip plane
		if(pRelfMaterial->GetMaxReflectionDistance() > 0)
		{
			cVector3f vForward = mpCurrentFrustum->GetForward()*-1;
			float fMaxReflDist = pRelfMaterial->GetMaxReflectionDistance();
			cPlanef maxRelfctionDistPlane;

			///////////////////////////////
			//Forward and normal is aligned, the normal of plane becomes inverse forward
			float fFDotN = cMath::Vector3Dot(vForward, vSurfaceNormal);
			if(fFDotN <-0.99999f)
			{
				cVector3f vClipNormal, vClipPoint;
				vClipNormal = vForward*-1;
				vClipPoint = mpCurrentFrustum->GetOrigin() + vForward*pRelfMaterial->GetMaxReflectionDistance();

				maxRelfctionDistPlane.FromNormalPoint(vClipNormal, vClipPoint);
			}
			///////////////////////////////
			//Get the plane into camera space and then get a point where z=max reflection distance.
			//Note: Because of test above a and b in plane cannot be 0!
			else
			{
				cPlanef cameraSpacePlane = cMath::TransformPlane(mpCurrentFrustum->GetViewMatrix(), reflectPlane);

				cVector3f vPoint1 = cVector3f(0,0, -fMaxReflDist);
				cVector3f vPoint2 = cVector3f(0,0, -fMaxReflDist);

				//Vertical row (x always same)
				if(fabs(cameraSpacePlane.b) < 0.0001f)
				{
					vPoint1.x = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.a;
					vPoint2 = vPoint1;
					vPoint2.y+=1;
				}
				//Horizontal row (y always same)
				else if(fabs(cameraSpacePlane.a) < 0.0001f)
				{
					vPoint1.y = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.b;
					vPoint2 = vPoint1;
					vPoint2.x+=1;
				}
				//Oblique row (x and y changes)
				else
				{
					vPoint1.x = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.a;
					vPoint2.y = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.b;
				}

				cMatrixf mtxInvCamera = cMath::MatrixInverse(mpCurrentFrustum->GetViewMatrix());
				vPoint1 = cMath::MatrixMul(mtxInvCamera, vPoint1);
				vPoint2 = cMath::MatrixMul(mtxInvCamera, vPoint2);

				cVector3f vNormal = cMath::Vector3Cross(vPoint1-vReflOrigin, vPoint2-vReflOrigin);
				vNormal.Normalize();
				//make sure normal has correct sign!
				if(cMath::Vector3Dot(vSurfaceNormal, vNormal)<0) vNormal = vNormal*-1;

				maxRelfctionDistPlane.FromNormalPoint(vNormal, vPoint1);
			}


			mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(maxRelfctionDistPlane);
		}

		//////////////////////////
		// Add screen rect Occlusion planes!
		if(mpCurrentSettings->mbClipReflectionScreenRect)
		{
			cVector3f vUp = reflectFrustum.GetViewMatrix().GetUp();
			cVector3f vRight = reflectFrustum.GetViewMatrix().GetRight();
			cVector3f vForward = reflectFrustum.GetViewMatrix().GetForward();
			cVector3f vOrigin = reflectFrustum.GetOrigin();

			float fNearPlane = reflectFrustum.GetNearPlane();
			float fHalfFovTan = tan(reflectFrustum.GetFOV()*0.5f);
			float fNearTop =  fHalfFovTan * fNearPlane;
			float fNearRight = reflectFrustum.GetAspect() * fNearTop;

			cVector3f vMin,vMax;
			bool bNeedsClipRect = false;
			bool bVisible = cMath::GetNormalizedClipRectFromBV(vMin, vMax, *pReflectionObject->GetBoundingVolume(), &reflectFrustum, fHalfFovTan);
			if(bVisible)
			{
				if(mbLog) Log("  Normalized Clip limits: (%s) -> (%s)\n", vMin.ToString().c_str(), vMax.ToString().c_str());
				////////////////////////////
				// Right
				if(vMax.x <1)
				{
					cVector3f vNearPlanePos = vOrigin + vRight*(vMax.x*fNearRight) + vForward*-fNearPlane;
					cPlanef rightPlane;
					rightPlane.FromPoints(vOrigin, vNearPlanePos, vNearPlanePos+vUp);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(rightPlane);
					bNeedsClipRect =true;
				}

				////////////////////////////
				// Left
				if(vMin.x >-1)
				{
					cVector3f vNearPlanePos = vOrigin + vRight*(vMin.x*fNearRight) + vForward*-fNearPlane;
					cPlanef leftPlane;
					leftPlane.FromPoints(vOrigin, vNearPlanePos+vUp, vNearPlanePos);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(leftPlane);
					bNeedsClipRect =true;
				}

				////////////////////////////
				// Top
				if(vMax.y <1)
				{
					cVector3f vNearPlanePos = vOrigin + vUp*(vMax.y*fNearTop) + vForward*-fNearPlane;
					cPlanef topPlane;
					topPlane.FromPoints(vOrigin, vNearPlanePos+vRight, vNearPlanePos);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(topPlane);
					bNeedsClipRect = true;
				}

				////////////////////////////
				// Bottom
				if(vMin.y >-1)
				{
					cVector3f vNearPlanePos = vOrigin + vUp*(vMin.y*fNearTop) + vForward*-fNearPlane;
					cPlanef bottomPlane;
					bottomPlane.FromPoints(vOrigin, vNearPlanePos, vNearPlanePos+vRight);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(bottomPlane);
					bNeedsClipRect =true;
				}

				////////////////////////////
				// Add a stencil rect!
				if(bNeedsClipRect)
				{
					// auto definition = renderTarget.GetSize();
					cVector2l vFrameBufferSize = renderTarget.GetSize();
					cVector2l vRenderTargetSize;
					vRenderTargetSize.x = vFrameBufferSize.x;
					vRenderTargetSize.y = vFrameBufferSize.y;

					cRect2l clipRect;
					cMath::GetClipRectFromBV(clipRect, *pReflectionObject->GetBoundingVolume(), mpCurrentFrustum, vRenderTargetSize, fHalfFovTan);

					if(mbLog) Log("  Setting up scissor rect. pos: (%d, %d)  %d x %d\n", clipRect.x, clipRect.y,clipRect.w, clipRect.h);

					mpCurrentSettings->mpReflectionSettings->mbUseScissorRect = true;
					mpCurrentSettings->mpReflectionSettings->mvScissorRectPos = cVector2l(clipRect.x, clipRect.y);
					mpCurrentSettings->mpReflectionSettings->mvScissorRectSize = cVector2l(clipRect.w, clipRect.h);
				}
			}
		}

		///////////////////////////
		//Render
		if(mpCurrentSettings->mbLog)
			Log("\n==============================\n= BEGIN RENDER REFLECTION\n==============================\n\n");

		Render(mfCurrentFrameTime, &reflectFrustum, mpCurrentWorld, mpCurrentSettings->mpReflectionSettings, renderTarget, false, mpCallbackList);

		if(mpCurrentSettings->mbLog)
			Log("\n==============================\n= END RENDER REFLECTION\n==============================\n\n");


		///////////////////////////
		//Set back to order!
		BeginRendering(	mfCurrentFrameTime, pSaved_Frustum, mpCurrentWorld, pSaved_Settings, pSaved_RenderTarget,
						bSaved_SendFrameBufferToPostEffects,mpCallbackList, false);


		SetAccumulationBuffer();

		//Set trans stuff
		SetDepthTest(true);
		SetDepthWrite(false);

		SetAlphaLimit(0.01f);
		SetAlphaMode(eMaterialAlphaMode_Trans);
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetAccumulationBuffer()
	{
		if(mpCurrentSettings->mbIsReflection)
			SetFrameBuffer(mpReflectionBuffer, true);
		else
			SetFrameBuffer(mpAccumBuffer, true);
	}

	//-----------------------------------------------------------------------

	[[deprecated("remove used of eGBufferComponents")]]
	void cRendererDeferred::SetGBuffer(eGBufferComponents aComponents)
	{
	}

	//-----------------------------------------------------------------------
	[[deprecated("remove used of eGBufferComponents")]]
	iFrameBuffer* cRendererDeferred::GetGBufferFrameBuffer(eGBufferComponents aComponents)
	{
		return nullptr;
	}

	//-----------------------------------------------------------------------

	iTexture* cRendererDeferred::GetBufferTexture(int alIdx)
	{
		// BX_ASSERT(false, "noop");
		return nullptr;
	}

	//-----------------------------------------------------------------------


	void cRendererDeferred::RenderGbufferContent()
	{
		START_RENDER_PASS(GBufferContent);

		//Pure testing below
		SetDepthTest(false);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetAccumulationBuffer();

		ClearFrameBuffer(eClearFrameBufferFlag_Depth | eClearFrameBufferFlag_Color, false);

		SetFlatProjection();
		SetProgram(NULL);

		SetTextureRange(NULL,1);

		SetTexture(0,GetBufferTexture(0));
		DrawQuad(cVector2f(0,0),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		SetTexture(0,GetBufferTexture(1));
		DrawQuad(cVector2f(0.5f,0),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		SetTexture(0,GetBufferTexture(2));
		DrawQuad(cVector2f(0,0.5f),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		if(mlNumOfGBufferTextures > 3)
		{
			SetTexture(0,GetBufferTexture(3));
			DrawQuad(cVector2f(0.5f,0.5f),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		}


		SetNormalFrustumProjection();
		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderReflectionContent()
	{
		START_RENDER_PASS(ReflectionContent);

		//Pure testing below
		SetDepthTest(false);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetAccumulationBuffer();

		SetFlatProjection();
		SetProgram(NULL);

		SetTextureRange(NULL,1);

		SetTexture(0,mpReflectionTexture);
		DrawQuad(cVector2f(0,0),cVector2f(1,1), 0, mvScreenSizeFloat / (float)mlReflectionSizeDiv, true);


		SetNormalFrustumProjection();
		END_RENDER_PASS();
	}

}
