#include "graphics/RendererDeferred2.h"
#include "Common_3/Utilities/RingBuffer.h"
#include "graphics/DrawPacket.h"
#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/ImageBindlessPool.h"
#include "graphics/Material.h"
#include "graphics/Renderable.h"
#include "graphics/SceneResource.h"

#include "scene/FogArea.h"
#include "scene/Light.h"
#include "scene/LightSpot.h"
#include "scene/ParticleEmitter.h"
#include "scene/RenderableContainer.h"

#include "math/MathTypes.h"
#include "resources/TextureManager.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/common.hpp"

#include "FixPreprocessor.h"

#include "tinyimageformat_base.h"
#include <limits>
#include <memory>
#include <span>
#include <tinyimageformat_query.h>

namespace hpl {

    namespace detail {

        static float calcLightLevel(std::span<iLight*> lights, iRenderable* apObject) {
            cVector3f vCenterPos = apObject->GetBoundingVolume()->GetWorldCenter();
            float fLightAmount = 0.0f;

            // Iterate lights and add light amount
            for (auto& light : lights) {
                auto maxColorValue = [](const cColor& aCol) {
                    return cMath::Max(cMath::Max(aCol.r, aCol.g), aCol.b);
                };
                // Check if there is an intersection
                if (light->CheckObjectIntersection(apObject)) {
                    if (light->GetLightType() == eLightType_Box) {
                        fLightAmount += maxColorValue(light->GetDiffuseColor());
                    } else {
                        float fDist = cMath::Vector3Dist(light->GetWorldPosition(), vCenterPos);
                        fLightAmount += maxColorValue(light->GetDiffuseColor()) * cMath::Max(1.0f - (fDist / light->GetRadius()), 0.0f);
                    }

                    if (fLightAmount >= 1.0f) {
                        fLightAmount = 1.0f;
                        break;
                    }
                }
            }
            return fLightAmount;
        }

        static inline float GetFogAreaVisibilityForObject(
            const cRendererDeferred2::FogRendererData& fogData, cFrustum& frustum, iRenderable* apObject) {
            cFogArea* pFogArea = fogData.m_fogArea;

            cVector3f vObjectPos = apObject->GetBoundingVolume()->GetWorldCenter();
            cVector3f vRayDir = vObjectPos - frustum.GetOrigin();
            float fCameraDistance = vRayDir.Length();
            vRayDir = vRayDir / fCameraDistance;

            float fEntryDist, fExitDist;

            auto checkFogAreaIntersection =
                [&fEntryDist,
                 &fExitDist](const cMatrixf& a_mtxInvBoxModelMatrix, const cVector3f& avBoxSpaceRayStart, const cVector3f& avRayDir) {
                    cVector3f vBoxSpaceDir = cMath::MatrixMul3x3(a_mtxInvBoxModelMatrix, avRayDir);

                    bool bFoundIntersection = false;
                    fExitDist = 0;

                    std::array<cVector3f, 6> fBoxPlaneNormals = { {
                        cVector3f(-1, 0, 0), // Left
                        cVector3f(1, 0, 0), // Right

                        cVector3f(0, -1, 0), // Bottom
                        cVector3f(0, 1, 0), // Top

                        cVector3f(0, 0, -1), // Back
                        cVector3f(0, 0, 1), // Front
                    } };

                    // Iterate the sides of the cube
                    for (auto& planeNormal : fBoxPlaneNormals) {
                        ///////////////////////////////////
                        // Calculate plane intersection
                        float fMul = cMath::Vector3Dot(planeNormal, vBoxSpaceDir);
                        if (fabs(fMul) < 0.0001f) {
                            continue;
                        }
                        float fNegDist = -(cMath::Vector3Dot(planeNormal, avBoxSpaceRayStart) + 0.5f);

                        float fT = fNegDist / fMul;
                        if (fT < 0)
                            continue;
                        cVector3f vAbsNrmIntersect = cMath::Vector3Abs(vBoxSpaceDir * fT + avBoxSpaceRayStart);

                        ///////////////////////////////////
                        // Check if the intersection is inside the cube
                        if (cMath::Vector3LessEqual(vAbsNrmIntersect, cVector3f(0.5001f))) {
                            //////////////////////
                            // First intersection
                            if (bFoundIntersection == false) {
                                fEntryDist = fT;
                                fExitDist = fT;
                                bFoundIntersection = true;
                            }
                            //////////////////////
                            // There has already been a intersection.
                            else {
                                fEntryDist = cMath::Min(fEntryDist, fT);
                                fExitDist = cMath::Max(fExitDist, fT);
                            }
                        }
                    }

                    if (fExitDist < 0)
                        return false;

                    return bFoundIntersection;
                };
            if (checkFogAreaIntersection(fogData.m_mtxInvBoxSpace, fogData.m_boxSpaceFrustumOrigin, vRayDir) == false) {
                return 1.0f;
            }

            if (fogData.m_insideNearFrustum == false && fCameraDistance < fEntryDist) {
                return 1.0f;
            }

            //////////////////////////////
            // Calculate the distance the ray travels in the fog
            float fFogDist;
            if (fogData.m_insideNearFrustum) {
                if (pFogArea->GetShowBacksideWhenInside())
                    fFogDist = cMath::Min(fExitDist, fCameraDistance);
                else
                    fFogDist = fCameraDistance;
            } else {
                if (pFogArea->GetShowBacksideWhenOutside())
                    fFogDist = cMath::Min(fExitDist - fEntryDist, fCameraDistance - fEntryDist);
                else
                    fFogDist = fCameraDistance - fEntryDist;
            }

            //////////////////////////////
            // Calculate the alpha
            if (fFogDist <= 0)
                return 1.0f;

            float fFogStart = pFogArea->GetStart();
            float fFogEnd = pFogArea->GetEnd();
            float fFogAlpha = 1 - pFogArea->GetColor().a;

            if (fFogDist < fFogStart)
                return 1.0f;

            if (fFogDist > fFogEnd)
                return fFogAlpha;

            float fAlpha = (fFogDist - fFogStart) / (fFogEnd - fFogStart);
            if (pFogArea->GetFalloffExp() != 1)
                fAlpha = powf(fAlpha, pFogArea->GetFalloffExp());

            return (1.0f - fAlpha) + fFogAlpha * fAlpha;
        }

        static inline std::vector<cRendererDeferred2::FogRendererData> createFogRenderData(
            std::span<cFogArea*> fogAreas, cFrustum* apFrustum) {
            std::vector<cRendererDeferred2::FogRendererData> fogRenderData;
            fogRenderData.reserve(fogAreas.size());
            for (const auto& fogArea : fogAreas) {
                auto& fogData = fogRenderData.emplace_back(
                    cRendererDeferred2::FogRendererData{ fogArea, false, cVector3f(0.0f), cMatrixf(cMatrixf::Identity) });
                fogData.m_fogArea = fogArea;
                fogData.m_mtxInvBoxSpace = cMath::MatrixInverse(*fogArea->GetModelMatrixPtr());
                fogData.m_boxSpaceFrustumOrigin = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, apFrustum->GetOrigin());
                fogData.m_insideNearFrustum = ([&]() {
                    std::array<cVector3f, 4> nearPlaneVtx;
                    cVector3f min(-0.5f);
                    cVector3f max(0.5f);

                    for (size_t i = 0; i < nearPlaneVtx.size(); ++i) {
                        nearPlaneVtx[i] = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, apFrustum->GetVertex(i));
                    }
                    for (size_t i = 0; i < nearPlaneVtx.size(); ++i) {
                        if (cMath::CheckPointInAABBIntersection(nearPlaneVtx[i], min, max)) {
                            return true;
                        }
                    }
                    // Check if near plane points intersect with box
                    if (cMath::CheckPointsAABBPlanesCollision(nearPlaneVtx.data(), 4, min, max) != eCollision_Outside) {
                        return true;
                    }
                    return false;
                })();
            }
            return fogRenderData;
        }

    } // namespace detail

    cRendererDeferred2::cRendererDeferred2(cGraphics* apGraphics, cResources* apResources, std::shared_ptr<DebugDraw> debug)
        : iRenderer("Deferred2", apGraphics, apResources),
            m_copySubpass(*Interface<ForgeRenderer>::Get()){
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        m_sceneTexture2DPool = BindlessDescriptorPool(ForgeRenderer::SwapChainLength, resource::MaxScene2DTextureCount);
        m_sceneTextureCubePool = BindlessDescriptorPool(ForgeRenderer::SwapChainLength, resource::MaxSceneCubeTextureCount);
        m_sceneTransientImage2DPool = ImageBindlessPool(&m_sceneTexture2DPool, TransientImagePoolCount);
        m_supportIndirectRootConstant = forgeRenderer->Rend()->pGpu->mSettings.mIndirectRootConstant;

        m_samplerNearEdgeClamp.Load(forgeRenderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc bilinearClampDesc = { FILTER_NEAREST,
                                              FILTER_NEAREST,
                                              MIPMAP_MODE_NEAREST,
                                              ADDRESS_MODE_CLAMP_TO_EDGE,
                                              ADDRESS_MODE_CLAMP_TO_EDGE,
                                              ADDRESS_MODE_CLAMP_TO_EDGE };
            addSampler(forgeRenderer->Rend(), &bilinearClampDesc, sampler);
            return true;
        });
        m_samplerPointClampToBorder.Load(forgeRenderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc pointSamplerDesc = { FILTER_NEAREST,
                                             FILTER_NEAREST,
                                             MIPMAP_MODE_NEAREST,
                                             ADDRESS_MODE_CLAMP_TO_BORDER,
                                             ADDRESS_MODE_CLAMP_TO_BORDER,
                                             ADDRESS_MODE_CLAMP_TO_BORDER };
            addSampler(forgeRenderer->Rend(), &pointSamplerDesc, sampler);
            return true;
        });
        m_samplerPointWrap.Load(forgeRenderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc pointSamplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
                                             ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
            addSampler(forgeRenderer->Rend(), &pointSamplerDesc, sampler);
            return true;
        });
        m_samplerMaterial.Load(forgeRenderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc pointSamplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_NEAREST,
                                             ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
            addSampler(forgeRenderer->Rend(), &pointSamplerDesc, sampler);
            return true;
        });
        m_samplerLinearClampToBorder.Load(forgeRenderer->Rend(), [&](Sampler** sampler) {
            SamplerDesc pointSamplerDesc = { FILTER_LINEAR,
                                             FILTER_LINEAR,
                                             MIPMAP_MODE_LINEAR,
                                             ADDRESS_MODE_CLAMP_TO_BORDER,
                                             ADDRESS_MODE_CLAMP_TO_BORDER,
                                             ADDRESS_MODE_CLAMP_TO_BORDER };
            addSampler(forgeRenderer->Rend(), &pointSamplerDesc, sampler);
            return true;
        });
        m_dissolveImage = mpResources->GetTextureManager()->Create2DImage("core_dissolve.tga", false);
        m_emptyTexture2D.Load([&](Texture** texture) {
            TextureDesc textureDesc = {};
            textureDesc.mArraySize = 1;
            textureDesc.mMipLevels = 1;
            textureDesc.mDepth = 1;
            textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            textureDesc.mWidth = 16;
            textureDesc.mHeight = 16;
            textureDesc.mSampleCount = SAMPLE_COUNT_1;
            textureDesc.mStartState = RESOURCE_STATE_COMMON;
            textureDesc.pName = "empty texture";
            TextureLoadDesc loadDesc = {};
            loadDesc.ppTexture = texture;
            loadDesc.pDesc = &textureDesc;
            addResource(&loadDesc, nullptr);
            return true;
        });
        m_emptyTextureCube.Load([&](Texture** texture) {
            TextureDesc textureDesc = {};
            textureDesc.mArraySize = 6;
            textureDesc.mMipLevels = 1;
            textureDesc.mDepth = 1;
            textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE;
            textureDesc.mWidth = 16;
            textureDesc.mHeight = 16;
            textureDesc.mSampleCount = SAMPLE_COUNT_1;
            textureDesc.mStartState = RESOURCE_STATE_COMMON;
            textureDesc.pName = "empty texture";
            TextureLoadDesc loadDesc = {};
            loadDesc.ppTexture = texture;
            loadDesc.pDesc = &textureDesc;
            addResource(&loadDesc, nullptr);
            return true;
        });

        m_visibilityBufferPassShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "visibilityBuffer_pass.vert";
            loadDesc.mStages[1].pFileName = "visibilityBuffer_pass.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });
        m_lightClusterShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "scene_light_cluster.comp";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });
        m_clearLightClusterShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "scene_light_cluster_clear.comp";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });
        m_visibilityBufferAlphaPassShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "visibilityBuffer_alpha_pass.vert";
            loadDesc.mStages[1].pFileName = "visibilityBuffer_alpha_pass.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });

        m_visibilityBufferEmitPassShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "visibility_shade_pass.vert";
            loadDesc.mStages[1].pFileName = "visibility_emit_shade_pass.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });
        m_visibilityShadePassShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "visibility_shade_pass.vert";
            loadDesc.mStages[1].pFileName = "visibility_shade_pass.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });

        struct {
            const char* fragmentShader;
            SharedShader* shader;
        } particleShadersInit[] = { { "particle_shade_add.frag", &m_particleShaderAdd },
                                    { "particle_shade_mul.frag", &m_particleShaderMul },
                                    { "particle_shade_mulX2.frag", &m_particleShaderMulX2 },
                                    { "particle_shade_alpha.frag", &m_particleShaderAlpha },
                                    { "particle_shade_premul_alpha.frag", &m_particleShaderPremulAlpha } };
        for (auto& init : particleShadersInit) {
            init.shader->Load(forgeRenderer->Rend(), [&](Shader** shader) {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0].pFileName = "particle_shade.vert";
                loadDesc.mStages[1].pFileName = init.fragmentShader;
                addShader(forgeRenderer->Rend(), &loadDesc, shader);
                return true;
            });
        }

        struct {
            const char* fragmentShader;
            SharedShader* shader;
        } translucencyShadersInit[] = { { "translucency_shade_add.frag", &m_translucencyShaderAdd },
                                        { "translucency_shade_mul.frag", &m_translucencyShaderMul },
                                        { "translucency_shade_mulX2.frag", &m_translucencyShaderMulX2 },
                                        { "translucency_shade_alpha.frag", &m_translucencyShaderAlpha },
                                        { "translucency_shade_premul_alpha.frag", &m_translucencyShaderPremulAlpha },
                                        // refraction
                                        { "translucency_refraction_shade_add.frag", &m_translucencyRefractionShaderAdd },
                                        { "translucency_refraction_shade_mul.frag", &m_translucencyRefractionShaderMul },
                                        { "translucency_refraction_shade_mulX2.frag", &m_translucencyRefractionShaderMulX2 },
                                        { "translucency_refraction_shade_alpha.frag", &m_translucencyRefractionShaderAlpha },
                                        { "translucency_refraction_shade_premul_alpha.frag", &m_translucencyRefractionShaderPremulAlpha },
                                        // illumination
                                        { "translucency_shade_illumination_add.frag", &m_translucencyIlluminationShaderAdd },
                                        // water
                                        { "translucency_water_shade.frag", &m_translucencyWaterShader } };
        for (auto& init : translucencyShadersInit) {
            init.shader->Load(forgeRenderer->Rend(), [&](Shader** shader) {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0].pFileName = "translucency_shade.vert";
                loadDesc.mStages[1].pFileName = init.fragmentShader;
                addShader(forgeRenderer->Rend(), &loadDesc, shader);
                return true;
            });
        }

        m_decalShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "decal_shade.vert";
            loadDesc.mStages[1].pFileName = "decal_shade.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });

        m_lightClusterRootSignature.Load(forgeRenderer->Rend(), [&](RootSignature** signature) {
            RootSignatureDesc rootSignatureDesc = {};
            std::array shaders = { m_lightClusterShader.m_handle, m_clearLightClusterShader.m_handle };
            const char* pStaticSamplers[] = { "depthSampler" };
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            rootSignatureDesc.mStaticSamplerCount = 1;
            rootSignatureDesc.ppStaticSamplers = &m_samplerPointClampToBorder.m_handle;
            rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, signature);
            return true;
        });

        m_sceneRootSignature.Load(forgeRenderer->Rend(), [&](RootSignature** signature) {
            ASSERT(m_lightClusterShader.IsValid());
            ASSERT(m_clearLightClusterShader.IsValid());
            ASSERT(m_visibilityBufferAlphaPassShader.IsValid());
            ASSERT(m_visibilityBufferPassShader.IsValid());
            ASSERT(m_visibilityShadePassShader.IsValid());
            std::array shaders = { m_visibilityBufferAlphaPassShader.m_handle,
                                   m_visibilityBufferPassShader.m_handle,
                                   m_visibilityShadePassShader.m_handle,
                                   m_visibilityBufferEmitPassShader.m_handle,
                                   m_particleShaderAdd.m_handle,
                                   m_particleShaderMul.m_handle,
                                   m_particleShaderMulX2.m_handle,
                                   m_particleShaderAlpha.m_handle,
                                   m_particleShaderPremulAlpha.m_handle,
                                   m_decalShader.m_handle,
                                   m_translucencyShaderAdd.m_handle,
                                   m_translucencyShaderMul.m_handle,
                                   m_translucencyShaderMulX2.m_handle,
                                   m_translucencyShaderAlpha.m_handle,
                                   m_translucencyShaderPremulAlpha.m_handle,
                                   m_translucencyIlluminationShaderAdd.m_handle,
                                   m_translucencyRefractionShaderAdd.m_handle,
                                   m_translucencyWaterShader.m_handle,
                                   m_translucencyRefractionShaderMul.m_handle,
                                   m_translucencyRefractionShaderMulX2.m_handle,
                                   m_translucencyRefractionShaderAlpha.m_handle,
                                   m_translucencyRefractionShaderPremulAlpha.m_handle };
            std::array vbShadeSceneSamplers = { m_samplerLinearClampToBorder.m_handle,
                                                m_samplerMaterial.m_handle,
                                                m_samplerNearEdgeClamp.m_handle,
                                                m_samplerPointWrap.m_handle };
            std::array vbShadeSceneSamplersNames = {
                "linearBorderSampler", "sceneSampler", "nearEdgeClampSampler", "nearPointWrapSampler"
            };
            RootSignatureDesc rootSignatureDesc = {};
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            rootSignatureDesc.mStaticSamplerCount = vbShadeSceneSamplersNames.size();
            rootSignatureDesc.ppStaticSamplers = vbShadeSceneSamplers.data();
            rootSignatureDesc.ppStaticSamplerNames = vbShadeSceneSamplersNames.data();
            rootSignatureDesc.mMaxBindlessTextures = hpl::resource::MaxScene2DTextureCount;
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, signature);
            return true;
        });



        // Indirect Argument signature
        {
            uint32_t indirectArgCount = 0;
            std::array<IndirectArgumentDescriptor, 2> indirectArgs = {};
            if (m_supportIndirectRootConstant) {
                indirectArgs[indirectArgCount].mType = INDIRECT_CONSTANT;
                indirectArgs[indirectArgCount].mIndex = getDescriptorIndexFromName(m_sceneRootSignature.m_handle, "indirectRootConstant");
                indirectArgs[indirectArgCount].mByteSize = sizeof(uint32_t);
                ++indirectArgCount;
            }
            indirectArgs[indirectArgCount++].mType = INDIRECT_DRAW_INDEX;
            CommandSignatureDesc vbPassDesc = { m_sceneRootSignature.m_handle, indirectArgs.data(), indirectArgCount };
            addIndirectCommandSignature(forgeRenderer->Rend(), &vbPassDesc, &m_cmdSignatureVBPass);
        }

        m_lightClusterPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
            pipelineDesc.pName = "light cluster pipeline";
            ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
            computePipelineDesc.pShaderProgram = m_lightClusterShader.m_handle;
            computePipelineDesc.pRootSignature = m_lightClusterRootSignature.m_handle;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });

        m_clearClusterPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
            pipelineDesc.pName = "light cluster clear pipeline";
            ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
            computePipelineDesc.pShaderProgram = m_clearLightClusterShader.m_handle;
            computePipelineDesc.pRootSignature = m_lightClusterRootSignature.m_handle;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });
        m_visbilityAlphaBufferPass.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            VertexLayout vertexLayout = {};
            vertexLayout.mBindingCount = 4;
            vertexLayout.mBindings[0] = { .mStride = sizeof(float3) };
            vertexLayout.mBindings[1] = { .mStride = sizeof(float2) };
            vertexLayout.mBindings[2] = { .mStride = sizeof(float3) };
            vertexLayout.mBindings[3] = { .mStride = sizeof(float3) };
            vertexLayout.mAttribCount = 4;
            vertexLayout.mAttribs[0] = VertexAttrib{
                .mSemantic = SEMANTIC_POSITION, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 0, .mLocation = 0, .mOffset = 0
            };
            vertexLayout.mAttribs[1] = VertexAttrib{
                .mSemantic = SEMANTIC_TEXCOORD0, .mFormat = TinyImageFormat_R32G32_SFLOAT, .mBinding = 1, .mLocation = 1, .mOffset = 0
            };
            vertexLayout.mAttribs[2] = VertexAttrib{
                .mSemantic = SEMANTIC_NORMAL, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 2, .mLocation = 2, .mOffset = 0
            };
            vertexLayout.mAttribs[3] = VertexAttrib{
                .mSemantic = SEMANTIC_TANGENT, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 3, .mLocation = 3, .mOffset = 0
            };
            std::array colorFormats = { ColorBufferFormat };
            DepthStateDesc depthStateDesc = { .mDepthTest = true, .mDepthWrite = true, .mDepthFunc = CMP_LEQUAL };

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
            rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.pName = "visibility alpha pass";
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_visibilityBufferAlphaPassShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &vertexLayout;
            pipelineSettings.mSupportIndirectCommandBuffer = true;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });


        m_visbilityEmitBufferPass.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            std::array colorFormats = { ColorBufferFormat };
            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
            rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;

            DepthStateDesc depthStateDisabledDesc = {};
            depthStateDisabledDesc.mDepthWrite = false;
            depthStateDisabledDesc.mDepthTest = false;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.pName = "visibility emit shade pass";
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_visibilityBufferEmitPassShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pDepthState = &depthStateDisabledDesc;
            pipelineSettings.pVertexLayout = nullptr;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });

        m_visiblityShadePass.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            std::array colorFormats = { ColorBufferFormat };
            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
            rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;


            DepthStateDesc depthStateDisabledDesc = {};
            depthStateDisabledDesc.mDepthWrite = false;
            depthStateDisabledDesc.mDepthTest = false;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.pName = "visibility shade pass";
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_visibilityShadePassShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pDepthState = &depthStateDisabledDesc;
            pipelineSettings.pVertexLayout = nullptr;
           // pipelineSettings.pBlendState = &blendStateDesc;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });


        m_visibilityBufferPass.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
            VertexLayout vertexLayout = {};
            vertexLayout.mBindingCount = 1;
            vertexLayout.mBindings[0] = { .mStride = sizeof(float3) };
            vertexLayout.mAttribCount = 1;
            vertexLayout.mAttribs[0] = VertexAttrib{
                .mSemantic = SEMANTIC_POSITION, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 0, .mLocation = 0, .mOffset = 0
            };
            std::array colorFormats = { ColorBufferFormat };
            DepthStateDesc depthStateDesc = { .mDepthTest = true, .mDepthWrite = true, .mDepthFunc = CMP_LEQUAL };

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
            rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            pipelineDesc.pName = "visibility buffer pass";
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_visibilityBufferPassShader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &vertexLayout;
            pipelineSettings.mSupportIndirectCommandBuffer = true;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
            return true;
        });

        struct {
            SharedPipeline* pipeline;
            eMaterialBlendMode blendMode;
        } decalPipelines[] = { { &m_decalPipelines.m_pipelineBlendAdd, eMaterialBlendMode_Add },
                               { &m_decalPipelines.m_pipelineBlendMul, eMaterialBlendMode_Mul },
                               { &m_decalPipelines.m_pipelineBlendMulX2, eMaterialBlendMode_MulX2 },
                               { &m_decalPipelines.m_pipelineBlendAlpha, eMaterialBlendMode_Alpha },
                               { &m_decalPipelines.m_pipelineBlendPremulAlpha, eMaterialBlendMode_PremulAlpha } };

        for (auto& config : decalPipelines) {
            config.pipeline->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                VertexLayout vertexLayout = {};
                vertexLayout.mBindingCount = 3;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
                vertexLayout.mBindings[2].mStride = sizeof(float4);

                vertexLayout.mAttribCount = 3;
                vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
                vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[0].mBinding = 0;
                vertexLayout.mAttribs[0].mLocation = 0;
                vertexLayout.mAttribs[0].mOffset = 0;

                vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
                vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
                vertexLayout.mAttribs[1].mBinding = 1;
                vertexLayout.mAttribs[1].mLocation = 1;
                vertexLayout.mAttribs[1].mOffset = 0;

                vertexLayout.mAttribs[2].mSemantic = SEMANTIC_COLOR;
                vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
                vertexLayout.mAttribs[2].mBinding = 2;
                vertexLayout.mAttribs[2].mLocation = 2;
                vertexLayout.mAttribs[2].mOffset = 0;

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

                DepthStateDesc depthStateDesc = {};
                depthStateDesc.mDepthTest = true;
                depthStateDesc.mDepthWrite = false;
                depthStateDesc.mDepthFunc = CMP_LEQUAL;

                BlendStateDesc blendStateDesc{};
                blendStateDesc.mSrcFactors[0] = hpl::HPL2BlendTable[config.blendMode].src;
                blendStateDesc.mDstFactors[0] = hpl::HPL2BlendTable[config.blendMode].dst;
                blendStateDesc.mBlendModes[0] = hpl::HPL2BlendTable[config.blendMode].mode;

                blendStateDesc.mSrcAlphaFactors[0] = hpl::HPL2BlendTable[config.blendMode].srcAlpha;
                blendStateDesc.mDstAlphaFactors[0] = hpl::HPL2BlendTable[config.blendMode].dstAlpha;
                blendStateDesc.mBlendAlphaModes[0] = hpl::HPL2BlendTable[config.blendMode].alphaMode;
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN | ColorMask::COLOR_MASK_BLUE;

                std::array colorFormats = { ColorBufferFormat };
                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.pDepthState = &depthStateDesc;
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
                pipelineSettings.pShaderProgram = m_decalShader.m_handle;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.mSupportIndirectCommandBuffer = true;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                return true;
            });
        }

        {
            VertexLayout vertexLayout = {};

            vertexLayout.mBindingCount = 5;
            vertexLayout.mBindings[0].mStride = sizeof(float3);
            vertexLayout.mBindings[1].mStride = sizeof(float2);
            vertexLayout.mBindings[2].mStride = sizeof(float3);
            vertexLayout.mBindings[3].mStride = sizeof(float3);
            vertexLayout.mBindings[4].mStride = sizeof(float3);
            vertexLayout.mAttribCount = 5;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
            vertexLayout.mAttribs[1].mBinding = 1;
            vertexLayout.mAttribs[1].mLocation = 1;
            vertexLayout.mAttribs[1].mOffset = 0;

            vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
            vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[2].mBinding = 2;
            vertexLayout.mAttribs[2].mLocation = 2;
            vertexLayout.mAttribs[2].mOffset = 0;

            vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
            vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[3].mBinding = 3;
            vertexLayout.mAttribs[3].mLocation = 3;
            vertexLayout.mAttribs[3].mOffset = 0;

            vertexLayout.mAttribs[4].mSemantic = SEMANTIC_COLOR;
            vertexLayout.mAttribs[4].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
            vertexLayout.mAttribs[4].mBinding = 4;
            vertexLayout.mAttribs[4].mLocation = 4;
            vertexLayout.mAttribs[4].mOffset = 0;
            struct {
                SharedShader* shader;
                SharedPipeline* pipline;
                SharedPipeline* piplineNoDepth;
                eMaterialBlendMode blendMode;
            } translucencyPipelineInit[] = {
                { &m_translucencyShaderAdd,
                  &m_translucencyPipline.m_pipelineBlendAdd,
                  &m_translucencyPiplineNoDepth.m_pipelineBlendAdd,
                  eMaterialBlendMode_Add },
                { &m_translucencyShaderMul,
                  &m_translucencyPipline.m_pipelineBlendMul,
                  &m_translucencyPiplineNoDepth.m_pipelineBlendMul,
                  eMaterialBlendMode_Mul },
                { &m_translucencyShaderMulX2,
                  &m_translucencyPipline.m_pipelineBlendMulX2,
                  &m_translucencyPiplineNoDepth.m_pipelineBlendMulX2,
                  eMaterialBlendMode_MulX2 },
                { &m_translucencyShaderAlpha,
                  &m_translucencyPipline.m_pipelineBlendAlpha,
                  &m_translucencyPiplineNoDepth.m_pipelineBlendAlpha,
                  eMaterialBlendMode_Alpha },
                { &m_translucencyShaderPremulAlpha,
                  &m_translucencyPipline.m_pipelineBlendPremulAlpha,
                  &m_translucencyPiplineNoDepth.m_pipelineBlendPremulAlpha,
                  eMaterialBlendMode_PremulAlpha },
            };

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthFunc = CMP_LEQUAL;

            DepthStateDesc noDepthStateDesc = {};
            noDepthStateDesc.mDepthWrite = false;
            noDepthStateDesc.mDepthTest = false;
            noDepthStateDesc.mDepthFunc = CMP_ALWAYS;
            std::array colorFormats = { ColorBufferFormat };
            for (auto& init : translucencyPipelineInit) {
                BlendStateDesc blendStateDesc{};
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_ALL;
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pShaderProgram = init.shader->m_handle;

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
                rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;

                blendStateDesc.mSrcFactors[0] = hpl::HPL2BlendTable[init.blendMode].src;
                blendStateDesc.mDstFactors[0] = hpl::HPL2BlendTable[init.blendMode].dst;
                blendStateDesc.mBlendModes[0] = hpl::HPL2BlendTable[init.blendMode].mode;

                blendStateDesc.mSrcAlphaFactors[0] = hpl::HPL2BlendTable[init.blendMode].srcAlpha;
                blendStateDesc.mDstAlphaFactors[0] = hpl::HPL2BlendTable[init.blendMode].dstAlpha;
                blendStateDesc.mBlendAlphaModes[0] = hpl::HPL2BlendTable[init.blendMode].alphaMode;

                pipelineSettings.pDepthState = &depthStateDesc;
                init.pipline->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });

                pipelineSettings.pDepthState = &noDepthStateDesc;
                init.piplineNoDepth->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });
            }

            struct {
                SharedShader* shader;
                SharedPipeline* pipline;
                SharedPipeline* piplineNoDepth;
                eMaterialBlendMode blendMode;
            } translucencyRefractionPipelineInit[] = {
                { &m_translucencyRefractionShaderAdd,
                  &m_translucencyRefractionPipline.m_pipelineBlendAdd,
                  &m_translucencyRefractionPiplineNoDepth.m_pipelineBlendAdd,
                  eMaterialBlendMode_Add },
                { &m_translucencyRefractionShaderMul,
                  &m_translucencyRefractionPipline.m_pipelineBlendMul,
                  &m_translucencyRefractionPiplineNoDepth.m_pipelineBlendMul,
                  eMaterialBlendMode_Mul },
                { &m_translucencyRefractionShaderMulX2,
                  &m_translucencyRefractionPipline.m_pipelineBlendMulX2,
                  &m_translucencyRefractionPiplineNoDepth.m_pipelineBlendMulX2,
                  eMaterialBlendMode_MulX2 },
                { &m_translucencyRefractionShaderAlpha,
                  &m_translucencyRefractionPipline.m_pipelineBlendAlpha,
                  &m_translucencyRefractionPiplineNoDepth.m_pipelineBlendAlpha,
                  eMaterialBlendMode_Alpha },
                { &m_translucencyRefractionShaderPremulAlpha,
                  &m_translucencyRefractionPipline.m_pipelineBlendPremulAlpha,
                  &m_translucencyRefractionPiplineNoDepth.m_pipelineBlendPremulAlpha,
                  eMaterialBlendMode_PremulAlpha },
            };

            for (auto& init : translucencyRefractionPipelineInit) {
                BlendStateDesc blendStateDesc{};
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_ALL;
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pShaderProgram = init.shader->m_handle;

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
                rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;

                blendStateDesc.mSrcFactors[0] = BlendConstant::BC_ONE;
                blendStateDesc.mDstFactors[0] = BlendConstant::BC_SRC_ALPHA;
                blendStateDesc.mBlendModes[0] = BlendMode::BM_ADD;

                blendStateDesc.mSrcAlphaFactors[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].srcAlpha;
                blendStateDesc.mDstAlphaFactors[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].dstAlpha;
                blendStateDesc.mBlendAlphaModes[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].alphaMode;

                pipelineSettings.pDepthState = &depthStateDesc;
                init.pipline->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });

                pipelineSettings.pDepthState = &noDepthStateDesc;
                init.piplineNoDepth->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });
            }
            {
                BlendStateDesc blendStateDesc{};
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_ALL;
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pShaderProgram = m_translucencyIlluminationShaderAdd.m_handle;

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
                rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;

                blendStateDesc.mSrcFactors[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].src;
                blendStateDesc.mDstFactors[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].dst;
                blendStateDesc.mBlendModes[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].mode;

                blendStateDesc.mSrcAlphaFactors[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].srcAlpha;
                blendStateDesc.mDstAlphaFactors[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].dstAlpha;
                blendStateDesc.mBlendAlphaModes[0] = hpl::HPL2BlendTable[eMaterialBlendMode_Add].alphaMode;

                pipelineSettings.pDepthState = &depthStateDesc;
                m_translucencyIlluminationPipline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });

                pipelineSettings.pDepthState = &noDepthStateDesc;
                m_translucencyIlluminationPiplineNoDepth.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });
            }

            {
                BlendStateDesc blendStateDesc{};
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN | ColorMask::COLOR_MASK_BLUE;
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
                pipelineSettings.pVertexLayout = &vertexLayout;

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
                rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;

                blendStateDesc.mSrcFactors[0] = BC_ONE;
                blendStateDesc.mDstFactors[0] = BC_ZERO;
                blendStateDesc.mBlendModes[0] = BlendMode::BM_ADD;
                pipelineSettings.pShaderProgram = m_translucencyWaterShader.m_handle;


                pipelineSettings.pDepthState = &depthStateDesc;
                m_translucencyWaterPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });

                pipelineSettings.pDepthState = &noDepthStateDesc;
                m_translucencyWaterPipelineNoDepth.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                    addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                    return true;
                });
            }
        }
        struct {
            SharedShader* shader;
            SharedPipeline* pipline;
            SharedPipeline* piplineNoDepth;
            eMaterialBlendMode blendMode;
        } particlePipelineInit[] = {
            { &m_particleShaderAdd,
              &m_particlePipeline.m_pipelineBlendAdd,
              &m_particlePipelineNoDepth.m_pipelineBlendAdd,
              eMaterialBlendMode_Add },
            { &m_particleShaderMul,
              &m_particlePipeline.m_pipelineBlendMul,
              &m_particlePipelineNoDepth.m_pipelineBlendMul,
              eMaterialBlendMode_Mul },
            { &m_particleShaderMulX2,
              &m_particlePipeline.m_pipelineBlendMulX2,
              &m_particlePipelineNoDepth.m_pipelineBlendMulX2,
              eMaterialBlendMode_MulX2 },
            { &m_particleShaderAlpha,
              &m_particlePipeline.m_pipelineBlendAlpha,
              &m_particlePipelineNoDepth.m_pipelineBlendAlpha,
              eMaterialBlendMode_Alpha },
            { &m_particleShaderPremulAlpha,
              &m_particlePipeline.m_pipelineBlendPremulAlpha,
              &m_particlePipelineNoDepth.m_pipelineBlendPremulAlpha,
              eMaterialBlendMode_PremulAlpha },
        };

        for (auto& init : particlePipelineInit) {
            BlendStateDesc blendStateDesc{};
            blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN | ColorMask::COLOR_MASK_BLUE;
            blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
            blendStateDesc.mIndependentBlend = false;
            std::array colorFormats = { ColorBufferFormat };

            VertexLayout particleVertexLayout = {};

            particleVertexLayout.mBindingCount = 3;
            particleVertexLayout.mBindings[0].mStride = sizeof(float3);
            particleVertexLayout.mBindings[1].mStride = sizeof(float2);
            particleVertexLayout.mBindings[2].mStride = sizeof(float4);

            particleVertexLayout.mAttribCount = 3;
            particleVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            particleVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            particleVertexLayout.mAttribs[0].mBinding = 0;
            particleVertexLayout.mAttribs[0].mLocation = 0;
            particleVertexLayout.mAttribs[0].mOffset = 0;

            particleVertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
            particleVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
            particleVertexLayout.mAttribs[1].mBinding = 1;
            particleVertexLayout.mAttribs[1].mLocation = 1;
            particleVertexLayout.mAttribs[1].mOffset = 0;

            particleVertexLayout.mAttribs[2].mSemantic = SEMANTIC_COLOR;
            particleVertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
            particleVertexLayout.mAttribs[2].mBinding = 2;
            particleVertexLayout.mAttribs[2].mLocation = 2;
            particleVertexLayout.mAttribs[2].mOffset = 0;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.pRootSignature = m_sceneRootSignature.m_handle;
            pipelineSettings.pVertexLayout = &particleVertexLayout;
            pipelineSettings.mSupportIndirectCommandBuffer = true;

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
            rasterizerStateDesc.mFrontFace = FRONT_FACE_CCW;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthFunc = CMP_LEQUAL;

            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pShaderProgram = init.shader->m_handle;

            blendStateDesc.mSrcFactors[0] = hpl::HPL2BlendTable[init.blendMode].src;
            blendStateDesc.mDstFactors[0] = hpl::HPL2BlendTable[init.blendMode].dst;
            blendStateDesc.mBlendModes[0] = hpl::HPL2BlendTable[init.blendMode].mode;

            blendStateDesc.mSrcAlphaFactors[0] = hpl::HPL2BlendTable[init.blendMode].srcAlpha;
            blendStateDesc.mDstAlphaFactors[0] = hpl::HPL2BlendTable[init.blendMode].dstAlpha;
            blendStateDesc.mBlendAlphaModes[0] = hpl::HPL2BlendTable[init.blendMode].alphaMode;

            init.pipline->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                return true;
            });

            depthStateDesc.mDepthTest = false;
            depthStateDesc.mDepthFunc = CMP_ALWAYS;
            init.piplineNoDepth->Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                return true;
            });
        }

        for (size_t i = 0; i < ForgeRenderer::SwapChainLength; i++) {
            ASSERT(m_objectUniformBuffer.size() == ForgeRenderer::SwapChainLength);
            ASSERT(m_perSceneInfoBuffer.size() == ForgeRenderer::SwapChainLength);
            ASSERT(m_indirectDrawArgsBuffer.size() == ForgeRenderer::SwapChainLength);
            m_lightClustersBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc bufferDesc = {};
                bufferDesc.mDesc.mElementCount =
                    resource::LightClusterLightCount * LightClusterWidth * LightClusterHeight * LightClusterSlices;
                bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * sizeof(uint32_t);
                bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
                bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                bufferDesc.mDesc.mFirstElement = 0;
                bufferDesc.mDesc.mStructStride = sizeof(uint32_t);
                bufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
                bufferDesc.mDesc.pName = "Light Cluster";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });
            m_lightClusterCountBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc bufferDesc = {};
                bufferDesc.mDesc.mElementCount = LightClusterWidth * LightClusterHeight * LightClusterSlices;
                bufferDesc.mDesc.mStructStride = sizeof(uint32_t);
                bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * sizeof(uint32_t);
                bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
                bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                bufferDesc.mDesc.mFirstElement = 0;
                bufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
                bufferDesc.pData = nullptr;
                bufferDesc.mDesc.pName = "Light Cluster Count";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });

            m_lightBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc bufferDesc = {};
                bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                bufferDesc.mDesc.mFirstElement = 0;
                bufferDesc.mDesc.mElementCount = resource::MaxLightUniforms;
                bufferDesc.mDesc.mStructStride = sizeof(resource::SceneLight);
                bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * bufferDesc.mDesc.mStructStride;
                bufferDesc.mDesc.pName = "lights";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });

            m_particleBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc bufferDesc = {};
                bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                bufferDesc.mDesc.mFirstElement = 0;
                bufferDesc.mDesc.mElementCount = resource::MaxParticleUniform;
                bufferDesc.mDesc.mStructStride = sizeof(resource::SceneParticle);
                bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * bufferDesc.mDesc.mStructStride;
                bufferDesc.mDesc.pName = "lights";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });

            m_decalBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc bufferDesc = {};
                bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                bufferDesc.mDesc.mFirstElement = 0;
                bufferDesc.mDesc.mElementCount = resource::MaxDecalUniforms;
                bufferDesc.mDesc.mStructStride = sizeof(resource::SceneDecal);
                bufferDesc.mDesc.mSize = bufferDesc.mDesc.mElementCount * bufferDesc.mDesc.mStructStride;
                bufferDesc.mDesc.pName = "decals";
                bufferDesc.ppBuffer = buffer;
                addResource(&bufferDesc, nullptr);
                return true;
            });

            m_objectUniformBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = resource::MaxObjectUniforms;
                desc.mDesc.mStructStride = sizeof(hpl::resource::SceneObject);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.mDesc.pName = "object buffer";
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });
            m_diffuseMatUniformBuffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = resource::MaxSolidDiffuseMaterials;
                desc.mDesc.mStructStride = sizeof(resource::DiffuseMaterial);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.mDesc.pName = "diffuse solid material";
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });
            m_waterMatBuffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = resource::MaxWaterMaterials;
                desc.mDesc.mStructStride = sizeof(resource::WaterMaterial);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.mDesc.pName = "diffuse solid material";
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });

            m_translucencyMatBuffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = resource::MaxTranslucenctMaterials;
                desc.mDesc.mStructStride = sizeof(resource::TranslucentMaterial);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.mDesc.pName = "diffuse solid material";
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });

            m_perSceneInfoBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mElementCount = 1;
                desc.mDesc.mStructStride = sizeof(hpl::resource::SceneInfoResource);
                desc.mDesc.mSize = sizeof(hpl::resource::SceneInfoResource);
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.pName = "scene info";
                desc.pData = nullptr;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });

            m_indirectDrawArgsBuffer[i].Load([&](Buffer** buffer) {
                BufferLoadDesc indirectBufferDesc = {};
                indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
                indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                indirectBufferDesc.mDesc.mElementCount = resource::MaxIndirectDrawElements * IndirectArgumentSize;
                indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
                indirectBufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INDIRECT_ARGUMENT;
                indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
                indirectBufferDesc.mDesc.pName = "indirect buffer";
                indirectBufferDesc.ppBuffer = buffer;
                addResource(&indirectBufferDesc, NULL);
                return true;
            });
        }
        m_diffuseIndexPool = IndexPool(resource::MaxSolidDiffuseMaterials);
        m_translucencyIndexPool = IndexPool(resource::MaxTranslucenctMaterials);
        m_waterIndexPool = IndexPool(resource::MaxWaterMaterials);

        addUniformGPURingBuffer(forgeRenderer->Rend(), ViewportRingBufferSize, &m_viewPortUniformBuffer);


        // create Descriptor sets
        m_sceneDescriptorConstSet.Load(forgeRenderer->Rend(), [&](DescriptorSet** descSet) {
            DescriptorSetDesc descriptorSetDesc{ m_sceneRootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(forgeRenderer->Rend(), &descriptorSetDesc, descSet);
            return true;
        });
        // update descriptorSet
        for (size_t swapchainIndex = 0; swapchainIndex < ForgeRenderer::SwapChainLength; swapchainIndex++) {
            m_sceneDescriptorPerFrameSet[swapchainIndex].Load(forgeRenderer->Rend(), [&](DescriptorSet** descSet) {
                DescriptorSetDesc descriptorSetDesc{ m_sceneRootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 1 };
                addDescriptorSet(forgeRenderer->Rend(), &descriptorSetDesc, descSet);
                return true;
            });
            m_sceneDescriptorPerBatchSet[swapchainIndex].Load(forgeRenderer->Rend(), [&](DescriptorSet** descSet) {
                DescriptorSetDesc descriptorSetDesc{ m_sceneRootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, ScenePerDescriptorBatchSize  };
                addDescriptorSet(forgeRenderer->Rend(), &descriptorSetDesc, descSet);
                return true;
            });
            m_lightDescriptorPerFrameSet[swapchainIndex].Load(forgeRenderer->Rend(), [&](DescriptorSet** descSet) {
                DescriptorSetDesc descriptorSetDesc{ m_lightClusterRootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 1 };
                addDescriptorSet(forgeRenderer->Rend(), &descriptorSetDesc, descSet);
                return true;
            });
            {
                std::array params = {
                    DescriptorData{ .pName = "decals", .ppBuffers = &m_decalBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "particles", .ppBuffers = &m_particleBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "lights", .ppBuffers = &m_lightBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "lightClustersCount", .ppBuffers = &m_lightClusterCountBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "lightClusters", .ppBuffers = &m_lightClustersBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "sceneObjects", .ppBuffers = &m_objectUniformBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "sceneInfo", .ppBuffers = &m_perSceneInfoBuffer[swapchainIndex].m_handle },
                };
                updateDescriptorSet(
                    forgeRenderer->Rend(), 0, m_sceneDescriptorPerFrameSet[swapchainIndex].m_handle, params.size(), params.data());
            }
            {
                std::array params = {
                    DescriptorData{ .pName = "lights", .ppBuffers = &m_lightBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "lightClustersCount", .ppBuffers = &m_lightClusterCountBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "lightClusters", .ppBuffers = &m_lightClustersBuffer[swapchainIndex].m_handle },
                    DescriptorData{ .pName = "sceneInfo", .ppBuffers = &m_perSceneInfoBuffer[swapchainIndex].m_handle },
                };
                updateDescriptorSet(
                    forgeRenderer->Rend(), 0, m_lightDescriptorPerFrameSet[swapchainIndex].m_handle, params.size(), params.data());
            }
        }
        {
            auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
            auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);

            std::array params = {
                DescriptorData{ .pName = "dissolveTexture", .ppTextures = &m_dissolveImage->GetTexture().m_handle },
                DescriptorData{ .pName = "sceneDiffuseMat", .ppBuffers = &m_diffuseMatUniformBuffer.m_handle },
                DescriptorData{ .pName = "sceneTranslucentMat", .ppBuffers = &m_translucencyMatBuffer.m_handle },
                DescriptorData{ .pName = "sceneWaterMat", .ppBuffers = &m_waterMatBuffer.m_handle },
                DescriptorData{ .pName = "vtxOpaqueIndex", .ppBuffers = &opaqueSet.indexBuffer().m_handle },
                DescriptorData{ .pName = "vtxOpaquePosition",
                                .ppBuffers = &opaqueSet.getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION)->buffer().m_handle },
                DescriptorData{ .pName = "vtxOpaqueTangnet",
                                .ppBuffers = &opaqueSet.getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT)->buffer().m_handle },
                DescriptorData{ .pName = "vtxOpaqueNormal",
                                .ppBuffers = &opaqueSet.getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL)->buffer().m_handle },
                DescriptorData{ .pName = "vtxOpaqueUv",
                                .ppBuffers = &opaqueSet.getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0)->buffer().m_handle },
                // DescriptorData{ .pName = "vtxOpaqueColor", .ppBuffers =
                // &opaqueSet.getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR)->buffer().m_handle },
            };
            updateDescriptorSet(forgeRenderer->Rend(), 0, m_sceneDescriptorConstSet.m_handle, params.size(), params.data());
        }
    }

    cRendererDeferred2::~cRendererDeferred2() {
    }

    cRendererDeferred2::MaterialSet& cRendererDeferred2::ResourceMaterial::resolveSet(MaterialSetType set) {
        auto it = std::find_if(m_sets.begin(), m_sets.end(), [&](auto& handle) {
            return handle.m_type == set;
        });

        if (it == m_sets.end()) {
            auto& materialSet = m_sets.emplace_back();
            materialSet.m_type = set;
            return materialSet;
        }
        return (*it);
    }

    cRendererDeferred2::ResourceMaterial::ResourceMaterial() {
        for (auto& handles : m_textureHandles) {
            handles = resource::InvalidSceneTexture;
        }
    }

    cRendererDeferred2::ResourceMaterial& cRendererDeferred2::resolveResourceMaterial(cMaterial* material) {
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        auto& sharedMat = m_sharedMaterial[material->Index()];
        if (sharedMat.m_material != material || sharedMat.m_version != material->Generation()) {
            sharedMat.m_material = material;

            for (uint32_t slot = 0; slot < eMaterialTexture_LastEnum; slot++) {
                eMaterialTexture textureType = static_cast<eMaterialTexture>(slot);
                auto* image = material->GetImage(textureType);
                auto& textureHandle = sharedMat.m_textureHandles[slot];
                switch (textureType) {
                case eMaterialTexture_Diffuse:
                case eMaterialTexture_NMap:
                case eMaterialTexture_Specular:
                case eMaterialTexture_Alpha:
                case eMaterialTexture_Height:
                case eMaterialTexture_Illumination:
                case eMaterialTexture_DissolveAlpha:
                case eMaterialTexture_CubeMapAlpha:
                    if (textureHandle < resource::MaxScene2DTextureCount) {
                        m_sceneTexture2DPool.dispose(textureHandle);
                    }
                    textureHandle = image ? m_sceneTexture2DPool.request(image->GetTexture()) : std::numeric_limits<uint32_t>::max();
                    break;
                case eMaterialTexture_CubeMap:
                    if (textureHandle < resource::MaxScene2DTextureCount) {
                        m_sceneTextureCubePool.dispose(textureHandle);
                    }
                    textureHandle = image ? m_sceneTextureCubePool.request(image->GetTexture()) : std::numeric_limits<uint32_t>::max();
                    break;
                default:
                    ASSERT(false);
                    break;
                }
            }
            sharedMat.m_sets.clear();
        }

        return sharedMat;
    }

    SharedRenderTarget cRendererDeferred2::GetOutputImage(uint32_t frameIndex, cViewport& viewport) {
        auto sharedData = m_boundViewportData.resolve(viewport);
        if (!sharedData) {
            return SharedRenderTarget();
        }
        return sharedData->m_outputBuffer[frameIndex];
    }

    void cRendererDeferred2::setIndirectDrawArg(
        const ForgeRenderer::Frame& frame, uint32_t drawArgIndex, uint32_t slot, DrawPacket& packet) {
        BufferUpdateDesc updateDesc = { m_indirectDrawArgsBuffer[frame.m_frameIndex].m_handle,
                                        drawArgIndex * IndirectArgumentSize,
                                        IndirectArgumentSize };
        beginUpdateResource(&updateDesc);
        if (m_supportIndirectRootConstant) {
            auto* indirectDrawArgs = reinterpret_cast<RootConstantDrawIndexArguments*>(updateDesc.pMappedData);
            indirectDrawArgs->mDrawId = slot;
            indirectDrawArgs->mStartInstance = drawArgIndex;
            indirectDrawArgs->mIndexCount = packet.m_unified.m_numIndices;
            indirectDrawArgs->mStartIndex = packet.m_unified.m_subAllocation->indexOffset();
            indirectDrawArgs->mVertexOffset = packet.m_unified.m_subAllocation->vertexOffset();
            indirectDrawArgs->mInstanceCount = 1;
        } else {
            auto* indirectDrawArgs = reinterpret_cast<IndirectDrawIndexArguments*>(updateDesc.pMappedData);
            indirectDrawArgs->mStartInstance = slot;
            indirectDrawArgs->mIndexCount = packet.m_unified.m_numIndices;
            indirectDrawArgs->mStartIndex = packet.m_unified.m_subAllocation->indexOffset();
            indirectDrawArgs->mVertexOffset = packet.m_unified.m_subAllocation->vertexOffset();
            indirectDrawArgs->mInstanceCount = 1;
        }
        endUpdateResource(&updateDesc);
    }

    void cRendererDeferred2::Draw(
        Cmd* cmd,
        ForgeRenderer::Frame& frame,
        cViewport& viewport,
        float frameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings) {

        BeginRendering(frameTime, apFrustum, apWorld, apSettings, false);

        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();

        if (m_resetHandler.Check(frame)) {
            m_sceneTexture2DPool.reset([&](BindlessDescriptorPool::Action action, uint32_t slot, SharedTexture& texture) {
                std::array<DescriptorData, 1> params = { DescriptorData{
                    .pName = "sceneTextures",
                    .mCount = 1,
                    .mArrayOffset = slot,
                    .ppTextures =
                        (action == BindlessDescriptorPool::Action::UpdateSlot ? &texture.m_handle : &m_emptyTexture2D.m_handle) } };
                updateDescriptorSet(
                    forgeRenderer->Rend(), 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle, params.size(), params.data());
            });
            m_sceneTextureCubePool.reset([&](BindlessDescriptorPool::Action action, uint32_t slot, SharedTexture& texture) {
                std::array<DescriptorData, 1> params = { DescriptorData{
                    .pName = "sceneCubeTextures",
                    .mCount = 1,
                    .mArrayOffset = slot,
                    .ppTextures =
                        (action == BindlessDescriptorPool::Action::UpdateSlot ? &texture.m_handle : &m_emptyTextureCube.m_handle) } };
                updateDescriptorSet(
                    forgeRenderer->Rend(), 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle, params.size(), params.data());
            });
            m_sceneTransientImage2DPool.reset(frame);  // this is bad need something different ...
        }

        auto& frameVars = m_variables.Fetch(frame, [&](TransientFrameVars& vars) {
            vars.m_objectIndex = 0;
            vars.m_indirectIndex = 0;
            vars.m_particleIndex = 0;
            vars.m_viewportIndex = 0;
            vars.m_objectSlotIndex.clear();
        });


        auto viewportDatum = m_boundViewportData.resolve(viewport);
        if (!viewportDatum || viewportDatum->m_size != viewport.GetSizeU2()) {
            auto updateDatum = std::make_unique<ViewportData>();
            updateDatum->m_size = viewport.GetSizeU2();
            for (size_t i = 0; i < ForgeRenderer::SwapChainLength; i++) {
                updateDatum->m_outputBuffer[i].Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = optimizedColorClearBlack;
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mWidth = updateDatum->m_size.x;
                    renderTargetDesc.mHeight = updateDatum->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                    renderTargetDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
                    renderTargetDesc.pName = "output RT";
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });

                updateDatum->m_testBuffer[i].Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = { .depth = 1.0f, .stencil = 0 };
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = updateDatum->m_size.x;
                    renderTargetDesc.mHeight = updateDatum->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
                    renderTargetDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
                    renderTargetDesc.pName = "parallax RT";
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });
                updateDatum->m_depthBuffer[i].Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = { .depth = 1.0f, .stencil = 0 };
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = updateDatum->m_size.x;
                    renderTargetDesc.mHeight = updateDatum->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mFormat = DepthBufferFormat;
                    renderTargetDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
                    renderTargetDesc.pName = "Depth RT";
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });

                updateDatum->m_albedoBuffer[i].Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = { .depth = 1.0f, .stencil = 0 };
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = updateDatum->m_size.x;
                    renderTargetDesc.mHeight = updateDatum->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                    renderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                    renderTargetDesc.pName = "albedo RT";
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });

                updateDatum->m_refractionImage[i].Load([&](Texture** texture) {
                    TextureLoadDesc loadDesc = {};
                    loadDesc.ppTexture = texture;
                    TextureDesc refractionImageDesc = {};
                    refractionImageDesc.mArraySize = 1;
                    refractionImageDesc.mDepth = 1;
                    refractionImageDesc.mMipLevels = 1;
                    refractionImageDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                    refractionImageDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
                    refractionImageDesc.mWidth = updateDatum->m_size.x;
                    refractionImageDesc.mHeight = updateDatum->m_size.y;
                    refractionImageDesc.mSampleCount = SAMPLE_COUNT_1;
                    refractionImageDesc.mSampleQuality = 0;
                    refractionImageDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                    refractionImageDesc.pName = "Refraction Image";
                    loadDesc.pDesc = &refractionImageDesc;
                    addResource(&loadDesc, nullptr);
                    return true;
                });
                updateDatum->m_visiblityBuffer[i].Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = optimizedColorClearBlack;
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = updateDatum->m_size.x;
                    renderTargetDesc.mHeight = updateDatum->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
                    renderTargetDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });
            }
            viewportDatum = m_boundViewportData.update(viewport, std::move(updateDatum));
        }

        frame.m_resourcePool->Push(viewportDatum->m_testBuffer[frame.m_frameIndex]);
        frame.m_resourcePool->Push(viewportDatum->m_visiblityBuffer[frame.m_frameIndex]);
        frame.m_resourcePool->Push(viewportDatum->m_depthBuffer[frame.m_frameIndex]);
        frame.m_resourcePool->Push(viewportDatum->m_outputBuffer[frame.m_frameIndex]);
        frame.m_resourcePool->Push(viewportDatum->m_albedoBuffer[frame.m_frameIndex]);
        frame.m_resourcePool->Push(viewportDatum->m_refractionImage[frame.m_frameIndex]);

        uint32_t mainBatchIndex = frameVars.m_viewportIndex++;
        {
	        GPURingBufferOffset offset = getGPURingBufferOffset(&m_viewPortUniformBuffer, sizeof(resource::ViewportInfo));
	        BufferUpdateDesc updateDesc = { offset.pBuffer, offset.mOffset };
	        beginUpdateResource(&updateDesc);
            resource::ViewportInfo* viewportInfo = reinterpret_cast<resource::ViewportInfo*>(updateDesc.pMappedData);
	        (*viewportInfo) = resource::ViewportInfo::create(apFrustum, float4(0.0f, 0.0f, static_cast<float>(viewportDatum->m_size.x), static_cast<float>(viewportDatum->m_size.y)));
	        endUpdateResource(&updateDesc);

            {
                DescriptorDataRange viewportRange = { (uint32_t)offset.mOffset, sizeof(resource::ViewportInfo) };
                std::array params = {
                    DescriptorData { .pName = "viewportBlock", .pRanges = &viewportRange, .ppBuffers = &offset.pBuffer }
                };
                updateDescriptorSet(
                    forgeRenderer->Rend(), mainBatchIndex, m_sceneDescriptorPerBatchSet[frame.m_frameIndex].m_handle, params.size(), params.data());
            }
        }

        // ----------------
        // Setup Data
        // -----------------
        {
            std::array params = {
                DescriptorData{ .pName = "visibilityTexture",
                                .ppTextures = &viewportDatum->m_visiblityBuffer[frame.m_frameIndex].m_handle->pTexture },
                DescriptorData{ .pName = "albedoTexture",
                                .ppTextures = &viewportDatum->m_albedoBuffer[frame.m_frameIndex].m_handle->pTexture },
                DescriptorData{ .pName = "refractionTexture",
                                .ppTextures = &viewportDatum->m_refractionImage[frame.m_frameIndex].m_handle },
            };
            updateDescriptorSet(
                forgeRenderer->Rend(), 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle, params.size(), params.data());
        }



        {
            BufferUpdateDesc updateDesc = { m_perSceneInfoBuffer[frame.m_frameIndex].m_handle, 0, sizeof(resource::SceneInfoResource) };
            beginUpdateResource(&updateDesc);
            resource::SceneInfoResource* sceneInfo = reinterpret_cast<resource::SceneInfoResource*>(updateDesc.pMappedData);
            const auto fogColor = apWorld->GetFogColor();
            sceneInfo->m_worldInfo.m_fogColor = float4(fogColor.r, fogColor.g, fogColor.b, fogColor.a);
            sceneInfo->m_worldInfo.m_worldFogStart = apWorld->GetFogStart();
            sceneInfo->m_worldInfo.m_worldFogLength = apWorld->GetFogEnd() - apWorld->GetFogStart();
            sceneInfo->m_worldInfo.m_oneMinusFogAlpha = 1.0f - apWorld->GetFogColor().a;
            sceneInfo->m_worldInfo.m_fogFalloffExp = apWorld->GetFogFalloffExp();
            sceneInfo->m_worldInfo.m_flags = (apWorld->GetFogActive() ? resource::WorldFogEnabled : 0);

            auto& primaryViewport = sceneInfo->m_viewports[resource::ViewportInfo::PrmaryViewportIndex];
            primaryViewport.m_viewMat = apFrustum->GetViewMat();
            primaryViewport.m_projMat = apFrustum->GetProjectionMat();
            primaryViewport.m_invProjMat = inverse(apFrustum->GetProjectionMat());
            primaryViewport.m_invViewMat = inverse(apFrustum->GetViewMat());
            primaryViewport.m_invViewProj = inverse(primaryViewport.m_projMat * primaryViewport.m_viewMat);
            primaryViewport.m_zFar = apFrustum->GetFarPlane();
            primaryViewport.m_zNear = apFrustum->GetNearPlane();
            primaryViewport.m_rect =
                float4(0.0f, 0.0f, static_cast<float>(viewportDatum->m_size.x), static_cast<float>(viewportDatum->m_size.y));
            primaryViewport.m_cameraPosition = v3ToF3(cMath::ToForgeVec3(apFrustum->GetOrigin()));
            endUpdateResource(&updateDesc);
        }

        // setup render list
        {
            m_rendererList.BeginAndReset(frameTime, apFrustum);
            auto* dynamicContainer = apWorld->GetRenderableContainer(eWorldContainerType_Dynamic);
            auto* staticContainer = apWorld->GetRenderableContainer(eWorldContainerType_Static);
            dynamicContainer->UpdateBeforeRendering();
            staticContainer->UpdateBeforeRendering();

            auto prepareObjectHandler = [&](iRenderable* pObject) {
                if (!iRenderable::IsObjectIsVisible(*pObject, eRenderableFlag_VisibleInNonReflection, {})) {
                    return;
                }
                m_rendererList.AddObject(pObject);
            };
            iRenderableContainer::WalkRenderableContainer(
                *dynamicContainer, apFrustum, prepareObjectHandler, eRenderableFlag_VisibleInNonReflection);
            iRenderableContainer::WalkRenderableContainer(
                *staticContainer, apFrustum, prepareObjectHandler, eRenderableFlag_VisibleInNonReflection);
            m_rendererList.End(
                eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Translucent | eRenderListCompileFlag_Decal |
                eRenderListCompileFlag_Illumination | eRenderListCompileFlag_FogArea);
        }

        RangeSubsetAlloc::RangeSubset opaqueIndirectArgs;
        RangeSubsetAlloc::RangeSubset opaqueCutIndirectArgs;

        struct DecalArg {
            RangeSubsetAlloc::RangeSubset range;
            eMaterialBlendMode blend;
        };
        enum class TranslucentDrawType : uint8_t {
            Translucency,
            Particle,
            Water
        };
        struct TranslucentArgs {
            TranslucentDrawType type;
            iRenderable* renderable;
            Pipeline* pipeline;
            Pipeline* illuminationPipline;
            uint32_t indirectDrawIndex;
            uint32_t numberIndecies;
            uint32_t indexOffset;
            uint32_t vertexOffset;
        };

        std::vector<DecalArg> decalIndirectArgs;
        std::vector<TranslucentArgs> translucenctArgs;
        std::vector<cRendererDeferred2::FogRendererData> fogRenderData = detail::createFogRenderData(m_rendererList.GetFogAreas(), apFrustum);

        RangeSubsetAlloc indirectAllocSession(frameVars.m_indirectIndex);
        for (auto& renderable : m_rendererList.GetRenderableItems(eRenderListType_Translucent)) {
            cMaterial* material = renderable->GetMaterial();
            if (material == nullptr) {
                continue;
            }

            if (!renderable->UpdateGraphicsForViewport(apFrustum, frameTime)) {
                continue;
            }

            auto& particleSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::ParticleSet);
            DrawPacket packet = renderable->ResolveDrawPacket(frame);
            if (packet.m_type == DrawPacket::Unknown) {
                continue;
            }
            ASSERT(packet.m_type == DrawPacket::DrawPacketType::DrawGeometryset);

            cMatrixf* pMatrix = renderable->GetModelMatrix(apFrustum);
            Matrix4 modelMat = pMatrix ? cMath::ToForgeMatrix4(*pMatrix) : Matrix4::identity();

            auto& descriptor = material->Descriptor();

            if (TypeInfo<iParticleEmitter>::IsSubtype(*renderable)) {
                auto& sharedMaterial = resolveResourceMaterial(material);

                BufferUpdateDesc updateDesc = { m_particleBuffer[frame.m_frameIndex].m_handle,
                                                frameVars.m_particleIndex * sizeof(resource::SceneParticle) };
                beginUpdateResource(&updateDesc);
                auto* sceneParticle = static_cast<resource::SceneParticle*>(updateDesc.pMappedData);
                sceneParticle->diffuseTextureIndex = sharedMaterial.m_textureHandles[eMaterialTexture_Diffuse];
                sceneParticle->sceneAlpha = 1.0f;
                for (auto& fogArea : fogRenderData) {
                    sceneParticle->sceneAlpha *= detail::GetFogAreaVisibilityForObject(fogArea, *apFrustum, renderable);
                }
                sceneParticle->lightLevel = 1.0f;
                if (material->IsAffectedByLightLevel()) {
                    sceneParticle->lightLevel = detail::calcLightLevel(m_rendererList.GetLights(), renderable);
                }
                sceneParticle->modelMat = modelMat;
                sceneParticle->uvMat = cMath::ToForgeMatrix4(material->GetUvMatrix().GetTranspose());
                endUpdateResource(&updateDesc);

                translucenctArgs.push_back({
                    TranslucentDrawType::Particle,
                    renderable,
                    material->GetDepthTest()?
                        m_particlePipeline.getPipelineByBlendMode(material->GetBlendMode()).m_handle:
                        m_particlePipelineNoDepth.getPipelineByBlendMode(material->GetBlendMode()).m_handle,
                    nullptr,
                    frameVars.m_particleIndex,
                    packet.m_unified.m_numIndices,
                    packet.m_unified.m_subAllocation->indexOffset(),
                    packet.m_unified.m_subAllocation->vertexOffset(),
                });
                frameVars.m_particleIndex++;
            } else {
                const bool isRefraction = iRenderer::GetRefractionEnabled() && material->HasRefraction();
                const bool isReflection = material->HasWorldReflection() && renderable->GetRenderType() == eRenderableType_SubMesh &&
                    mpCurrentSettings->mbRenderWorldReflection;
                const auto cubeMap = material->GetImage(eMaterialTexture_CubeMap);

                auto& resourceMaterial = resolveResourceMaterial(material);
                auto& materialSet = resourceMaterial.resolveSet(MaterialSetType::PrimarySet);
                const uint32_t slot = frameVars.m_objectIndex++;


                if (!materialSet.m_slot.isValid()) {
                    const auto alphaMapImage = material->GetImage(eMaterialTexture_Alpha);
                    const auto heightMapImage = material->GetImage(eMaterialTexture_Height);
                    switch (descriptor.m_id) {
                    case MaterialID::Translucent:
                        {
                            materialSet.m_slot = IndexPoolHandle(&m_translucencyIndexPool);
                            BufferUpdateDesc updateDesc = { m_translucencyMatBuffer.m_handle,
                                                            sizeof(resource::TranslucentMaterial) * materialSet.m_slot.get() };
                            beginUpdateResource(&updateDesc);
                            auto& translucenctMaterial = (*reinterpret_cast<resource::TranslucentMaterial*>(updateDesc.pMappedData));
                            struct {
                                eMaterialTexture m_type;
                                uint32_t* m_value;
                            } textures[] = {
                                { eMaterialTexture_Diffuse, &translucenctMaterial.m_diffuseTextureIndex },
                                { eMaterialTexture_NMap, &translucenctMaterial.m_normalTextureIndex },
                                { eMaterialTexture_Alpha, &translucenctMaterial.m_alphaTextureIndex },
                                { eMaterialTexture_Specular, &translucenctMaterial.m_specularTextureIndex },
                                { eMaterialTexture_Height, &translucenctMaterial.m_heightTextureIndex },
                                { eMaterialTexture_Illumination, &translucenctMaterial.m_illuminiationTextureIndex },
                                { eMaterialTexture_DissolveAlpha, &translucenctMaterial.m_dissolveAlphaTextureIndex },
                                { eMaterialTexture_CubeMap, &translucenctMaterial.m_cubeMapTextureIndex },
                                { eMaterialTexture_CubeMapAlpha, &translucenctMaterial.m_cubeMapAlphaTextureIndex },
                            };

                            for (auto& tex : textures) {
                                (*tex.m_value) = resourceMaterial.m_textureHandles[tex.m_type];
                            }
                            translucenctMaterial.m_refractionScale = descriptor.m_translucent.m_refractionScale;
                            translucenctMaterial.m_frenselBias = descriptor.m_translucent.m_frenselBias;
                            translucenctMaterial.m_frenselPow = descriptor.m_translucent.m_frenselPow;
                            translucenctMaterial.m_rimMulLight = descriptor.m_translucent.m_rimLightMul;
                            translucenctMaterial.m_rimMulPower = descriptor.m_translucent.m_rimLightPow;
                            translucenctMaterial.m_config = ((alphaMapImage &&TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(alphaMapImage->GetTexture().m_handle->mFormat)) == 1) ? resource::IsAlphaSingleChannel : 0) |
                                ((heightMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(heightMapImage->GetTexture().m_handle->mFormat)) == 1) ? resource::IsHeightSingleChannel : 0);
                            endUpdateResource(&updateDesc);
                            break;
                        }
                    case MaterialID::Water:
                        {
                            materialSet.m_slot = IndexPoolHandle(&m_waterIndexPool);
                            BufferUpdateDesc updateDesc = { m_translucencyMatBuffer.m_handle,
                                                            sizeof(resource::WaterMaterial) * materialSet.m_slot.get() };
                            beginUpdateResource(&updateDesc);
                            auto& waterMaterial = (*reinterpret_cast<resource::WaterMaterial*>(updateDesc.pMappedData));
                            struct {
                                eMaterialTexture m_type;
                                uint32_t* m_value;
                            } textures[] = {
                                { eMaterialTexture_Diffuse, &waterMaterial.m_diffuseTextureIndex },
                                { eMaterialTexture_NMap, &waterMaterial.m_normalTextureIndex },
                                { eMaterialTexture_CubeMap, &waterMaterial.m_cubemapTextureIndex },
                            };
                            for (auto& tex : textures) {
                                (*tex.m_value) = resourceMaterial.m_textureHandles[tex.m_type];
                            }

                            waterMaterial.m_config =
                                (isReflection ? resource::UseReflection : 0) |
                                (isRefraction ? resource::UseRefraction : 0);

                            waterMaterial.m_refractionScale = descriptor.m_water.m_refractionScale;
                            waterMaterial.m_frenselBias = descriptor.m_water.m_frenselBias;
                            waterMaterial.m_frenselPow = descriptor.m_water.m_frenselPow;

                            waterMaterial.m_reflectionFadeStart = descriptor.m_water.m_reflectionFadeStart;
                            waterMaterial.m_reflectionFadeEnd = descriptor.m_water.m_reflectionFadeEnd;
                            waterMaterial.m_waveSpeed = descriptor.m_water.m_waveSpeed;
                            waterMaterial.m_waveAmplitude = descriptor.m_water.m_waveAmplitude;

                            waterMaterial.m_waveFreq = descriptor.m_water.m_waveFreq;
                            endUpdateResource(&updateDesc);
                            break;
                        }
                    default:
                        ASSERT(false);
                        break;
                    }
                }
                BufferUpdateDesc updateDesc = { m_objectUniformBuffer[frame.m_frameIndex].m_handle, sizeof(resource::SceneObject) * slot };
                beginUpdateResource(&updateDesc);
                auto& uniformObjectData = (*reinterpret_cast<resource::SceneObject*>(updateDesc.pMappedData));
                uniformObjectData.m_dissolveAmount = renderable->GetCoverageAmount();
                uniformObjectData.m_modelMat = modelMat;
                uniformObjectData.m_invModelMat = inverse(modelMat);
                uniformObjectData.m_lightLevel = 1.0f;
                uniformObjectData.m_alphaAmount = 1.0f;
                if (material->IsAffectedByLightLevel()) {
                    uniformObjectData.m_lightLevel = detail::calcLightLevel(m_rendererList.GetLights(), renderable);
                }
                for (auto& fogArea : fogRenderData) {
                    uniformObjectData.m_alphaAmount *= detail::GetFogAreaVisibilityForObject(fogArea, *apFrustum, renderable);
                }
                uniformObjectData.m_illuminationAmount = renderable->GetIlluminationAmount();
                uniformObjectData.m_materialIndex = materialSet.m_slot.get();
                uniformObjectData.m_materialType = static_cast<uint32_t>(descriptor.m_id);
                uniformObjectData.m_vertexOffset = packet.m_unified.m_subAllocation->vertexOffset();
                uniformObjectData.m_indexOffset = packet.m_unified.m_subAllocation->indexOffset();
                uniformObjectData.m_uvMat = cMath::ToForgeMatrix4(material->GetUvMatrix().GetTranspose());
                endUpdateResource(&updateDesc);

                translucenctArgs.push_back({
                    ([&]() {
                        if (descriptor.m_id == MaterialID::Water) {
                            return TranslucentDrawType::Water;
                        }
                        return TranslucentDrawType::Translucency;
                    })(),
                    renderable,
                    ([&]() {
                        if (isRefraction) {
                            if (material->GetDepthTest()) {
                                return m_translucencyRefractionPipline.getPipelineByBlendMode(material->GetBlendMode()).m_handle;
                            }
                            return m_translucencyRefractionPiplineNoDepth.getPipelineByBlendMode(material->GetBlendMode()).m_handle;
                        }
                        if (material->GetDepthTest()) {
                            return m_translucencyPipline.getPipelineByBlendMode(material->GetBlendMode()).m_handle;
                        }
                        return m_translucencyPiplineNoDepth.getPipelineByBlendMode(material->GetBlendMode()).m_handle;
                    })(),
                    (cubeMap && !isRefraction) ? (material->GetDepthTest() ? m_translucencyIlluminationPipline.m_handle
                                                                           : m_translucencyIlluminationPiplineNoDepth.m_handle)
                                               : nullptr,
                    slot,
                    packet.m_unified.m_numIndices,
                    packet.m_unified.m_subAllocation->indexOffset(),
                    packet.m_unified.m_subAllocation->vertexOffset(),
                });
            }
        }

        {
            auto consumeDiffuseRenderable = [&](iRenderable* renderable) {
                cMaterial* material = renderable->GetMaterial();

                DrawPacket packet = renderable->ResolveDrawPacket(frame);
                if (packet.m_type == DrawPacket::Unknown) {
                    return;
                }

                ASSERT(material->Descriptor().m_id == MaterialID::SolidDiffuse && "Invalid material type");
                ASSERT(packet.m_type == DrawPacket::DrawPacketType::DrawGeometryset);
                ASSERT(packet.m_unified.m_set == GraphicsAllocator::AllocationSet::OpaqueSet);

                const Matrix4 modelMat = renderable->GetModelMatrixPtr() ? cMath::ToForgeMatrix4(*renderable->GetModelMatrixPtr()) : Matrix4::identity();

                auto& resourceMaterial = resolveResourceMaterial(material);
                auto& materialSet = resourceMaterial.resolveSet(MaterialSetType::PrimarySet);
                auto& descriptor = material->Descriptor();
                ASSERT(descriptor.m_id == MaterialID::SolidDiffuse);
                if (!materialSet.m_slot.isValid()) {
                    const auto alphaMapImage = material->GetImage(eMaterialTexture_Alpha);
                    const auto heightMapImage = material->GetImage(eMaterialTexture_Height);
                    materialSet.m_slot = IndexPoolHandle(&m_diffuseIndexPool);
                    BufferUpdateDesc updateDesc = { m_diffuseMatUniformBuffer.m_handle,
                                                    sizeof(resource::DiffuseMaterial) * materialSet.m_slot.get() };
                    beginUpdateResource(&updateDesc);
                    auto& diffuseMaterial = (*reinterpret_cast<resource::DiffuseMaterial*>(updateDesc.pMappedData));
                    struct {
                        eMaterialTexture m_type;
                        uint32_t* m_value;
                    } textures[] = {
                        { eMaterialTexture_Diffuse, &diffuseMaterial.m_diffuseTextureIndex },
                        { eMaterialTexture_NMap, &diffuseMaterial.m_normalTextureIndex },
                        { eMaterialTexture_Alpha, &diffuseMaterial.m_alphaTextureIndex },
                        { eMaterialTexture_Specular, &diffuseMaterial.m_specularTextureIndex },
                        { eMaterialTexture_Height, &diffuseMaterial.m_heightTextureIndex },
                        { eMaterialTexture_Illumination, &diffuseMaterial.m_illuminiationTextureIndex },
                        { eMaterialTexture_DissolveAlpha, &diffuseMaterial.m_dissolveAlphaTextureIndex },
                    };
                    for (auto& tex : textures) {
                        (*tex.m_value) = resourceMaterial.m_textureHandles[tex.m_type];
                    }
                    diffuseMaterial.m_materialConfig =
                        ((alphaMapImage &&
                          TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(alphaMapImage->GetTexture().m_handle->mFormat)) == 1)
                             ? resource::IsAlphaSingleChannel
                             : 0) |
                        ((heightMapImage &&
                          TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(heightMapImage->GetTexture().m_handle->mFormat)) == 1)
                             ? resource::IsHeightSingleChannel
                             : 0) |
                        (descriptor.m_solid.m_alphaDissolveFilter ? resource::UseAlphaDissolveFilter : 0);

                    diffuseMaterial.m_heightMapScale = descriptor.m_solid.m_heightMapScale;
                    diffuseMaterial.m_heigtMapBias = descriptor.m_solid.m_heightMapBias;
                    diffuseMaterial.m_frenselBias = descriptor.m_solid.m_frenselBias;
                    diffuseMaterial.m_frenselPow = descriptor.m_solid.m_frenselPow;
                    endUpdateResource(&updateDesc);
                }

                uint32_t objectSlotIndex = 0;
                auto it = frameVars.m_objectSlotIndex.find(renderable);
                if(it == frameVars.m_objectSlotIndex.end()) {
                    objectSlotIndex = frameVars.m_objectIndex++;

                    BufferUpdateDesc updateDesc = { m_objectUniformBuffer[frame.m_frameIndex].m_handle,
                                                    sizeof(resource::SceneObject) * objectSlotIndex };
                    beginUpdateResource(&updateDesc);
                    auto& uniformObjectData = (*reinterpret_cast<resource::SceneObject*>(updateDesc.pMappedData));
                    uniformObjectData.m_dissolveAmount = renderable->GetCoverageAmount();
                    uniformObjectData.m_modelMat = modelMat;
                    uniformObjectData.m_invModelMat = inverse(modelMat);
                    uniformObjectData.m_lightLevel = 1.0f;
                    if (material->IsAffectedByLightLevel()) {
                        uniformObjectData.m_lightLevel = detail::calcLightLevel(m_rendererList.GetLights(), renderable);
                    }
                    uniformObjectData.m_illuminationAmount = renderable->GetIlluminationAmount();
                    uniformObjectData.m_materialIndex = materialSet.m_slot.get();
                    uniformObjectData.m_materialType = static_cast<uint32_t>(material->Descriptor().m_id);
                    uniformObjectData.m_vertexOffset = packet.m_unified.m_subAllocation->vertexOffset();
                    uniformObjectData.m_indexOffset = packet.m_unified.m_subAllocation->indexOffset();
                    uniformObjectData.m_uvMat = cMath::ToForgeMatrix4(material->GetUvMatrix().GetTranspose());
                    endUpdateResource(&updateDesc);

                    frameVars.m_objectSlotIndex[renderable] = objectSlotIndex;
                } else {
                    objectSlotIndex = it->second;
                }
                setIndirectDrawArg(frame, indirectAllocSession.Increment(), objectSlotIndex, packet);
            };

            std::vector<iRenderable*> diffuseCutRenderables;
            // diffuse - building indirect draw list
            for (auto& renderable : m_rendererList.GetRenderableItems(eRenderListType_Diffuse)) {
                cMaterial* material = renderable->GetMaterial();
                if(!material) {
                    continue;
                }
                if (renderable->GetCoverageAmount() < 1.0 || cMaterial::IsTranslucent(material->Descriptor().m_id) || material->GetImage(eMaterialTexture_Alpha)) {
                    diffuseCutRenderables.push_back(renderable);
                    continue;
                }
                consumeDiffuseRenderable(renderable);
            }
            opaqueIndirectArgs = indirectAllocSession.End();
            // diffuse build indirect list for opaque
            for (auto& renderable : diffuseCutRenderables) {
                consumeDiffuseRenderable(renderable);
            }
            opaqueCutIndirectArgs = indirectAllocSession.End();
        }

        {
            uint32_t decalIndex = 0;
            auto renderables = m_rendererList.GetRenderableItems(eRenderListType_Decal);
            const auto isValidRenderable = [](iRenderable* renderable) {
                return renderable->GetMaterial();
            };

            auto it = renderables.begin();
            while (it != renderables.end()) {
                eMaterialBlendMode lastBlendMode = eMaterialBlendMode_None;
                eMaterialBlendMode currentBlendMode = (*it)->GetMaterial()->GetBlendMode();
                do {
                    {
                        cMaterial* material = (*it)->GetMaterial();
                        DrawPacket packet = (*it)->ResolveDrawPacket(frame);
                        if (packet.m_type == DrawPacket::Unknown) {
                            continue;
                        }
                        auto& sharedMaterial = resolveResourceMaterial(material);
                        ASSERT(packet.m_type == DrawPacket::DrawPacketType::DrawGeometryset);
                        ASSERT(packet.m_unified.m_set == GraphicsAllocator::AllocationSet::OpaqueSet);
                        ASSERT(material->Descriptor().m_id == MaterialID::Decal && "Invalid material type");

                        BufferUpdateDesc updateDesc = { m_decalBuffer[frame.m_frameIndex].m_handle,
                                                        decalIndex * sizeof(resource::SceneDecal) };
                        beginUpdateResource(&updateDesc);
                        auto* sceneDecal = static_cast<resource::SceneDecal*>(updateDesc.pMappedData);
                        sceneDecal->diffuseTextureIndex = sharedMaterial.m_textureHandles[eMaterialTexture_Diffuse];
                        sceneDecal->modelMat =
                            (*it)->GetModelMatrixPtr() ? cMath::ToForgeMatrix4(*(*it)->GetModelMatrixPtr()) : Matrix4::identity();
                        sceneDecal->uvMat = cMath::ToForgeMatrix4(material->GetUvMatrix().GetTranspose());
                        endUpdateResource(&updateDesc);
                        setIndirectDrawArg(frame, indirectAllocSession.Increment(), decalIndex, packet);

                        decalIndex++;
                    }

                    lastBlendMode = currentBlendMode;

                    it++;
                    if (it == renderables.end()) {
                        break;
                    }

                    {
                        cMaterial* material = (*it)->GetMaterial();
                        currentBlendMode = material->GetBlendMode();
                    }

                } while (currentBlendMode == lastBlendMode);
                decalIndirectArgs.push_back({ indirectAllocSession.End(), lastBlendMode });
            }
        }

        uint32_t lightCount = 0;
        {
            for (auto& light : m_rendererList.GetLights()) {
                switch (light->GetLightType()) {
                case eLightType_Point:
                    {
                        BufferUpdateDesc lightUpdateDesc = { m_lightBuffer[frame.m_frameIndex].m_handle,
                                                             (lightCount++) * sizeof(resource::SceneLight),
                                                             sizeof(resource::SceneLight) };
                        beginUpdateResource(&lightUpdateDesc);
                        auto lightData = reinterpret_cast<resource::SceneLight*>(lightUpdateDesc.pMappedData);
                        const cColor color = light->GetDiffuseColor();
                        lightData->m_goboTexture = resource::InvalidSceneTexture;
                        lightData->m_lightType = hpl::resource::LightType::PointLight;
                        lightData->m_radius = light->GetRadius();
                        lightData->m_position = v3ToF3(cMath::ToForgeVec3(light->GetWorldPosition()));
                        lightData->m_color = float4(color.r, color.g, color.b, color.a);
                        endUpdateResource(&lightUpdateDesc);
                        break;
                    }
                case eLightType_Spot:
                    {
                        cLightSpot* pLightSpot = static_cast<cLightSpot*>(light);
                        float fFarHeight = pLightSpot->GetTanHalfFOV() * pLightSpot->GetRadius() * 2.0f;
                        // Note: Aspect might be wonky if there is no gobo.
                        float fFarWidth = fFarHeight * pLightSpot->GetAspect();
                        const cColor color = light->GetDiffuseColor();
                        Matrix4 spotViewProj = cMath::ToForgeMatrix4(pLightSpot->GetViewProjMatrix()); //* inverse(apFrustum->GetViewMat());

                        cVector3f forward = cMath::MatrixMul3x3(pLightSpot->GetWorldMatrix(), cVector3f(0, 0, -1));
                        auto goboImage = pLightSpot->GetGoboTexture();
                        BufferUpdateDesc lightUpdateDesc = { m_lightBuffer[frame.m_frameIndex].m_handle,
                                                             (lightCount++) * sizeof(resource::SceneLight),
                                                             sizeof(resource::SceneLight) };
                        beginUpdateResource(&lightUpdateDesc);
                        auto lightData = reinterpret_cast<resource::SceneLight*>(lightUpdateDesc.pMappedData);
                        lightData->m_lightType = hpl::resource::LightType::SpotLight;
                        lightData->m_radius = light->GetRadius();
                        lightData->m_direction = normalize(float3(forward.x, forward.y, forward.z));
                        lightData->m_color = float4(color.r, color.g, color.b, color.a);
                        lightData->m_position = v3ToF3(cMath::ToForgeVec3(light->GetWorldPosition()));
                        lightData->m_angle = pLightSpot->GetFOV();
                        lightData->m_viewProjection = spotViewProj;
                        lightData->m_goboTexture =
                            goboImage ? m_sceneTransientImage2DPool.request(goboImage) : resource::InvalidSceneTexture;
                        endUpdateResource(&lightUpdateDesc);
                        break;
                    }
                default:
                    break;
                }
            }
        }

        // ----------------
        // Compute pipeline
        // -----------------
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

            cmdBindPipeline(cmd, m_clearClusterPipeline.m_handle);
            cmdBindDescriptorSet(cmd, 0, m_lightDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
            cmdDispatch(cmd, 1, 1, LightClusterSlices);
            {
                std::array barriers = { BufferBarrier{ m_lightClusterCountBuffer[frame.m_frameIndex].m_handle,
                                                       RESOURCE_STATE_UNORDERED_ACCESS,
                                                       RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(cmd, barriers.size(), barriers.data(), 0, nullptr, 0, nullptr);
            }
            cmdBindPipeline(cmd, m_lightClusterPipeline.m_handle);
            cmdBindDescriptorSet(cmd, 0, m_lightDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
            cmdDispatch(cmd, lightCount, 1, LightClusterSlices);
            {
                std::array barriers = { BufferBarrier{ m_lightClustersBuffer[frame.m_frameIndex].m_handle,
                                                       RESOURCE_STATE_UNORDERED_ACCESS,
                                                       RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(cmd, barriers.size(), barriers.data(), 0, nullptr, 0, nullptr);
            }
        }

        // ----------------
        // Visibility pass
        // -----------------
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array barriers = { BufferBarrier{ m_lightClusterCountBuffer[frame.m_frameIndex].m_handle,
                                                   RESOURCE_STATE_UNORDERED_ACCESS,
                                                   RESOURCE_STATE_SHADER_RESOURCE },
                                    BufferBarrier{ m_lightClustersBuffer[frame.m_frameIndex].m_handle,
                                                   RESOURCE_STATE_UNORDERED_ACCESS,
                                                   RESOURCE_STATE_SHADER_RESOURCE} };
            cmdResourceBarrier(cmd, barriers.size(), barriers.data(), 0, nullptr, 0, nullptr);
        }
        {
            auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
            //cmdBeginDebugMarker(cmd, 0, 1, 0, "Visibility Buffer Pass");
            {
                std::array semantics = { ShaderSemantic::SEMANTIC_POSITION };
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
                loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
                loadActions.mClearColorValues[0] = { .r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f };
                loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
                loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;

                std::array targets = { viewportDatum->m_visiblityBuffer[frame.m_frameIndex].m_handle };
                cmdBindRenderTargets(
                    cmd,
                    targets.size(),
                    targets.data(),
                    viewportDatum->m_depthBuffer[frame.m_frameIndex].m_handle,
                    &loadActions,
                    NULL,
                    NULL,
                    -1,
                    -1);
                cmdSetViewport(cmd, 0.0f, 0.0f, viewportDatum->m_size.x, viewportDatum->m_size.y, 0.0f, 1.0f);
                cmdSetScissor(cmd, 0, 0, viewportDatum->m_size.x, viewportDatum->m_size.y);

                cmdBindPipeline(cmd, m_visibilityBufferPass.m_handle);
                opaqueSet.cmdBindGeometrySet(cmd, semantics);
                cmdBindIndexBuffer(cmd, opaqueSet.indexBuffer().m_handle, INDEX_TYPE_UINT32, 0);
                cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
                cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
                cmdExecuteIndirect(
                    cmd,
                    m_cmdSignatureVBPass,
                    opaqueIndirectArgs.size(),
                    m_indirectDrawArgsBuffer[frame.m_frameIndex].m_handle,
                    opaqueIndirectArgs.m_start * IndirectArgumentSize,
                    nullptr,
                    0);
            }
            {
                std::array semantics = { ShaderSemantic::SEMANTIC_POSITION,
                                         ShaderSemantic::SEMANTIC_TEXCOORD0,
                                         ShaderSemantic::SEMANTIC_NORMAL,
                                         ShaderSemantic::SEMANTIC_TANGENT };
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
                loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
                loadActions.mClearColorValues[0] = { .r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f };
                loadActions.mClearColorValues[1] = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
                loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
                loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

                std::array targets = { viewportDatum->m_visiblityBuffer[frame.m_frameIndex].m_handle };
                cmdBindRenderTargets(
                    cmd,
                    targets.size(),
                    targets.data(),
                    viewportDatum->m_depthBuffer[frame.m_frameIndex].m_handle,
                    &loadActions,
                    NULL,
                    NULL,
                    -1,
                    -1);
                cmdSetViewport(cmd, 0.0f, 0.0f, viewportDatum->m_size.x, viewportDatum->m_size.y, 0.0f, 1.0f);
                cmdSetScissor(cmd, 0, 0, viewportDatum->m_size.x, viewportDatum->m_size.y);

                cmdBindPipeline(cmd, m_visbilityAlphaBufferPass.m_handle);
                opaqueSet.cmdBindGeometrySet(cmd, semantics);
                cmdBindIndexBuffer(cmd, opaqueSet.indexBuffer().m_handle, INDEX_TYPE_UINT32, 0);
                cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
                cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
                cmdExecuteIndirect(
                    cmd,
                    m_cmdSignatureVBPass,
                    opaqueCutIndirectArgs.size(),
                    m_indirectDrawArgsBuffer[frame.m_frameIndex].m_handle,
                    opaqueCutIndirectArgs.m_start * IndirectArgumentSize,
                    nullptr,
                    0);
            }
        }
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{ viewportDatum->m_visiblityBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_RENDER_TARGET,
                                     RESOURCE_STATE_PIXEL_SHADER_RESOURCE},
                RenderTargetBarrier{ viewportDatum->m_albedoBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_SHADER_RESOURCE,
                                     RESOURCE_STATE_RENDER_TARGET},
            };
            cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, rtBarriers.size(), rtBarriers.data());
        }
        {
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
            loadActions.mClearColorValues[0] = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
            loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
            loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
            std::array targets = { viewportDatum->m_albedoBuffer[frame.m_frameIndex].m_handle };
            cmdBindRenderTargets(cmd, targets.size(), targets.data(), nullptr, &loadActions, NULL, NULL, -1, -1);
            cmdSetViewport(cmd, 0.0f, 0.0f, viewportDatum->m_size.x, viewportDatum->m_size.y, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, viewportDatum->m_size.x, viewportDatum->m_size.y);
            cmdBindPipeline(cmd, m_visbilityEmitBufferPass.m_handle);
            cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
            cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
            cmdBindDescriptorSet(cmd, mainBatchIndex, m_sceneDescriptorPerBatchSet[frame.m_frameIndex].m_handle);
            cmdDraw(cmd, 3, 0);
        }
        {
            //cmdBeginDebugMarker(cmd, 0, 1, 0, "Decal Output Buffer Pass");
            auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
            std::array semantics = { ShaderSemantic::SEMANTIC_POSITION,
                                     ShaderSemantic::SEMANTIC_TEXCOORD0,
                                     ShaderSemantic::SEMANTIC_COLOR };
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            loadActions.mClearColorValues[0] = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
            loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
            loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
            std::array targets = { viewportDatum->m_albedoBuffer[frame.m_frameIndex].m_handle };
            cmdBindRenderTargets(
                cmd,
                targets.size(),
                targets.data(),
                viewportDatum->m_depthBuffer[frame.m_frameIndex].m_handle,
                &loadActions,
                NULL,
                NULL,
                -1,
                -1);
            cmdSetViewport(cmd, 0.0f, 0.0f, viewportDatum->m_size.x, viewportDatum->m_size.y, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, viewportDatum->m_size.x, viewportDatum->m_size.y);
            opaqueSet.cmdBindGeometrySet(cmd, semantics);
            cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
            cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);

            struct DecalDrawArg {
                SharedPipeline* pipeline;
            } drawArgsType[] = {
                { &m_decalPipelines.m_pipelineBlendAdd }, /* eMaterialBlendMode_None */
                { &m_decalPipelines.m_pipelineBlendAdd }, /* eMaterialBlendMode_Add */
                { &m_decalPipelines.m_pipelineBlendMul }, /* eMaterialBlendMode_Mul */
                { &m_decalPipelines.m_pipelineBlendMulX2 }, /* eMaterialBlendMode_MulX2 */
                { &m_decalPipelines.m_pipelineBlendAlpha }, /* eMaterialBlendMode_Alpha */
                { &m_decalPipelines.m_pipelineBlendPremulAlpha }, /* eMaterialBlendMode_PremulAlpha */
            };

            for (auto& indirectArg : decalIndirectArgs) {
                cmdBindPipeline(cmd, drawArgsType[indirectArg.blend].pipeline->m_handle);
                cmdExecuteIndirect(
                    cmd,
                    m_cmdSignatureVBPass,
                    indirectArg.range.size(),
                    m_indirectDrawArgsBuffer[frame.m_frameIndex].m_handle,
                    indirectArg.range.m_start * IndirectArgumentSize,
                    nullptr,
                    0);
            }

            //cmdEndDebugMarker(cmd);
        }
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{ viewportDatum->m_albedoBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_RENDER_TARGET,
                                     RESOURCE_STATE_SHADER_RESOURCE},
                RenderTargetBarrier{ viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_SHADER_RESOURCE,
                                     RESOURCE_STATE_RENDER_TARGET},
            };
            cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, rtBarriers.size(), rtBarriers.data());
        }
        {
            //cmdBeginDebugMarker(cmd, 0, 1, 0, "Visibility Output Buffer Pass");
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
            loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
            std::array targets = { viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle};
            cmdBindRenderTargets(cmd, targets.size(), targets.data(), nullptr, &loadActions, NULL, NULL, -1, -1);
            cmdSetViewport(cmd, 0.0f, 0.0f, viewportDatum->m_size.x, viewportDatum->m_size.y, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, viewportDatum->m_size.x, viewportDatum->m_size.y);
            cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
            cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
            cmdBindPipeline(cmd, m_visiblityShadePass.m_handle);
            cmdDraw(cmd, 3, 0);
            // cmdEndDebugMarker(cmd);
        }
        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array textureBarriers = {
                TextureBarrier{ viewportDatum->m_refractionImage[frame.m_frameIndex].m_handle,
                                RESOURCE_STATE_SHADER_RESOURCE,
                                RESOURCE_STATE_UNORDERED_ACCESS },
            };
            std::array rtBarriers = {
                RenderTargetBarrier{ viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_RENDER_TARGET,
                                     RESOURCE_STATE_SHADER_RESOURCE },
            };
            cmdResourceBarrier(frame.m_cmd, 0, NULL, textureBarriers.size(), textureBarriers.data(), rtBarriers.size(), rtBarriers.data());
        }
        m_copySubpass.Dispatch(
            frame,
            viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle->pTexture,
            viewportDatum->m_refractionImage[frame.m_frameIndex].m_handle);
        {
            std::array textureBarriers = {
                TextureBarrier{ viewportDatum->m_refractionImage[frame.m_frameIndex].m_handle,
                                RESOURCE_STATE_UNORDERED_ACCESS,
                                RESOURCE_STATE_SHADER_RESOURCE },
            };
            std::array rtBarriers = {
                RenderTargetBarrier{
                    viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle,
                    RESOURCE_STATE_SHADER_RESOURCE,
                    RESOURCE_STATE_RENDER_TARGET,
                },
            };
            cmdResourceBarrier(frame.m_cmd, 0, NULL, textureBarriers.size(), textureBarriers.data(), rtBarriers.size(), rtBarriers.data());
        }
        {
            auto& particleSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::ParticleSet);
            auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            loadActions.mClearColorValues[0] = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
            loadActions.mClearDepth = { .depth = 1.0f, .stencil = 0 };
            loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
            std::array targets = { viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle };
            cmdBindRenderTargets(
                cmd,
                targets.size(),
                targets.data(),
                viewportDatum->m_depthBuffer[frame.m_frameIndex].m_handle,
                &loadActions,
                NULL,
                NULL,
                -1,
                -1);
            cmdSetViewport(cmd, 0.0f, 0.0f, viewportDatum->m_size.x, viewportDatum->m_size.y, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, viewportDatum->m_size.x, viewportDatum->m_size.y);

            for (auto& arg : translucenctArgs) {
                cMaterial* pMaterial = arg.renderable->GetMaterial();
                switch (arg.type) {
                case TranslucentDrawType::Particle:
                    {
                        std::array semantics = { ShaderSemantic::SEMANTIC_POSITION,
                                                 ShaderSemantic::SEMANTIC_TEXCOORD0,
                                                 ShaderSemantic::SEMANTIC_COLOR };
                        particleSet.cmdBindGeometrySet(cmd, semantics);
                        cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
                        cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
                        cmdBindPipeline(cmd, arg.pipeline);
                        if (m_supportIndirectRootConstant) {
                            const uint32_t pushConstantIndex =
                                getDescriptorIndexFromName(m_sceneRootSignature.m_handle, "indirectRootConstant");
                            cmdBindPushConstants(cmd, m_sceneRootSignature.m_handle, pushConstantIndex, &arg.indirectDrawIndex);
                        }
                        cmdDrawIndexedInstanced(cmd, arg.numberIndecies, arg.indexOffset, 1, arg.vertexOffset, arg.indirectDrawIndex);
                        break;
                    }
                case TranslucentDrawType::Water: {
                    std::array semantics = { ShaderSemantic::SEMANTIC_POSITION,
                                             ShaderSemantic::SEMANTIC_TEXCOORD0,
                                             ShaderSemantic::SEMANTIC_NORMAL,
                                             ShaderSemantic::SEMANTIC_TANGENT,
                                             ShaderSemantic::SEMANTIC_COLOR };
                    opaqueSet.cmdBindGeometrySet(cmd, semantics);
                    cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
                    cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
                    cmdBindPipeline(cmd, m_translucencyWaterPipeline.m_handle);
                    if (m_supportIndirectRootConstant) {
                        const uint32_t pushConstantIndex =
                            getDescriptorIndexFromName(m_sceneRootSignature.m_handle, "indirectRootConstant");
                        cmdBindPushConstants(cmd, m_sceneRootSignature.m_handle, pushConstantIndex, &arg.indirectDrawIndex);
                    }
                    cmdDrawIndexedInstanced(cmd, arg.numberIndecies, arg.indexOffset, 1, arg.vertexOffset, arg.indirectDrawIndex);
                    break;
                }
                case TranslucentDrawType::Translucency:
                    {
                        std::array semantics = { ShaderSemantic::SEMANTIC_POSITION,
                                                 ShaderSemantic::SEMANTIC_TEXCOORD0,
                                                 ShaderSemantic::SEMANTIC_NORMAL,
                                                 ShaderSemantic::SEMANTIC_TANGENT,
                                                 ShaderSemantic::SEMANTIC_COLOR };
                        opaqueSet.cmdBindGeometrySet(cmd, semantics);
                        cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorConstSet.m_handle);
                        cmdBindDescriptorSet(cmd, 0, m_sceneDescriptorPerFrameSet[frame.m_frameIndex].m_handle);
                        cmdBindPipeline(cmd, arg.pipeline);

                        if (m_supportIndirectRootConstant) {
                            const uint32_t pushConstantIndex =
                                getDescriptorIndexFromName(m_sceneRootSignature.m_handle, "indirectRootConstant");
                            cmdBindPushConstants(cmd, m_sceneRootSignature.m_handle, pushConstantIndex, &arg.indirectDrawIndex);
                        }
                        cmdDrawIndexedInstanced(cmd, arg.numberIndecies, arg.indexOffset, 1, arg.vertexOffset, arg.indirectDrawIndex);

                        if (arg.illuminationPipline) {
                            cmdBindPipeline(cmd, arg.illuminationPipline);
                            cmdDrawIndexedInstanced(cmd, arg.numberIndecies, arg.indexOffset, 1, arg.vertexOffset, arg.indirectDrawIndex);
                        }
                        break;
                    }
                default:
                    break;
                }
            }
        }

        {
            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array barriers = { BufferBarrier{ m_lightClusterCountBuffer[frame.m_frameIndex].m_handle,
                                                   RESOURCE_STATE_SHADER_RESOURCE,
                                                   RESOURCE_STATE_UNORDERED_ACCESS },
                                    BufferBarrier{ m_lightClustersBuffer[frame.m_frameIndex].m_handle,
                                                   RESOURCE_STATE_SHADER_RESOURCE,
                                                   RESOURCE_STATE_UNORDERED_ACCESS } };
            std::array rtBarriers = {
                RenderTargetBarrier{ viewportDatum->m_visiblityBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                     RESOURCE_STATE_RENDER_TARGET},
                RenderTargetBarrier{ viewportDatum->m_outputBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_RENDER_TARGET,
                                     RESOURCE_STATE_SHADER_RESOURCE },
            };
            cmdResourceBarrier(cmd, barriers.size(), barriers.data(), 0, nullptr, rtBarriers.size(), rtBarriers.data());
        }


    }

} // namespace hpl
