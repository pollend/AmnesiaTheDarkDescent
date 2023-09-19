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

#include "engine/Event.h"
#include "graphics/AnimatedImage.h"
#include "graphics/IndexPool.h"
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


    struct MaterialDecal {
        eMaterialBlendMode m_blend;
    };
    struct MaterialDiffuseSolid {
        float m_heightMapScale;
        float m_heightMapBias;
        float m_frenselBias;
        float m_frenselPow;
        bool m_alphaDissolveFilter;
    };

    struct MaterialTranslucent {
        eMaterialBlendMode m_blend;

        bool m_isAffectedByLightLevel;
        bool m_hasRefraction;
        bool m_refractionEdgeCheck;
        bool m_refractionNormals;
        bool m_affectedByLightLevel;

        float m_refractionScale;
        float m_frenselBias;
        float m_frenselPow;
        float m_rimLightMul;
        float m_rimLightPow;
    };

    // always has refraction if enabled
    struct MaterialWater {
        bool m_hasReflection;
        bool m_isLargeSurface;
        float m_maxReflectionDistance;

        float m_refractionScale;
        float m_frenselBias;
        float mfFrenselPow;
        float mfReflectionFadeStart;
        float mfReflectionFadeEnd;
        float mfWaveSpeed;
        float mfWaveAmplitude;
        float mfWaveFreq;
    };

    enum class MaterialID : uint8_t { Unknown = 0, SolidDiffuse, Translucent, Water, Decal, MaterialIDCount };

    struct MaterialDescriptor final {
        MaterialID m_id = MaterialID::Unknown;
        union {
            MaterialDecal m_decal;
            MaterialDiffuseSolid m_solid;
            MaterialTranslucent m_translucent;
            MaterialWater m_water;
        };
    };


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


	class cMaterial : public iResourceBase {
	friend class iMaterialType;
	public:
		static constexpr uint32_t MaxMaterialID = 2048;

        enum TextureAntistropy {
            Antistropy_None = 0,
            Antistropy_8 = 1,
            Antistropy_16 = 2,
            Antistropy_Count = 3
        };

		struct MaterialCommonBlock {
			uint32_t m_materialConfig;
		};

		struct MaterialSolidUniformBlock {
			uint32_t m_materialConfig;
			float m_heightMapScale;
			float m_heightMapBias;
			float m_frenselBias;

			float m_frenselPow;
			uint32_t m_pad0;
			uint32_t m_pad1;
			uint32_t m_pad2;
		};

		struct MaterialTranslucentUniformBlock {
			uint32_t m_materialConfig;
			float mfRefractionScale;
			float mfFrenselBias;
			float mfFrenselPow;

			float mfRimLightMul;
			float mfRimLightPow;
			uint32_t m_pad0;
			uint32_t m_pad1;
		};

		struct MaterialWaterUniformBlock {
			uint32_t m_materialConfig;
			float mfRefractionScale;
			float mfFrenselBias;
			float mfFrenselPow;

			float mfReflectionFadeStart;
			float mfReflectionFadeEnd;
			float mfWaveSpeed;
			float mfWaveAmplitude;

			float mfWaveFreq;
			uint32_t m_pad0;
			uint32_t m_pad1;
			uint32_t m_pad2;
		};


		struct MaterialType {
			//
			bool m_affectByLightLevel = false;

			MaterialID m_id;
			IndexPoolHandle m_handle;
			// solid
			bool m_alphaDissolveFilter = false;
			bool m_refractionNormals = false;
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

		static constexpr const std::array MaterialMetaTable {
			MaterialMeta { MaterialID::SolidDiffuse,"soliddiffuse", false, false, std::span(SolidMaterialUsedTextures)},
			MaterialMeta { MaterialID::Translucent,"translucent", false, true, std::span(TranslucentMaterialUsedTextures)},
			MaterialMeta { MaterialID::Water,"water", false, true, std::span(WaterMaterialUsedTextures)},
			MaterialMeta { MaterialID::Decal,"decal", true, true, std::span(DecalMaterialUsedTextures)}
		};

		cMaterial(const tString& asName, const tWString& asFullPath, cGraphics *apGraphics, cResources *apResources, iMaterialType *apType);
		virtual ~cMaterial();

		void SetType(iMaterialType* apType);
		iMaterialType * GetType(){ return mpType; }

		void Compile();
		void SetImage(eMaterialTexture aType, iResourceBase *apTexture);
		Image* GetImage(eMaterialTexture aType);
		const Image* GetImage(eMaterialTexture aType) const;

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

		void SetTextureAnisotropy(float afx);
		void SetBlendMode(eMaterialBlendMode aBlendMode);
		void SetAlphaMode(eMaterialAlphaMode aAlphaMode);
		void SetDepthTest(bool abDepthTest);

		bool HasRefraction(){ return mbHasRefraction; }
		bool HasRefractionNormals(){ return mbHasRefractionNormals; }
		bool IsRefractionEdgeCheck(){ return mbUseRefractionEdgeCheck;}
		void SetHasRefraction(bool abX){ mbHasRefraction = abX; }
		void SetHasRefractionNormals(bool abX){ mbHasRefractionNormals = abX; }
		void SetIsAffectedByLightLevel(bool abX){ mbIsAffectedByLightLevel = abX;}
		bool IsAffectedByLightLevel(){ return mbIsAffectedByLightLevel;}
		void SetUseRefractionEdgeCheck(bool abX){ mbUseRefractionEdgeCheck = abX;}

		bool HasWorldReflection(){ return mbHasWorldReflection; }
		void SetHasWorldReflection(bool abX){ mbHasWorldReflection = abX; }
		void  SetWorldReflectionOcclusionTest(bool abX){ mbWorldReflectionOcclusionTest=abX;}
		void SetMaxReflectionDistance(float afX){ mfMaxReflectionDistance = afX;}
		bool  GetWorldReflectionOcclusionTest(){ return mbWorldReflectionOcclusionTest;}
		float GetMaxReflectionDistance(){ return mfMaxReflectionDistance;}

		//void SetHasTranslucentIllumination(bool abX){ mbHasTranslucentIllumination = abX;}
		//bool HasTranslucentIllumination(){ return mbHasTranslucentIllumination;}

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

        inline eTextureFilter GetTextureFilter() const { return m_textureFilter; }
        inline TextureAntistropy GetTextureAntistropy() const { return m_antistropy; }
        inline eTextureWrap GetTextureWrap() const { return m_textureWrap; }

        inline void setTextureFilter(eTextureFilter filter) {
            m_textureFilter = filter;
            Dirty();
        }
        inline void setTextureWrap(eTextureWrap wrap) {
            m_textureWrap = wrap;
            Dirty();
        }
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

        // we want to build a derived state from the matera information
        // decouples the state managment to work on forward++ model
        eMaterialBlendMode Desc_GetBlendMode() const;
        eMaterialAlphaMode Desc_GetAlphaMode() const;
        bool Desc_GetHasRefraction() const;
        bool Desc_GetHasReflection() const;
        bool Desc_GetHasWorldReflections() const;
        bool Desc_GetIsAffectedByLight() const;
        bool Desc_GetLargeTransperantSurface() const;
        float Desc_maxReflectionDistance() const;

		/**
		 * This is used so that materials do not call type specific things after types have been destroyed!
		 * Shall only be set by graphics!
		 */
		static void SetDestroyTypeSpecifics(bool abX){ mbDestroyTypeSpecifics = abX; }
		static bool GetDestroyTypeSpecifics(){ return mbDestroyTypeSpecifics; }

		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}

        inline void SetDescriptor(const MaterialDescriptor& desc) {
            m_descriptor = desc;
            Dirty();
        }
        const MaterialDescriptor& Descriptor() const {
            return m_descriptor;
        }
		inline MaterialType& type() { return m_info; }

		inline uint32_t materialID() { return m_info.m_handle.get(); }
		inline uint32_t Version() { return m_version; }
	    inline void Dirty() { m_version++; }
    private:
		void UpdateFlags();
		void UpdateAnimations(float afTimeStep);
		uint32_t m_version = 0; // used to check if the material has changed since last frame
		MaterialType m_info;
        TextureAntistropy m_antistropy = Antistropy_None;
        eTextureWrap m_textureWrap = eTextureWrap::eTextureWrap_Clamp;
        eTextureFilter m_textureFilter = eTextureFilter::eTextureFilter_Nearest;

		cGraphics *mpGraphics;
		cResources *mpResources;
		iMaterialType *mpType;
		iMaterialVars *mpVars;
	    MaterialDescriptor m_descriptor;

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
		bool mbHasRefractionNormals;
		bool mbUseRefractionEdgeCheck;
		bool mbHasWorldReflection;
		bool mbWorldReflectionOcclusionTest;
		//bool mbHasTranslucentIllumination = false;
		bool mbLargeTransperantSurface;
		bool mbAffectedByFog;
		bool mbUseAlphaDissolveFilter;
		bool mbHasUvAnimation;
		bool mbIsAffectedByLightLevel;

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
