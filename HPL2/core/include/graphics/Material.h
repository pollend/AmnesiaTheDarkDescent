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

#ifndef HPL_MATERIAL_H
#define HPL_MATERIAL_H

#include "engine/Event.h"
#include "graphics/AnimatedImage.h"
#include "resources/Resources.h"
#include <graphics/Image.h>
#include <graphics/ImageResourceWrapper.h>
#include <system/SystemTypes.h>
#include <math/MathTypes.h>
#include <graphics/GraphicsTypes.h>
#include <resources/ResourceBase.h>

namespace hpl {

	//---------------------------------------------------

	class cGraphics;
	class cResources;
	class iTexture;
	class iMaterialType;
	class cResourceVarsObject;


	class iMaterialVars
	{
	public:
		virtual ~iMaterialVars(){}
	};


	//---------------------------------------------------

	class cMaterialUvAnimation
	{
	public:
		cMaterialUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis) :
							  mType(aType), mfSpeed(afSpeed), mfAmp(afAmp), mAxis(aAxis) {}

	    eMaterialUvAnimation mType;

		float mfSpeed;
		float mfAmp;

		eMaterialAnimationAxis mAxis;
	};


	class cMaterial : public iResourceBase
	{
	friend class iMaterialType;
	public:
		static constexpr uint32_t MaxMaterialID = 2048;

		enum MaterialID: uint8_t {
            SolidDiffuse,
            Translucent,
            Water,
            Decal,
            MaterialIDCount
        };

		enum TextureConfigFlags {
			EnableDiffuse = 1 << 0,
			EnableNormal = 1 << 1,
			EnableSpecular = 1 << 2,
			EnableAlpha = 1 << 3,
			EnableHeight = 1 << 4,
			EnableIllumination = 1 << 5,
			EnableCubeMap = 1 << 6,
			EnableDissolveAlpha = 1 << 7,
			EnableDissolveAlphaFilter = 1 << 8,

			// additional flags
			//Solid Diffuse
			UseDissolveFilter = 1 << 9,
			IsAlphaSingleChannel = 1 << 10,
			IsHeightMapSingleChannel = 1 << 11,
		};

		struct MaterialCommonBlock {
			uint32_t m_textureConfig;
		};

		struct MaterialSolidUniformBlock {
			MaterialCommonBlock m_common;
			float m_heightMapScale;
			float m_heightMapBias;
			float m_frenselBias;
			float m_frenselPow;
		};

		struct MaterialTranslucentUniformBlock {
			MaterialCommonBlock m_common;

			// bool mbAffectedByLightLevel;
			// bool mbRefraction;
			// bool mbRefractionEdgeCheck;
			// bool mbRefractionNormals;
			float mfRefractionScale;
			float mfFrenselBias;
			float mfFrenselPow;
			float mfRimLightMul;
			float mfRimLightPow;
		};

		struct MaterialWaterUniformBlock {
			MaterialCommonBlock m_common;

			// bool mbHasReflection;
			float mfRefractionScale;
			float mfFrenselBias;
			float mfFrenselPow;
			float mfReflectionFadeStart;
			float mfReflectionFadeEnd;
			float mfWaveSpeed;
			float mfWaveAmplitude;
			float mfWaveFreq;
		};

		
		struct MaterialType {
			
			MaterialID m_id;
			// solid
			bool m_alphaDissolveFilter;
			union MaterialData {
				MaterialData() {}
				MaterialCommonBlock m_common;
				MaterialSolidUniformBlock m_solid;
				MaterialTranslucentUniformBlock m_translucentUniformBlock;
				MaterialWaterUniformBlock m_waterUniformBlock;
			} m_data;
		};

		struct MaterialUserVariable {
			const char* m_name;
			const char* m_type;
			const char* m_defaultValue;
		};

		struct MaterialMeta {
			MaterialID m_id;
			const char* m_name;
			bool m_isDecal ;
			bool m_isTranslucent;
			std::span<const eMaterialTexture> m_usedTextures;
			std::span<const MaterialUserVariable> m_userVariables;
		};
	

		static constexpr eMaterialTexture SolidMaterialUsedTextures[] = {
			eMaterialTexture_Diffuse ,eMaterialTexture_NMap ,eMaterialTexture_Alpha ,eMaterialTexture_Specular ,eMaterialTexture_Height ,eMaterialTexture_Illumination ,eMaterialTexture_DissolveAlpha ,eMaterialTexture_CubeMap ,eMaterialTexture_CubeMapAlpha
		};
		static constexpr eMaterialTexture TranslucentMaterialUsedTextures[] = {
			eMaterialTexture_Diffuse, eMaterialTexture_NMap, eMaterialTexture_CubeMap, eMaterialTexture_CubeMapAlpha
		};
		static constexpr eMaterialTexture WaterMaterialUsedTextures[] = {
			eMaterialTexture_Diffuse, eMaterialTexture_NMap, eMaterialTexture_CubeMap
		};
		static constexpr eMaterialTexture DecalMaterialUsedTextures[] = {
			eMaterialTexture_Diffuse
		};
 
		static constexpr const std::array MetaInfo {
			MaterialMeta {MaterialID::SolidDiffuse,"soliddiffuse", false, false, std::span(SolidMaterialUsedTextures)},
			MaterialMeta {MaterialID::Translucent,"translucent", false, true, std::span(TranslucentMaterialUsedTextures)},
			MaterialMeta {MaterialID::Water,"water", false, true, std::span(WaterMaterialUsedTextures)},
			MaterialMeta {MaterialID::Decal,"decal", true, true, std::span(DecalMaterialUsedTextures)}
		};

		cMaterial(const tString& asName, const tWString& asFullPath, cGraphics *apGraphics, cResources *apResources, iMaterialType *apType);
		virtual ~cMaterial();

		void SetType(iMaterialType* apType);
		iMaterialType * GetType(){ return mpType; }

		void Compile();
		void SetImage(eMaterialTexture aType, iResourceBase *apTexture);
		Image *GetImage(eMaterialTexture aType);

		void SetVars(iMaterialVars *apVars){ mpVars = apVars;}
		iMaterialVars* GetVars(){ return mpVars;}
		cResourceVarsObject* GetVarsObject();
		void LoadVariablesFromVarsObject(cResourceVarsObject* apVarsObject);

		void SetAutoDestroyTextures(bool abX) { 
			mbAutoDestroyTextures = abX;
			for(auto& image: m_image) {
				image.SetAutoDestroyResource(abX);
			}
		}

		void SetBlendMode(eMaterialBlendMode aBlendMode);
		void SetAlphaMode(eMaterialAlphaMode aAlphaMode);
		void SetDepthTest(bool abDepthTest);

		bool HasRefraction(){ return mbHasRefraction; }
		bool UseRefractionEdgeCheck(){ return mbUseRefractionEdgeCheck;}
		void SetHasRefraction(bool abX){ mbHasRefraction = abX; }
		void SetUseRefractionEdgeCheck(bool abX){ mbUseRefractionEdgeCheck = abX;}

		bool HasWorldReflection(){ return mbHasWorldReflection; }
		void SetHasWorldReflection(bool abX){ mbHasWorldReflection = abX; }
		void  SetWorldReflectionOcclusionTest(bool abX){ mbWorldReflectionOcclusionTest=abX;}
		void SetMaxReflectionDistance(float afX){ mfMaxReflectionDistance = afX;}
		bool  GetWorldReflectionOcclusionTest(){ return mbWorldReflectionOcclusionTest;}
		float GetMaxReflectionDistance(){ return mfMaxReflectionDistance;}

		void SetHasTranslucentIllumination(bool abX){ mbHasTranslucentIllumination = abX;}
		bool HasTranslucentIllumination(){ return mbHasTranslucentIllumination;}

		void SetLargeTransperantSurface(bool abX){ mbLargeTransperantSurface = abX;}
		bool GetLargeTransperantSurface(){ return mbLargeTransperantSurface;}

		bool GetUseAlphaDissolveFilter(){ return mbUseAlphaDissolveFilter;}
		void SetUseAlphaDissolveFilter(bool abX){ mbUseAlphaDissolveFilter = abX;}

		void SetAffectedByFog(bool abX){ mbAffectedByFog = abX;}
		bool GetAffectedByFog(){ return mbAffectedByFog;}

		inline eMaterialBlendMode GetBlendMode() const { return mBlendMode; }
		inline eMaterialAlphaMode GetAlphaMode() const { return mAlphaMode; }
		inline bool GetDepthTest() const { return mbDepthTest; }

		void SetPhysicsMaterial(const tString & asPhysicsMaterial){ msPhysicsMaterial = asPhysicsMaterial;}
		const tString& GetPhysicsMaterial(){ return msPhysicsMaterial;}

		void UpdateBeforeRendering(float afTimeStep);

		inline int GetRenderFrameCount() const { return mlRenderFrameCount;}
		inline void SetRenderFrameCount(const int alCount) { mlRenderFrameCount = alCount;}

		inline bool GetHasSpecificSettings(eMaterialRenderMode aMode) const{ return mbHasSpecificSettings[aMode];}
		void SetHasSpecificSettings(eMaterialRenderMode aMode,bool abX){ mbHasSpecificSettings[aMode] = abX;}

		inline bool HasObjectSpecificsSettings(eMaterialRenderMode aMode)const { return  mbHasObjectSpecificsSettings[aMode];}
		void SetHasObjectSpecificsSettings(eMaterialRenderMode aMode,bool abX){ mbHasObjectSpecificsSettings[aMode] = abX;}

		//Animation
		void AddUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis);
		int GetUvAnimationNum(){ return (int)mvUvAnimations.size();}
		cMaterialUvAnimation *GetUvAnimation(int alIdx){ return &mvUvAnimations[alIdx]; }
		inline bool HasUvAnimation()const { return mbHasUvAnimation; }
		inline const cMatrixf& GetUvMatrix() const { return m_mtxUV;}
		void ClearUvAnimations();

		/**
		 * This is used so that materials do not call type specific things after types have been destroyed!
		 * Shall only be set by graphics!
		 */
		static void SetDestroyTypeSpecifics(bool abX){ mbDestroyTypeSpecifics = abX; }
		static bool GetDestroyTypeSpecifics(){ return mbDestroyTypeSpecifics; }

		//resources stuff.
		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}
	

		inline MaterialType& type() { return m_info; }
		bool updateDescriptorSet(const ForgeRenderer::Frame& frame, eMaterialRenderMode mode, DescriptorSet* set);

		// inline ForgeBufferHandle& uniformHandle() { return m_bufferHandle; }
		inline uint32_t materialID() { return m_materialID; }
		inline uint32_t Version() { return m_version; }
	private:
		void UpdateFlags();
		void UpdateAnimations(float afTimeStep);
		uint32_t m_materialID = 0;
		uint32_t m_version = 0; // used to check if the material has changed since last frame

		// std::array<ForgeBufferHandle,  ForgeRenderer::SwapChainLength> m_materialBuffer;
		MaterialType m_info;

		cGraphics *mpGraphics;
		cResources *mpResources;
		iMaterialType *mpType;
		iMaterialVars *mpVars;

		bool mbAutoDestroyTextures;
		bool mbHasSpecificSettings[eMaterialRenderMode_LastEnum];
		bool mbHasObjectSpecificsSettings[eMaterialRenderMode_LastEnum];

		eMaterialBlendMode mBlendMode;
		eMaterialAlphaMode mAlphaMode;
		int mlRefractionTextureUnit;

		int mlWorldReflectionTextureUnit;
		float mfMaxReflectionDistance;

		bool mbDepthTest;
		bool mbHasRefraction;
		bool mbUseRefractionEdgeCheck;
		bool mbHasWorldReflection;
		bool mbWorldReflectionOcclusionTest;
		bool mbHasTranslucentIllumination;
		bool mbLargeTransperantSurface;
		bool mbAffectedByFog;
		bool mbUseAlphaDissolveFilter;
		bool mbHasUvAnimation;

		std::array<ImageResourceWrapper, eMaterialTexture_LastEnum> m_image = {ImageResourceWrapper()};

		std::vector<cMaterialUvAnimation> mvUvAnimations;
		cMatrixf m_mtxUV;
		
		float mfAnimTime;

		int mlRenderFrameCount;

		tString msPhysicsMaterial;

		static bool mbDestroyTypeSpecifics;
		uint32_t m_stageDirtyBits = 0;
	};

	//---------------------------------------------------

};
#endif // HPL_MATERIAL_H
