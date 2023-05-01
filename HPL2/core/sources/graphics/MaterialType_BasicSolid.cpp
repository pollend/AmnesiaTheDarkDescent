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

#include "bgfx/bgfx.h"
#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/ShaderUtil.h"
#include "graphics/ShaderVariantCollection.h"
#include "math/MathTypes.h"
#include "math/Matrix.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"

#include "math/Frustum.h"
#include "math/Math.h"

#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
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
        // m_diffuseProgramVariant.Initialize(ShaderHelper::LoadProgramHandlerDefault(
        //     "vs_basic_solid_diffuse",
        //      "fs_basic_solid_diffuse", false, true));
        // m_ZProgramVariant.Initialize(
        //     ShaderHelper::LoadProgramHandlerDefault("vs_basic_solid_z", "fs_basic_solid_z", false, true));

        // m_illuminationProgram = hpl::loadProgram("vs_basic_solid_illumination", "fs_basic_solid_illumination");
        
        // m_s_normalMap = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
        // m_s_specularMap = bgfx::createUniform("s_specularMap", bgfx::UniformType::Sampler);
        // m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
        // m_s_envMapAlphaMap = bgfx::createUniform("s_envMapAlphaMap", bgfx::UniformType::Sampler);
        // m_s_heightMap = bgfx::createUniform("s_heightMap", bgfx::UniformType::Sampler);

        // m_s_dissolveMap = bgfx::createUniform("s_dissolveMap", bgfx::UniformType::Sampler);
        // m_s_dissolveAlphaMap = bgfx::createUniform("s_dissolveAlphaMap", bgfx::UniformType::Sampler);

        // m_s_envMap = bgfx::createUniform("s_envMap", bgfx::UniformType::Sampler);

        // m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
        // m_u_mtxUv = bgfx::createUniform("u_mtxUV", bgfx::UniformType::Mat4);
        // m_u_normalMtx = bgfx::createUniform("u_normalMtx", bgfx::UniformType::Mat4);
        // m_u_mtxInvViewRotation = bgfx::createUniform("u_mtxInvViewRotation", bgfx::UniformType::Mat4);
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
        if (apMaterial->GetImage(eMaterialTexture_Alpha))
        {
            apMaterial->SetAlphaMode(eMaterialAlphaMode_Trans);
        }
        else
        {
            apMaterial->SetAlphaMode(eMaterialAlphaMode_Solid);
        }

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


    void cMaterialType_SolidDiffuse::ResolveShaderProgram(
            eMaterialRenderMode aRenderMode,
            cViewport& viewport,
            cMaterial* apMaterial,
            iRenderable* apObject,
            iRenderer* apRenderer, 
			std::function<void(GraphicsContext::ShaderProgram&)> handler) {
        GraphicsContext::ShaderProgram program;
        cMatrixf mtxUv = apMaterial->HasUvAnimation() ? apMaterial->GetUvMatrix().GetTranspose() : cMatrixf::Identity;
        program.m_uniforms.push_back({m_u_mtxUv, &mtxUv.v});
        
        auto pipeline = Interface<ForgeRenderer>::Get();
        auto* pVars = static_cast<cMaterialType_SolidDiffuse_Vars*>(apMaterial->GetVars());
        switch(aRenderMode) {
            case eMaterialRenderMode_Diffuse: {

                auto& descriptorSet = pVars->m_descriptorSet[eMaterialRenderMode_Diffuse];

                auto diffuseMap = apMaterial->GetImage(eMaterialTexture_Diffuse);
                auto normalImage = apMaterial->GetImage(eMaterialTexture_NMap);
                auto specularMap = apMaterial->GetImage(eMaterialTexture_Specular);
                auto heightMap = apMaterial->GetImage(eMaterialTexture_Height);
                auto diffuseEnvMap = apMaterial->GetImage(eMaterialTexture_CubeMap);
                auto cubemapAlphaMap = apMaterial->GetImage(eMaterialTexture_CubeMapAlpha);
                
                std::array<eMaterialTexture, 6> poolTextures = {
                    eMaterialTexture_Diffuse,
                    eMaterialTexture_NMap,
                    eMaterialTexture_Specular,
                    eMaterialTexture_Height,
                    eMaterialTexture_CubeMap,
                    eMaterialTexture_CubeMapAlpha
                };
                // pipeline->CmdPool(0).AddMaterial(*apMaterial, poolTextures);
                
                std::array params = {
                    [&]{
                        DescriptorData data = {};
                        data.pName = "s_diffuseMap";
                        data.ppTextures = &diffuseMap->GetTexture().m_handle;
                        return data;
                    }()
                };

                updateDescriptorSet(
                    pipeline->Rend(), 0, descriptorSet.m_handle, params.size(), params.data());

                struct {
                    float heightMapScale;
                    float heightMapBias;
                    float fresnelBias;
                    float fresnelPow;

                    float useCubeMapAlpha;
                    float alphaReject;
                    float padding[2];
                } param = {0};
                param.alphaReject = 0.5f;
                uint32_t flags = 0;
               
                if(diffuseMap) {
                    program.m_textures.push_back({m_s_diffuseMap, diffuseMap->GetHandle(), 4});
                }
                
                if(normalImage) {
                    flags |= material::solid::DiffuseVariant::Diffuse_NormalMap;
                    program.m_textures.push_back({m_s_normalMap, normalImage->GetHandle(), 1});
                }
                if(specularMap) {
                    flags |= material::solid::DiffuseVariant::Diffuse_SpecularMap;
                    program.m_textures.push_back({m_s_specularMap, specularMap->GetHandle(), 2});
                }

                if(heightMap && iRenderer::GetParallaxEnabled()) {
                    flags |= material::solid::DiffuseVariant::Diffuse_ParallaxMap;
                    param.heightMapScale = pVars->mfHeightMapScale;
                    param.heightMapBias = pVars->mfHeightMapBias;
                    program.m_textures.push_back({m_s_heightMap, heightMap->GetHandle(), 3});
                }

                if(diffuseEnvMap) {
                    param.fresnelPow = pVars->mfFrenselPow;
                    param.fresnelBias = pVars->mfFrenselPow;

                    flags |= material::solid::DiffuseVariant::Diffuse_EnvMap;
                    program.m_textures.push_back({m_s_envMap, diffuseEnvMap->GetHandle(), 0});
                    if(cubemapAlphaMap) {
                        param.useCubeMapAlpha = 1.0f;
                        program.m_textures.push_back({m_s_envMapAlphaMap, cubemapAlphaMap->GetHandle(), 5});
                    }
                }
                program.m_uniforms.push_back({m_u_param, &param, 2});
                program.m_handle = m_diffuseProgramVariant.GetVariant(flags);
                handler(program);
                break;
            }
            case eMaterialRenderMode_Z: {
                BX_ASSERT(m_dissolveImage, "Dissolve image is not set");
                struct {
                    float alphaReject;
                    float padding[3];
                } param = {0};
                param.alphaReject = 0.5f;
                auto diffuseMap = apMaterial->GetImage(eMaterialTexture_Alpha);
                uint32_t flags = pVars->mbAlphaDissolveFilter ? material::solid::Z_UseDissolveFilter : 0;
                program.m_textures.push_back({m_s_dissolveMap, m_dissolveImage->GetHandle()});
                if(diffuseMap) {
                    flags |= material::solid::Z_UseAlphaMap;
                    program.m_textures.push_back({m_s_diffuseMap, diffuseMap->GetHandle(), 0});
                }
                program.m_uniforms.push_back({m_u_param, &param});
                program.m_handle = m_ZProgramVariant.GetVariant(flags);
                handler(program);
                break;
            }
            case eMaterialRenderMode_Z_Dissolve: {
                struct {
                    float alphaReject;
                    float padding[3];
                } param = {0};
                param.alphaReject = 0.5f;
                auto diffuseMap = apMaterial->GetImage(eMaterialTexture_Alpha);
                auto alphaDissolveMap = apMaterial->GetImage(eMaterialTexture_DissolveAlpha);
                uint32_t flags = pVars->mbAlphaDissolveFilter ? material::solid::Z_UseDissolveFilter : 0;
                if(diffuseMap) {
                    flags |= material::solid::Z_UseAlphaMap;
                    program.m_textures.push_back({m_s_diffuseMap, diffuseMap->GetHandle()});
                }
                program.m_textures.push_back({m_s_dissolveMap, m_dissolveImage->GetHandle()});
                if(alphaDissolveMap) {
                    flags |= material::solid::Z_UseDissolveAlphaMap;
                    program.m_textures.push_back({m_s_dissolveAlphaMap, alphaDissolveMap->GetHandle()});
                }
                program.m_uniforms.push_back({m_u_param, &param});
                program.m_handle = m_ZProgramVariant.GetVariant(flags);
                handler(program);
                break;
            }
            case eMaterialRenderMode_Illumination: {
                auto illumination = apMaterial->GetImage(eMaterialTexture_Illumination);
                struct {
                    float colorMul;
                    float padding[3];
                } param = {};
                param.colorMul = apObject->GetIlluminationAmount();

                if(illumination) {
                    program.m_textures.push_back({m_s_diffuseMap, illumination->GetHandle()});
                }
                program.m_uniforms.push_back({m_u_param, &param});
                program.m_handle = m_illuminationProgram;
                handler(program);
                break;
            }

            default:
                break;
        }
    }

    iMaterialVars* cMaterialType_SolidDiffuse::CreateSpecificVariables()
    {
        auto pVars = new cMaterialType_SolidDiffuse_Vars();
        auto pipeline = Interface<ForgeRenderer>::Get();
        DescriptorSetDesc desc = { pipeline->PipelineSignature(), DESCRIPTOR_UPDATE_FREQ_PER_BATCH, 1 };

        // DescriptorSet* descriptorSet = {};
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Diffuse].m_handle);
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Z].m_handle);
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Z_Dissolve].m_handle);
        // addDescriptorSet(pipeline->Rend(), &desc, &pVars->m_descriptorSet[eMaterialRenderMode_Illumination].m_handle);

        pVars->m_descriptorSet[eMaterialRenderMode_Diffuse].Initialize();
        pVars->m_descriptorSet[eMaterialRenderMode_Z].Initialize();
        pVars->m_descriptorSet[eMaterialRenderMode_Z_Dissolve].Initialize();
        pVars->m_descriptorSet[eMaterialRenderMode_Illumination].Initialize();
        
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
