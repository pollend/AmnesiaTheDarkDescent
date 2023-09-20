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

#include "graphics/MaterialType_Water.h"

#include "graphics/RendererDeferred.h"
#include "math/MathTypes.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include <algorithm>
#include <iterator>

#include "resources/Resources.h"

#include "math/Frustum.h"
#include "math/Math.h"

#include "scene/World.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"

namespace hpl {
    cMaterialType_Water::cMaterialType_Water(cGraphics* apGraphics, cResources* apResources)
        : iMaterialType(apGraphics, apResources) {
        // m_waterVariant.Initialize(ShaderHelper::LoadProgramHandlerDefault("vs_water_material", "fs_water_material", false, true));

        mbIsTranslucent = true;

        AddUsedTexture(eMaterialTexture_Diffuse);
        AddUsedTexture(eMaterialTexture_NMap);
        AddUsedTexture(eMaterialTexture_CubeMap);

        AddVarFloat("RefractionScale", 0.1f, "The amount reflection and refraction is offset by ripples in water.");
        AddVarFloat(
            "FrenselBias",
            0.2f,
            "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at water.");
        AddVarFloat(
            "FrenselPow", 8.0f, "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.");
        AddVarFloat("WaveSpeed", 1.0f, "The speed of the waves.");
        AddVarFloat("WaveAmplitude", 1.0f, "The size of the waves.");
        AddVarFloat("WaveFreq", 1.0f, "The frequency of the waves.");
        AddVarFloat("ReflectionFadeStart", 0, "Where the reflection starts fading.");
        AddVarFloat("ReflectionFadeEnd", 0, "Where the reflection stops fading. 0 or less means no fading.");

        AddVarBool("HasReflection", true, "If a reflection should be shown or not.");
        AddVarBool("OcclusionCullWorldReflection", true, "If occlusion culling should be used on reflection.");
        AddVarBool(
            "LargeSurface",
            true,
            "If the water will cover a large surface and will need special sorting when rendering other transperant objects.");

        mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;
        mbHasTypeSpecifics[eMaterialRenderMode_DiffuseFog] = true;
    }

    cMaterialType_Water::~cMaterialType_Water() {
    }

    void cMaterialType_Water::LoadData() {
    }

    void cMaterialType_Water::DestroyData() {
    }

    iMaterialVars* cMaterialType_Water::CreateSpecificVariables() {
        cMaterialType_Water_Vars* pVars = hplNew(cMaterialType_Water_Vars, ());

        pVars->mbHasReflection = true;
        pVars->mfRefractionScale = 0.1f;
        pVars->mfFrenselBias = 0.2f;
        pVars->mfFrenselPow = 8.0f;

        return pVars;
    }

    void cMaterialType_Water::LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars) {
       // cMaterialType_Water_Vars* pVars = (cMaterialType_Water_Vars*)apMaterial->GetVars();
       // if (pVars == NULL) {
       //     pVars = (cMaterialType_Water_Vars*)CreateSpecificVariables();
       //     apMaterial->SetVars(pVars);
       // }

       // pVars->mbHasReflection = apVars->GetVarBool("HasReflection", true);
       // pVars->mfRefractionScale = apVars->GetVarFloat("RefractionScale", 0.1f);
       // pVars->mfFrenselBias = apVars->GetVarFloat("FrenselBias", 0.2f);
       // pVars->mfFrenselPow = apVars->GetVarFloat("FrenselPow", 8.0f);
       // pVars->mfReflectionFadeStart = apVars->GetVarFloat("ReflectionFadeStart", 0);
       // pVars->mfReflectionFadeEnd = apVars->GetVarFloat("ReflectionFadeEnd", 0);
       // pVars->mfWaveSpeed = apVars->GetVarFloat("WaveSpeed", 1.0f);
       // pVars->mfWaveAmplitude = apVars->GetVarFloat("WaveAmplitude", 1.0f);
       // pVars->mfWaveFreq = apVars->GetVarFloat("WaveFreq", 1.0f);

       // apMaterial->SetWorldReflectionOcclusionTest(apVars->GetVarBool("OcclusionCullWorldReflection", true));
       // apMaterial->SetMaxReflectionDistance(apVars->GetVarFloat("ReflectionFadeEnd", 0.0f));
       // apMaterial->SetLargeTransperantSurface(apVars->GetVarBool("LargeSurface", false));
    }

    void cMaterialType_Water::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars) {
       // cMaterialType_Water_Vars* pVars = (cMaterialType_Water_Vars*)apMaterial->GetVars();

       // apVars->AddVarBool("HasReflection", pVars->mbHasReflection);
       // apVars->AddVarFloat("RefractionScale", pVars->mfRefractionScale);
       // apVars->AddVarFloat("FrenselBias", pVars->mfFrenselBias);
       // apVars->AddVarFloat("FrenselPow", pVars->mfFrenselPow);
       // apVars->AddVarFloat("ReflectionFadeStart", pVars->mfReflectionFadeStart);
       // apVars->AddVarFloat("ReflectionFadeEnd", pVars->mfReflectionFadeEnd);
       // apVars->AddVarFloat("WaveSpeed", pVars->mfWaveSpeed);
       // apVars->AddVarFloat("WaveAmplitude", pVars->mfWaveAmplitude);
       // apVars->AddVarFloat("WaveFreq", pVars->mfWaveFreq);

       // apVars->AddVarBool("OcclusionCullWorldReflection", apMaterial->GetWorldReflectionOcclusionTest());
       // apVars->AddVarBool("LargeSurface", apMaterial->GetLargeTransperantSurface());
    }

    void cMaterialType_Water::CompileMaterialSpecifics(cMaterial* apMaterial) {
        //cMaterialType_Water_Vars* pVars = static_cast<cMaterialType_Water_Vars*>(apMaterial->GetVars());

       // /////////////////////////////////////
       // // Set if has specific variables
       // apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
       // apMaterial->SetHasSpecificSettings(eMaterialRenderMode_DiffuseFog, true);

        /////////////////////////////////////
        // Set up the blend mode
       // if (iRenderer::GetRefractionEnabled()) {
       //     apMaterial->SetBlendMode(eMaterialBlendMode_None);
       // } else {
       //     apMaterial->SetBlendMode(eMaterialBlendMode_Mul);
       // }

       // /////////////////////////////////////
       // // Set up the refraction
       // if (iRenderer::GetRefractionEnabled()) {
       //     apMaterial->SetHasRefraction(true);
       //     apMaterial->SetUseRefractionEdgeCheck(true);
       // }

       // ///////////////////////////
       // // Set up reflection
       // if (pVars->mbHasReflection && iRenderer::GetRefractionEnabled()) {
       //     if (apMaterial->GetImage(eMaterialTexture_CubeMap) == NULL) {
       //         apMaterial->SetHasWorldReflection(true);
       //     }
       // }
    }

} // namespace hpl
