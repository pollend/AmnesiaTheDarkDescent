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

#include "graphics/MaterialType_BasicTranslucent.h"

#include "bgfx/bgfx.h"
#include "graphics/BGFXProgram.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/ShaderUtil.h"
#include "math/MathTypes.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/String.h"

#include "resources/Resources.h"

#include "scene/Light.h"
#include "scene/World.h"

#include "math/Frustum.h"
#include "math/Math.h"

#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/RenderList.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "system/SystemTypes.h"
#include <iterator>

namespace hpl
{

    //------------------------------
    // Variables
    //------------------------------
    #define kVar_afAlpha 0
    #define kVar_avFogStartAndLength 1
    #define kVar_afOneMinusFogAlpha 2
    #define kVar_afFalloffExp 3
    #define kVar_a_mtxUV 4
    #define kVar_afRefractionScale 5
    #define kVar_a_mtxInvViewRotation 6
    #define kVar_avFrenselBiasPow 7
    #define kVar_avRimLightMulPow 8
    #define kVar_afLightLevel 9

    //------------------------------
    // Diffuse Features and data
    //------------------------------
    #define eFeature_Diffuse_Fog eFlagBit_0
    #define eFeature_Diffuse_UvAnimation eFlagBit_1
    #define eFeature_Diffuse_UseRefraction eFlagBit_2
    #define eFeature_Diffuse_NormalMap eFlagBit_3
    #define eFeature_Diffuse_EnvMap eFlagBit_4
    #define eFeature_Diffuse_DiffuseMap eFlagBit_5
    #define eFeature_Diffuse_CubeMapAlpha eFlagBit_6
    #define eFeature_Diffuse_UseScreenNormal eFlagBit_7

    #define kDiffuseFeatureNum 8


    namespace material::translucent
    {

        static const auto afAlpha = MemberID("afAlpha", kVar_afAlpha);
        static const auto avFogStartAndLength = MemberID("avFogStartAndLength", kVar_avFogStartAndLength);
        static const auto afOneMinusFogAlpha = MemberID("afOneMinusFogAlpha", kVar_afOneMinusFogAlpha);
        static const auto afFalloffExp = MemberID("afFalloffExp", kVar_afFalloffExp);
        static const auto a_mtxUV = MemberID("a_mtxUV", kVar_a_mtxUV);
        static const auto afRefractionScale = MemberID("afRefractionScale", kVar_afRefractionScale);
        static const auto a_mtxInvViewRotation = MemberID("a_mtxInvViewRotation", kVar_a_mtxInvViewRotation);
        static const auto avFrenselBiasPow = MemberID("avFrenselBiasPow", kVar_avFrenselBiasPow);
        static const auto avRimLightMulPow = MemberID("avRimLightMulPow", kVar_avRimLightMulPow);
        static const auto afLightLevel = MemberID("afLightLevel", kVar_afLightLevel);

        static const auto s_diffuseMap = MemberID("s_diffuseMap");
        static const auto s_normalMap = MemberID("s_normalMap");
        static const auto s_refractionMap = MemberID("s_refractionMap");
        static const auto s_envMapAlphaMap = MemberID("s_envMapAlphaMap");
        static const auto s_envMap = MemberID("s_envMap");
    }

    static cProgramComboFeature vDiffuseFeatureVec[] = {
        cProgramComboFeature("UseFog", kPC_FragmentBit | kPC_VertexBit),
        cProgramComboFeature("UseUvAnimation", kPC_VertexBit),
        cProgramComboFeature("UseRefraction", kPC_FragmentBit | kPC_VertexBit),
        cProgramComboFeature("UseNormalMapping", kPC_FragmentBit | kPC_VertexBit),
        cProgramComboFeature("UseEnvMap", kPC_FragmentBit | kPC_VertexBit),
        cProgramComboFeature("UseDiffuseMap", kPC_FragmentBit),
        cProgramComboFeature("UseCubeMapAlpha", kPC_FragmentBit),
        cProgramComboFeature("UseScreenNormal", kPC_FragmentBit),
    };

    //////////////////////////////////////////////////////////////////////////
    // TRANSLUCENT
    //////////////////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------

    cMaterialType_Translucent::cMaterialType_Translucent(cGraphics* apGraphics, cResources* apResources)
        : iMaterialType(apGraphics, apResources)
    {
        mbIsTranslucent = true;

        AddUsedTexture(eMaterialTexture_Diffuse);
        AddUsedTexture(eMaterialTexture_NMap);
        AddUsedTexture(eMaterialTexture_CubeMap);
        AddUsedTexture(eMaterialTexture_CubeMapAlpha);

        AddVarBool("Refraction", false, "If the material has refraction (distortion of bg). Uses NMap and/or normals of mesh");
        AddVarBool("RefractionEdgeCheck", true, "If true, there is no bleeding with foreground objects, but takes some extra power.");
        AddVarBool("RefractionNormals", false, "If normals should be used when refracting. If no NMap is set this is forced true!");
        AddVarBool("RefractionNormals", false, "If normals should be used when refracting. If no NMap is set this is forced true!");
        AddVarFloat("RefractionScale", 0.1f, "The amount refraction offsets the background");
        AddVarFloat(
            "FrenselBias",
            0.2f,
            "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at the surface.");
        AddVarFloat(
            "FrenselPow", 8.0f, "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.");
        AddVarFloat(
            "RimLightMul",
            0.0f,
            "The amount of rim light based on the reflection. This gives an edge to the object. Values: 0 - inf (although 1.0f should be "
            "used for max)");
        AddVarFloat("RimLightPow", 8.0f, "The sharpness of the rim lighting.");
        AddVarBool("AffectedByLightLevel", false, "The the material alpha is affected by the light level.");

        for (int i = 0; i < 5; ++i)
            mpBlendProgramManager[i] =
                hplNew(cProgramComboManager, ("Blend" + cString::ToString(i), mpGraphics, mpResources, eMaterialRenderMode_LastEnum));

        mbHasTypeSpecifics[eMaterialRenderMode_Diffuse] = true;
        mbHasTypeSpecifics[eMaterialRenderMode_DiffuseFog] = true;
        mbHasTypeSpecifics[eMaterialRenderMode_Illumination] = true;
        mbHasTypeSpecifics[eMaterialRenderMode_IlluminationFog] = true;
        
        _u_param = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, 6);
        _u_mtxUv = bgfx::createUniform("u_mtxUV", bgfx::UniformType::Mat4);
        _u_normalMtx = bgfx::createUniform("u_normalMtx", bgfx::UniformType::Mat4);
        _u_invViewRotation = bgfx::createUniform("mtxInvViewRotation", bgfx::UniformType::Mat4);
        
        _s_diffuseMap  = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
        _s_normalMap  = bgfx::createUniform("s_normalMap", bgfx::UniformType::Sampler);
        _s_refractionMap  = bgfx::createUniform("s_refractionMap", bgfx::UniformType::Sampler);
        _s_envMapAlphaMap  = bgfx::createUniform("s_envMapAlphaMap", bgfx::UniformType::Sampler);
        _s_envMap  = bgfx::createUniform("s_envMap", bgfx::UniformType::Sampler);

        _programHandle = hpl::loadProgram("vs_basic_translucent_material", "fs_basic_translucent_material");
    }

    cMaterialType_Translucent::~cMaterialType_Translucent()
    {
        for (int i = 0; i < 5; ++i)
            hplDelete(mpBlendProgramManager[i]);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::DestroyProgram(
        cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
    {
        int lProgramNum = apMaterial->GetBlendMode() - 1;

        // These render modes always use add!!
        if (aRenderMode == eMaterialRenderMode_Illumination || aRenderMode == eMaterialRenderMode_IlluminationFog)
        {
            lProgramNum = eMaterialBlendMode_Add - 1;
        }

        // Log("Destroying mat '%s' program '%s' / %d manager num: %d\n", apMaterial->GetName().c_str(),
        // apProgram->GetName().c_str(),apProgram, lProgramNum);
        mpBlendProgramManager[lProgramNum]->DestroyGeneratedProgram(eMaterialRenderMode_Diffuse, apProgram);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::LoadData()
    {
        for (int i = 0; i < 5; ++i)
        {
            cParserVarContainer defaultVars;
            defaultVars.Add("UseUv");
            defaultVars.Add("UseNormals");
            defaultVars.Add("UseColor");

            if (i == 0)
                defaultVars.Add("BlendMode_Add");
            if (i == 1)
                defaultVars.Add("BlendMode_Mul");
            if (i == 2)
                defaultVars.Add("BlendMode_MulX2");
            if (i == 3)
                defaultVars.Add("BlendMode_Alpha");
            if (i == 4)
                defaultVars.Add("BlendMode_PremulAlpha");

            mpBlendProgramManager[i]->SetupGenerateProgramData(
                eMaterialRenderMode_Diffuse,
                "Diffuse",
                "deferred_base_vtx.glsl",
                "deferred_transparent_frag.glsl",
                vDiffuseFeatureVec,
                kDiffuseFeatureNum,
                defaultVars);

            ////////////////////////////////
            // Set up variable ids
            mpBlendProgramManager[i]->AddGenerateProgramVariableId("afAlpha", kVar_afAlpha, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId(
                "avFogStartAndLength", kVar_avFogStartAndLength, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId(
                "afOneMinusFogAlpha", kVar_afOneMinusFogAlpha, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId("afFalloffExp", kVar_afFalloffExp, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId("a_mtxUV", kVar_a_mtxUV, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId(
                "afRefractionScale", kVar_afRefractionScale, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId(
                "a_mtxInvViewRotation", kVar_a_mtxInvViewRotation, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId("avFrenselBiasPow", kVar_avFrenselBiasPow, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId("avRimLightMulPow", kVar_avRimLightMulPow, eMaterialRenderMode_Diffuse);
            mpBlendProgramManager[i]->AddGenerateProgramVariableId("afLightLevel", kVar_afLightLevel, eMaterialRenderMode_Diffuse);
        }
    }
    void cMaterialType_Translucent::DestroyData()
    {
        for (int i = 0; i < 5; ++i)
            mpBlendProgramManager[i]->DestroyShadersAndPrograms();
    }

    //--------------------------------------------------------------------------

    iTexture* cMaterialType_Translucent::GetTextureForUnit(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, int alUnit)
    {
        cMaterialType_Translucent_Vars* pVars = (cMaterialType_Translucent_Vars*)apMaterial->GetVars();

        bool bRefractionEnabled = pVars->mbRefraction && iRenderer::GetRefractionEnabled();

        ////////////////////////////
        // Diffuse
        if (aRenderMode == eMaterialRenderMode_Diffuse || aRenderMode == eMaterialRenderMode_DiffuseFog)
        {
            switch (alUnit)
            {
            case 0:
                return apMaterial->GetTexture(eMaterialTexture_Diffuse);
            case 1:
                return apMaterial->GetTexture(eMaterialTexture_NMap);
            case 2:
                if (bRefractionEnabled)
                    return mpGraphics->GetRenderer(eRenderer_Main)->GetRefractionTexture();
                else
                    return NULL;
            case 3:
                return apMaterial->GetTexture(eMaterialTexture_CubeMap);
            case 4:
                return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            }
        }
        ////////////////////////////
        // Illumination
        else if (aRenderMode == eMaterialRenderMode_Illumination || aRenderMode == eMaterialRenderMode_IlluminationFog)
        {
            switch (alUnit)
            {
            case 1:
                return apMaterial->GetTexture(eMaterialTexture_NMap);
            case 3:
                return apMaterial->GetTexture(eMaterialTexture_CubeMap);
            case 4:
                return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            }
        }

        return NULL;
    }

    //--------------------------------------------------------------------------

    iTexture* cMaterialType_Translucent::GetSpecialTexture(
        cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iRenderer* apRenderer, int alUnit)
    {
        return NULL;
    }

    //--------------------------------------------------------------------------

    iGpuProgram* cMaterialType_Translucent::GetGpuProgram(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
    {
        cMaterialType_Translucent_Vars* pVars = (cMaterialType_Translucent_Vars*)apMaterial->GetVars();

        bool bRefractionEnabled = pVars->mbRefraction && iRenderer::GetRefractionEnabled();

        struct RefractionData
        {
            float normalMtx[16];
            float mtxInvViewRotation[16];
            float mtxUV[16];

            bgfx::TextureHandle s_diffuseMap = BGFX_INVALID_HANDLE;
            bgfx::TextureHandle s_normalMap = BGFX_INVALID_HANDLE;
            bgfx::TextureHandle s_refractionMap = BGFX_INVALID_HANDLE;
            bgfx::TextureHandle s_envMapAlphaMap = BGFX_INVALID_HANDLE;
            bgfx::TextureHandle s_envMap = BGFX_INVALID_HANDLE;

            struct
            {
                float useBlendModeAdd;
                float useBlendModeMul;
                float useBlendModeMulX2;
                float useBlendModeAlpha;

                float useBlendModePremulAlpha;
                float useEnvMap;
                float useDiffuseMap;
                float useFog;

                float useScreenNormal;
                float useNormalMap;
                float useRefraction;
                float fogStart;

                float fogLength;
                float alpha;
                float lightLevel;
                float oneMinusFogAlpha;

                float frenselBiasPow[2];
                float rimLightMulPow[2];

                float falloffExp;
                float refractionScale;
                float useCubeMapAlpha;
            } params;
        };
        using MaterialTranslucentProgram = BGFXProgram<RefractionData>;

        ////////////////////////////
        // Diffuse
        if (aRenderMode == eMaterialRenderMode_Diffuse || aRenderMode == eMaterialRenderMode_DiffuseFog)
        {
            int lProgramNum = apMaterial->GetBlendMode() - 1;
            eMaterialBlendMode blendMode = apMaterial->GetBlendMode();

            const bool hasDiffuseTexture = apMaterial->GetTexture(eMaterialTexture_Diffuse);
            const bool hasDiffuseFog = (aRenderMode == eMaterialRenderMode_DiffuseFog);
            const bool hasUVAnimation = apMaterial->HasUvAnimation();
            const bool hasDiffuseNormalMap = apMaterial->GetTexture(eMaterialTexture_NMap);

            const bool hasDiffuseEnvMap = bRefractionEnabled && apMaterial->GetTexture(eMaterialTexture_CubeMap);
            const bool hasCubeMapAlpha = bRefractionEnabled && apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            const bool useScreenSpaceNormal = pVars->mbRefractionNormals && bRefractionEnabled;

            tFlag lFlags = (hasDiffuseTexture ? eFeature_Diffuse_DiffuseMap : 0) | 
                (hasDiffuseFog ? eFeature_Diffuse_Fog : 0) |
                (hasUVAnimation ? eFeature_Diffuse_UvAnimation : 0) | 
                (hasDiffuseNormalMap ? eFeature_Diffuse_NormalMap : 0) |
                (hasDiffuseEnvMap ? eFeature_Diffuse_EnvMap : 0) | 
                (hasCubeMapAlpha ? eFeature_Diffuse_CubeMapAlpha : 0) |
                (bRefractionEnabled ? eFeature_Diffuse_UseRefraction : 0) | 
                (useScreenSpaceNormal ? eFeature_Diffuse_UseScreenNormal : 0);

            return mpBlendProgramManager[lProgramNum]->GenerateProgram(
                eMaterialRenderMode_Diffuse,
                lFlags,
                [blendMode,
                 hasDiffuseTexture,
                 hasDiffuseFog,
                 hasUVAnimation,
                 hasDiffuseNormalMap,
                 hasDiffuseEnvMap,
                 hasCubeMapAlpha,
                 bRefractionEnabled,
                 useScreenSpaceNormal,
                 u_param = _u_param,
                 u_mtxUv = _u_mtxUv,
                 u_normalMtx = _u_normalMtx,
                 u_invViewRotation = _u_invViewRotation,
                 programHandle = _programHandle](const tString& name)
                {
                    auto program = new MaterialTranslucentProgram(
                        {
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::afAlpha,
                                MaterialTranslucentProgram::FloatMapper(
                                    [](RefractionData& data, float afx)
                                    {
                                        data.params.alpha = afx;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::avFogStartAndLength,
                                MaterialTranslucentProgram::Vec2Mapper(
                                    [](RefractionData& data, float afx, float afy)
                                    {
                                        data.params.fogStart = afx;
                                        data.params.fogLength = afy;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::afOneMinusFogAlpha,
                                MaterialTranslucentProgram::FloatMapper(
                                    [](RefractionData& data, float afx)
                                    {
                                        data.params.oneMinusFogAlpha = afx;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::afFalloffExp,
                                MaterialTranslucentProgram::FloatMapper(
                                    [](RefractionData& data, float afx)
                                    {
                                        data.params.falloffExp = afx;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::a_mtxUV,
                                MaterialTranslucentProgram::Matrix4fMapper(
                                    [](RefractionData& data, const cMatrixf& value)
                                    {
                                        std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxUV));
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::afRefractionScale,
                                MaterialTranslucentProgram::FloatMapper(
                                    [](RefractionData& data, float afx)
                                    {
                                        data.params.refractionScale = afx;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::a_mtxInvViewRotation,
                                MaterialTranslucentProgram::Matrix4fMapper(
                                    [](RefractionData& data, const cMatrixf& value)
                                    {
                                        std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxInvViewRotation));
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::avFrenselBiasPow,
                                MaterialTranslucentProgram::Vec2Mapper(
                                    [](RefractionData& data, float afx, float afY)
                                    {
                                        data.params.frenselBiasPow[0] = afx;
                                        data.params.frenselBiasPow[1] = afY;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::avRimLightMulPow,
                                MaterialTranslucentProgram::Vec2Mapper(
                                    [](RefractionData& data, float afx, float afY)
                                    {
                                        data.params.rimLightMulPow[0] = afx;
                                        data.params.rimLightMulPow[1] = afY;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::afLightLevel,
                                MaterialTranslucentProgram::FloatMapper(
                                    [](RefractionData& data, float afx)
                                    {
                                        data.params.lightLevel = afx;
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_diffuseMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_diffuseMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_normalMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_normalMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_refractionMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_refractionMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_envMapAlphaMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_envMapAlphaMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_envMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_envMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                        },
                        name,
                        programHandle,
                        false,
                        eGpuProgramFormat_BGFX);



                    program->SetSubmitHandler(
                        [
                            u_param,
                            u_mtxUv,
                            u_normalMtx,
                            u_invViewRotation
                        ](const RefractionData& data, bgfx::ViewId view, GraphicsContext& context)
                        {
                            bgfx::setUniform(u_param, &data.params, 6);
                            bgfx::setUniform(u_mtxUv, &data.mtxUV);
                            bgfx::setUniform(u_normalMtx, &data.normalMtx);
                            bgfx::setUniform(u_invViewRotation, &data.mtxInvViewRotation);
                        });

                    program->data().params.useBlendModeAdd = (blendMode == eMaterialBlendMode_Add) ? 1.0f : 0.0f;
                    program->data().params.useBlendModeMul =  (blendMode == eMaterialBlendMode_Mul) ? 1.0f : 0.0f;
                    program->data().params.useBlendModeMulX2 =  (blendMode == eMaterialBlendMode_MulX2) ? 1.0f : 0.0f;
                    program->data().params.useBlendModeAlpha =  (blendMode == eMaterialBlendMode_Alpha) ? 1.0f : 0.0f;
                    program->data().params.useBlendModePremulAlpha =  (blendMode == eMaterialBlendMode_PremulAlpha) ? 1.0f : 0.0f;

                    program->data().params.useEnvMap = hasDiffuseEnvMap ? 1.0f : 0.0f;
                    program->data().params.useDiffuseMap = hasDiffuseTexture ? 1.0f : 0.0f;
                    program->data().params.useFog = hasDiffuseFog ? 1.0f : 0.0f;
                    program->data().params.useScreenNormal = useScreenSpaceNormal ? 1.0f : 0.0f;
                    program->data().params.useNormalMap = hasDiffuseNormalMap ? 1.0f : 0.0f;
                    program->data().params.useRefraction = bRefractionEnabled ? 1.0f : 0.0f;
                    program->data().params.useCubeMapAlpha = hasCubeMapAlpha ? 1.0f : 0.0f;
                    
                    return program;
                });
        }
        ////////////////////////////
        // Illumination
        if (aRenderMode == eMaterialRenderMode_Illumination || aRenderMode == eMaterialRenderMode_IlluminationFog)
        {
            if (bRefractionEnabled == false && apMaterial->GetTexture(eMaterialTexture_CubeMap))
            {
                int lProgramNum = apMaterial->GetBlendMode() - 1;
                eMaterialBlendMode blendMode = apMaterial->GetBlendMode();

                const bool hasDiffuseFog = (aRenderMode == eMaterialRenderMode_IlluminationFog);
                const bool hasDiffuseNormalMap = apMaterial->GetTexture(eMaterialTexture_NMap);
                const bool hasEnvMap = apMaterial->GetTexture(eMaterialTexture_CubeMap);
                const bool hasCubeMapAlpha = hasEnvMap && apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);

                tFlag lFlags = (hasDiffuseFog ? eFeature_Diffuse_Fog : 0) | 
                            (hasDiffuseNormalMap ? eFeature_Diffuse_NormalMap : 0) |
                            (hasEnvMap ? eFeature_Diffuse_EnvMap : 0) | 
                            (hasCubeMapAlpha ? eFeature_Diffuse_CubeMapAlpha : 0);

                return mpBlendProgramManager[lProgramNum]->GenerateProgram(
                    eMaterialRenderMode_Diffuse,
                    lFlags,
                    [hasDiffuseFog,
                     hasDiffuseNormalMap,
                     hasEnvMap,
                     hasCubeMapAlpha,
                     blendMode,
                     u_param = _u_param,
                     u_mtxUv = _u_mtxUv,
                     u_normalMtx = _u_normalMtx,
                     u_invViewRotation = _u_invViewRotation,
                     programHandle = _programHandle,
                     s_diffuseMap = _s_diffuseMap,
                     s_normalMap = _s_normalMap,
                     s_refractionMap = _s_refractionMap,
                     s_envMapAlphaMap = _s_envMapAlphaMap,
                     s_envMap = _s_envMap](const tString& name)
                    {
                        auto program = new MaterialTranslucentProgram(
                            {
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::afAlpha,
                                    MaterialTranslucentProgram::FloatMapper(
                                        [](RefractionData& data, float afx)
                                        {
                                            data.params.alpha = afx;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::avFogStartAndLength,
                                    MaterialTranslucentProgram::Vec2Mapper(
                                        [](RefractionData& data, float afx, float afy)
                                        {
                                            data.params.fogStart = afx;
                                            data.params.fogLength = afy;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::afOneMinusFogAlpha,
                                    MaterialTranslucentProgram::FloatMapper(
                                        [](RefractionData& data, float afx)
                                        {
                                            data.params.oneMinusFogAlpha = afx;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::afFalloffExp,
                                    MaterialTranslucentProgram::FloatMapper(
                                        [](RefractionData& data, float afx)
                                        {
                                            data.params.falloffExp = afx;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::a_mtxUV,
                                    MaterialTranslucentProgram::Matrix4fMapper(
                                        [](RefractionData& data, const cMatrixf& value)
                                        {
                                            std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxUV));
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::afRefractionScale,
                                    MaterialTranslucentProgram::FloatMapper(
                                        [](RefractionData& data, float afx)
                                        {
                                            data.params.refractionScale = afx;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::a_mtxInvViewRotation,
                                    MaterialTranslucentProgram::Matrix4fMapper(
                                        [](RefractionData& data, const cMatrixf& value)
                                        {
                                            std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxInvViewRotation));
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::avFrenselBiasPow,
                                    MaterialTranslucentProgram::Vec2Mapper(
                                        [](RefractionData& data, float afx, float afY)
                                        {
                                            data.params.frenselBiasPow[0] = afx;
                                            data.params.frenselBiasPow[1] = afY;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::avRimLightMulPow,
                                    MaterialTranslucentProgram::Vec2Mapper(
                                        [](RefractionData& data, float afx, float afY)
                                        {
                                            data.params.rimLightMulPow[0] = afx;
                                            data.params.rimLightMulPow[1] = afY;
                                        })),
                                MaterialTranslucentProgram::ParameterField(
                                    material::translucent::afLightLevel,
                                    MaterialTranslucentProgram::FloatMapper(
                                        [](RefractionData& data, float afx)
                                        {
                                            data.params.lightLevel = afx;
                                        })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_diffuseMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_diffuseMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_normalMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_normalMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_refractionMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_refractionMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_envMapAlphaMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_envMapAlphaMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            MaterialTranslucentProgram::ParameterField(
                                material::translucent::s_envMap,
                                MaterialTranslucentProgram::ImageMapper(
                                    [](RefractionData& data,const Image* value)
                                    {
                                        data.s_envMap = value ? value->GetHandle() : bgfx::TextureHandle{BGFX_INVALID_HANDLE};
                                    })),
                            },
                            name,
                            programHandle,
                            false,
                            eGpuProgramFormat_BGFX);
                        program->SetSubmitHandler(
                            [u_param,
                             u_mtxUv,
                             u_normalMtx,
                             u_invViewRotation,
                             s_diffuseMap,
                             s_normalMap,
                             s_refractionMap,
                             s_envMapAlphaMap,
                             s_envMap](const RefractionData& data, bgfx::ViewId view, GraphicsContext& context)
                            {
                                bgfx::setUniform(u_param, &data.params, 6);
                                bgfx::setUniform(u_mtxUv, &data.mtxUV);
                                bgfx::setUniform(u_normalMtx, &data.normalMtx);
                                bgfx::setUniform(u_invViewRotation, &data.mtxInvViewRotation);

                                bgfx::setTexture(0, s_diffuseMap, data.s_diffuseMap);
                                bgfx::setTexture(1, s_normalMap, data.s_normalMap);
                                bgfx::setTexture(2, s_refractionMap, data.s_refractionMap);
                                bgfx::setTexture(3, s_envMapAlphaMap, data.s_envMapAlphaMap);
                                bgfx::setTexture(4, s_envMap, data.s_envMap);
                            });

                        program->data().params.useBlendModeAdd = (blendMode == eMaterialBlendMode_Add) ? 1.0f : 0.0f;
                        program->data().params.useBlendModeMul = (blendMode == eMaterialBlendMode_Mul) ? 1.0f : 0.0f;
                        program->data().params.useBlendModeMulX2 = (blendMode == eMaterialBlendMode_MulX2) ? 1.0f : 0.0f;
                        program->data().params.useBlendModeAlpha = (blendMode == eMaterialBlendMode_Alpha) ? 1.0f : 0.0f;
                        program->data().params.useBlendModePremulAlpha = (blendMode == eMaterialBlendMode_PremulAlpha) ? 1.0f : 0.0f;

                        program->data().params.useEnvMap = 0.0f;
                        program->data().params.useDiffuseMap = 0.0f;
                        program->data().params.useFog = hasDiffuseFog ? 1.0f : 0.0f;
                        program->data().params.useScreenNormal = 0.0f;
                        program->data().params.useNormalMap = hasDiffuseNormalMap ? 1.0f : 0.0f;
                        program->data().params.useRefraction = 0.0f;
                        program->data().params.useCubeMapAlpha = hasCubeMapAlpha ? 1.0f : 0.0f;

                        return program;
                    });
            }
        }

        return NULL;
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer* apRenderer)
    {
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::SetupMaterialSpecificData(
        eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial* apMaterial, iRenderer* apRenderer)
    {
    }

    //--------------------------------------------------------------------------

    static inline float GetMaxColorValue(const cColor& aCol)
    {
        return cMath::Max(cMath::Max(aCol.r, aCol.g), aCol.b);
    }

    void cMaterialType_Translucent::SetupObjectSpecificData(
        eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable* apObject, iRenderer* apRenderer)
    {
        
    }

    void cMaterialType_Translucent::SubmitMaterial(
        bgfx::ViewId id,
        GraphicsContext& context,
        eMaterialRenderMode aRenderMode,
        iGpuProgram* apProgram,
        cMaterial* apMaterial,
        iRenderable* apObject,
        iRenderer* apRenderer)
    {
        cMaterialType_Translucent_Vars* pVars = (cMaterialType_Translucent_Vars*)apObject->GetMaterial()->GetVars();

        bool bIlluminationPass = (aRenderMode == eMaterialRenderMode_IlluminationFog || aRenderMode == eMaterialRenderMode_Illumination);
        bool bRefractionEnabled = pVars->mbRefraction && iRenderer::GetRefractionEnabled();


        ////////////////////////////
        // Diffuse
        if (aRenderMode == eMaterialRenderMode_Diffuse || aRenderMode == eMaterialRenderMode_DiffuseFog)
        {
            apProgram->setImage(material::translucent::s_diffuseMap, apMaterial->GetImage(eMaterialTexture_Diffuse));
            apProgram->setImage(material::translucent::s_normalMap, apMaterial->GetImage(eMaterialTexture_NMap));
            apProgram->setImage(material::translucent::s_refractionMap,  nullptr);
            apProgram->setImage(material::translucent::s_envMap, apMaterial->GetImage(eMaterialTexture_CubeMap));
            apProgram->setImage(material::translucent::s_envMapAlphaMap, apMaterial->GetImage(eMaterialTexture_CubeMapAlpha));

            // switch (alUnit)
            // {
            // case 0:
            //     return apMaterial->GetTexture(eMaterialTexture_Diffuse);
            // case 1:
            //     return apMaterial->GetTexture(eMaterialTexture_NMap);
            // case 2:
            //     if (bRefractionEnabled)
            //         return mpGraphics->GetRenderer(eRenderer_Main)->GetRefractionTexture();
            //     else
            //         return NULL;
            // case 3:
            //     return apMaterial->GetTexture(eMaterialTexture_CubeMap);
            // case 4:
            //     return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            // }
        }
        ////////////////////////////
        // Illumination
        else if (aRenderMode == eMaterialRenderMode_Illumination || aRenderMode == eMaterialRenderMode_IlluminationFog)
        {

            apProgram->setImage(material::translucent::s_normalMap, apMaterial->GetImage(eMaterialTexture_NMap));
            apProgram->setImage(material::translucent::s_envMap, apMaterial->GetImage(eMaterialTexture_CubeMap));
            apProgram->setImage(material::translucent::s_envMapAlphaMap, apMaterial->GetImage(eMaterialTexture_CubeMapAlpha));

            // switch (alUnit)
            // {
            // case 1:
            //     return apMaterial->GetTexture(eMaterialTexture_NMap);
            // case 3:
            //     return apMaterial->GetTexture(eMaterialTexture_CubeMap);
            // case 4:
            //     return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            // }
        }

        /////////////////////////
        // UV Animation
        if (apMaterial->HasUvAnimation())
        {
            apProgram->SetMatrixf(kVar_a_mtxUV, apMaterial->GetUvMatrix());
        }

        ////////////////////////////
        // Reflection vars
        if (apMaterial->GetTexture(eMaterialTexture_CubeMap) && (bRefractionEnabled && bIlluminationPass == false) ||
            (bRefractionEnabled == false && bIlluminationPass))
        {
            cMatrixf mtxInvView = apRenderer->GetCurrentFrustum()->GetViewMatrix().GetTranspose();
            apProgram->SetMatrixf(kVar_a_mtxInvViewRotation, mtxInvView.GetRotation());

            apProgram->SetVec2f(kVar_avFrenselBiasPow, cVector2f(pVars->mfFrenselBias, pVars->mfFrenselPow));
            apProgram->SetVec2f(kVar_avRimLightMulPow, cVector2f(pVars->mfRimLightMul, pVars->mfRimLightPow));
        }

        ////////////////////////////
        // Refraction vars
        if (bRefractionEnabled && (aRenderMode == eMaterialRenderMode_DiffuseFog || aRenderMode == eMaterialRenderMode_Diffuse))
        {
            apProgram->SetFloat(kVar_afRefractionScale, pVars->mfRefractionScale * (float)apRenderer->GetRenderTargetSize().x);
        }

        ////////////////////////////
        // Fog
        if (aRenderMode == eMaterialRenderMode_DiffuseFog || aRenderMode == eMaterialRenderMode_IlluminationFog)
        {
            cWorld* pWorld = apRenderer->GetCurrentWorld();

            apProgram->SetVec2f(kVar_avFogStartAndLength, cVector2f(pWorld->GetFogStart(), pWorld->GetFogEnd() - pWorld->GetFogStart()));
            apProgram->SetFloat(kVar_afOneMinusFogAlpha, 1 - pWorld->GetFogColor().a);
            apProgram->SetFloat(kVar_afFalloffExp, pWorld->GetFogFalloffExp());
        }
        ////////////////////////////
        // Light affects Alpha
        if (pVars->mbAffectedByLightLevel)
        {
            cVector3f vCenterPos = apObject->GetBoundingVolume()->GetWorldCenter();
            cRenderList* pRenderList = apRenderer->GetCurrentRenderList();

            float fLightAmount = 0.0f;

            ////////////////////////////////////////
            // Iterate lights and add light amount
            for (int i = 0; i < pRenderList->GetLightNum(); ++i)
            {
                iLight* pLight = pRenderList->GetLight(i);

                // Check if there is an intersection
                if (pLight->CheckObjectIntersection(apObject))
                {
                    if (pLight->GetLightType() == eLightType_Box)
                    {
                        fLightAmount += GetMaxColorValue(pLight->GetDiffuseColor());
                    }
                    else
                    {
                        float fDist = cMath::Vector3Dist(pLight->GetWorldPosition(), vCenterPos);

                        fLightAmount +=
                            GetMaxColorValue(pLight->GetDiffuseColor()) * cMath::Max(1.0f - (fDist / pLight->GetRadius()), 0.0f);
                    }

                    if (fLightAmount >= 1.0f)
                    {
                        fLightAmount = 1.0f;
                        break;
                    }
                }
            }

            ////////////////////////////////////////
            // Set up variable
            apProgram->SetFloat(kVar_afAlpha, apRenderer->GetTempAlpha());
            apProgram->SetFloat(kVar_afLightLevel, fLightAmount);
        }
        else
        {
            apProgram->SetFloat(kVar_afAlpha, apRenderer->GetTempAlpha());
            apProgram->SetFloat(kVar_afLightLevel, 1.0f);
        }
        apProgram->Submit(id, context);
    }

    //--------------------------------------------------------------------------

    iMaterialVars* cMaterialType_Translucent::CreateSpecificVariables()
    {
        cMaterialType_Translucent_Vars* pVars = hplNew(cMaterialType_Translucent_Vars, ());

        return pVars;
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
        cMaterialType_Translucent_Vars* pVars = (cMaterialType_Translucent_Vars*)apMaterial->GetVars();
        if (pVars == NULL)
        {
            pVars = (cMaterialType_Translucent_Vars*)CreateSpecificVariables();
            apMaterial->SetVars(pVars);
        }

        pVars->mbRefraction = apVars->GetVarBool("Refraction", false);
        pVars->mbRefractionEdgeCheck = apVars->GetVarBool("RefractionEdgeCheck", true);
        pVars->mbRefractionNormals = apVars->GetVarBool("RefractionNormals", true);
        pVars->mfRefractionScale = apVars->GetVarFloat("RefractionScale", 1.0f);
        pVars->mfFrenselBias = apVars->GetVarFloat("FrenselBias", 0.2f);
        pVars->mfFrenselPow = apVars->GetVarFloat("FrenselPow", 8.0);
        pVars->mfRimLightMul = apVars->GetVarFloat("RimLightMul", 0.0f);
        pVars->mfRimLightPow = apVars->GetVarFloat("RimLightPow", 8.0f);
        pVars->mbAffectedByLightLevel = apVars->GetVarBool("AffectedByLightLevel", false);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
        cMaterialType_Translucent_Vars* pVars = (cMaterialType_Translucent_Vars*)apMaterial->GetVars();

        apVars->AddVarBool("Refraction", pVars->mbRefraction);
        apVars->AddVarBool("RefractionEdgeCheck", pVars->mbRefractionEdgeCheck);
        apVars->AddVarBool("RefractionNormals", pVars->mbRefractionNormals);
        apVars->AddVarFloat("RefractionScale", pVars->mfRefractionScale);
        apVars->AddVarFloat("FrenselBias", pVars->mfFrenselBias);
        apVars->AddVarFloat("FrenselPow", pVars->mfFrenselPow);
        apVars->AddVarFloat("RimLightMul", pVars->mfRimLightMul);
        apVars->AddVarFloat("RimLightPow", pVars->mfRimLightPow);
        apVars->AddVarBool("AffectedByLightLevel", pVars->mbAffectedByLightLevel);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_Translucent::CompileMaterialSpecifics(cMaterial* apMaterial)
    {
        cMaterialType_Translucent_Vars* pVars = static_cast<cMaterialType_Translucent_Vars*>(apMaterial->GetVars());

        /////////////////////////////////////
        // Set up specifics
        apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
        apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Diffuse, true);

        apMaterial->SetHasSpecificSettings(eMaterialRenderMode_DiffuseFog, true);
        apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_DiffuseFog, true);

        apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Illumination, true);
        apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Illumination, true);

        apMaterial->SetHasSpecificSettings(eMaterialRenderMode_IlluminationFog, true);
        apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_IlluminationFog, true);

        /////////////////////////////////////
        // Set up the refraction
        bool bRefractionEnabled = pVars->mbRefraction && iRenderer::GetRefractionEnabled();

        if (bRefractionEnabled)
        {
            apMaterial->SetHasRefraction(true);
            apMaterial->SetUseRefractionEdgeCheck(pVars->mbRefractionEdgeCheck);
            // Note: No need to set blend mode to None since rendered sets that when refraction is true!
            //		Also, this gives problems when recompiling, since the material data would not be accurate!
        }

        /////////////////////////////////////
        // Set up the reflections
        if (apMaterial->GetTexture(eMaterialTexture_CubeMap))
        {
            if (bRefractionEnabled == false)
                apMaterial->SetHasTranslucentIllumination(true);
        }
    }

    //--------------------------------------------------------------------------

} // namespace hpl
