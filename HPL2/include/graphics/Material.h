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
#include "graphics/Renderer.h"
#include "resources/Resources.h"
#include <graphics/GraphicsTypes.h>
#include <graphics/Image.h>
#include <graphics/ImageResourceWrapper.h>
#include <math/MathTypes.h>
#include <resources/ResourceBase.h>
#include <system/SystemTypes.h>

namespace hpl {

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

    struct MaterialType final {
        MaterialID m_id = MaterialID::Unknown;
        union {
            MaterialDecal m_decal;
            MaterialDiffuseSolid m_solid;
            MaterialTranslucent m_translucent;
            MaterialWater m_water;
        };
    };

    class iMaterialVars {
    public:
        virtual ~iMaterialVars() {
        }
    };

    class cMaterialUvAnimation {
    public:
        cMaterialUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis)
            : mType(aType)
            , mfSpeed(afSpeed)
            , mfAmp(afAmp)
            , mAxis(aAxis) {
        }

        eMaterialUvAnimation mType;

        float mfSpeed;
        float mfAmp;

        eMaterialAnimationAxis mAxis;
    };

    class cMaterial : public iResourceBase {
        friend class iMaterialType;

    public:
        static constexpr uint32_t MaxMaterialID = 2048;

        enum TextureAntistropy { Antistropy_None = 0, Antistropy_8 = 1, Antistropy_16 = 2, Antistropy_Count = 3 };

        struct MaterialUserVariable {
            const char* m_name;
            const char* m_type;
            const char* m_defaultValue;
        };

        struct MaterialMeta {
            MaterialID m_id;
            const char* m_name;
            bool m_isDecal;
            bool m_isTranslucent;

            std::span<const eMaterialTexture> m_usedTextures;
            std::span<const MaterialUserVariable> m_userVariables;
        };

        static constexpr eMaterialTexture SolidMaterialUsedTextures[] = { eMaterialTexture_Diffuse,       eMaterialTexture_NMap,
                                                                          eMaterialTexture_Alpha,         eMaterialTexture_Specular,
                                                                          eMaterialTexture_Height,        eMaterialTexture_Illumination,
                                                                          eMaterialTexture_DissolveAlpha, eMaterialTexture_CubeMap,
                                                                          eMaterialTexture_CubeMapAlpha };
        static constexpr eMaterialTexture TranslucentMaterialUsedTextures[] = {
            eMaterialTexture_Diffuse, eMaterialTexture_NMap, eMaterialTexture_CubeMap, eMaterialTexture_CubeMapAlpha
        };
        static constexpr eMaterialTexture WaterMaterialUsedTextures[] = { eMaterialTexture_Diffuse,
                                                                          eMaterialTexture_NMap,
                                                                          eMaterialTexture_CubeMap };
        static constexpr eMaterialTexture DecalMaterialUsedTextures[] = { eMaterialTexture_Diffuse };

        // TODO: move this outside of material potentialy
        // Meta information about the material
        static constexpr const std::array MaterialMetaTable{
            MaterialMeta{ MaterialID::SolidDiffuse, "soliddiffuse", false, false, std::span(SolidMaterialUsedTextures) },
            MaterialMeta{ MaterialID::Translucent, "translucent", false, true, std::span(TranslucentMaterialUsedTextures) },
            MaterialMeta{ MaterialID::Water, "water", false, true, std::span(WaterMaterialUsedTextures) },
            MaterialMeta{ MaterialID::Decal, "decal", true, true, std::span(DecalMaterialUsedTextures) }
        };

        cMaterial(const tString& asName, const tWString& asFullPath, cGraphics* apGraphics, cResources* apResources);
        virtual ~cMaterial();

        std::optional<const cMaterial::MaterialMeta*> Meta();

        void Compile();
        void SetImage(eMaterialTexture aType, iResourceBase* apTexture);
        Image* GetImage(eMaterialTexture aType);
        const Image* GetImage(eMaterialTexture aType) const;

        void SetAutoDestroyTextures(bool abX) {
            mbAutoDestroyTextures = abX;
            for (auto& image : m_image) {
                image.SetAutoDestroyResource(abX);
            }
        }
        inline void SetType(const MaterialType& desc) {
            m_descriptor = desc;
            IncreaseGeneration();
        }
        const MaterialType& Type() {
            return m_descriptor;
        }

        void SetTextureAnisotropy(float afx);
        void SetDepthTest(bool abDepthTest);
        inline bool GetDepthTest() {
            return mbDepthTest;
        };

        void SetPhysicsMaterial(const tString& asPhysicsMaterial) {
            msPhysicsMaterial = asPhysicsMaterial;
        }
        const tString& GetPhysicsMaterial() {
            return msPhysicsMaterial;
        }

        void UpdateBeforeRendering(float afTimeStep);

        inline eTextureFilter GetTextureFilter() const {
            return m_textureFilter;
        }
        inline TextureAntistropy GetTextureAntistropy() const {
            return m_antistropy;
        }
        inline eTextureWrap GetTextureWrap() const {
            return m_textureWrap;
        }

        // we want to build a derived state from the matera information
        // decouples the state managment to work on forward++ model
        eMaterialBlendMode GetBlendMode() const;
        eMaterialAlphaMode GetAlphaMode() const;
        bool GetHasRefraction() const;
        bool GetHasReflection() const;
        bool GetHasWorldReflections() const;
        bool GetLargeTransperantSurface() const;

        // water
        float maxReflectionDistance() const;

        void Serialize(cResourceVarsObject* vars);
        void Deserialize(cResourceVarsObject* vars);
        inline void setTextureFilter(eTextureFilter filter) {
            m_textureFilter = filter;
            IncreaseGeneration();
        }
        inline void setTextureWrap(eTextureWrap wrap) {
            m_textureWrap = wrap;
            IncreaseGeneration();
        }
        inline int GetRenderFrameCount() const {
            return mlRenderFrameCount;
        }
        inline void SetRenderFrameCount(const int alCount) {
            mlRenderFrameCount = alCount;
        }
        bool IsAffectedByLightLevel() const;

        // Animation
        void AddUvAnimation(eMaterialUvAnimation aType, float afSpeed, float afAmp, eMaterialAnimationAxis aAxis);
        int GetUvAnimationNum() {
            return (int)mvUvAnimations.size();
        }
        cMaterialUvAnimation* GetUvAnimation(int alIdx) {
            return &mvUvAnimations[alIdx];
        }
        inline bool HasUvAnimation() const {
            return mbHasUvAnimation;
        }
        inline const cMatrixf& GetUvMatrix() const {
            return m_mtxUV;
        }
        void ClearUvAnimations();

        inline const uint32_t GetHandle() {
            return m_resourceHandle.get();
        }
        inline void setHandle(IndexPoolHandle&& handle) {
            m_resourceHandle = std::move(handle);
        }

        bool Reload() {
            return false;
        }
        void Unload() {
        }
        void Destroy() {
        }

        inline uint32_t Generation() { return m_generation;}
        inline void IncreaseGeneration() { m_generation++; }

    private:
        void UpdateAnimations(float afTimeStep);
        uint32_t m_generation = 0; // used to check if the material has changed since last frame
        TextureAntistropy m_antistropy = Antistropy_None;
        eTextureWrap m_textureWrap = eTextureWrap::eTextureWrap_Clamp;
        eTextureFilter m_textureFilter = eTextureFilter::eTextureFilter_Nearest;
        MaterialType m_descriptor;

        bool mbDepthTest;

        cResources* mpResources;
        IndexPoolHandle m_resourceHandle;

        bool mbAutoDestroyTextures;
        bool mbHasUvAnimation;
        std::array<ImageResourceWrapper, eMaterialTexture_LastEnum> m_image = { ImageResourceWrapper() };

        std::vector<cMaterialUvAnimation> mvUvAnimations;
        cMatrixf m_mtxUV;

        float mfAnimTime;
        int mlRenderFrameCount;
        tString msPhysicsMaterial;
    };

    //---------------------------------------------------

}; // namespace hpl
