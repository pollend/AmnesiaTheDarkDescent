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

#include "LuxEffectRenderer.h"


#include "engine/Interface.h"
#include <graphics/Enum.h>
#include <graphics/RenderTarget.h>
#include <graphics/MaterialResource.h>
#include <math/MathTypes.h>
#include <memory>
#include <vector>

#include "graphics/DrawPacket.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/GraphicsTypes.h"

#include "impl/LegacyVertexBuffer.h"
#include "tinyimageformat_base.h"
#include <folly/small_vector.h>
#include "FixPreprocessor.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

cLuxEffectRenderer::cLuxEffectRenderer()
    : iLuxUpdateable("LuxEffectRenderer")
{

//     m_boundPostEffectData = std::move(UniqueViewportData<LuxPostEffectData>([](cViewport& viewport) {
//        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
//        auto postEffect = std::make_unique<LuxPostEffectData>();
//        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
//
//        postEffect->m_size = viewport.GetSize();
//        postEffect->m_outlineBuffer.Load([&](RenderTarget** target) {
//            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
//            RenderTargetDesc renderTarget = {};
//            renderTarget.mArraySize = 1;
//            renderTarget.mClearValue = optimizedColorClearBlack;
//            renderTarget.mDepth = 1;
//            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
//            renderTarget.mWidth = viewport.GetSize().x;
//            renderTarget.mHeight = viewport.GetSize().y;
//            renderTarget.mSampleCount = SAMPLE_COUNT_1;
//            renderTarget.mSampleQuality = 0;
//            renderTarget.mStartState = RESOURCE_STATE_RENDER_TARGET;
//            renderTarget.pName = "postEffect Outline";
//            renderTarget.mFormat = getRecommendedSwapchainFormat(false, false);
//            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
//            return true;
//        });
//        ASSERT(postEffect->m_blurTarget.size() == 2);
//        postEffect->m_blurTarget[0].Load([&](RenderTarget** target) {
//            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
//            RenderTargetDesc renderTarget = {};
//            renderTarget.mArraySize = 1;
//            renderTarget.mClearValue = optimizedColorClearBlack;
//            renderTarget.mDepth = 1;
//            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
//            renderTarget.mWidth = viewport.GetSize().x / cLuxEffectRenderer::BlurSize;
//            renderTarget.mHeight = viewport.GetSize().y / cLuxEffectRenderer::BlurSize;
//            renderTarget.mSampleCount = SAMPLE_COUNT_1;
//            renderTarget.mSampleQuality = 0;
//            renderTarget.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
//            renderTarget.pName = "blur Target";
//            renderTarget.mFormat = getRecommendedSwapchainFormat(false, false);
//            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
//            return true;
//        });
//        postEffect->m_blurTarget[1].Load([&](RenderTarget** target) {
//            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
//            RenderTargetDesc renderTarget = {};
//            renderTarget.mArraySize = 1;
//            renderTarget.mClearValue = optimizedColorClearBlack;
//            renderTarget.mDepth = 1;
//            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
//            renderTarget.mWidth = viewport.GetSize().x / cLuxEffectRenderer::BlurSize;
//            renderTarget.mHeight = viewport.GetSize().y / cLuxEffectRenderer::BlurSize;
//            renderTarget.mSampleCount = SAMPLE_COUNT_1;
//            renderTarget.mSampleQuality = 0;
//            renderTarget.mStartState = RESOURCE_STATE_RENDER_TARGET;
//            renderTarget.pName = "blur Target";
//            renderTarget.mFormat = getRecommendedSwapchainFormat(false, false);
//            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
//            return true;
//        });
//
//        return postEffect;
//    }, [&](cViewport& viewport, LuxPostEffectData& target) {
//        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
//        return viewport.GetSize() == target.m_size;
//    }));

    auto* forgeRenderer = Interface<ForgeRenderer>::Get();

    {
        SamplerDesc samplerDesc = {};
        addSampler(forgeRenderer->Rend(), &samplerDesc,  &m_diffuseSampler);
    }
    {

		addUniformGPURingBuffer(forgeRenderer->Rend(),
                          sizeof(LuxEffectObjectUniform) * MaxObjectUniform, &m_uniformBuffer, true);

                m_outlineStencilShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
                    ShaderLoadDesc loadDesc = {};
                    loadDesc.mStages[0].pFileName = "dds_outline_stencil.vert";
                    loadDesc.mStages[1].pFileName = "dds_outline_stencil.frag";
                    addShader(forgeRenderer->Rend(), &loadDesc, shader);
                    return true;
                });
                m_outlineShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
                    ShaderLoadDesc loadDesc = {};
                    loadDesc.mStages[0].pFileName = "dds_outline.vert";
                    loadDesc.mStages[1].pFileName = "dds_outline.frag";
                    addShader(forgeRenderer->Rend(), &loadDesc, shader);
                    return true;
                });
                m_objectFlashShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
                    ShaderLoadDesc loadDesc = {};
                    loadDesc.mStages[0].pFileName = "dds_flash.vert";
                    loadDesc.mStages[1].pFileName = "dds_flash.frag";
                    addShader(forgeRenderer->Rend(), &loadDesc, shader);
                    return true;
                });
                m_blurShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
                    ShaderLoadDesc loadDesc = {};
                    loadDesc.mStages[0].pFileName = "fullscreen.vert";
                    loadDesc.mStages[1].pFileName = "blur_posteffect.frag";
                    addShader(forgeRenderer->Rend(), &loadDesc, shader);
                    return true;
                });
                m_outlineCopyShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
                    ShaderLoadDesc loadDesc = {};
                    loadDesc.mStages[0].pFileName = "fullscreen.vert";
                    loadDesc.mStages[1].pFileName = "dds_outline_copy.frag";
                    addShader(forgeRenderer->Rend(), &loadDesc, shader);
                    return true;
                });
                m_enemyGlowShader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
                    ShaderLoadDesc loadDesc = {};
                    loadDesc.mStages[0].pFileName = "dds_enemy_glow.vert";
                    loadDesc.mStages[1].pFileName = "dds_enemy_glow.frag";
                    addShader(forgeRenderer->Rend(), &loadDesc, shader);
                    return true;
                });

                {
                    std::array shaders = { m_outlineCopyShader.m_handle, m_blurShader.m_handle };
                    const char* pStaticSamplers[] = { "inputSampler" };
                    RootSignatureDesc rootSignatureDesc = {};
                    rootSignatureDesc.ppStaticSamplers = &m_diffuseSampler;
                    rootSignatureDesc.mStaticSamplerCount = 1;
                    rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
                    rootSignatureDesc.ppShaders = shaders.data();
                    rootSignatureDesc.mShaderCount = shaders.size();
                    addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, &m_postProcessingRootSignature);

                    DescriptorSetDesc setDesc = { m_postProcessingRootSignature,
                                                  DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                                  cLuxEffectRenderer::DescriptorSetPostEffectSize };
                    for (auto& descSet : m_outlinePostprocessingDescriptorSet) {
                        addDescriptorSet(forgeRenderer->Rend(), &setDesc, &descSet);
                    }
        }
        {
            std::array shaders = { m_outlineStencilShader.m_handle, m_outlineShader.m_handle, m_enemyGlowShader.m_handle, m_objectFlashShader.m_handle};
            const char* pStaticSamplers[] = { "diffuseSampler" };
            RootSignatureDesc rootSignatureDesc = {};
            rootSignatureDesc.ppStaticSamplers = &m_diffuseSampler;
            rootSignatureDesc.mStaticSamplerCount = 1;
            rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, &m_perObjectRootSignature);

            DescriptorSetDesc setDesc = { m_perObjectRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, cLuxEffectRenderer::DescriptorOutlineSize  };
            for(auto& descSet: m_perObjectDescriptorSet) {
                addDescriptorSet(forgeRenderer->Rend(), &setDesc, &descSet);
            }
        }
        {
            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

            std::array colorFormats = {
                TinyImageFormat_R8G8B8A8_UNORM
            };

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = CMP_LEQUAL;
            {
                VertexLayout vertexLayout = {};
            #ifndef USE_THE_FORGE_LEGACY
                vertexLayout.mBindingCount = 2;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
            #endif
                vertexLayout.mAttribCount = 2;
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

                RasterizerStateDesc outlineRasterizerDesc = {};
                outlineRasterizerDesc.mCullMode = CULL_MODE_FRONT;
                outlineRasterizerDesc.mFrontFace = FrontFace::FRONT_FACE_CW;

                DepthStateDesc depthStateOutlineDesc = {};
                depthStateOutlineDesc.mDepthTest = true;
                depthStateOutlineDesc.mDepthWrite = false;
                depthStateOutlineDesc.mDepthFunc = CMP_LEQUAL;
                depthStateOutlineDesc.mStencilTest =  false;
                depthStateOutlineDesc.mStencilFrontFunc = CMP_NOTEQUAL;
                depthStateOutlineDesc.mStencilFrontFail = STENCIL_OP_KEEP;
                depthStateOutlineDesc.mStencilFrontPass = STENCIL_OP_KEEP;
                depthStateOutlineDesc.mStencilFrontPass = STENCIL_OP_KEEP;
                depthStateOutlineDesc.mDepthFrontFail = STENCIL_OP_KEEP;
                depthStateOutlineDesc.mStencilWriteMask = 0xff;
                depthStateOutlineDesc.mStencilReadMask = 0xff;

                BlendStateDesc blendStateDesc{};
            #ifdef USE_THE_FORGE_LEGACY
                blendStateDesc.mMasks[0] = ALL;
            #else
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_ALL;
            #endif

                blendStateDesc.mSrcFactors[0] = BC_ONE;
                blendStateDesc.mDstFactors[0] = BC_ZERO;
                blendStateDesc.mBlendModes[0] = BM_ADD;
                blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
                blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
                blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = 1;
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.pDepthState = &depthStateOutlineDesc;
                pipelineSettings.pShaderProgram = m_outlineShader.m_handle;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pRasterizerState = &outlineRasterizerDesc;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_outlinePipeline);
            }
            {
                VertexLayout vertexLayout = {};
            #ifndef USE_THE_FORGE_LEGACY
                vertexLayout.mBindingCount = 2;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
            #endif
                vertexLayout.mAttribCount = 2;
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

                RasterizerStateDesc stencilRasterizeDesc = {};
                stencilRasterizeDesc.mCullMode = CULL_MODE_NONE;
                stencilRasterizeDesc.mFrontFace = FrontFace::FRONT_FACE_CW;

                DepthStateDesc depthStateOutlineDesc = {};
                depthStateOutlineDesc.mDepthTest = true;
                depthStateOutlineDesc.mDepthWrite = false;
                depthStateOutlineDesc.mStencilTest = true;
                depthStateOutlineDesc.mDepthFunc = CMP_LEQUAL;
                depthStateOutlineDesc.mStencilTest =  true;
                depthStateOutlineDesc.mStencilBackFunc = CMP_ALWAYS;
                depthStateOutlineDesc.mStencilBackFunc = CMP_ALWAYS;
                depthStateOutlineDesc.mDepthBackFail = STENCIL_OP_REPLACE;
                depthStateOutlineDesc.mStencilWriteMask = 0xff;
                depthStateOutlineDesc.mStencilReadMask = 0xff;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = 1;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.pDepthState = &depthStateOutlineDesc;
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.pRasterizerState = &stencilRasterizeDesc;
                pipelineSettings.pShaderProgram = m_outlineStencilShader.m_handle;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_outlineStencilPipeline);
            }
            {
                VertexLayout vertexLayout = {};
            #ifndef USE_THE_FORGE_LEGACY
                vertexLayout.mBindingCount = 3;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
            #endif
                vertexLayout.mAttribCount = 3;
                vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
                vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[0].mBinding = 0;
                vertexLayout.mAttribs[0].mLocation = 0;
                vertexLayout.mAttribs[0].mOffset = 0;
                vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
                vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[1].mBinding = 1;
                vertexLayout.mAttribs[1].mLocation = 1;
                vertexLayout.mAttribs[2].mOffset = 0;
                vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
                vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
                vertexLayout.mAttribs[2].mBinding = 2;
                vertexLayout.mAttribs[2].mLocation = 2;
                vertexLayout.mAttribs[2].mOffset = 0;

                BlendStateDesc blendStateDesc{};
                blendStateDesc.mSrcFactors[0] = BC_ONE;
                blendStateDesc.mDstFactors[0] = BC_ONE;
                blendStateDesc.mBlendModes[0] = BM_ADD;
                blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
                blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
                blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
                #ifdef USE_THE_FORGE_LEGACY
                    blendStateDesc.mMasks[0] = RED | GREEN | BLUE;
                #else
                    blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN |  ColorMask::COLOR_MASK_BLUE;
                #endif
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mRenderTargetCount = 1;
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.pDepthState = &depthStateDesc;
                pipelineSettings.pShaderProgram = m_objectFlashShader.m_handle;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_flashPipeline);
            }
            {
                VertexLayout vertexLayout = {};
            #ifndef USE_THE_FORGE_LEGACY
                vertexLayout.mBindingCount = 3;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
            #endif
                vertexLayout.mAttribCount = 3;
                vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
                vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[0].mBinding = 0;
                vertexLayout.mAttribs[0].mLocation = 0;
                vertexLayout.mAttribs[0].mOffset = 0;
                vertexLayout.mAttribs[1].mSemantic =  SEMANTIC_NORMAL;
                vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
                vertexLayout.mAttribs[1].mBinding = 1;
                vertexLayout.mAttribs[1].mLocation = 1;
                vertexLayout.mAttribs[2].mOffset = 0;
                vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
                vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
                vertexLayout.mAttribs[2].mBinding = 2;
                vertexLayout.mAttribs[2].mLocation = 2;
                vertexLayout.mAttribs[2].mOffset = 0;

                BlendStateDesc blendStateDesc{};
                blendStateDesc.mSrcFactors[0] = BC_ONE;
                blendStateDesc.mDstFactors[0] = BC_ONE;
                blendStateDesc.mBlendModes[0] = BM_ADD;
                blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
                blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
                blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
                #ifdef USE_THE_FORGE_LEGACY
                    blendStateDesc.mMasks[0] = RED | GREEN | BLUE;
                #else
                    blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN |  ColorMask::COLOR_MASK_BLUE;
                #endif

                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mRenderTargetCount = 1;
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.pDepthState = &depthStateDesc;
                pipelineSettings.pShaderProgram = m_enemyGlowShader.m_handle;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_enemyGlowPipeline);
            }

            m_blurPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                DepthStateDesc depthStateDisabledDesc = {};
                depthStateDisabledDesc.mDepthWrite = false;
                depthStateDisabledDesc.mDepthTest = false;

                RasterizerStateDesc rasterStateNoneDesc = {};
                rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
                graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                graphicsPipelineDesc.pShaderProgram = m_blurShader.m_handle;
                graphicsPipelineDesc.pRootSignature = m_postProcessingRootSignature;
                graphicsPipelineDesc.mRenderTargetCount = 1;
                graphicsPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
                graphicsPipelineDesc.pVertexLayout = NULL;
                graphicsPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
                graphicsPipelineDesc.pDepthState = &depthStateDisabledDesc;
                graphicsPipelineDesc.pBlendState = NULL;
                graphicsPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
                graphicsPipelineDesc.mSampleQuality = 0;
                graphicsPipelineDesc.pColorFormats = colorFormats.data();
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                return true;
            });

            m_combineOutlineAddPipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipeline) {
                BlendStateDesc blendStateDesc{};
                blendStateDesc.mSrcFactors[0] = BC_ONE;
                blendStateDesc.mDstFactors[0] = BC_ONE;
                blendStateDesc.mBlendModes[0] = BM_ADD;
                blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
                blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
                blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
                #ifdef USE_THE_FORGE_LEGACY
                    blendStateDesc.mMasks[0] = RED | GREEN |  BLUE;
                #else
                    blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN |  ColorMask::COLOR_MASK_BLUE;
                #endif
                blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
                blendStateDesc.mIndependentBlend = false;

                DepthStateDesc depthStateDisabledDesc = {};
                depthStateDisabledDesc.mDepthWrite = false;
                depthStateDisabledDesc.mDepthTest = false;

                RasterizerStateDesc rasterStateNoneDesc = {};
                rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
                graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                graphicsPipelineDesc.pShaderProgram = m_outlineCopyShader.m_handle;
                graphicsPipelineDesc.pRootSignature = m_postProcessingRootSignature;
                graphicsPipelineDesc.mRenderTargetCount = 1;
                graphicsPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
                graphicsPipelineDesc.pVertexLayout = NULL;
                graphicsPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
                graphicsPipelineDesc.pDepthState = &depthStateDisabledDesc;
                graphicsPipelineDesc.pBlendState = &blendStateDesc;
                graphicsPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
                graphicsPipelineDesc.mSampleQuality = 0;
                graphicsPipelineDesc.pColorFormats = colorFormats.data();
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipeline);
                return true;
            });
        }
    }

    ///////////////////////////
    // Reset variables
    mFlashOscill.SetUp(0, 1, 0, 1, 1);

    ///////////////////////////
    // Reset variables
    Reset();
}

//-----------------------------------------------------------------------

cLuxEffectRenderer::~cLuxEffectRenderer()
{
}

void cLuxEffectRenderer::Reset()
{
    mvOutlineObjects.clear();
    ClearRenderLists();
}

void cLuxEffectRenderer::ClearRenderLists()
{
    mvFlashObjects.clear();
    mvEnemyGlowObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::Update(float afTimeStep)
{
    mFlashOscill.Update(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderSolid(cViewport::PostSolidDrawPacket&  input)
{
}

void cLuxEffectRenderer::RenderTrans(cViewport::PostTranslucenceDrawPacket&  input)
{

    auto postEffectData = m_boundPostEffectData.resolve(*input.m_viewport);
    if(!postEffectData || postEffectData->m_size != input.m_viewport->GetSize()) {
        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
        auto newPostEffect = std::make_unique<LuxPostEffectData>();
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();

        newPostEffect->m_size = input.m_viewport->GetSize();
        newPostEffect->m_outlineBuffer.Load(forgeRenderer->Rend(),[&](RenderTarget** target) {
            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
            RenderTargetDesc renderTarget = {};
            renderTarget.mArraySize = 1;
            renderTarget.mClearValue = optimizedColorClearBlack;
            renderTarget.mDepth = 1;
            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTarget.mWidth = input.m_viewport->GetSize().x;
            renderTarget.mHeight = input.m_viewport->GetSize().y;
            renderTarget.mSampleCount = SAMPLE_COUNT_1;
            renderTarget.mSampleQuality = 0;
            renderTarget.mStartState = RESOURCE_STATE_RENDER_TARGET;
            renderTarget.pName = "postEffect Outline";
            renderTarget.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
            return true;
        });
        ASSERT(newPostEffect->m_blurTarget.size() == 2);
        newPostEffect->m_blurTarget[0].Load(forgeRenderer->Rend(),[&](RenderTarget** target) {
            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
            RenderTargetDesc renderTarget = {};
            renderTarget.mArraySize = 1;
            renderTarget.mClearValue = optimizedColorClearBlack;
            renderTarget.mDepth = 1;
            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTarget.mWidth = input.m_viewport->GetSize().x / cLuxEffectRenderer::BlurSize;
            renderTarget.mHeight = input.m_viewport->GetSize().y / cLuxEffectRenderer::BlurSize;
            renderTarget.mSampleCount = SAMPLE_COUNT_1;
            renderTarget.mSampleQuality = 0;
            renderTarget.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            renderTarget.pName = "blur Target";
            renderTarget.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
            return true;
        });
        newPostEffect->m_blurTarget[1].Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
            RenderTargetDesc renderTarget = {};
            renderTarget.mArraySize = 1;
            renderTarget.mClearValue = optimizedColorClearBlack;
            renderTarget.mDepth = 1;
            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTarget.mWidth = input.m_viewport->GetSize().x / cLuxEffectRenderer::BlurSize;
            renderTarget.mHeight = input.m_viewport->GetSize().y / cLuxEffectRenderer::BlurSize;
            renderTarget.mSampleCount = SAMPLE_COUNT_1;
            renderTarget.mSampleQuality = 0;
            renderTarget.mStartState = RESOURCE_STATE_RENDER_TARGET;
            renderTarget.pName = "blur Target";
            renderTarget.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
            return true;
        });
        postEffectData = m_boundPostEffectData.update(*input.m_viewport, std::move(newPostEffect));
    }

    cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
    cRendererDeferred* pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));
    auto sharedData = pRendererDeferred->GetSharedData(*input.m_viewport);
    auto& frame = input.m_frame;
    cRendererDeferred::GBuffer& currentGBuffer = sharedData->m_gBuffer[input.m_frame->m_frameIndex];
    if(!currentGBuffer.m_depthBuffer.IsValid() ||
        !currentGBuffer.m_outputBuffer.IsValid()) {
        return;
    }

    const cMatrixf mainFrustumView = input.m_frustum->GetViewMatrix();
    const cMatrixf mainFrustumProj = input.m_frustum->GetProjectionMatrix();
    const cMatrixf mainFrustumViewProj = cMath::MatrixMul(mainFrustumProj, mainFrustumView);
    float fGlobalAlpha = (0.5f + mFlashOscill.val * 0.5f);
    uint32_t objectIndex = 0;

    cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Flashing");
    {
        LoadActionsDesc loadActions = {};
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
        loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
        loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;
        loadActions.mClearDepth = {.depth = 1.0f, .stencil = 0};

        cmdBindRenderTargets(input.m_frame->m_cmd, 1, &currentGBuffer.m_outputBuffer.m_handle, currentGBuffer.m_depthBuffer.m_handle, &loadActions, NULL, NULL, -1, -1);
        cmdSetViewport(input.m_frame->m_cmd, 0.0f, 0.0f, (float)sharedData->m_size.x, (float)sharedData->m_size.y, 0.0f, 1.0f);
        cmdSetScissor(input.m_frame->m_cmd, 0, 0, sharedData->m_size.x, sharedData->m_size.y);
    }
    // render Flashing objects ---------------------------------------------------------------------------------------------------
    cmdBindPipeline(input.m_frame->m_cmd, m_flashPipeline);
    for(auto& flashObject: mvFlashObjects) {
        auto* pObject = flashObject.mpObject;
        std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Normal, eVertexBufferElement_Texture0 };
        DrawPacket packet = pObject->ResolveDrawPacket(*frame, targets);
        if (!pObject->CollidesWithFrustum(input.m_frustum) && packet.m_type == DrawPacket::Unknown)
        {
            continue;
        }


        uint32_t requestSize = round_up(sizeof(LuxEffectObjectUniform::FlashUniform), 256);
        #ifdef USE_THE_FORGE_LEGACY
            GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(m_uniformBuffer, requestSize);
        #else
            GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, requestSize);
        #endif

        cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;
        const cMatrixf modelViewMat = cMath::MatrixMul(mainFrustumView, worldMatrix);
        const cMatrixf mvp = cMath::MatrixMul(mainFrustumViewProj, worldMatrix);
        LuxEffectObjectUniform::FlashUniform uniform{};

        uniform.m_mvp = cMath::ToForgeMatrix4(mvp);
        uniform.m_colorMul = float4(flashObject.mfAlpha * fGlobalAlpha);
        uniform.m_normalMat = cMath::ToForgeMat3(cMath::MatrixInverse(modelViewMat).GetTranspose());

        std::array<DescriptorData, 2> params = {};
        DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, requestSize };
        params[0].pName = "uniformBlock";
        params[0].ppBuffers = &uniformBlockOffset.pBuffer;
        params[0].pRanges = &range;
        params[1].pName = "diffuseMap";
        params[1].ppTextures = &pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetTexture().m_handle;
        updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

        BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
        beginUpdateResource(&updateDesc);
        (*reinterpret_cast<LuxEffectObjectUniform::FlashUniform*>(updateDesc.pMappedData)) = uniform;
        endUpdateResource(&updateDesc);

        cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
        DrawPacket::cmdBindBuffers(frame->m_cmd, frame->m_resourcePool, &packet);
        for(size_t i = 0; i < 2; i++) {
            cmdDrawIndexed(input.m_frame->m_cmd, packet.numberOfIndecies(), 0, 0);
        }
    }
    cmdEndDebugMarker(input.m_frame->m_cmd);
    // Render Enemy Glow ---------------------------------------------------------------------------------------------------

    cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Enemy Glow");
    cmdBindPipeline(frame->m_cmd, m_enemyGlowPipeline);
    for(auto& enemyGlow: mvEnemyGlowObjects) {
        auto* pObject = enemyGlow.mpObject;

        auto* vertexBuffer = pObject->GetVertexBuffer();
        if (!pObject->CollidesWithFrustum(input.m_frustum)) {
            continue;
        }
        std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Normal, eVertexBufferElement_Texture0 };
        DrawPacket packet = pObject->ResolveDrawPacket(*frame, targets);

        uint32_t requestSize = round_up(sizeof(LuxEffectObjectUniform::FlashUniform), 256);
        #ifdef USE_THE_FORGE_LEGACY
            GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(m_uniformBuffer, requestSize);
        #else
            GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, requestSize);
        #endif
        cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;
        const cMatrixf modelViewMat = cMath::MatrixMul(mainFrustumView, worldMatrix);
        const cMatrixf mvp = cMath::MatrixMul(mainFrustumViewProj, worldMatrix);
        LuxEffectObjectUniform::FlashUniform uniform{};

        uniform.m_mvp = cMath::ToForgeMatrix4(mvp);
        uniform.m_colorMul = float4(enemyGlow.mfAlpha * fGlobalAlpha);
        uniform.m_normalMat = cMath::ToForgeMat3(cMath::MatrixInverse(modelViewMat).GetTranspose());

        std::array<DescriptorData, 2> params = {};
        DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, requestSize };
        params[0].pName = "uniformBlock";
        params[0].ppBuffers = &uniformBlockOffset.pBuffer;
        params[0].pRanges = &range;
        params[1].pName = "diffuseMap";
        params[1].ppTextures = &pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetTexture().m_handle;
        updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

        BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
        beginUpdateResource(&updateDesc);
        (*reinterpret_cast<LuxEffectObjectUniform::FlashUniform*>(updateDesc.pMappedData)) = uniform;
        endUpdateResource(&updateDesc);

        cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
        DrawPacket::cmdBindBuffers(frame->m_cmd, frame->m_resourcePool, &packet);
        cmdDrawIndexed(input.m_frame->m_cmd, packet.numberOfIndecies(), 0, 0);
    }
    cmdEndDebugMarker(input.m_frame->m_cmd);

    if (input.m_renderSettings->mbIsReflection == false) {
        folly::small_vector<iRenderable*, 15> filteredObjects;
        for(auto& outline: mvOutlineObjects) {
            if (outline->CollidesWithFrustum(input.m_frustum)) {
                filteredObjects.push_back(outline);
            }
        }

        LoadActionsDesc loadActions = {};
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
        loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
        loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
        loadActions.mClearColorValues[0] = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
        cmdBindRenderTargets(frame->m_cmd, 1, &postEffectData->m_outlineBuffer.m_handle, currentGBuffer.m_depthBuffer.m_handle, &loadActions, NULL, NULL, -1, -1);

        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline Stencil");
        cmdBindPipeline(frame->m_cmd, m_outlineStencilPipeline);
        cmdSetStencilReferenceValue(frame->m_cmd, 0xff);
        for(auto& pObject: filteredObjects) {
            auto* vertexBuffer = pObject->GetVertexBuffer();
            auto* pMaterial = pObject->GetMaterial();

            if (!pObject->CollidesWithFrustum(input.m_frustum)) {
                continue;
            }
            auto* imageAlpha = pMaterial->GetImage(eMaterialTexture_Alpha);
            std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Texture0 };
            DrawPacket packet = pObject->ResolveDrawPacket(*frame, targets);

            uint32_t requestSize = round_up(sizeof(LuxEffectObjectUniform::OutlineUniform), 256);
            #ifdef USE_THE_FORGE_LEGACY
                GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(m_uniformBuffer, requestSize);
            #else
                GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, requestSize);
            #endif

            cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;
            const cMatrixf modelViewMat = cMath::MatrixMul(mainFrustumView, worldMatrix);

            LuxEffectObjectUniform::OutlineUniform  uniform{};
            uniform.m_mvp = cMath::ToForgeMatrix4(cMath::MatrixMul(mainFrustumViewProj, worldMatrix));
            uniform.m_feature = hpl::material::UniformMaterialBlock::CreateMaterailConfigFlags(*pMaterial);

            BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
            beginUpdateResource(&updateDesc);
            (*reinterpret_cast<LuxEffectObjectUniform::OutlineUniform*>(updateDesc.pMappedData)) = uniform;
            endUpdateResource(&updateDesc);

            folly::small_vector<DescriptorData, 2 > params = {};
            DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, requestSize };
            {
                auto& param = params.emplace_back(DescriptorData{});
                param.pName = "uniformBlock";
                param.ppBuffers = &uniformBlockOffset.pBuffer;
                param.pRanges = &range;
            }
            if(imageAlpha) {
                auto& param = params.emplace_back(DescriptorData{});
                param.pName = "diffuseMap";
                param.ppTextures = &imageAlpha->GetTexture().m_handle;
            }

            updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

            cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
            DrawPacket::cmdBindBuffers(input.m_frame->m_cmd, frame->m_resourcePool, &packet);
            cmdDrawIndexed(input.m_frame->m_cmd, packet.numberOfIndecies(), 0, 0);
        }
        cmdEndDebugMarker(input.m_frame->m_cmd);

        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline");
        cmdBindPipeline(frame->m_cmd, m_outlinePipeline);
        cmdSetStencilReferenceValue(frame->m_cmd, 0xff);
        for(auto& pObject: filteredObjects) {
            auto* vertexBuffer = pObject->GetVertexBuffer();
            auto* pMaterial = pObject->GetMaterial();

            std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Texture0 };
            DrawPacket drawPacket = pObject->ResolveDrawPacket(*frame, targets);
            if (!pObject->CollidesWithFrustum(input.m_frustum) || drawPacket.m_type == DrawPacket::Unknown) {
                continue;
            }

            uint32_t requestSize = round_up(sizeof(LuxEffectObjectUniform::OutlineUniform), 256);
            #ifdef USE_THE_FORGE_LEGACY
                GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(m_uniformBuffer, requestSize);
            #else
                GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, requestSize);
            #endif

            cBoundingVolume* pBV = pObject->GetBoundingVolume();
            cVector3f vLocalSize = pBV->GetLocalMax() - pBV->GetLocalMin();
            cVector3f vScale = (cVector3f(1.0f) / vLocalSize) * 0.02f+ cVector3f(1.0f);

            cMatrixf mtxScale = cMath::MatrixMul(cMath::MatrixScale(vScale), cMath::MatrixTranslate(pBV->GetLocalCenter() * -1));
            mtxScale.SetTranslation(mtxScale.GetTranslation() + pBV->GetLocalCenter());
            cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;

            LuxEffectObjectUniform::OutlineUniform uniform{};
            uniform.m_mvp = cMath::ToForgeMatrix4(cMath::MatrixMul(mainFrustumViewProj, cMath::MatrixMul(worldMatrix, mtxScale)));
            uniform.m_feature =hpl::material::UniformMaterialBlock::CreateMaterailConfigFlags(*pMaterial);

            BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
            beginUpdateResource(&updateDesc);
            (*reinterpret_cast<LuxEffectObjectUniform::OutlineUniform*>(updateDesc.pMappedData)) = uniform;
            endUpdateResource(&updateDesc);

            std::array<DescriptorData, 2> params = {};
            DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, requestSize };
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &uniformBlockOffset.pBuffer;
            params[0].pRanges = &range;
            params[1].pName = "diffuseMap";
            params[1].ppTextures = &pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetTexture().m_handle;
            updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

            cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
            DrawPacket::cmdBindBuffers(input.m_frame->m_cmd, input.m_frame->m_resourcePool, &drawPacket);
            cmdDrawIndexed(input.m_frame->m_cmd, drawPacket.numberOfIndecies(), 0, 0);
        }
        cmdEndDebugMarker(input.m_frame->m_cmd);

        uint32_t postProcessingIndex = 0;
        uint32_t blurPostEffectConstIndex = getDescriptorIndexFromName(m_postProcessingRootSignature, "rootConstant");
        auto requestBlur = [&](Texture** input) {
            ASSERT(input && "Invalid input texture");
            {
                cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                std::array rtBarriers = {
                    RenderTargetBarrier{ postEffectData->m_blurTarget[0].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                    RenderTargetBarrier{ postEffectData->m_blurTarget[1].m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE},
                };
                cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
            }
            {
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
                loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
                auto& blurTarget = postEffectData->m_blurTarget[0].m_handle;
                cmdBindRenderTargets(frame->m_cmd, 1, &blurTarget , NULL, &loadActions, NULL, NULL, -1, -1);
                cmdSetViewport(
                    frame->m_cmd, 0.0f, 0.0f, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight), 0.0f, 1.0f);
                cmdSetScissor(frame->m_cmd, 0, 0, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight));

                std::array<DescriptorData, 1> params = {};
                params[0].pName = "sourceInput";
                params[0].ppTextures = input;
                cmdBindPipeline(frame->m_cmd, m_blurPipeline.m_handle);
                updateDescriptorSet(
                    frame->m_renderer->Rend(), postProcessingIndex , m_outlinePostprocessingDescriptorSet[frame->m_frameIndex], params.size(), params.data());

                cmdBindDescriptorSet(frame->m_cmd, postProcessingIndex++, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex]);
                float2 blurScale = float2(1.0f, 0.0f);
                cmdBindPushConstants(frame->m_cmd, m_postProcessingRootSignature, blurPostEffectConstIndex, &blurScale);
                cmdDraw(frame->m_cmd, 3, 0);

            }
            {
                cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                std::array rtBarriers = {
                    RenderTargetBarrier{
                        postEffectData->m_blurTarget[0].m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                    RenderTargetBarrier{
                        postEffectData->m_blurTarget[1].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                };
                cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
            }
            {
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
                loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

                auto& blurTarget = postEffectData->m_blurTarget[1].m_handle;
                cmdBindRenderTargets(frame->m_cmd, 1, &blurTarget, NULL, &loadActions, NULL, NULL, -1, -1);
                cmdSetViewport(
                    frame->m_cmd, 0.0f, 0.0f, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight), 0.0f, 1.0f);
                cmdSetScissor(frame->m_cmd, 0, 0, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight));

                std::array<DescriptorData, 1> params = {};
                params[0].pName = "sourceInput";
                params[0].ppTextures = &postEffectData->m_blurTarget[0].m_handle->pTexture;
                cmdBindPipeline(frame->m_cmd, m_blurPipeline.m_handle);

                updateDescriptorSet(
                    frame->m_renderer->Rend(), postProcessingIndex, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex], params.size(), params.data());

                float2 blurScale = float2(0.0f, 1.0f);
                cmdBindPushConstants(frame->m_cmd, m_postProcessingRootSignature, blurPostEffectConstIndex, &blurScale);
                cmdBindDescriptorSet(frame->m_cmd, postProcessingIndex++, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex]);
                cmdDraw(frame->m_cmd, 3, 0);
            }
        };
        {
            cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{postEffectData->m_outlineBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
            };
            cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline Blur");
        requestBlur(&postEffectData->m_outlineBuffer.m_handle->pTexture);
        for (size_t i = 0; i < 2; ++i) {
            requestBlur(&postEffectData->m_blurTarget[1].m_handle->pTexture);
        }
        cmdEndDebugMarker(input.m_frame->m_cmd);

        {
            cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{postEffectData->m_blurTarget[1].m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
            };
            cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        {
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
            cmdBindRenderTargets(input.m_frame->m_cmd, 1, &currentGBuffer.m_outputBuffer.m_handle, NULL, &loadActions, NULL, NULL, -1, -1);
        }
        {
            std::array<DescriptorData, 1> params = {};
            params[0].pName = "sourceInput";
            params[0].ppTextures = &postEffectData->m_blurTarget[1].m_handle->pTexture;
            updateDescriptorSet(
                frame->m_renderer->Rend(), postProcessingIndex, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex], params.size(), params.data());
        }

        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline Combine");
        cmdSetViewport(frame->m_cmd, 0.0f, 0.0f, static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mWidth), static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mHeight), 0.0f, 1.0f);
        cmdSetScissor(frame->m_cmd, 0, 0, static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mWidth), static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mHeight));
        cmdBindPipeline(frame->m_cmd, m_combineOutlineAddPipeline.m_handle);

        cmdBindDescriptorSet(frame->m_cmd, postProcessingIndex++, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex]);
        cmdDraw(frame->m_cmd, 3, 0);
        cmdEndDebugMarker(input.m_frame->m_cmd);
        {
            cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{postEffectData->m_outlineBuffer.m_handle,  RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET  },
                RenderTargetBarrier{postEffectData->m_blurTarget[1].m_handle,  RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
            };
            cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
    }
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddOutlineObject(iRenderable* apObject)
{
    mvOutlineObjects.push_back(apObject);
}

void cLuxEffectRenderer::ClearOutlineObjects()
{
    mvOutlineObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddFlashObject(iRenderable* apObject, float afAlpha)
{
    mvFlashObjects.push_back(cGlowObject(apObject, afAlpha));
}

void cLuxEffectRenderer::AddEnemyGlow(iRenderable* apObject, float afAlpha)
{
    mvEnemyGlowObjects.push_back(cGlowObject(apObject, afAlpha));
}

