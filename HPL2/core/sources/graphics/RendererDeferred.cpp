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
#include "engine/Event.h"
#include "engine/Interface.h"
#include "graphics/ImmediateDrawBatch.h"
#include "scene/Viewport.h"
#include "windowing/NativeWindow.h"
#include <bx/debug.h>

#include <cstdint>
#include <graphics/Enum.h>

#include "bx/math.h"

#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
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
#include "graphics/TextureCreator.h"
#include "graphics/ShaderUtil.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/MeshManager.h"
#include "graphics/GPUShader.h"

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
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

namespace hpl {

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

        namespace detail {
            struct FogRendererData {
                cFogArea* m_fogArea;
                bool m_insideNearFrustum;
                cVector3f m_boxSpaceFrustumOrigin;
                cMatrixf m_mtxInvBoxSpace;
            };
            float GetFogAreaVisibilityForObject(const FogRendererData& fogData, cFrustum& frustum, iRenderable* apObject) {
                cFogArea* pFogArea = fogData.m_fogArea;

                cVector3f vObjectPos = apObject->GetBoundingVolume()->GetWorldCenter();
                cVector3f vRayDir = vObjectPos - frustum.GetOrigin();
                float fCameraDistance = vRayDir.Length();
                vRayDir = vRayDir / fCameraDistance;

                float fEntryDist, fExitDist;

                auto checkFogAreaIntersection =
                    [&fEntryDist,
                     &fExitDist](const cMatrixf& a_mtxInvBoxModelMatrix, const cVector3f& avBoxSpaceRayStart, const cVector3f& avRayDir) {
                        cVector3f vBoxSpaceDir = cMath::MatrixMul3x3(a_mtxInvBoxModelMatrix, avRayDir);

                        bool bFoundIntersection = false;
                        fExitDist = 0;

                        std::array<cVector3f, 6> fBoxPlaneNormals = { {
                            cVector3f(-1, 0, 0), // Left
                            cVector3f(1, 0, 0), // Right

                            cVector3f(0, -1, 0), // Bottom
                            cVector3f(0, 1, 0), // Top

                            cVector3f(0, 0, -1), // Back
                            cVector3f(0, 0, 1), // Front
                        } };

                        ///////////////////////////////////
                        // Iterate the sides of the cube
                        for (auto& planeNormal : fBoxPlaneNormals) {
                            ///////////////////////////////////
                            // Calculate plane intersection
                            float fMul = cMath::Vector3Dot(planeNormal, vBoxSpaceDir);
                            if (fabs(fMul) < 0.0001f) {
                                continue;
							}
                            float fNegDist = -(cMath::Vector3Dot(planeNormal, avBoxSpaceRayStart) + 0.5f);

                            float fT = fNegDist / fMul;
                            if (fT < 0)
                                continue;
                            cVector3f vAbsNrmIntersect = cMath::Vector3Abs(vBoxSpaceDir * fT + avBoxSpaceRayStart);

                            ///////////////////////////////////
                            // Check if the intersection is inside the cube
                            if (cMath::Vector3LessEqual(vAbsNrmIntersect, cVector3f(0.5001f))) {
                                //////////////////////
                                // First intersection
                                if (bFoundIntersection == false) {
                                    fEntryDist = fT;
                                    fExitDist = fT;
                                    bFoundIntersection = true;
                                }
                                //////////////////////
                                // There has already been a intersection.
                                else {
                                    fEntryDist = cMath::Min(fEntryDist, fT);
                                    fExitDist = cMath::Max(fExitDist, fT);
                                }
                            }
                        }

                        if (fExitDist < 0)
                            return false;

                        return bFoundIntersection;
                    };
                if (checkFogAreaIntersection(fogData.m_mtxInvBoxSpace, fogData.m_boxSpaceFrustumOrigin, vRayDir) == false) {
                    return 1.0f;
                }

                if (fogData.m_insideNearFrustum == false && fCameraDistance < fEntryDist) {
                    return 1.0f;
                }

                //////////////////////////////
                // Calculate the distance the ray travels in the fog
                float fFogDist;
                if (fogData.m_insideNearFrustum) {
                    if (pFogArea->GetShowBacksideWhenInside())
                        fFogDist = cMath::Min(fExitDist, fCameraDistance);
                    else
                        fFogDist = fCameraDistance;
                } else {
                    if (pFogArea->GetShowBacksideWhenOutside())
                        fFogDist = cMath::Min(fExitDist - fEntryDist, fCameraDistance - fEntryDist);
                    else
                        fFogDist = fCameraDistance - fEntryDist;
                }

                //////////////////////////////
                // Calculate the alpha
                if (fFogDist <= 0)
                    return 1.0f;

                float fFogStart = pFogArea->GetStart();
                float fFogEnd = pFogArea->GetEnd();
                float fFogAlpha = 1 - pFogArea->GetColor().a;

                if (fFogDist < fFogStart)
                    return 1.0f;

                if (fFogDist > fFogEnd)
                    return fFogAlpha;

                float fAlpha = (fFogDist - fFogStart) / (fFogEnd - fFogStart);
                if (pFogArea->GetFalloffExp() != 1)
                    fAlpha = powf(fAlpha, pFogArea->GetFalloffExp());

                return (1.0f - fAlpha) + fFogAlpha * fAlpha;
            }
        } // namespace detail

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

		mfMinLargeLightNormalizedArea = 0.2f*0.2f;
		// mfMinRenderReflectionNormilzedLength = 0.15f;

		m_shadowDistanceMedium = 10;
		m_shadowDistanceLow = 20;
		m_shadowDistanceNone = 40;

		mlMaxBatchLights = 100;

		m_boundViewportData = std::move(UniqueViewportData<SharedViewportData>([](cViewport& viewport) {
			auto sharedData = std::make_unique<SharedViewportData>();
			sharedData->m_size = viewport.GetSize();

			auto colorImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(sharedData->m_size.x, sharedData->m_size.y, false, bgfx::TextureFormat::Enum::RGBA8);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			auto positionImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(sharedData->m_size.x, sharedData->m_size.y, false, bgfx::TextureFormat::Enum::RGBA32F);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			auto normalImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(sharedData->m_size.x, sharedData->m_size.y, false, bgfx::TextureFormat::Enum::RGBA16F);
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};

			auto depthImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(sharedData->m_size.x, sharedData->m_size.y, false, bgfx::TextureFormat::Enum::D24S8);
				desc.m_configuration.m_rt = RTType::RT_Write;
				desc.m_configuration.m_minFilter = FilterType::Point;
				desc.m_configuration.m_magFilter = FilterType::Point;
				desc.m_configuration.m_mipFilter = FilterType::Point;
				desc.m_configuration.m_comparsion = DepthTest::LessEqual;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			};
			sharedData->m_gBufferColor = colorImage();
			sharedData->m_gBufferNormalImage = normalImage();
			sharedData->m_gBufferPositionImage = positionImage();
			sharedData->m_gBufferSpecular = colorImage();
			sharedData->m_gBufferDepthStencil = depthImage();
			sharedData->m_outputImage = colorImage();
			sharedData->m_refractionImage = [&] {
				auto desc = ImageDescriptor::CreateTexture2D(viewport.GetSize().x, viewport.GetSize().y, false, bgfx::TextureFormat::Enum::RGBA8);
				desc.m_configuration.m_computeWrite = true;
				desc.m_configuration.m_rt = RTType::RT_Write;
				auto image = std::make_shared<Image>();
				image->Initialize(desc);
				return image;
			}();
			{
				std::array<std::shared_ptr<Image>, 5> images = {sharedData->m_gBufferColor, sharedData->m_gBufferNormalImage, sharedData->m_gBufferPositionImage, sharedData->m_gBufferSpecular, sharedData->m_gBufferDepthStencil};
				sharedData->m_gBuffer_full = RenderTarget(std::span(images));
			}
			{
				std::array<std::shared_ptr<Image>, 2> images = {sharedData->m_gBufferColor, sharedData->m_gBufferDepthStencil};
				sharedData->m_gBuffer_colorAndDepth = RenderTarget(std::span(images));
			}
			{
				std::array<std::shared_ptr<Image>, 2> images = {sharedData->m_outputImage, sharedData->m_gBufferDepthStencil };
				sharedData->m_output_target = RenderTarget(std::span(images));
			}

			sharedData->m_gBuffer_color = RenderTarget(sharedData->m_gBufferColor);
			sharedData->m_gBuffer_depth = RenderTarget(sharedData->m_gBufferDepthStencil);
			sharedData->m_gBuffer_normals = RenderTarget(sharedData->m_gBufferNormalImage);
			sharedData->m_gBuffer_linearDepth = RenderTarget(sharedData->m_gBufferPositionImage);
			return sharedData;
		}, [](cViewport& viewport, SharedViewportData& target) {
			return target.m_size == viewport.GetSize();
		}));

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
            desc.m_configuration.m_minFilter = FilterType::Point;
            desc.m_configuration.m_magFilter = FilterType::Point;
            desc.m_configuration.m_mipFilter = FilterType::Point;
            desc.m_configuration.m_comparsion = DepthTest::LessEqual;
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

		//High
		if(mShadowMapQuality == eShadowMapQuality_High)	{
			m_shadowJitterSize = 64;
			m_shadowJitterSamples = 32;	//64 here instead? I mean, ATI has to deal with medium has max? or different max for ATI?
			m_spotlightVariants.Initialize(
				ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight_high", false, true));
		}
		//Medium
		else if(mShadowMapQuality == eShadowMapQuality_Medium) {
			m_shadowJitterSize = 32;
			m_shadowJitterSamples = 16;
			m_spotlightVariants.Initialize(
				ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight_medium", false, true));
		}
		//Low
		else {
			m_shadowJitterSize = 0;
			m_shadowJitterSamples = 0;
			m_spotlightVariants.Initialize(
				ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight_low", false, true));
		}


		if(mShadowMapQuality != eShadowMapQuality_Low)
		{
			m_shadowJitterImage = std::make_shared<Image>();
			TextureCreator::GenerateScatterDiskMap2D(*m_shadowJitterImage, m_shadowJitterSize,m_shadowJitterSamples, true);
		}
		
		m_u_param.Initialize();
		m_u_lightPos.Initialize();
		m_u_fogColor.Initialize();
		m_u_lightColor.Initialize();
		m_u_overrideColor.Initialize();
		m_u_copyRegion.Initialize();
		m_u_spotViewProj.Initialize();
		m_u_mtxInvRotation.Initialize();
		m_u_mtxInvViewRotation.Initialize();

		m_s_depthMap.Initialize();
		m_s_positionMap.Initialize();
		m_s_diffuseMap.Initialize();
		m_s_normalMap.Initialize();
		m_s_specularMap.Initialize();
		m_s_attenuationLightMap.Initialize();
		m_s_spotFalloffMap.Initialize();
		m_s_shadowMap.Initialize();
		m_s_goboMap.Initialize();
		m_s_shadowOffsetMap.Initialize();
		m_s_diffuseMapOut.Initialize();


		m_copyRegionProgram = hpl::loadProgram("cs_copy_region");
		m_lightBoxProgram = hpl::loadProgram("vs_light_box", "fs_light_box");
		m_fogVariant.Initialize(
			ShaderHelper::LoadProgramHandlerDefault("vs_deferred_fog", "fs_deferred_fog", true, true));
		m_pointLightVariants.Initialize(
			ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_pointlight", false, true));
		
		////////////////////////////////////
		//Create SSAO programs and textures
		// if(mbSSAOLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
		// {
		// 	mbSSAOLoaded = false;
		// 	Warning("System does not support float textures! SSAO is disabled.\n");
		// }
		// if(mbSSAOLoaded)
		// {
		// 	cVector2l vSSAOSize = mvScreenSize / mlSSAOBufferSizeDiv;

		// }

		// ////////////////////////////////////
		// //Create Smooth Edge and textures
		// if(mbEdgeSmoothLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
		// {
		// 	mbEdgeSmoothLoaded = false;
		// 	Warning("System does not support float textures! Edge smooth is disabled.\n");
		// }
		// if(mbEdgeSmoothLoaded)
		// {
			
		// }

		////////////////////////////////////
		//Create light shapes
		tFlag lVtxFlag = eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0;
		m_shapeSphere[eDeferredShapeQuality_High] = std::unique_ptr<iVertexBuffer>(LoadVertexBufferFromMesh("core_12_12_sphere.dae",lVtxFlag));
		m_shapeSphere[eDeferredShapeQuality_Medium] = std::unique_ptr<iVertexBuffer>(LoadVertexBufferFromMesh("core_7_7_sphere.dae",lVtxFlag));
		m_shapeSphere[eDeferredShapeQuality_Low] = std::unique_ptr<iVertexBuffer>(LoadVertexBufferFromMesh("core_5_5_sphere.dae",lVtxFlag));

		m_shapePyramid = std::unique_ptr<iVertexBuffer>(LoadVertexBufferFromMesh("core_pyramid.dae",lVtxFlag));

		////////////////////////////////////
		//Batch vertex buffer
		mlMaxBatchVertices = m_shapeSphere[eDeferredShapeQuality_Low]->GetVertexNum() * mlMaxBatchLights;
		mlMaxBatchIndices = m_shapeSphere[eDeferredShapeQuality_Low]->GetIndexNum() * mlMaxBatchLights;

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
		return true;
	}


	void cRendererDeferred::DestroyData()
	{
		// for(auto& shape: m_shapeSphere) {
		// 	if(shape) {
		// 		delete shape;
		// 	}
		// }
		// if(m_shapePyramid) hplDelete(m_shapePyramid);


		// mpGraphics->DestroyTexture(mpReflectionTexture);


		/////////////////////////
		//Shadow textures
		// DestroyShadowMaps();

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

	std::shared_ptr<Image> cRendererDeferred::GetOutputImage(cViewport& viewport) {
		return m_boundViewportData.resolve(viewport).m_outputImage;
	}

	void cRendererDeferred::RenderEdgeSmoothPass(GraphicsContext& context, cViewport& viewport, RenderTarget& rt) {

		GraphicsContext::ViewConfiguration viewConfig {m_edgeSmooth_LinearDepth};
		auto edgeSmoothView = context.StartPass("EdgeSmooth", viewConfig);
		cVector3f vQuadPos = cVector3f(m_farLeft,m_farBottom,-m_farPlane);
		cVector2f vQuadSize = cVector2f(m_farRight*2,m_farTop*2);

		GraphicsContext::ShaderProgram shaderProgram;
		shaderProgram.m_handle = m_edgeSmooth_UnpackDepthProgram;
		// shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_gBufferPositionImage)->GetHandle(), 0});
		// shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_gBufferNormalImage)->GetHandle(), 0});

		GraphicsContext::LayoutStream layout;
		context.Quad(layout, vQuadPos, cVector2f(1,1), cVector2f(0,0), cVector2f(1,1));

		GraphicsContext::DrawRequest drawRequest {layout, shaderProgram};
		context.Submit(edgeSmoothView, drawRequest);
	}


	void cRendererDeferred::RenderDiffusePass(GraphicsContext& context, cViewport& viewport, RenderTarget& rt) {
		BX_ASSERT(rt.GetImages().size() >= 4, "expected 4 images Diffuse(0), Normal(1), Position(2), Specular(3), Depth(4)");
		BX_ASSERT(rt.IsValid(), "Invalid render target");
		
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Diffuse)) {
			return;
		}
		auto& sharedData = m_boundViewportData.resolve(viewport);

		GraphicsContext::ViewConfiguration viewConfig {rt};
		viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
		viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
		viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
		auto view = context.StartPass("Diffuse", viewConfig);
		rendering::detail::RenderableMaterialIter(this, mpCurrentRenderList->GetRenderableItems(eRenderListType_Diffuse), viewport, eMaterialRenderMode_Diffuse, [&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			shaderInput.m_configuration.m_write = Write::RGBA;
			shaderInput.m_configuration.m_cull = Cull::CounterClockwise;

			shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ?  obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();
			shaderInput.m_normalMtx = cMath::MatrixInverse(cMath::MatrixMul(shaderInput.m_modelTransform, viewConfig.m_view)); // matrix is already transposed

			GraphicsContext::DrawRequest drawRequest {layoutInput, shaderInput};
			context.Submit(view, drawRequest);
		});
	}

	void rendering::detail::RenderZPassObject(bgfx::ViewId view, GraphicsContext& context, cViewport& viewport, iRenderer* renderer, iRenderable* object, Cull cull) {
		eMaterialRenderMode renderMode = object->GetCoverageAmount() >= 1 ? eMaterialRenderMode_Z : eMaterialRenderMode_Z_Dissolve;
		cMaterial *pMaterial = object->GetMaterial();
		iMaterialType* materialType = pMaterial->GetType();
		iVertexBuffer* vertexBuffer = object->GetVertexBuffer();
		if(vertexBuffer == nullptr || materialType == nullptr) {
			return;
		}

		GraphicsContext::LayoutStream layoutInput;
		vertexBuffer->GetLayoutStream(layoutInput);
		materialType->ResolveShaderProgram(renderMode, viewport, pMaterial, object, renderer, [&](GraphicsContext::ShaderProgram& shaderInput) {
			shaderInput.m_configuration.m_write = Write::Depth;
			shaderInput.m_configuration.m_cull = cull;
			shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
			
			shaderInput.m_modelTransform = object->GetModelMatrixPtr() ? object->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity ;

			GraphicsContext::DrawRequest drawRequest {layoutInput, shaderInput};
			context.Submit(view, drawRequest);
		});
		
	}

	void cRendererDeferred::RenderZPass(GraphicsContext& context, cViewport& viewport, RenderTarget& rt) {
		if(!mpCurrentRenderList->ArrayHasObjects(eRenderListType_Z)) {
			return;
		}
		auto& sharedData = m_boundViewportData.resolve(viewport);
		GraphicsContext::ViewConfiguration viewConfig {rt};
		viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
		viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
		viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();

		auto view = context.StartPass("RenderZ", viewConfig);
		for(auto& obj: mpCurrentRenderList->GetRenderableItems(eRenderListType_Z)) {
			rendering::detail::RenderZPassObject(view, context, viewport, this, obj);
		}
	}


	void cRendererDeferred::Draw(GraphicsContext& context, cViewport& viewport, float afFrameTime, cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings,
					bool abSendFrameBufferToPostEffects, tRendererCallbackList *apCallbackList)  {
		// keep around for the moment ...
		BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects, apCallbackList);

		mpCurrentRenderList->Setup(mfCurrentFrameTime,mpCurrentFrustum);
		
		const cMatrixf frustumView = mpCurrentFrustum->GetViewMatrix().GetTranspose();
		const cMatrixf frustumProj = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
		
		//Setup far plane coordinates
		m_farPlane = mpCurrentFrustum->GetFarPlane();
		m_farTop = -tan(mpCurrentFrustum->GetFOV()*0.5f) * m_farPlane;
		m_farBottom = -m_farTop;
		m_farRight = m_farBottom * mpCurrentFrustum->GetAspect();
		m_farLeft = -m_farRight;

		cRendererCallbackFunctions handler(context, viewport, this);

		auto& sharedData = m_boundViewportData.resolve(viewport);
		
		[&]{
			GraphicsContext::ViewConfiguration viewConfig {sharedData.m_gBuffer_full};
			viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			viewConfig.m_clear = {0, 1, 0, ClearOp::Depth | ClearOp::Stencil | ClearOp::Color};
			bgfx::touch(context.StartPass("Clear Depth", viewConfig));
		}();

		tRenderableFlag lVisibleFlags= (mpCurrentSettings->mbIsReflection) ?
			eRenderableFlag_VisibleInReflection : eRenderableFlag_VisibleInNonReflection;

		///////////////////////////
		//Occlusion testing
		if(mpCurrentSettings->mbUseOcclusionCulling)
		{
			// GraphicsContext::ViewConfiguration viewConfig {resolveRenderTarget(m_gBuffer_depth)};
			// auto view = context.StartPass("Render Z", viewConfig);
			RenderZPassWithVisibilityCheck(	
				context,
				mpCurrentSettings->mpVisibleNodeTracker,
				eObjectVariabilityFlag_All,
				lVisibleFlags,
				sharedData.m_gBuffer_depth,
			[&](bgfx::ViewId view, iRenderable* object) {
				cMaterial *pMaterial = object->GetMaterial();

				mpCurrentRenderList->AddObject(object);

				if(!pMaterial || pMaterial->GetType()->IsTranslucent()) {
					return false;
				}
				rendering::detail::RenderZPassObject(view, context, viewport, this, object);
				return true;
			});

			// this occlusion query logic is used for reflections just cut this for now
			GraphicsContext::ViewConfiguration occlusionPassConfig {sharedData.m_gBuffer_depth};
			occlusionPassConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			occlusionPassConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			occlusionPassConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			AssignAndRenderOcclusionQueryObjects(
				context.StartPass("Render Occlusion", occlusionPassConfig), 
				context, 
				false, 
				true);

			SetupLightsAndRenderQueries(context, sharedData.m_gBuffer_depth);

			mpCurrentRenderList->Compile(	eRenderListCompileFlag_Diffuse |
											eRenderListCompileFlag_Translucent |
											eRenderListCompileFlag_Decal |
											eRenderListCompileFlag_Illumination |
											eRenderListCompileFlag_FogArea );
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
											eRenderListCompileFlag_Illumination |
											eRenderListCompileFlag_FogArea );
			RenderZPass(context, viewport, sharedData.m_gBuffer_depth );

			// this occlusion query logic is used for reflections just cut this for now

			GraphicsContext::ViewConfiguration occlusionPassConfig {sharedData.m_gBuffer_depth};
			occlusionPassConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			occlusionPassConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			occlusionPassConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			AssignAndRenderOcclusionQueryObjects(
				context.StartPass("Render Occlusion Pass", occlusionPassConfig), 
				context, 
				false, 
				true);

			SetupLightsAndRenderQueries(context, sharedData.m_gBuffer_depth);
		}


		// Render GBuffer to m_gBuffer_full old method is RenderGbuffer(context);
		RenderDiffusePass(context, viewport, sharedData.m_gBuffer_full);

		// ------------------------------------------------------------------------------------
		//  Render Decal Pass render to color and depth
		// ------------------------------------------------------------------------------------ 
		([&]() {
			BX_ASSERT(sharedData.m_gBuffer_colorAndDepth.IsValid(), "Invalid render target");
			BX_ASSERT(sharedData.m_gBuffer_colorAndDepth.GetImages().size() >= 1, "expected atleast 1 image Color(0)");
			auto decalSpan = mpCurrentRenderList->GetRenderableItems(eRenderListType_Decal);
			if(decalSpan.empty()) {
				return;
			}

			GraphicsContext::ViewConfiguration viewConfig {sharedData.m_gBuffer_colorAndDepth};
			viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			auto view = context.StartPass("RenderDecals", viewConfig);
			rendering::detail::RenderableMaterialIter(this, 
				decalSpan, 
				viewport, 
				eMaterialRenderMode_Diffuse, 
				[&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
					shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
					shaderInput.m_configuration.m_write = Write::RGBA;

					cMaterial *pMaterial = obj->GetMaterial();
					shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
					shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
					shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ?  obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();

					GraphicsContext::DrawRequest drawRequest {layoutInput, shaderInput};
					context.Submit(view, drawRequest);
				});
		})();

		
		RunCallback(eRendererMessage_PostGBuffer, handler);

		RenderLightPass(context, viewport, sharedData.m_output_target);

		// ------------------------------------------------------------------------
		// Render Illumination Pass --> renders to output target
		// ------------------------------------------------------------------------ 
		([&]() {
			auto illuminationSpan = mpCurrentRenderList->GetRenderableItems(eRenderListType_Illumination);
			if(illuminationSpan.empty()) {
				return;
			}

			GraphicsContext::ViewConfiguration viewConfig {sharedData.m_output_target};
			viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			viewConfig.m_viewRect = cRect2l(0, 0, sharedData.m_size.x, sharedData.m_size.y);
			bgfx::ViewId view = context.StartPass("RenderIllumination", viewConfig);
			rendering::detail::RenderableMaterialIter(this, 
				illuminationSpan, 
				viewport, 
				eMaterialRenderMode_Illumination, [&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
					shaderInput.m_configuration.m_depthTest = DepthTest::Equal;
					shaderInput.m_configuration.m_write = Write::RGBA;

					shaderInput.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
					shaderInput.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

					shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ?  obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();

					GraphicsContext::DrawRequest drawRequest {layoutInput, shaderInput};
					context.Submit(view, drawRequest);
				});
		})();

		// ------------------------------------------------------------------------
		// Setup Fog Pass
		// ------------------------------------------------------------------------
		std::vector<detail::FogRendererData> fogRenderData;
		auto fogAreas = mpCurrentRenderList->GetFogAreas();
		fogRenderData.reserve(fogAreas.size());
		for(const auto& fogArea: fogAreas ) {
			auto& fogData = fogRenderData.emplace_back(detail::FogRendererData {
				fogArea,
				false,
				cVector3f(0.0f),
				cMatrixf(cMatrixf::Identity)
			});
			fogData.m_fogArea = fogArea;
			fogData.m_mtxInvBoxSpace = cMath::MatrixInverse(*fogArea->GetModelMatrixPtr());
			fogData.m_boxSpaceFrustumOrigin = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, mpCurrentFrustum->GetOrigin());
			fogData.m_insideNearFrustum = ([&]() {
					std::array<cVector3f, 4> nearPlaneVtx;
					cVector3f min(-0.5f);
					cVector3f max(0.5f);
					
					for (size_t i = 0; i < nearPlaneVtx.size(); ++i)
					{
						nearPlaneVtx[i] = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, mpCurrentFrustum->GetVertex(i));
					}
					for (size_t i = 0; i < nearPlaneVtx.size(); ++i)
					{
						if (cMath::CheckPointInAABBIntersection(nearPlaneVtx[i], min, max)) {
							return true;
						}
					}
					//////////////////////////////
					// Check if near plane points intersect with box
					if (cMath::CheckPointsAABBPlanesCollision(nearPlaneVtx.begin(), 4, min, max) != eCollision_Outside) {
						return true;
					}
					return false;
			})();
		}
		
		// ------------------------------------------------------------------------
		// Render Fog Pass --> output target
		// ------------------------------------------------------------------------
		([&]() {
			if(fogRenderData.empty()) {
				return;
			}

			GraphicsContext::ViewConfiguration viewConfig {sharedData.m_output_target};
			viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			const auto view = context.StartPass("Fog Pass", viewConfig);
			for(const auto& fogArea: fogRenderData) {
				struct {
					float u_fogStart;
					float u_fogLength;
					float u_fogFalloffExp;

					float u_fogRayCastStart[3];
					float u_fogNegPlaneDistNeg[3];
					float u_fogNegPlaneDistPos[3];
				} uniforms;

				uniforms.u_fogStart = fogArea.m_fogArea->GetStart();
				uniforms.u_fogLength = fogArea.m_fogArea->GetEnd() - fogArea.m_fogArea->GetStart();
				uniforms.u_fogFalloffExp = fogArea.m_fogArea->GetFalloffExp();
				//Outside of box setup
				cMatrixf rotationMatrix = cMatrixf(cMatrixf::Identity);
				uint32_t flags =  rendering::detail::FogVariant_None;
		
				GraphicsContext::LayoutStream layoutStream;
				GraphicsContext::ShaderProgram shaderProgram;

				mpShapeBox->GetLayoutStream(layoutStream);
				
				shaderProgram.m_modelTransform = fogArea.m_fogArea->GetModelMatrixPtr() ? fogArea.m_fogArea->GetModelMatrixPtr()->GetTranspose(): cMatrixf::Identity;
				shaderProgram.m_configuration.m_rgbBlendFunc =  CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
				shaderProgram.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
				shaderProgram.m_configuration.m_write = Write::RGB;

				if(fogArea.m_insideNearFrustum)
				{
					shaderProgram.m_configuration.m_cull = Cull::Clockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::Always;
					flags |=  fogArea.m_fogArea->GetShowBacksideWhenInside() ? rendering::detail::FogVariant_UseBackSide : rendering::detail::FogVariant_None;
				} 
				else 
				{

					cMatrixf mtxInvModelView = cMath::MatrixInverse( cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), *fogArea.m_fogArea->GetModelMatrixPtr()));
					cVector3f vRayCastStart = cMath::MatrixMul(mtxInvModelView, cVector3f(0));
					// rotationMatrix = mtxInvModelView.GetRotation().GetTranspose();

					cVector3f vNegPlaneDistNeg( cMath::PlaneToPointDist(cPlanef(-1,0,0,0.5f),vRayCastStart), cMath::PlaneToPointDist(cPlanef(0,-1,0,0.5f),vRayCastStart),
												cMath::PlaneToPointDist(cPlanef(0,0,-1,0.5f),vRayCastStart));
					cVector3f vNegPlaneDistPos( cMath::PlaneToPointDist(cPlanef(1,0,0,0.5f),vRayCastStart), cMath::PlaneToPointDist(cPlanef(0,1,0,0.5f),vRayCastStart),
												cMath::PlaneToPointDist(cPlanef(0,0,1,0.5f),vRayCastStart));
					
					uniforms.u_fogRayCastStart[0] = vRayCastStart.x;
					uniforms.u_fogRayCastStart[1] = vRayCastStart.y;
					uniforms.u_fogRayCastStart[2] = vRayCastStart.z;

					uniforms.u_fogNegPlaneDistNeg[0] = vNegPlaneDistNeg.x * -1;
					uniforms.u_fogNegPlaneDistNeg[1] = vNegPlaneDistNeg.y * -1;
					uniforms.u_fogNegPlaneDistNeg[2] = vNegPlaneDistNeg.z * -1;

					uniforms.u_fogNegPlaneDistPos[0] = vNegPlaneDistPos.x * -1;
					uniforms.u_fogNegPlaneDistPos[1] = vNegPlaneDistPos.y * -1;
					uniforms.u_fogNegPlaneDistPos[2] = vNegPlaneDistPos.z * -1;

					shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
					shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
					flags |= rendering::detail::FogVariant_UseOutsideBox;
					flags |=  fogArea.m_fogArea->GetShowBacksideWhenOutside() ? rendering::detail::FogVariant_UseBackSide : rendering::detail::FogVariant_None;
				}
				const auto fogColor = fogArea.m_fogArea->GetColor();
				float uniformFogColor[4] = {fogColor.r, fogColor.g, fogColor.b, fogColor.a};

				shaderProgram.m_handle = m_fogVariant.GetVariant(flags);
				
				shaderProgram.m_uniforms.push_back(	{m_u_mtxInvRotation, &rotationMatrix.v});
				
				shaderProgram.m_uniforms.push_back({m_u_param, &uniforms, 3});
				shaderProgram.m_uniforms.push_back({m_u_fogColor, &uniformFogColor});
				
				shaderProgram.m_textures.push_back({m_s_positionMap, sharedData.m_gBufferPositionImage->GetHandle(), 0});
				
				GraphicsContext::DrawRequest drawRequest {layoutStream, shaderProgram};
				context.Submit(view, drawRequest);
			}
		})();

		// ------------------------------------------------------------------------
		// Render Fullscreen Fog Pass
		// ------------------------------------------------------------------------
		if(mpCurrentWorld->GetFogActive()) {
			GraphicsContext::LayoutStream layout;
			cMatrixf projMtx;
			context.ScreenSpaceQuad(layout, projMtx, sharedData.m_size.x, sharedData.m_size.y);
			
			GraphicsContext::ViewConfiguration viewConfig {sharedData.m_output_target};
			viewConfig.m_projection = projMtx;
			viewConfig.m_viewRect = cRect2l(0, 0, sharedData.m_size.x, sharedData.m_size.y);
			const auto view = context.StartPass("Full Screen Fog", viewConfig);
			
			struct {
				float u_fogStart;
				float u_fogLength;
				float u_fogFalloffExp;
			} uniforms = {{0}};

			uniforms.u_fogStart = mpCurrentWorld->GetFogStart();
			uniforms.u_fogLength = mpCurrentWorld->GetFogEnd() - mpCurrentWorld->GetFogStart();
			uniforms.u_fogFalloffExp = mpCurrentWorld->GetFogFalloffExp();
			
			GraphicsContext::ShaderProgram shaderProgram;
			shaderProgram.m_configuration.m_write = Write::RGBA;
			shaderProgram.m_configuration.m_depthTest = DepthTest::Always;

			shaderProgram.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
			shaderProgram.m_configuration.m_write = Write::RGB;

			cMatrixf rotationMatrix = cMatrixf::Identity;
			const auto fogColor = mpCurrentWorld->GetFogColor();
			float uniformFogColor[4] = {fogColor.r, fogColor.g, fogColor.b, fogColor.a};

			shaderProgram.m_handle = m_fogVariant.GetVariant(rendering::detail::FogVariant_None);
			
			shaderProgram.m_textures.push_back({m_s_positionMap, sharedData.m_gBufferPositionImage->GetHandle(), 0});
				
			shaderProgram.m_uniforms.push_back(	{m_u_mtxInvRotation, &rotationMatrix.v});

			shaderProgram.m_uniforms.push_back({m_u_param, &uniforms, 1});
			shaderProgram.m_uniforms.push_back({m_u_fogColor, &uniformFogColor});
			
			GraphicsContext::DrawRequest drawRequest {layout, shaderProgram};
			context.Submit(view, drawRequest);
		}
	
		// notify post draw listeners
		ImmediateDrawBatch postSolidBatch(context, sharedData.m_output_target, frustumView, frustumProj);
		cViewport::PostSolidDrawPacket postSolidEvent = cViewport::PostSolidDrawPacket({
			.m_frustum = mpCurrentFrustum,
			.m_context = &context,
			.m_outputTarget = &sharedData.m_output_target,
			.m_viewport = &viewport,
			.m_renderSettings = mpCurrentSettings,
			.m_immediateDrawBatch = &postSolidBatch,
		});
		viewport.SignalDraw(postSolidEvent);
		postSolidBatch.flush();
		RunCallback(eRendererMessage_PostSolid, handler);

		// not going to even try.
		// calls back through RendererDeffered need to untangle all the complicated state ...

		([&]() {
			auto translucentSpan = mpCurrentRenderList->GetRenderableItems(eRenderListType_Translucent);
			if(translucentSpan.empty()) {
				return;
			}
			
			GraphicsContext::ViewConfiguration viewConfig {sharedData.m_output_target};
			viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			auto view = context.StartPass("Translucent", viewConfig);
			bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
			const float fHalfFovTan = tan(mpCurrentFrustum->GetFOV()*0.5f);
			for(auto& obj: translucentSpan)
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
					for(auto& fogArea: fogRenderData)
					{
						mfTempAlpha *= detail::GetFogAreaVisibilityForObject(fogArea, *mpCurrentFrustum, obj);
					}
				}

				if(!obj->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime)) 
				{
					continue;
				}

				if(!obj->RetrieveOcculsionQuery(this))
				{
					continue;
				}
				
				if(pMaterial->HasRefraction())
				{
					if(!CheckRenderablePlaneIsVisible(obj, mpCurrentFrustum)) {
						continue;
					}

					cBoundingVolume *pBV = obj->GetBoundingVolume();
					cRect2l clipRect = GetClipRectFromObject(obj, 0.2f, mpCurrentFrustum, sharedData.m_size, fHalfFovTan);
					if(clipRect.w <= 0 || clipRect.h <= 0) {
						continue;
					}

					GraphicsContext::ShaderProgram shaderInput;
					shaderInput.m_handle = m_copyRegionProgram;
					shaderInput.m_textures.push_back({m_s_diffuseMap, sharedData.m_outputImage->GetHandle(), 0});
					shaderInput.m_uavImage.push_back({sharedData.m_refractionImage->GetHandle(), 1, 0, bgfx::Access::Write, bgfx::TextureFormat::Enum::RGBA8});

					float copyRegion[4] = {static_cast<float>(clipRect.x), static_cast<float>(sharedData.m_size.y - (clipRect.h + clipRect.y)), static_cast<float>(clipRect.w), static_cast<float>(clipRect.h)};
					shaderInput.m_uniforms.push_back({m_u_copyRegion, &copyRegion, 1});
					
					GraphicsContext::ComputeRequest computeRequest {shaderInput,  static_cast<uint32_t>((clipRect.w/16) + 1), static_cast<uint32_t>((clipRect.h/16) + 1), 1};
					context.Submit(view, computeRequest);
				}

				if(pMaterial->HasWorldReflection() && obj->GetRenderType() == eRenderableType_SubMesh)
				{
					// cBoundingVolume *pBV = obj->GetBoundingVolume();

					// cRect2l clipRect = GetClipRectFromObject(obj, 0.2f, mpCurrentFrustum, mvRenderTargetSize, fHalfFovTan);
					// if(!CheckRenderablePlaneIsVisible(obj, mpCurrentFrustum)) {
					// 	continue;
					// }

					// cRect2l rect = cRect2l(0, 0, mvRenderTargetSize.x, mvRenderTargetSize.y);
					// auto copyImage = resolveRenderImage(m_gBufferColor);
					// context.CopyTextureToFrameBuffer( *copyImage, rect, m_refractionTarget);
				}

				pMaterialType->ResolveShaderProgram(renderMode, viewport, pMaterial, obj, this,[&](GraphicsContext::ShaderProgram& shaderInput) {
					GraphicsContext::LayoutStream layoutInput;
					vertexBuffer->GetLayoutStream(layoutInput);
					
					shaderInput.m_configuration.m_depthTest = pMaterial->GetDepthTest() ? DepthTest::LessEqual: DepthTest::None;
					shaderInput.m_configuration.m_write = Write::RGB;
					shaderInput.m_configuration.m_cull = Cull::None;
					
					// shaderInput.m_uniforms.push_back({m_u_overrideColor, overrideColor });

					// shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					// shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderInput.m_modelTransform = pMatrix ?  pMatrix->GetTranspose() : cMatrixf::Identity;
					
					if(!pMaterial->HasRefraction()) {
						shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
						shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
					}

					GraphicsContext::DrawRequest drawRequest {layoutInput, shaderInput};
					// drawRequest.m_width = mvScreenSize.x;
					// drawRequest.m_height = mvScreenSize.y;
					context.Submit(view, drawRequest);
				});

				if(pMaterial->HasTranslucentIllumination())
				{
					pMaterialType->ResolveShaderProgram(
						(renderMode == eMaterialRenderMode_Diffuse ? eMaterialRenderMode_Illumination : eMaterialRenderMode_IlluminationFog), viewport, pMaterial, obj, this,[&](GraphicsContext::ShaderProgram& shaderInput) {
						GraphicsContext::LayoutStream layoutInput;
						vertexBuffer->GetLayoutStream(layoutInput);

						shaderInput.m_configuration.m_depthTest = pMaterial->GetDepthTest() ? DepthTest::LessEqual: DepthTest::None;
						shaderInput.m_configuration.m_write = Write::RGB;
						shaderInput.m_configuration.m_cull = Cull::None;

						shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);
						shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);

						// shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
						// shaderInput.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
						shaderInput.m_modelTransform = pMatrix ?  pMatrix->GetTranspose() : cMatrixf::Identity;
						
						GraphicsContext::DrawRequest drawRequest {layoutInput, shaderInput};
						// drawRequest.m_width = mvScreenSize.x;
						// drawRequest.m_height = mvScreenSize.y;
						context.Submit(view, drawRequest);
					});
				}
			}
		})();

		RunCallback(eRendererMessage_PostTranslucent, handler);
		
		ImmediateDrawBatch postTransBatch(context, sharedData.m_output_target, frustumView, frustumProj);
		cViewport::PostTranslucenceDrawPacket translucenceEvent = cViewport::PostTranslucenceDrawPacket({
			.m_frustum = mpCurrentFrustum,
			.m_context = &context,
			.m_outputTarget = &sharedData.m_output_target,
			.m_viewport = &viewport,
			.m_renderSettings = mpCurrentSettings,
			.m_immediateDrawBatch = &postTransBatch,
		});
		viewport.SignalDraw(translucenceEvent);
		postTransBatch.flush();
	
	}

	void cRendererDeferred::RenderShadowPass(GraphicsContext& context, cViewport& viewport, const cDeferredLight& apLightData, RenderTarget& rt) {
 		BX_ASSERT(apLightData.mpLight->GetLightType() == eLightType_Spot, "Only spot lights are supported for shadow rendering")
		cVector2l size = rt.GetImage()->GetImageSize();
		cLightSpot *pSpotLight = static_cast<cLightSpot*>(apLightData.mpLight);
		cFrustum *pLightFrustum = pSpotLight->GetFrustum();

		GraphicsContext::ViewConfiguration shadowPassViewConfig {rt};
		shadowPassViewConfig.m_clear = {0, 1.0, 0, ClearOp::Depth};
		shadowPassViewConfig.m_view = pLightFrustum->GetViewMatrix().GetTranspose();
		shadowPassViewConfig.m_projection = pLightFrustum->GetProjectionMatrix().GetTranspose();
		shadowPassViewConfig.m_viewRect = {0, 0, static_cast<uint16_t>(size.x), static_cast<uint16_t>(size.y)};
		bgfx::ViewId view = context.StartPass("Shadow Pass", shadowPassViewConfig);
		for(auto& shadowCaster: mvShadowCasters)
		{
			rendering::detail::RenderZPassObject(view, context, viewport, this, shadowCaster, pLightFrustum->GetInvertsCullMode() ? Cull::Clockwise : Cull::CounterClockwise);
		}
	}

	void cRendererDeferred::RenderLightPass(GraphicsContext& context, cViewport& viewport, RenderTarget& rt) {
		InitLightRendering();
		auto& sharedData = m_boundViewportData.resolve(viewport);

		{
			GraphicsContext::ViewConfiguration clearBackBufferView {rt};
			clearBackBufferView.m_clear = {0, 1, 0, ClearOp::Color};
			clearBackBufferView.m_viewRect = {0, 0, static_cast<uint16_t>(sharedData.m_size.x), static_cast<uint16_t>(sharedData.m_size.y)};
			bgfx::touch(context.StartPass("Clear Backbuffer", clearBackBufferView));
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
				shaderProgram.m_textures.push_back({m_s_diffuseMap, sharedData.m_gBufferColor->GetHandle(), 0});
				shaderProgram.m_uniforms.push_back({m_u_lightColor, lightColor});

				// shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
				// shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
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

				GraphicsContext::DrawRequest drawRequest {layoutStream, shaderProgram};
				context.Submit(view, drawRequest);
			};


			GraphicsContext::ViewConfiguration stencilFrontBackViewConfig {rt};
			stencilFrontBackViewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			stencilFrontBackViewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			stencilFrontBackViewConfig.m_clear = {0, 1.0, 0, ClearOp::Stencil};
			stencilFrontBackViewConfig.m_viewRect = {0, 0, static_cast<uint16_t>(sharedData.m_size.x), static_cast<uint16_t>(sharedData.m_size.y)};
			const auto boxStencilPass = context.StartPass("eDeferredLightList_Box_StencilFront_RenderBack", stencilFrontBackViewConfig);
			bgfx::setViewMode(boxStencilPass, bgfx::ViewMode::Sequential);

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
		
					shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(), light->GetLightMtx()).GetTranspose();

					GraphicsContext::DrawRequest drawRequest {layoutStream, shaderProgram};
					// drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
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

			GraphicsContext::ViewConfiguration boxLightConfig {rt};
			boxLightConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			boxLightConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			boxLightConfig.m_viewRect = {0, 0, static_cast<uint16_t>(sharedData.m_size.x), static_cast<uint16_t>(sharedData.m_size.y)};
			const auto boxLightBackPass = context.StartPass("eDeferredLightList_Box_RenderBack", boxLightConfig);
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
				GraphicsContext::DrawRequest drawRequest {layoutStream, shaderProgram};
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
						shaderProgram.m_uniforms.push_back({m_u_mtxInvViewRotation, mtxInvViewRotation.v});

						shaderProgram.m_textures.push_back({m_s_diffuseMap, sharedData.m_gBufferColor->GetHandle(), 1});
						shaderProgram.m_textures.push_back({m_s_normalMap, sharedData.m_gBufferNormalImage->GetHandle(), 2});
						shaderProgram.m_textures.push_back({m_s_positionMap, sharedData.m_gBufferPositionImage->GetHandle(), 3});
						shaderProgram.m_textures.push_back({m_s_specularMap, sharedData.m_gBufferSpecular->GetHandle(), 4});
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
							float shadowMapOffset[2];
							float pad;
						} uParam = {
							apLightData->mpLight->GetRadius(),
							{vForward.x, vForward.y, vForward.z},
							1 - pLightSpot->GetCosHalfFOV(),
							{0 , 0},
							0

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
						shaderProgram.m_uniforms.push_back({m_u_spotViewProj, &spotViewProj.v});

						
						shaderProgram.m_textures.push_back({m_s_diffuseMap, sharedData.m_gBufferColor->GetHandle(), 0});
						shaderProgram.m_textures.push_back({m_s_normalMap, sharedData.m_gBufferNormalImage->GetHandle(), 1});
						shaderProgram.m_textures.push_back({m_s_positionMap, sharedData.m_gBufferPositionImage->GetHandle(), 2});
						shaderProgram.m_textures.push_back({m_s_specularMap, sharedData.m_gBufferSpecular->GetHandle(), 3});
					
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
								RenderShadowPass(context, viewport, *apLightData, pShadowData->m_target);
							}
							const auto imageSize = pShadowData->m_target.GetImage()->GetImageSize();
							uParam.shadowMapOffset[0] = 1.0f / imageSize.x ;
							uParam.shadowMapOffset[1] = 1.0f / imageSize.y ;
							if(m_shadowJitterImage) {
								shaderProgram.m_textures.push_back({m_s_shadowOffsetMap, m_shadowJitterImage->GetHandle(), 7});
							}
							shaderProgram.m_textures.push_back({m_s_shadowMap, pShadowData->m_target.GetImage()->GetHandle(), 6});
						}
						shaderProgram.m_uniforms.push_back({m_u_param, &uParam, 2});
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

			GraphicsContext::ViewConfiguration viewConfig {rt};
			viewConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			viewConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			viewConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			const auto lightStencilBackPass = context.StartPass("eDeferredLightList_StencilFront_RenderBack", viewConfig);
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
					
					// shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
					// shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
					shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(), light->GetLightMtx()).GetTranspose();
					
					shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
						StencilFunction::Always,
						StencilFail::Keep, 
						StencilDepthFail::Replace, 
						StencilDepthPass::Keep,
						0xff, 0xff);
				
					GraphicsContext::DrawRequest drawRequest {layoutStream, shaderProgram};
					// drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
					// drawRequest.m_width = mvScreenSize.x;
					// drawRequest.m_height = mvScreenSize.y;
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
					
					shaderProgram.m_modelTransform = cMath::MatrixMul(light->mpLight->GetWorldMatrix(),light->GetLightMtx()).GetTranspose();
					
					drawLight(lightStencilBackPass, shaderProgram, light);
				}
			}

			GraphicsContext::ViewConfiguration lightBackPassConfig {rt};
			lightBackPassConfig.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
			lightBackPassConfig.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
			lightBackPassConfig.m_viewRect = {0, 0, sharedData.m_size.x, sharedData.m_size.y};
			const auto lightBackPass = context.StartPass("eDeferredLightList_RenderBack", lightBackPassConfig);
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
				
				// shaderProgram.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
				// shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
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
			return m_shapeSphere[aQuality].get();
		}
		///////////////////
		// Spot Light
		else if(apLight->GetLightType() == eLightType_Spot)
		{
			return m_shapePyramid.get();
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
		auto rtSize = rt.GetImage()->GetImageSize();

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
		float fScreenArea = (float)(rtSize.x*rtSize.y);

		mlMinLargeLightArea =	(int)(mfMinLargeLightNormalizedArea * fScreenArea);
		// GraphicsContext::ViewConfiguration lightBackPassConfig {rt};
		// auto view = context.StartPass("Light Render Queries");

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
																	mpCurrentFrustum,rtSize,
																	true,mfScissorLastTanHalfFov);
			}
			//Spot
			else if(lightType == eLightType_Spot)
			{
				cMath::GetClipRectFromBV(	pLightData->mClipRect, *pLight->GetBoundingVolume(), mpCurrentFrustum,
											rtSize, mfScissorLastTanHalfFov);
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
					if(fDistToLight > m_shadowDistanceNone)
					{
						pLightData->mbCastShadows = false;
					}
					///////////////////////
					//Use Low
					else if(fDistToLight > m_shadowDistanceLow)
					{
						if(pLightData->mShadowResolution == eShadowMapResolution_Low)
							pLightData->mbCastShadows = false;
						pLightData->mShadowResolution = eShadowMapResolution_Low;
					}
					///////////////////////
					//Use Medium
					else if(fDistToLight > m_shadowDistanceMedium)
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
