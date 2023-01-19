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

#include "bgfx/bgfx.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Image.h"
#include "graphics/ShaderUtil.h"
#include "math/MathTypes.h"
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

        m_u_mtxUV = bgfx::createUniform("a_mtxUV", bgfx::UniformType::Mat4);
        m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
        m_programHandler = hpl::loadProgram("vs_decal_material", "fs_decal_material");

        AddUsedTexture(eMaterialTexture_Diffuse);
    }

    cMaterialType_Decal::~cMaterialType_Decal()
    {
        if(bgfx::isValid(m_u_mtxUV)) {bgfx::destroy(m_u_mtxUV);}
        if(bgfx::isValid(m_s_diffuseMap)) {bgfx::destroy(m_s_diffuseMap);}
        if(bgfx::isValid(m_programHandler)) {bgfx::destroy(m_programHandler);}
    }

    void cMaterialType_Decal::LoadData()
    {

    }

    //--------------------------------------------------------------------------

    void cMaterialType_Decal::DestroyData()
    {
    }

    void cMaterialType_Decal::ResolveShaderProgram(
            eMaterialRenderMode aRenderMode,
            cMaterial* apMaterial,
            iRenderable* apObject,
            iRenderer* apRenderer, 
            std::function<void(GraphicsContext::ShaderProgram&)> handler)  {
        cMatrixf mtxUv = cMatrixf::Identity;
        GraphicsContext::ShaderProgram program;
        program.m_handle = m_programHandler;
        auto diffuseMap = apMaterial->GetImage(eMaterialTexture_Diffuse);
        if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            cMaterialType_Decal_Vars* pVars = static_cast<cMaterialType_Decal_Vars*>(apMaterial->GetVars());
            if (apMaterial->HasUvAnimation())
            {
                mtxUv = apMaterial->GetUvMatrix().GetTranspose();
            } 
        }
        program.m_uniforms.push_back({ m_u_mtxUV, mtxUv.m });
        if(diffuseMap) {
            program.m_textures.push_back({ m_s_diffuseMap, diffuseMap->GetHandle(), 0});
        }
        handler(program);
    }


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
