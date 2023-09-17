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

#include "graphics/MaterialType_BasicSolid.h"

#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "math/Matrix.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"

#include "math/Frustum.h"
#include "math/Math.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"
#include "system/SystemTypes.h"
#include <algorithm>
#include <cstdint>
#include <graphics/Image.h>
#include <iterator>
#include <memory>

namespace hpl
{
    float iMaterialType_SolidBase::mfVirtualPositionAddScale = 0.03f;

    iMaterialType_SolidBase::iMaterialType_SolidBase(cGraphics* apGraphics, cResources* apResources)
        : iMaterialType(apGraphics, apResources)
    {
    }

    //--------------------------------------------------------------------------

    iMaterialType_SolidBase::~iMaterialType_SolidBase()
    {
        mpResources->GetTextureManager()->Destroy(m_dissolveImage);
    }

    void iMaterialType_SolidBase::CreateGlobalPrograms()
    {
    }

    void iMaterialType_SolidBase::LoadData()
    {
        CreateGlobalPrograms();
        m_dissolveImage = mpResources->GetTextureManager()->Create2DImage("core_dissolve.tga", true);
    }

    void iMaterialType_SolidBase::DestroyData()
    {

    }

    void iMaterialType_SolidBase::LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
    }

    void iMaterialType_SolidBase::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
    }

    void iMaterialType_SolidBase::CompileMaterialSpecifics(cMaterial* apMaterial)
    {
       // if (apMaterial->GetImage(eMaterialTexture_Alpha))
       // {
       //     apMaterial->SetAlphaMode(eMaterialAlphaMode_Trans);
       // }
       // else
       // {
       //     apMaterial->SetAlphaMode(eMaterialAlphaMode_Solid);
       // }

        CompileSolidSpecifics(apMaterial);
    }

    cMaterialType_SolidDiffuse::cMaterialType_SolidDiffuse(cGraphics* apGraphics, cResources* apResources)
        : iMaterialType_SolidBase(apGraphics, apResources)
    {
        AddUsedTexture(eMaterialTexture_Diffuse);
        AddUsedTexture(eMaterialTexture_NMap);
        AddUsedTexture(eMaterialTexture_Alpha);
        AddUsedTexture(eMaterialTexture_Specular);
        AddUsedTexture(eMaterialTexture_Height);
        AddUsedTexture(eMaterialTexture_Illumination);
        AddUsedTexture(eMaterialTexture_DissolveAlpha);
        AddUsedTexture(eMaterialTexture_CubeMap);
        AddUsedTexture(eMaterialTexture_CubeMapAlpha);

        mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;

        AddVarFloat("HeightMapScale", 0.05f, "");
        AddVarFloat("HeightMapBias", 0, "");
        AddVarFloat(
            "FrenselBias",
            0.2f,
            "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at object.");
        AddVarFloat(
            "FrenselPow", 8.0f, "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.");
        AddVarBool(
            "AlphaDissolveFilter",
            false,
            "If alpha values between 0 and 1 should be used and dissolve the texture. This can be useful for things like hair.");
    }

    cMaterialType_SolidDiffuse::~cMaterialType_SolidDiffuse()
    {
    }

    void cMaterialType_SolidDiffuse::CompileSolidSpecifics(cMaterial* apMaterial)
    {
        cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

        //////////////////////////////////
        // Z specifics
        apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Z_Dissolve, true);
        apMaterial->SetUseAlphaDissolveFilter(pVars->mbAlphaDissolveFilter);

        //////////////////////////////////
        // Normal map and height specifics
        if (apMaterial->GetImage(eMaterialTexture_NMap))
        {
            if (apMaterial->GetImage(eMaterialTexture_Height))
            {
                apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
            }
        }

        //////////////////////////////////
        // Uv animation specifics
        if (apMaterial->HasUvAnimation())
        {
            apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Z, true);
            apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
            apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Illumination, true);
        }

        //////////////////////////////////
        // Cubemap
        if (apMaterial->GetImage(eMaterialTexture_CubeMap))
        {
            apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
        }

        //////////////////////////////////
        // Illuminations specifics
        if (apMaterial->GetImage(eMaterialTexture_Illumination))
        {
            apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Illumination, true);
        }
    }


    iMaterialVars* cMaterialType_SolidDiffuse::CreateSpecificVariables()
    {
        auto pVars = new cMaterialType_SolidDiffuse_Vars();
        auto pipeline = Interface<ForgeRenderer>::Get();
        DescriptorSetDesc desc = { pipeline->PipelineSignature(), DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 2 };

        BufferDesc bufferDesc = {};
        bufferDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        bufferDesc.mSize = sizeof(cMaterialType_SolidDiffuse_Vars::MaterialBufferData);
        pVars->m_materialId = (++m_materialCount) % MaxSolidMaterials;
        // for(auto& bufferHandle: pVars->m_diffuseBuffer) {
        //     BufferLoadDesc loadDesc = {};
        //     loadDesc.mDesc = bufferDesc;
        //     loadDesc.ppBuffer = &bufferHandle.m_handle;
        //     addResource(&loadDesc, nullptr);
        // }
        // DescriptorSet* descriptorSet = {};
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Diffuse].m_handle);
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Z].m_handle);
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Z_Dissolve].m_handle);
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Illumination].m_handle);

        // pVars->m_descriptorSet[eMaterialRenderMode_Diffuse].Initialize();
        // pVars->m_descriptorSet[eMaterialRenderMode_Z].Initialize();
        // pVars->m_descriptorSet[eMaterialRenderMode_Z_Dissolve].Initialize();
        // pVars->m_descriptorSet[eMaterialRenderMode_Illumination].Initialize();

        return new cMaterialType_SolidDiffuse_Vars();
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
        cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();
        if (pVars == NULL)
        {
            pVars = (cMaterialType_SolidDiffuse_Vars*)CreateSpecificVariables();
            apMaterial->SetVars(pVars);
        }

        pVars->mfHeightMapScale = apVars->GetVarFloat("HeightMapScale", 0.1f);
        pVars->mfHeightMapBias = apVars->GetVarFloat("HeightMapBias", 0);
        pVars->mfFrenselBias = apVars->GetVarFloat("FrenselBias", 0.2f);
        pVars->mfFrenselPow = apVars->GetVarFloat("FrenselPow", 8.0f);
        pVars->mbAlphaDissolveFilter = apVars->GetVarBool("AlphaDissolveFilter", false);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
        cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

        apVars->AddVarFloat("HeightMapScale", pVars->mfHeightMapScale);
        apVars->AddVarFloat("HeightMapBias", pVars->mfHeightMapBias);
        apVars->AddVarFloat("FrenselBias", pVars->mfFrenselBias);
        apVars->AddVarFloat("FrenselPow", pVars->mfFrenselPow);
        apVars->AddVarBool("AlphaDissolveFilter", pVars->mbAlphaDissolveFilter);
    }

    //--------------------------------------------------------------------------
} // namespace hpl
