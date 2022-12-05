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
#include "graphics/BGFXProgram.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/MemberID.h"
#include "graphics/ShaderUtil.h"
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
#include "graphics/ProgramComboManager.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"
#include "system/SystemTypes.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>

namespace hpl
{

//////////////////////////////////////////////////////////////////////////
// DEFINES
//////////////////////////////////////////////////////////////////////////

//------------------------------
// Variables
//------------------------------
#define kVar_afInvFarPlane 0
#define kVar_avHeightMapScaleAndBias 1
#define kVar_a_mtxUV 2
#define kVar_afColorMul 3
#define kVar_afDissolveAmount 4
#define kVar_avFrenselBiasPow 5
#define kVar_a_mtxInvViewRotation 6
#define kVar_a_Flag 7

//------------------------------
// Diffuse Features and data
//------------------------------
#define eFeature_Diffuse_NormalMaps eFlagBit_0
#define eFeature_Diffuse_Specular eFlagBit_1
#define eFeature_Diffuse_Parallax eFlagBit_2
#define eFeature_Diffuse_UvAnimation eFlagBit_3
#define eFeature_Diffuse_Skeleton eFlagBit_4
#define eFeature_Diffuse_EnvMap eFlagBit_5
#define eFeature_Diffuse_CubeMapAlpha eFlagBit_6

#define kDiffuseFeatureNum 7

    static cProgramComboFeature vDiffuseFeatureVec[] = {
        cProgramComboFeature("UseNormalMapping", kPC_VertexBit | kPC_FragmentBit),
        cProgramComboFeature("UseSpecular", kPC_FragmentBit),
        cProgramComboFeature("UseParallax", kPC_VertexBit | kPC_FragmentBit, eFeature_Diffuse_NormalMaps),
        cProgramComboFeature("UseUvAnimation", kPC_VertexBit),
        cProgramComboFeature("UseSkeleton", kPC_VertexBit),
        cProgramComboFeature("UseEnvMap", kPC_VertexBit | kPC_FragmentBit),
        cProgramComboFeature("UseCubeMapAlpha", kPC_FragmentBit),
    };

//------------------------------
// Illumination Features and data
//------------------------------
#define eFeature_Illum_UvAnimation eFlagBit_0
#define eFeature_Illum_Skeleton eFlagBit_1

#define kIllumFeatureNum 2

    cProgramComboFeature vIllumFeatureVec[] = {
        cProgramComboFeature("UseUvAnimation", kPC_VertexBit),
        cProgramComboFeature("UseSkeleton", kPC_VertexBit),
    };

//------------------------------
// Z Features and data
//------------------------------
#define eFeature_Z_UseAlpha eFlagBit_0
#define eFeature_Z_UvAnimation eFlagBit_1
#define eFeature_Z_Dissolve eFlagBit_2
#define eFeature_Z_DissolveAlpha eFlagBit_3
#define eFeature_Z_UseAlphaDissolveFilter eFlagBit_4

#define kZFeatureNum 5

    cProgramComboFeature vZFeatureVec[] = { cProgramComboFeature("UseAlphaMap", kPC_FragmentBit),
                                            cProgramComboFeature("UseUvAnimation", kPC_VertexBit),
                                            cProgramComboFeature("UseDissolve", kPC_FragmentBit),
                                            cProgramComboFeature("UseDissolveAlphaMap", kPC_FragmentBit),
                                            cProgramComboFeature("UseAlphaUseDissolveFilter", kPC_FragmentBit) };

    namespace material::solid
    {
        static const auto afInvFarPlane = MemberID("afInvFarPlane", kVar_afInvFarPlane);
        static const auto avHeightMapScaleAndBias = MemberID("avHeightMapScaleAndBias", kVar_avHeightMapScaleAndBias);
        static const auto a_mtxUV = MemberID("a_mtxUV", kVar_a_mtxUV);
        static const auto afColorMul = MemberID("afColorMul", kVar_afColorMul);
        static const auto afDissolveAmount = MemberID("afDissolveAmount", kVar_afDissolveAmount);
        static const auto avFrenselBiasPow = MemberID("avFrenselBiasPow", kVar_avFrenselBiasPow);
        static const auto mtxInvViewRotation = MemberID("a_mtxInvViewRotation", kVar_a_mtxInvViewRotation);
    } // namespace material::solid

    //------------------------------

    //////////////////////////////////////////////////////////////////////////
    // STATIC OBJECTS
    //////////////////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------

    bool iMaterialType_SolidBase::mbGlobalDataCreated = false;
    cProgramComboManager* iMaterialType_SolidBase::mpGlobalProgramManager;

    float iMaterialType_SolidBase::mfVirtualPositionAddScale = 0.03f;

    iMaterialType_SolidBase::iMaterialType_SolidBase(cGraphics* apGraphics, cResources* apResources)
        : iMaterialType(apGraphics, apResources)
    {
        _basicSolidZProgram = hpl::loadProgram("vs_basic_solid_z", "fs_basic_solid_z");
        _basicSolidDiffuseProgram = hpl::loadProgram("vs_basic_solid_diffuse", "fs_basic_solid_diffuse");
        _illuminationProgram = hpl::loadProgram("vs_basic_solid_illumination", "fs_basic_solid_illumination");
        _u_param = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, 1);
        _u_mtxUv = bgfx::createUniform("a_mtxUV", bgfx::UniformType::Mat4, 1);
        _u_mtxInvViewRotation = bgfx::createUniform("u_mtxInvViewRotation", bgfx::UniformType::Mat4, 1);
        _s_diffuseMap = bgfx::createUniform("diffuseMap", bgfx::UniformType::Sampler, 1);
        _s_dissolveMap = bgfx::createUniform("dissolveMap", bgfx::UniformType::Sampler, 1);

        mbIsGlobalDataCreator = false;
    }

    //--------------------------------------------------------------------------

    iMaterialType_SolidBase::~iMaterialType_SolidBase()
    {
    }

    //--------------------------------------------------------------------------

    void iMaterialType_SolidBase::DestroyProgram(
        cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, char alSkeleton)
    {
        /////////////////////////////
        // Remove from global manager
        if (aRenderMode == eMaterialRenderMode_Z || aRenderMode == eMaterialRenderMode_Z_Dissolve)
        {
            mpGlobalProgramManager->DestroyGeneratedProgram(eMaterialRenderMode_Z, apProgram);
        }
        /////////////////////////////
        // Remove from normal manager
        else
        {
            mpProgramManager->DestroyGeneratedProgram(aRenderMode, apProgram);
        }
    }

    //--------------------------------------------------------------------------

    void iMaterialType_SolidBase::CreateGlobalPrograms()
    {
        if (mbGlobalDataCreated)
            return;

        mbGlobalDataCreated = true;
        mbIsGlobalDataCreator = true;

        /////////////////////////////
        // Load programs
        // This makes this material's program manager responsible for managing the global programs!
        cParserVarContainer defaultVars;
        defaultVars.Add("UseUv");

        mpProgramManager->SetupGenerateProgramData(
            eMaterialRenderMode_Z, "Z", "vs_deferred_base", "fs_deferred_base", vZFeatureVec, kZFeatureNum, defaultVars);

        mpProgramManager->AddGenerateProgramVariableId("a_mtxUV", kVar_a_mtxUV, eMaterialRenderMode_Z);
        mpProgramManager->AddGenerateProgramVariableId("afDissolveAmount", kVar_afDissolveAmount, eMaterialRenderMode_Z);

        mpGlobalProgramManager = mpProgramManager;
    }

    //--------------------------------------------------------------------------

    void iMaterialType_SolidBase::LoadData()
    {
        /////////////////////////////
        // Global data init (that is shared between Solid materials)
        CreateGlobalPrograms();

        //////////////
        // Create textures
        mpDissolveTexture = mpResources->GetTextureManager()->Create2D("core_dissolve.tga", true);

        LoadSpecificData();
    }

    //--------------------------------------------------------------------------

    void iMaterialType_SolidBase::DestroyData()
    {
        if (mpDissolveTexture)
            mpResources->GetTextureManager()->Destroy(mpDissolveTexture);

        // If this instace was global data creator, then it needs to be recreated.
        if (mbIsGlobalDataCreator)
        {
            mbGlobalDataCreated = false;
        }
        mpProgramManager->DestroyShadersAndPrograms();
    }

    //--------------------------------------------------------------------------

    void iMaterialType_SolidBase::LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
    }

    void iMaterialType_SolidBase::GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars)
    {
    }

    //--------------------------------------------------------------------------

    void iMaterialType_SolidBase::CompileMaterialSpecifics(cMaterial* apMaterial)
    {
        ////////////////////////
        // If there is an alpha texture, set alpha mode to trans, else solid.
        if (apMaterial->GetTexture(eMaterialTexture_Alpha))
        {
            apMaterial->SetAlphaMode(eMaterialAlphaMode_Trans);
        }
        else
        {
            apMaterial->SetAlphaMode(eMaterialAlphaMode_Solid);
        }

        CompileSolidSpecifics(apMaterial);
    }

    //--------------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // SOLID DIFFUSE
    //////////////////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------

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

    //--------------------------------------------------------------------------

    cMaterialType_SolidDiffuse::~cMaterialType_SolidDiffuse()
    {
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::LoadSpecificData()
    {
        /////////////////////////////
        // Load Diffuse programs
        cParserVarContainer defaultVars;
        defaultVars.Add("UseUv");
        defaultVars.Add("UseNormals");
        defaultVars.Add("UseDepth");
        defaultVars.Add("VirtualPositionAddScale", mfVirtualPositionAddScale);

        // Get the G-buffer type
        if (cRendererDeferred::GetGBufferType() == eDeferredGBuffer_32Bit)
            defaultVars.Add("Deferred_32bit");
        else
            defaultVars.Add("Deferred_64bit");

        // Set up number of gbuffer textures used
        if (cRendererDeferred::GetNumOfGBufferTextures() == 4)
            defaultVars.Add("RenderTargets_4");
        else
            defaultVars.Add("RenderTargets_3");

        // Set up relief mapping method
        if (iRenderer::GetParallaxQuality() != eParallaxQuality_Low && mpGraphics->GetLowLevel()->GetCaps(eGraphicCaps_ShaderModel_3) != 0)
        {
            defaultVars.Add("ParallaxMethod_Relief");
        }
        else
        {
            defaultVars.Add("ParallaxMethod_Simple");
        }

        mpProgramManager->SetupGenerateProgramData(
            eMaterialRenderMode_Diffuse,
            "Diffuse",
            "deferred_base_vtx.glsl",
            "deferred_gbuffer_solid_frag.glsl",
            vDiffuseFeatureVec,
            kDiffuseFeatureNum,
            defaultVars);

        /////////////////////////////
        // Load Illumination programs
        defaultVars.Clear();
        defaultVars.Add("UseUv");
        mpProgramManager->SetupGenerateProgramData(
            eMaterialRenderMode_Illumination,
            "Illum",
            "deferred_base_vtx.glsl",
            "deferred_illumination_frag.glsl",
            vIllumFeatureVec,
            kIllumFeatureNum,
            defaultVars);

        mpProgramManager->AddGenerateProgramVariableId("afInvFarPlane", kVar_afInvFarPlane, eMaterialRenderMode_Diffuse);
        mpProgramManager->AddGenerateProgramVariableId(
            "avHeightMapScaleAndBias", kVar_avHeightMapScaleAndBias, eMaterialRenderMode_Diffuse);
        mpProgramManager->AddGenerateProgramVariableId("a_mtxUV", kVar_a_mtxUV, eMaterialRenderMode_Diffuse);
        mpProgramManager->AddGenerateProgramVariableId("avFrenselBiasPow", kVar_avFrenselBiasPow, eMaterialRenderMode_Diffuse);
        mpProgramManager->AddGenerateProgramVariableId("a_mtxInvViewRotation", kVar_a_mtxInvViewRotation, eMaterialRenderMode_Diffuse);

        mpProgramManager->AddGenerateProgramVariableId("a_mtxUV", kVar_a_mtxUV, eMaterialRenderMode_Illumination);
        mpProgramManager->AddGenerateProgramVariableId("afColorMul", kVar_afColorMul, eMaterialRenderMode_Illumination);
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::CompileSolidSpecifics(cMaterial* apMaterial)
    {
        cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

        //////////////////////////////////
        // Z specifics
        apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Z_Dissolve, true);
        apMaterial->SetUseAlphaDissolveFilter(pVars->mbAlphaDissolveFilter);

        //////////////////////////////////
        // Normal map and height specifics
        if (apMaterial->GetTexture(eMaterialTexture_NMap))
        {
            if (apMaterial->GetTexture(eMaterialTexture_Height))
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
        if (apMaterial->GetTexture(eMaterialTexture_CubeMap))
        {
            apMaterial->SetHasSpecificSettings(eMaterialRenderMode_Diffuse, true);
        }

        //////////////////////////////////
        // Illuminations specifics
        if (apMaterial->GetTexture(eMaterialTexture_Illumination))
        {
            apMaterial->SetHasObjectSpecificsSettings(eMaterialRenderMode_Illumination, true);
        }
    }

    //--------------------------------------------------------------------------

    iTexture* cMaterialType_SolidDiffuse::GetTextureForUnit(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, int alUnit)
    {
        cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

        ////////////////////////////
        // Z
        if (aRenderMode == eMaterialRenderMode_Z)
        {
            switch (alUnit)
            {
            case 0:
                return apMaterial->GetTexture(eMaterialTexture_Alpha);
            case 1:
                return mpDissolveTexture;
            }
        }
        ////////////////////////////
        // Z Dissolve
        else if (aRenderMode == eMaterialRenderMode_Z_Dissolve)
        {
            switch (alUnit)
            {
            case 0:
                return apMaterial->GetTexture(eMaterialTexture_Alpha);
            case 1:
                return mpDissolveTexture;
            case 2:
                return apMaterial->GetTexture(eMaterialTexture_DissolveAlpha);
            }
        }
        ////////////////////////////
        // Diffuse
        else if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            switch (alUnit)
            {
            case 0:
                return apMaterial->GetTexture(eMaterialTexture_Diffuse);
            case 1:
                return apMaterial->GetTexture(eMaterialTexture_NMap);
            case 2:
                return apMaterial->GetTexture(eMaterialTexture_Specular);
            case 3:
                return apMaterial->GetTexture(eMaterialTexture_Height);
            case 4:
                return apMaterial->GetTexture(eMaterialTexture_CubeMap);
            case 5:
                return apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            }
        }
        ////////////////////////////
        // Illumination
        else if (aRenderMode == eMaterialRenderMode_Illumination)
        {
            switch (alUnit)
            {
            case 0:
                return apMaterial->GetTexture(eMaterialTexture_Illumination);
            }
        }

        return NULL;
    }
    //--------------------------------------------------------------------------

    iTexture* cMaterialType_SolidDiffuse::GetSpecialTexture(
        cMaterial* apMaterial, eMaterialRenderMode aRenderMode, iRenderer* apRenderer, int alUnit)
    {
        return NULL;
    }

    //--------------------------------------------------------------------------

    iGpuProgram* cMaterialType_SolidDiffuse::GetGpuProgram(cMaterial* apMaterial, eMaterialRenderMode aRenderMode, char alSkeleton)
    {
        cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();

        struct RenderZData
        {
            float mtxUv[16];
            float heighScaleMapBias[2];
            struct u_params
            {
                float u_useAlphaMap;
                float u_useDissolveFilter;
                float u_useDissolveAlphaMap;
                float u_unused1;
            } params;
        };
        using MaterialZProgram = BGFXProgram<RenderZData>;

        struct RenderDiffuseData
        {
            float mtxUv[16];
            float mtxInvViewRotation[16];
            struct u_params
            {
                float heightMapScale[2];
                float frenselBiasPow[2];

                float u_useNormalMap;
                float u_useSpecular;
                float u_useEnvMap;
                float u_useCubeMapAlpha;

                float u_useParallax;
                float u_dissolveAmount;
                float u_unused3;
                float u_unused4;
            } params;
        };
        using MaterialDiffuseProgram = BGFXProgram<RenderDiffuseData>;

        struct RenderIlluminationData
        {
            float mtxUv[16];
            struct u_params
            {
                float u_colorMul;
                float u_unused1;
                float u_unused2;
                float u_unused3;
            } params;
        };
        using MaterialIlluminationProgram = BGFXProgram<RenderIlluminationData>;

        if (aRenderMode == eMaterialRenderMode_Z)
        {
            const bool useAlpha = apMaterial->GetTexture(eMaterialTexture_Alpha);
            const bool hasDissolveFilter = pVars->mbAlphaDissolveFilter;
            tFlag lFlags = (useAlpha ? eFeature_Z_UseAlpha : 0) | (hasDissolveFilter ? eFeature_Z_UseAlphaDissolveFilter : 0) |
                (apMaterial->HasUvAnimation() ? eFeature_Z_UvAnimation : 0);

            return mpGlobalProgramManager->GenerateProgram(
                eMaterialRenderMode_Z,
                lFlags,
                [useAlpha, hasDissolveFilter, programHandle = _basicSolidZProgram, u_param = _u_param, u_mtxUv = _u_mtxUv](
                    const tString& name)
                {
                    auto program = (new MaterialZProgram(
                        { MaterialZProgram::ParameterField(
                            material::solid::a_mtxUV,
                            MaterialZProgram::Matrix4fMapper(
                                [](RenderZData& data, const cMatrixf& value)
                                {
                                    std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxUv));
                                })) },
                        name,
                        programHandle,
                        false,
                        eGpuProgramFormat_BGFX));
                    program->SetSubmitHandler(
                        [u_param, u_mtxUv](const RenderZData& data, bgfx::ViewId view, GraphicsContext& context)
                        {
                            bgfx::setUniform(u_param, &data.params);
                            bgfx::setUniform(u_mtxUv, &data.mtxUv);
                        });

                    program->data().params.u_useAlphaMap = useAlpha ? 1.0f : 0.0f;
                    program->data().params.u_useDissolveFilter = hasDissolveFilter ? 1.0f : 0.0f;
                    program->data().params.u_useDissolveAlphaMap = 0.0f;
                    program->SetMatrixf(material::solid::a_mtxUV.id(), cMatrixf::Identity);
                    return program;
                });
        }
        else if (aRenderMode == eMaterialRenderMode_Z_Dissolve)
        {
            const bool useAlpha = apMaterial->GetTexture(eMaterialTexture_Alpha);
            const bool hasDissolveAlphaMap = apMaterial->GetTexture(eMaterialTexture_DissolveAlpha);
            const bool hasDissolveFilter = pVars->mbAlphaDissolveFilter;
            const bool useUvAnimation = apMaterial->HasUvAnimation();

            tFlag lFlags = eFeature_Z_Dissolve | (useAlpha ? eFeature_Z_UseAlpha : 0) |
                (hasDissolveFilter ? eFeature_Z_UseAlphaDissolveFilter : 0) | (hasDissolveAlphaMap ? eFeature_Z_DissolveAlpha : 0) |
                (useUvAnimation ? eFeature_Z_UvAnimation : 0);

            return mpGlobalProgramManager->GenerateProgram(
                eMaterialRenderMode_Z,
                lFlags,
                [useAlpha,
                 hasDissolveFilter,
                 hasDissolveAlphaMap,
                 programHandle = _basicSolidZProgram,
                 u_param = _u_param,
                 u_mtxUv = _u_mtxUv](const tString& name)
                {
                    auto program = (new MaterialZProgram(
                        { MaterialZProgram::ParameterField(
                              material::solid::a_mtxUV,
                              MaterialZProgram::Matrix4fMapper(
                                  [](RenderZData& data, const cMatrixf& value)
                                  {
                                      std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxUv));
                                  })),
                          MaterialZProgram::ParameterField(
                              material::solid::afDissolveAmount,
                              MaterialZProgram::FloatMapper(
                                  [](RenderZData& data, float value)
                                  {
                                      // data.params.dis
                                  })) },
                        name,
                        programHandle,
                        false,
                        eGpuProgramFormat_BGFX));
                    program->SetSubmitHandler(
                        [u_param, u_mtxUv](const RenderZData& data, bgfx::ViewId view, GraphicsContext& context)
                        {
                            bgfx::setUniform(u_param, &data.params);
                            bgfx::setUniform(u_mtxUv, &data.mtxUv);
                        });

                    program->data().params.u_useAlphaMap = useAlpha;
                    program->data().params.u_useDissolveFilter = hasDissolveFilter;
                    program->data().params.u_useDissolveAlphaMap = hasDissolveAlphaMap;
                    program->SetMatrixf(material::solid::a_mtxUV.id(), cMatrixf::Identity);
                    return program;
                });
        }
        ////////////////////////////
        // Diffuse
        else if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            const bool useDiffuseNormalMap = apMaterial->GetTexture(eMaterialTexture_NMap);
            const bool useSpecularMap = apMaterial->GetTexture(eMaterialTexture_Specular);
            const bool useDiffuseParallaxMap = apMaterial->GetTexture(eMaterialTexture_Height) && iRenderer::GetParallaxEnabled();
            const bool useDiffuseEnvMap = apMaterial->GetTexture(eMaterialTexture_CubeMap);
            const bool useCubeAlpha = useDiffuseEnvMap && apMaterial->GetTexture(eMaterialTexture_CubeMapAlpha);
            const bool useUvAnimation = apMaterial->HasUvAnimation();

            tFlag lFlags = (useDiffuseNormalMap ? eFeature_Diffuse_NormalMaps : 0) | (useSpecularMap ? eFeature_Diffuse_Specular : 0) |
                (useDiffuseParallaxMap ? eFeature_Diffuse_Parallax : 0) | (useDiffuseEnvMap ? eFeature_Diffuse_EnvMap : 0) |
                (useCubeAlpha ? eFeature_Diffuse_CubeMapAlpha : 0) | (useUvAnimation ? eFeature_Diffuse_UvAnimation : 0);

            return mpProgramManager->GenerateProgram(
                aRenderMode,
                lFlags,
                [programHandle = _basicSolidDiffuseProgram,
                 useDiffuseNormalMap,
                 useSpecularMap,
                 useDiffuseParallaxMap,
                 useDiffuseEnvMap,
                 useCubeAlpha,
                 useUvAnimation,
                 u_param = _u_param,
                 u_mtxUv = _u_mtxUv,
                 u_mtxInvViewRotation = _u_mtxInvViewRotation](const tString& name)
                {
                    auto program = (new MaterialDiffuseProgram(
                        { MaterialDiffuseProgram::ParameterField(
                              material::solid::a_mtxUV,
                              MaterialDiffuseProgram::Matrix4fMapper(
                                  [](RenderDiffuseData& data, const cMatrixf& value)
                                  {
                                      std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxUv));
                                  })),
                          MaterialDiffuseProgram::ParameterField(
                              material::solid::avFrenselBiasPow,
                              MaterialDiffuseProgram::Vec2Mapper(
                                  [](RenderDiffuseData& data, float afx, float afy)
                                  {
                                      data.params.frenselBiasPow[0] = afx;
                                      data.params.frenselBiasPow[1] = afy;
                                  })),
                          MaterialDiffuseProgram::ParameterField(
                              material::solid::avHeightMapScaleAndBias,
                              MaterialDiffuseProgram::Vec2Mapper(
                                  [](RenderDiffuseData& data, float afx, float afy)
                                  {
                                      data.params.heightMapScale[0] = afx;
                                      data.params.heightMapScale[1] = afy;
                                  })),
                          MaterialDiffuseProgram::ParameterField(
                              material::solid::mtxInvViewRotation,
                              MaterialDiffuseProgram::Matrix4fMapper(
                                  [](RenderDiffuseData& data, const cMatrixf& value)
                                  {
                                      std::copy(std::begin(value.v), std::end(value.v), std::begin(data.mtxInvViewRotation));
                                  })) },
                        name,
                        programHandle,
                        false,
                        eGpuProgramFormat_BGFX));

                    program->SetSubmitHandler(
                        [u_param, u_mtxUv, u_mtxInvViewRotation](const RenderDiffuseData& data, bgfx::ViewId view, GraphicsContext& context)
                        {
                            bgfx::setUniform(u_mtxUv, &data.mtxUv);
                            bgfx::setUniform(u_param, &data.params, 3);
                            bgfx::setUniform(u_mtxInvViewRotation, &data.mtxInvViewRotation);
                        });

                    program->data().params.u_useNormalMap = useDiffuseNormalMap;
                    program->data().params.u_useSpecular = useSpecularMap;
                    program->data().params.u_useEnvMap = useDiffuseEnvMap;
                    program->data().params.u_useCubeMapAlpha = useCubeAlpha;
                    program->data().params.u_useParallax = useDiffuseParallaxMap;

                    program->SetMatrixf(material::solid::a_mtxUV.id(), cMatrixf::Identity);
                    return program;
                });
        }
        ////////////////////////////
        // Illumination
        else if (aRenderMode == eMaterialRenderMode_Illumination)
        {
            const bool hasUvAnimation = apMaterial->HasUvAnimation();
            tFlag lFlags = (hasUvAnimation ? eFeature_Illum_UvAnimation : 0);

            return mpProgramManager->GenerateProgram(
                aRenderMode,
                lFlags,
                [programHandle = _illuminationProgram](const tString& name)
                {
                    auto program = (new MaterialIlluminationProgram({}, name, programHandle, false, eGpuProgramFormat_BGFX));
                    return program;
                });
        }

        return NULL;
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::SetupTypeSpecificData(eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderer* apRenderer)
    {
        ////////////////////////////
        // Diffuse
        if (aRenderMode == eMaterialRenderMode_Diffuse)
        {
            cFrustum* pFrustum = apRenderer->GetCurrentFrustum();

            apProgram->SetFloat(kVar_afInvFarPlane, 1.0f / pFrustum->GetFarPlane());
        }
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::SetupMaterialSpecificData(
        eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, cMaterial* apMaterial, iRenderer* apRenderer)
    {
        if (aRenderMode == eMaterialRenderMode_Diffuse || aRenderMode == eMaterialRenderMode_Z ||
            aRenderMode == eMaterialRenderMode_Z_Dissolve || aRenderMode == eMaterialRenderMode_Illumination)
        {
            /////////////////////////
            // UV Animation
            if (apMaterial->HasUvAnimation())
            {
                apProgram->SetMatrixf(kVar_a_mtxUV, apMaterial->GetUvMatrix());
            }

            if (aRenderMode == eMaterialRenderMode_Diffuse)
            {
                /////////////////////////
                // Parallax
                cMaterialType_SolidDiffuse_Vars* pVars = (cMaterialType_SolidDiffuse_Vars*)apMaterial->GetVars();
                if (apMaterial->GetTexture(eMaterialTexture_Height) && iRenderer::GetParallaxEnabled())
                {
                    apProgram->SetVec2f(kVar_avHeightMapScaleAndBias, pVars->mfHeightMapScale, pVars->mfHeightMapBias);
                }

                /////////////////////////
                // Cube Map
                if (apMaterial->GetTexture(eMaterialTexture_CubeMap))
                {
                    apProgram->SetVec2f(kVar_avFrenselBiasPow, pVars->mfFrenselBias, pVars->mfFrenselPow);

                    cMatrixf mtxInvView = apRenderer->GetCurrentFrustum()->GetViewMatrix().GetTranspose();
                    apProgram->SetMatrixf(kVar_a_mtxInvViewRotation, mtxInvView.GetRotation());
                }
            }
        }
    }

    //--------------------------------------------------------------------------

    void cMaterialType_SolidDiffuse::SetupObjectSpecificData(
        eMaterialRenderMode aRenderMode, iGpuProgram* apProgram, iRenderable* apObject, iRenderer* apRenderer)
    {
        ////////////////////////////
        // Z Dissolve
        if (aRenderMode == eMaterialRenderMode_Z_Dissolve)
        {
            bool bRet = apProgram->SetFloat(kVar_afDissolveAmount, apObject->GetCoverageAmount());
            if (bRet == false)
                Error("Could not set variable!\n");
        }
        ////////////////////////////
        // Illumination
        else if (aRenderMode == eMaterialRenderMode_Illumination)
        {
            bool bRet = apProgram->SetFloat(kVar_afColorMul, apObject->GetIlluminationAmount());
        }
    }

    iMaterialVars* cMaterialType_SolidDiffuse::CreateSpecificVariables()
    {
        return hplNew(cMaterialType_SolidDiffuse_Vars, ());
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
