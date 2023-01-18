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
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/ShaderVariantCollection.h"
#include <cstdint>

namespace hpl
{


    namespace material::translucent
    {
        enum TranslucentVariant: uint32_t
        {
            Translucent_None = 0,
            Translucent_DiffuseMap = 0x00001,
            Translucent_NormalMap = 0x00002,
            Translucent_Refraction = 0x00004,
            Translucent_UseCubeMap = 0x00008,
            Translucent_UseFog = 0x00010,
        };
    }
    class cMaterialType_Translucent_Vars : public iMaterialVars
    {
    public:
        cMaterialType_Translucent_Vars()
            : mbRefraction(false)
            , mbRefractionEdgeCheck(true)
            , mbRefractionNormals(false)
            , mfRefractionScale(0.1f)
            , mfFrenselBias(0.2f)
            , mfFrenselPow(8.0f)
        {
        }
        ~cMaterialType_Translucent_Vars()
        {
        }

        bool mbRefraction;
        bool mbRefractionEdgeCheck;
        bool mbRefractionNormals;
        float mfRefractionScale;
        float mfFrenselBias;
        float mfFrenselPow;
        float mfRimLightMul;
        float mfRimLightPow;
        bool mbAffectedByLightLevel;
    };

    //--------------------------------------------------

    class cMaterialType_Translucent : public iMaterialType
    {
        using TranslucentShaderCollection = ShaderVariantCollection<
			material::translucent::Translucent_DiffuseMap |
			material::translucent::Translucent_NormalMap |
			material::translucent::Translucent_Refraction |
			material::translucent::Translucent_UseCubeMap |
			material::translucent::Translucent_UseFog 
		>;

    public:
        cMaterialType_Translucent(cGraphics* apGraphics, cResources* apResources);
        ~cMaterialType_Translucent();

        void DestroyProgram(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton);

        bool SupportsHWSkinning()
        {
            return false;
        }

        iTexture* GetTextureForUnit(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, int alUnit);
        iTexture* GetSpecialTexture(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iRenderer* apRenderer, int alUnit);

        virtual void ResolveShaderProgram(
            eMaterialRenderMode aRenderMode,
            cMaterial* apMaterial,
            iRenderable* apObject,
            iRenderer* apRenderer, 
            std::function<void(GraphicsContext::ShaderProgram&)> handler) override;

        iGpuProgram* GetGpuProgram(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton);

        void SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer* apRenderer);
        void SetupMaterialSpecificData(
            eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial* apMaterial, iRenderer* apRenderer);
        void SetupObjectSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable* apObject, iRenderer* apRenderer);

        iMaterialVars* CreateSpecificVariables();
        void LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars);
        void GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars);

        void CompileMaterialSpecifics(cMaterial* apMaterial);

    private:
        bgfx::ProgramHandle _programHandle;

		bgfx::UniformHandle m_u_param;
		bgfx::UniformHandle m_u_mtxUv;
        bgfx::UniformHandle m_u_invViewRotation;

		bgfx::UniformHandle m_s_diffuseMap;
		bgfx::UniformHandle m_s_normalMap;
		bgfx::UniformHandle m_s_refractionMap;
		bgfx::UniformHandle m_s_envMapAlphaMap;
		bgfx::UniformHandle m_s_envMap;

        TranslucentShaderCollection m_translucent_blendModeAdd;
        TranslucentShaderCollection m_translucent_blendModeMul;
        TranslucentShaderCollection m_translucent_blendModeMulX2;
        TranslucentShaderCollection m_translucent_blendModeAlpha;
        TranslucentShaderCollection m_translucent_blendModePremulAlpha;

        void LoadData();
        void DestroyData();

        cProgramComboManager* mpBlendProgramManager[5];
    };

    //---------------------------------------------------

}; // namespace hpl
