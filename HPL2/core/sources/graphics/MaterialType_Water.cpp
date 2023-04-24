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

#include "bgfx/bgfx.h"
#include "graphics/GraphicsContext.h"
#include "graphics/RendererDeferred.h"
#include "graphics/ShaderUtil.h"
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
        m_waterVariant.Initialize(ShaderHelper::LoadProgramHandlerDefault("vs_water_material", "fs_water_material", false, true));

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

        m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
        m_u_mtxInvViewRotation = bgfx::createUniform("u_mtxInvViewRotation", bgfx::UniformType::Mat4);
        m_u_fogColor = bgfx::createUniform("u_fogColor", bgfx::UniformType::Vec4);

        m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
        m_s_normalMap = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
        m_s_refractionMap = bgfx::createUniform("s_refractionMap", bgfx::UniformType::Sampler);
        m_s_reflectionMap = bgfx::createUniform("s_reflectionMap", bgfx::UniformType::Sampler);
        m_s_envMap = bgfx::createUniform("s_envMap", bgfx::UniformType::Sampler);
    }

    cMaterialType_Water::~cMaterialType_Water() {
    }

    void cMaterialType_Water::LoadData() {
    }
    
    void cMaterialType_Water::DestroyData() {
    }

    void cMaterialType_Water::ResolveShaderProgram(
        eMaterialRenderMode aRenderMode,
        cViewport& viewport,
        cMaterial* apMaterial,
        iRenderable* apObject,
        iRenderer* apRenderer,
        std::function<void(GraphicsContext::ShaderProgram&)> handler) {
        GraphicsContext::ShaderProgram program;
        cVector2f reflectionMapSizeMul = cVector2f(1.0f / (float)iRenderer::GetReflectionSizeDiv());

        if (aRenderMode == eMaterialRenderMode_Diffuse || aRenderMode == eMaterialRenderMode_DiffuseFog) {
            cWorld* pWorld = apRenderer->GetCurrentWorld();
            struct u_params {
                float u_frenselBiasPow[2];
                float u_fogStart;
                float u_fogEnd;

                float reflectionMapSizeMul[2];
                float reflectionFadeStart;
                float reflectionFadeStarLength;

                float falloffExp;
                float afT;
                float afWaveAmplitude;
                float afWaveFreq;

                float afRefractionScale;
                float useRefractionFading;
                float padding[2];
            } params = { 0 };
            auto diffuseMap = apMaterial->GetImage(eMaterialTexture_Diffuse);
            auto normalMap = apMaterial->GetImage(eMaterialTexture_NMap);

            auto cubeMap = apMaterial->GetImage(eMaterialTexture_CubeMap);

            cMaterialType_Water_Vars* pVars = static_cast<cMaterialType_Water_Vars*>(apMaterial->GetVars());
            const bool refractionEnabled = iRenderer::GetRefractionEnabled();
            const bool hasReflection = refractionEnabled && pVars->mbHasReflection;
            const bool hasCubeMap = refractionEnabled && cubeMap;
            const bool hasDiffuseFog = (aRenderMode == eMaterialRenderMode_DiffuseFog);

            material::water::WaterVariant flags = static_cast<material::water::WaterVariant>(
                (hasReflection ? material::water::UseReflection : 0) | 
                (hasCubeMap ? material::water::UseCubeMapReflection : 0) |
                (hasDiffuseFog ? material::water::UseFog : 0) | 
                (refractionEnabled ? material::water::UseRefraction : 0));
            
            params.u_frenselBiasPow[0] = pVars->mfFrenselBias;
            params.u_frenselBiasPow[1] = pVars->mfFrenselPow;
            params.u_fogStart = pWorld->GetFogStart();
            params.u_fogEnd = pWorld->GetFogEnd() - pWorld->GetFogStart();
            params.reflectionFadeStart = pVars->mfReflectionFadeStart;
            params.reflectionFadeStarLength = pVars->mfReflectionFadeEnd - pVars->mfReflectionFadeStart;
            params.falloffExp = pWorld->GetFogFalloffExp();
            params.afT = apRenderer->GetTimeCount() * pVars->mfWaveSpeed;
            params.afWaveAmplitude = pVars->mfWaveAmplitude  * 0.04f;
            params.afWaveFreq = pVars->mfWaveFreq * 10.0f;
            params.afRefractionScale = pVars->mfRefractionScale * static_cast<float>(viewport.GetSize().x);
            params.useRefractionFading = pVars->mfReflectionFadeEnd > 0.0f ? 1.0f : 0.0f;

            cColor fogColor = pWorld->GetFogColor();
            if (cubeMap) {
                program.m_textures.push_back({ m_s_envMap, cubeMap->GetHandle(), 0 });
            }
            program.m_textures.push_back({ m_s_diffuseMap, diffuseMap->GetHandle(), 1 });
            if (normalMap) {
                program.m_textures.push_back({ m_s_normalMap, normalMap->GetHandle(), 2 });
            }

            if(refractionEnabled && apRenderer && TypeInfo<cRendererDeferred>::IsType(*apRenderer)) {
                auto* deferredRenderer = static_cast<cRendererDeferred*>(apRenderer);
                auto& sharedData = deferredRenderer->GetSharedData(viewport);
                // program.m_textures.push_back({m_s_refractionMap, sharedData.m_refractionImage->GetHandle(), 3});
                // program.m_textures.push_back({m_s_reflectionMap, sharedData.m_gBufferReflection.m_colorImage->GetHandle(), 4});
            }

            program.m_uniforms.push_back({ m_u_param, &params, 4 });
            const cMatrixf mtxInvView = ([&] {
                cMatrixf mtxInvView = apRenderer->GetCurrentFrustum()->GetViewMatrix().GetTranspose();
                return mtxInvView.GetRotation().GetTranspose();
            })();
            program.m_uniforms.push_back({ m_u_mtxInvViewRotation, &mtxInvView.v });
            program.m_uniforms.push_back({ m_u_fogColor, &fogColor.v });
            program.m_handle = m_waterVariant.GetVariant(flags);
            handler(program);
        }
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
        cMaterialType_Water_Vars* pVars = (cMaterialType_Water_Vars*)apMaterial->GetVars();
        if (pVars == NULL) {
            pVars = (cMaterialType_Water_Vars*)CreateSpecificVariables();
            apMaterial->SetVars(pVars);
        }

        pVars->mbHasReflection = apVars->GetVarBool("HasReflection", true);
        pVars->mfRefractionScale = apVars->GetVarFloat("RefractionScale", 0.1f);
        pVars->mfFrenselBias = apVars->GetVarFloat("FrenselBias", 0.2f);
        pVars->mfFrenselPow = apVars->GetVarFloat("FrenselPow", 8.0f);
        pVars->mfReflectionFadeStart = apVars->GetVarFloat("ReflectionFadeStart", 0);
        pVars->mfReflectionFadeEnd = apVars->GetVarFloat("ReflectionFadeEnd", 0);
        pVars->mfWaveSpeed = apVars->GetVarFloat("WaveSpeed", 1.0f);
        pVars->mfWaveAmplitude = apVars->GetVarFloat("WaveAmplitude", 1.0f);
        pVars->mfWaveFreq = apVars->GetVarFloat("WaveFreq", 1.0f);

        apMaterial->SetWorldReflectionOcclusionTest(apVars->GetVarBool("OcclusionCullWorldReflection", true));
        apMaterial->SetMaxReflectionDistance(apVars->GetVarFloat("ReflectionFadeEnd", 0.0f));
        apMaterial->SetLargeTransperantSurface(apVars->GetVarBool("LargeSurface", false));
    }

    void cMaterialType_Water::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars) {
        cMaterialType_Water_Vars* pVars = (cMaterialType_Water_Vars*)apMaterial->GetVars();

        apVars->AddVarBool("HasReflection", pVars->mbHasReflection);
        apVars->AddVarFloat("RefractionScale", pVars->mfRefractionScale);
        apVars->AddVarFloat("FrenselBias", pVars->mfFrenselBias);
        apVars->AddVarFloat("FrenselPow", pVars->mfFrenselPow);
        apVars->AddVarFloat("ReflectionFadeStart", pVars->mfReflectionFadeStart);
        apVars->AddVarFloat("ReflectionFadeEnd", pVars->mfReflectionFadeEnd);
        apVars->AddVarFloat("WaveSpeed", pVars->mfWaveSpeed);
        apVars->AddVarFloat("WaveAmplitude", pVars->mfWaveAmplitude);
        apVars->AddVarFloat("WaveFreq", pVars->mfWaveFreq);

        apVars->AddVarBool("OcclusionCullWorldReflection", apMaterial->GetWorldReflectionOcclusionTest());
        apVars->AddVarBool("LargeSurface", apMaterial->GetLargeTransperantSurface());
    }

    void cMaterialType_Water::CompileMaterialSpecifics(cMaterial* apMaterial) {
        cMaterialType_Water_Vars* pVars = static_cast<cMaterialType_Water_Vars*>(apMaterial->GetVars());

        /////////////////////////////////////
        // Set if has specific variables
        apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
        apMaterial->SetHasSpecificSettings(eMaterialRenderMode_DiffuseFog, true);

        /////////////////////////////////////
        // Set up the blend mode
        if (iRenderer::GetRefractionEnabled()) {
            apMaterial->SetBlendMode(eMaterialBlendMode_None);
        } else {
            apMaterial->SetBlendMode(eMaterialBlendMode_Mul);
        }

        /////////////////////////////////////
        // Set up the refraction
        if (iRenderer::GetRefractionEnabled()) {
            apMaterial->SetHasRefraction(true);
            apMaterial->SetUseRefractionEdgeCheck(true);
        }

        ///////////////////////////
        // Set up reflection
        if (pVars->mbHasReflection && iRenderer::GetRefractionEnabled()) {
            if (apMaterial->GetImage(eMaterialTexture_CubeMap) == NULL) {
                apMaterial->SetHasWorldReflection(true);
            }
        }
    }

} // namespace hpl
