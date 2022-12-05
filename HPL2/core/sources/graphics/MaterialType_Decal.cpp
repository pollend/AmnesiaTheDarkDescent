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

#include "graphics/MaterialType_Decal.h"

#include "graphics/BGFXProgram.h"
#include "graphics/ShaderUtil.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"

#include "resources/Resources.h"

#include "math/Frustum.h"
#include "math/Math.h"

#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "system/SystemTypes.h"

namespace hpl
{

//////////////////////////////////////////////////////////////////////////
// DEFINES
//////////////////////////////////////////////////////////////////////////

//------------------------------
// Variables
//------------------------------
#define kVar_a_mtxUV 0

    namespace material::decal
    {
        static const auto a_mtxUV = MemberID("a_mtxUV", kVar_a_mtxUV);
    };

//------------------------------
// Diffuse Features and data
//------------------------------
#define eFeature_Diffuse_UvAnimation eFlagBit_0

#define kDiffuseFeatureNum 1

    static cProgramComboFeature vDiffuseFeatureVec[] = {
        cProgramComboFeature("UseUvAnimation", kPC_VertexBit),
    };

    //////////////////////////////////////////////////////////////////////////
    // DECAL
    //////////////////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------

    cMaterialType_Decal::cMaterialType_Decal(cGraphics* apGraphics, cResources* apResources)
        : iMaterialType(apGraphics, apResources)
    {
        mbIsTranslucent = true;
        mbIsDecal = true;

        mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;

        _u_mtxUV = bgfx::createUniform("a_mtxUV", bgfx::UniformType::Mat4, 1);
        _programHandler = hpl::loadProgram("vs_decal", "fs_decal");

        AddUsedTexture(eMaterialTexture_Diffuse);
    }

    cMaterialType_Decal::~cMaterialType_Decal()
    {
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::DestroyProgram(
        cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
    {
        mpProgramManager->DestroyGeneratedProgram(aRenderMode, apProgram);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::LoadData()
    {
        cParserVarContainer defaultVars;
        defaultVars.Add("UseUv");
        defaultVars.Add("UseColor");

        mpProgramManager->SetupGenerateProgramData(
            eMaterialRenderMode_Diffuse,
            "Diffuse",
            "deferred_base_vtx.glsl",
            "deferred_decal_frag.glsl",
            vDiffuseFeatureVec,
            kDiffuseFeatureNum,
            defaultVars);

        ////////////////////////////////
        // Set up variable ids
        mpProgramManager->AddGenerateProgramVariableId("a_mtxUV", kVar_a_mtxUV, eMaterialRenderMode_Diffuse);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::DestroyData()
    {
        mpProgramManager->DestroyShadersAndPrograms();
    }

    //--------------------------------------------------------------------------

    iTexture* cMaterialType_Decal::GetTextureForUnit(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, int alUnit)
    {
        ////////////////////////////
        // Diffuse
        if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            switch (alUnit)
            {
            case 0:
                return apMaterial->GetTexture(eMaterialTexture_Diffuse);
            }
        }

        return NULL;
    }

    //--------------------------------------------------------------------------

    iTexture* cMaterialType_Decal::GetSpecialTexture(
        cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iRenderer* apRenderer, int alUnit)
    {
        return NULL;
    }

    //--------------------------------------------------------------------------

    iGpuProgram* cMaterialType_Decal::GetGpuProgram(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
    {
        struct DecalData
        {
            float mtxUv[16];
        };
        using MaterialDecalProgram = BGFXProgram<DecalData>;

        if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            const bool hasUVAnimation = apMaterial->HasUvAnimation();

            tFlag lFlags = (hasUVAnimation ? eFeature_Diffuse_UvAnimation : 0);

            return mpProgramManager->GenerateProgram(
                aRenderMode,
                lFlags,
                [programHandle = _programHandler, u_mtxUV = _u_mtxUV, hasUVAnimation](const tString& name)
                {
                    auto program = new MaterialDecalProgram(
                        { MaterialDecalProgram::ParameterField(
                            material::decal::a_mtxUV,
                            MaterialDecalProgram::Matrix4fMapper(
                                [](DecalData& data, const cMatrixf& value)
                                {
                                    std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxUv));
                                })) },
                        name,
                        programHandle,
                        false,
                        eGpuProgramFormat_BGFX);
                    program->SetSubmitHandler(
                        [u_mtxUV](const DecalData& data, bgfx::ViewId view, GraphicsContext& context)
                        {
                            bgfx::setUniform(u_mtxUV, &data.mtxUv);
                        });
                    program->SetMatrixf(material::decal::a_mtxUV.id(), cMatrixf::Identity);

                    return program;
                });
        }

        return NULL;
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer* apRenderer)
    {
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::SetupMaterialSpecificData(
        eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial* apMaterial, iRenderer* apRenderer)
    {
        ////////////////////////////
        // Diffuse
        if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            cMaterialType_Decal_Vars* pVars = static_cast<cMaterialType_Decal_Vars*>(apMaterial->GetVars());

            /////////////////////////
            // UV Animation
            if (apMaterial->HasUvAnimation())
            {
                apProgram->SetMatrixf(kVar_a_mtxUV, apMaterial->GetUvMatrix());
            }
        }
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::SetupObjectSpecificData(
        eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable* apObject, iRenderer* apRenderer)
    {
    }

    //--------------------------------------------------------------------------

    iMaterialVars* cMaterialType_Decal::CreateSpecificVariables()
    {
        return hplNew(cMaterialType_Decal_Vars, ());
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
        cMaterialType_Decal_Vars* pVars = (cMaterialType_Decal_Vars*)apMaterial->GetVars();
        if (pVars == NULL)
        {
            pVars = (cMaterialType_Decal_Vars*)CreateSpecificVariables();
            apMaterial->SetVars(pVars);
        }
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
        cMaterialType_Decal_Vars* pVars = (cMaterialType_Decal_Vars*)apMaterial->GetVars();
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::CompileMaterialSpecifics(cMaterial* apMaterial)
    {
        cMaterialType_Decal_Vars* pVars = static_cast<cMaterialType_Decal_Vars*>(apMaterial->GetVars());

        if (apMaterial->HasUvAnimation())
        {
            apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
        }
    }

} // namespace hpl
