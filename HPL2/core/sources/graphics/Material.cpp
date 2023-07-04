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

#include "graphics/Material.h"

#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/ImageManager.h"

#include "graphics/MaterialType.h"
#include "graphics/Renderable.h"

#include "math/Math.h"
#include <utility>
#include <bx/debug.h>



#include "tinyimageformat_query.h"

namespace hpl {

	namespace internal {
        static size_t m_id = 0;
        static std::vector<size_t> m_freelist;
	}

	bool cMaterial::mbDestroyTypeSpecifics = true;

	cMaterial::cMaterial(const tString& asName, const tWString& asFullPath, cGraphics *apGraphics, cResources *apResources, iMaterialType *apType)
		: iResourceBase(asName, asFullPath, 0)
	{
		if(internal::m_freelist.size() > 0) {
			m_materialID = internal::m_freelist.back();
			internal::m_freelist.pop_back();
		} else {
			m_materialID = internal::m_id++;
			ASSERT(m_materialID < MaxMaterialID && "Too many materials created");
		}

		mpGraphics = apGraphics;
		mpResources = apResources;

		mpType = NULL;
		mpVars = NULL;
		SetType(apType);

		mbAutoDestroyTextures = true;

		mlRenderFrameCount = -1;

		mbHasRefraction = false;
		mlRefractionTextureUnit =0;
		mbUseRefractionEdgeCheck = false;

		mbHasWorldReflection = false;
		mlWorldReflectionTextureUnit =0;
		mbWorldReflectionOcclusionTest = true;
		mfMaxReflectionDistance = 0;

		mbHasTranslucentIllumination = false; //If the material is translucent and also need an extra additive pass.

		mbLargeTransperantSurface = false;

		mbUseAlphaDissolveFilter = false;

		mbAffectedByFog = true;

		for(int i=0; i<eMaterialRenderMode_LastEnum; ++i)
		{
			mbHasSpecificSettings[i] = false;
			mbHasObjectSpecificsSettings[i] = false;
		}

		mfAnimTime = 0;
		m_mtxUV = cMatrixf::Identity;
		mbHasUvAnimation = false;


		///////////////////////
		//Set up depending in type
		if(mpType->IsTranslucent())
		{
			mBlendMode = eMaterialBlendMode_Add;
		}
		else
		{
			mBlendMode = eMaterialBlendMode_None;
		}
		mAlphaMode = eMaterialAlphaMode_Solid;
		mbDepthTest = true;

	}

	//-----------------------------------------------------------------------

	cMaterial::~cMaterial()
	{
		internal::m_freelist.push_back(m_materialID);
		m_materialID = 0;
		if(mpVars) {
			hplDelete(mpVars);
		}
	}

	void cMaterial::SetType(iMaterialType* apType)
	{
		if(mpType == apType) {
			return;
		}

		mpType = apType;

		if(mpVars) {
			hplDelete(mpVars);
		}
		if(mpType) {
			mpVars = mpType->CreateSpecificVariables();
		}
	}

	//-----------------------------------------------------------------------

	void cMaterial::Compile()
	{
		for(int i=0; i<eMaterialRenderMode_LastEnum; ++i)
		{
			mbHasSpecificSettings[i] = false;
			mbHasObjectSpecificsSettings[i] = false;
		}

		mpType->CompileMaterialSpecifics(this);
		UpdateFlags();
	}


	void cMaterial::UpdateFlags() {
		const auto alphaMapImage = GetImage(eMaterialTexture_Alpha);
		const auto heightMapImage = GetImage(eMaterialTexture_Height);

	    m_info.m_data.m_common.m_textureConfig =
					(GetImage(eMaterialTexture_Diffuse) ? EnableDiffuse: 0) |
					(GetImage(eMaterialTexture_NMap) ? EnableNormal: 0) |
 					(GetImage(eMaterialTexture_Specular) ? EnableSpecular: 0) |
					(alphaMapImage ? EnableAlpha: 0) |
					(heightMapImage ? EnableHeight: 0) |
					(GetImage(eMaterialTexture_Illumination) ? EnableIllumination: 0) |
					(GetImage(eMaterialTexture_CubeMap) ? EnableCubeMap: 0) |
					(GetImage(eMaterialTexture_DissolveAlpha) ? EnableDissolveAlpha: 0) |
					(GetImage(eMaterialTexture_CubeMapAlpha) ? EnableCubeMapAlpha: 0) |
					(m_info.m_alphaDissolveFilter ? UseDissolveFilter: 0);
		switch(m_info.m_id) {
			case MaterialID::SolidDiffuse: {
				m_info.m_data.m_common.m_textureConfig |=
					((alphaMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(alphaMapImage->GetTexture().m_handle->mFormat)) == 1) ? IsAlphaSingleChannel: 0) |
					((heightMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(heightMapImage->GetTexture().m_handle->mFormat)) == 1) ? IsHeightMapSingleChannel: 0);
				break;
			}
			case MaterialID::Translucent: {
				m_info.m_data.m_common.m_textureConfig |=
					(HasRefraction() ? UseRefractionNormals: 0)  |
					(IsRefractionEdgeCheck() ? UseRefractionEdgeCheck: 0);
			    break;
			}
			default:
				break;
		}
	}

	void cMaterial::SetImage(eMaterialTexture aType, iResourceBase *apTexture)
	{
		// increase version number to dirty material
		m_version++;
		UpdateFlags();
		m_image[aType].SetAutoDestroyResource(false);
		if(apTexture) {
			ASSERT(TypeInfo<Image>().IsType(*apTexture) || TypeInfo<AnimatedImage>().IsType(*apTexture) && "cMaterial::SetImage: apTexture is not an Image");
			m_image[aType] = std::move(ImageResourceWrapper(mpResources->GetTextureManager(), apTexture, mbAutoDestroyTextures));
		} else {
			m_image[aType] = ImageResourceWrapper();
		}
	}

	Image* cMaterial::GetImage(eMaterialTexture aType)
	{
		return m_image[aType].GetImage();
	}

	cResourceVarsObject* cMaterial::GetVarsObject()
	{
		cResourceVarsObject* pVarsObject = hplNew(cResourceVarsObject,());
		mpType->GetVariableValues(this, pVarsObject);

		return pVarsObject;
	}

	void cMaterial::LoadVariablesFromVarsObject(cResourceVarsObject* apVarsObject)
	{
		mpType->LoadVariables(this, apVarsObject);
	}

	void cMaterial::SetBlendMode(eMaterialBlendMode aBlendMode)
	{
		if(mpType->IsTranslucent()==false) return;

		mBlendMode = aBlendMode;
	}

	void cMaterial::SetAlphaMode(eMaterialAlphaMode aAlphaMode)
	{
		//if(mpType->IsTranslucent()) return;

		mAlphaMode = aAlphaMode;
	}

	void cMaterial::SetDepthTest(bool abDepthTest)
	{
		if(mpType->IsTranslucent()==false) return;

		mbDepthTest = abDepthTest;
	}

	//-----------------------------------------------------------------------

	void cMaterial::UpdateBeforeRendering(float afTimeStep)
	{
		if(mbHasUvAnimation) UpdateAnimations(afTimeStep);
	}

	//-----------------------------------------------------------------------

	void cMaterial::AddUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis)
	{
		mvUvAnimations.push_back(cMaterialUvAnimation(aType, afSpeed, afAmp, aAxis));

		mbHasUvAnimation = true;
	}

	//-----------------------------------------------------------------------

	void cMaterial::ClearUvAnimations()
	{
		mvUvAnimations.clear();

		mbHasUvAnimation = false;

		m_mtxUV = cMatrixf::Identity;
	}

	static cVector3f GetAxisVector(eMaterialAnimationAxis aAxis)
	{
		switch(aAxis)
		{
		case eMaterialAnimationAxis_X: return cVector3f(1,0,0);
		case eMaterialAnimationAxis_Y: return cVector3f(0,1,0);
		case eMaterialAnimationAxis_Z: return cVector3f(0,0,1);
		}
		return 0;
	}


	// bool cMaterial::updateDescriptorSet(const ForgeRenderer::Frame& frame, eMaterialRenderMode mode, DescriptorSet* descriptorSet) {
	// 	// if(m_descriptorSets[mode] != descriptorSet) {
	// 	// 	m_stageDirtyBits |= (1 << mode); // mark stage as dirty
	// 	// 	m_descriptorSets[mode] = descriptorSet;
	// 	// }

	// 	if((m_stageDirtyBits & (1 << mode))) {
	// 		m_stageDirtyBits &= ~(1 << mode); // clear dirty bit

	// 		switch(m_info.m_id) {
	// 			case SolidDiffuse: {
	// 				switch(mode) {
	// 					case eMaterialRenderMode_Z: {
	// 						std::array<DescriptorData, 2> descriptorData{};
	// 						descriptorData[0].pName = "uniformMaterialBlock";
	// 						descriptorData[0].ppBuffers = &m_bufferHandle[frame.m_frameIndex].m_handle;

	// 						return true;

	// 						break;
	// 					}
	// 					case eMaterialRenderMode_Z_Dissolve:
	// 						break;
	// 					case eMaterialRenderMode_Diffuse:
	// 						break;
	// 					case eMaterialRenderMode_DiffuseFog:
	// 						break;
	// 					case eMaterialRenderMode_Light:
	// 						break;
	// 					case eMaterialRenderMode_Illumination:
	// 						break;
	// 					case eMaterialRenderMode_IlluminationFog:
	// 						break;
	// 					default:
	// 						break;
	// 				}
	// 				break;
	// 			}
	// 			case Translucent: {
	// 				break;
	// 			}
	// 			case Water: {
	// 				break;
	// 			}
	// 			case Decal: {
	// 				break;
	// 			}
	// 			default: {
	// 				ASSERT(false && "Unknown material type");
	// 				break;
	// 			}
	// 		}
	// 	}
	// 	return false;
	// }

	void cMaterial::UpdateAnimations(float afTimeStep) {
		m_mtxUV = cMatrixf::Identity;

        for(size_t i=0; i<mvUvAnimations.size(); ++i)
		{
			cMaterialUvAnimation *pAnim = &mvUvAnimations[i];

			///////////////////////////
			// Translate
			if(pAnim->mType == eMaterialUvAnimation_Translate)
			{
				cVector3f vDir = GetAxisVector(pAnim->mAxis);

				cMatrixf mtxAdd = cMath::MatrixTranslate(vDir * pAnim->mfSpeed * mfAnimTime);
				m_mtxUV = cMath::MatrixMul(m_mtxUV, mtxAdd);
			}
			///////////////////////////
			// Sin
			else if(pAnim->mType == eMaterialUvAnimation_Sin)
			{
				cVector3f vDir = GetAxisVector(pAnim->mAxis);

				cMatrixf mtxAdd = cMath::MatrixTranslate(vDir * sin(mfAnimTime * pAnim->mfSpeed) * pAnim->mfAmp);
				m_mtxUV = cMath::MatrixMul(m_mtxUV, mtxAdd);
			}
			///////////////////////////
			// Rotate
			else if(pAnim->mType == eMaterialUvAnimation_Rotate)
			{
				cVector3f vDir = GetAxisVector(pAnim->mAxis);

				cMatrixf mtxRot = cMath::MatrixRotate(vDir * pAnim->mfSpeed * mfAnimTime,eEulerRotationOrder_XYZ);
				m_mtxUV = cMath::MatrixMul(m_mtxUV, mtxRot);
			}
		}

		// auto frame = Interface<ForgeRenderer>::Get()->GetFrame();
		// auto& handle = m_bufferHandle[frame.m_frameIndex];
		// BufferUpdateDesc uniformUpdate = { handle.m_handle};
		// beginUpdateResource(&uniformUpdate);
		// reinterpret_cast<MaterialCommonBlock*>(uniformUpdate.pMappedData)->m_textureMatrix = cMath::ToForgeMat4(m_mtxUV);
		// endUpdateResource(&uniformUpdate, nullptr);

		m_version++;
		mfAnimTime += afTimeStep;
	}

	//-----------------------------------------------------------------------

}
