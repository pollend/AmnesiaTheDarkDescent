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
#include <absl/container/inlined_vector.h>
#include <graphics/Enum.h>
#include <graphics/GraphicsContext.h>
#include <graphics/RenderTarget.h>
#include <math/MathTypes.h>
#include <memory>
#include <vector>

#include "bgfx/bgfx.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/ShaderUtil.h"
#include <bx/debug.h>

#include "impl/LegacyVertexBuffer.h"
#include "tinyimageformat_base.h"
#include <folly/small_vector.h>
#include "FixPreprocessor.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/common.hpp"
#include "FixPreprocessor.h"

cLuxEffectRenderer::cLuxEffectRenderer()
    : iLuxUpdateable("LuxEffectRenderer")
{

     m_boundPostEffectData = std::move(UniqueViewportData<LuxPostEffectData>([](cViewport& viewport) {
        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
        //cRendererDeferred* pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));
        //auto& sharedData = pRendererDeferred->GetSharedData(viewport);
        auto postEffect = std::make_unique<LuxPostEffectData>();
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();

        postEffect->m_size = viewport.GetSize();
        postEffect->m_outlineBuffer.Load([&](RenderTarget** target) {
            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
            RenderTargetDesc renderTarget = {};
            renderTarget.mArraySize = 1;
            renderTarget.mClearValue = optimizedColorClearBlack;
            renderTarget.mDepth = 1;
            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTarget.mWidth = viewport.GetSize().x;
            renderTarget.mHeight = viewport.GetSize().y;
            renderTarget.mSampleCount = SAMPLE_COUNT_1;
            renderTarget.mSampleQuality = 0;
            renderTarget.mStartState = RESOURCE_STATE_RENDER_TARGET;
            renderTarget.pName = "postEffect Outline";
            renderTarget.mFormat = getRecommendedSwapchainFormat(false, false);
            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
            return true;
        });
        ASSERT(postEffect->m_blurTarget.size() == 2);
        postEffect->m_blurTarget[0].Load([&](RenderTarget** target) {
            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
            RenderTargetDesc renderTarget = {};
            renderTarget.mArraySize = 1;
            renderTarget.mClearValue = optimizedColorClearBlack;
            renderTarget.mDepth = 1;
            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTarget.mWidth = viewport.GetSize().x / cLuxEffectRenderer::BlurSize;
            renderTarget.mHeight = viewport.GetSize().y / cLuxEffectRenderer::BlurSize;
            renderTarget.mSampleCount = SAMPLE_COUNT_1;
            renderTarget.mSampleQuality = 0;
            renderTarget.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            renderTarget.pName = "blur Target";
            renderTarget.mFormat = getRecommendedSwapchainFormat(false, false);
            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
            return true;
        });
        postEffect->m_blurTarget[1].Load([&](RenderTarget** target) {
            ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
            RenderTargetDesc renderTarget = {};
            renderTarget.mArraySize = 1;
            renderTarget.mClearValue = optimizedColorClearBlack;
            renderTarget.mDepth = 1;
            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTarget.mWidth = viewport.GetSize().x / cLuxEffectRenderer::BlurSize;
            renderTarget.mHeight = viewport.GetSize().y / cLuxEffectRenderer::BlurSize;
            renderTarget.mSampleCount = SAMPLE_COUNT_1;
            renderTarget.mSampleQuality = 0;
            renderTarget.mStartState = RESOURCE_STATE_RENDER_TARGET;
            renderTarget.pName = "blur Target";
            renderTarget.mFormat = getRecommendedSwapchainFormat(false, false);
            addRenderTarget(forgeRenderer->Rend(), &renderTarget, target);
            return true;
        });

        //  auto blurImageDesc = [&]
        //  {
        //      auto desc = ImageDescriptor::CreateTexture2D(
        //          sharedData.m_size.x / cLuxEffectRenderer::BlurSize,
        //          sharedData.m_size.y / cLuxEffectRenderer::BlurSize,
        //          false,
        //          bgfx::TextureFormat::Enum::RGBA8);
        //      desc.m_configuration.m_rt = RTType::RT_Write;
        //      auto image = std::make_shared<Image>();
        //      image->Initialize(desc);
        //      return image;
        //  };

        //  postEffect->m_mainOutputBuffer = {
        //      sharedData.m_gBuffer[0].m_outputBuffer,
        //      sharedData.m_gBuffer[1].m_outputBuffer
        //  };

        //  postEffect->m_mainDepthBuffer = {
        //      sharedData.m_gBuffer[0].m_depthBuffer,
        //      sharedData.m_gBuffer[1].m_outputBuffer
        //  };

        // postEffect->m_outputImage = sharedData.m_gBuffer.m_outputImage;
        // postEffect->m_gBufferDepthStencil = sharedData.m_gBuffer.m_depthStencilImage;

      //  postEffect->m_blurTarget[0] = LegacyRenderTarget(blurImageDesc());
      //  postEffect->m_blurTarget[1] = LegacyRenderTarget(blurImageDesc());

      //  {
      //      std::array<std::shared_ptr<Image>, 2> image = {postEffect->m_outputImage, postEffect->m_gBufferDepthStencil};
      //      postEffect->m_outputTarget = LegacyRenderTarget(image);
      //  }

      //  auto outlineImageDesc = ImageDescriptor::CreateTexture2D(sharedData.m_size.x, sharedData.m_size.y, false, bgfx::TextureFormat::RGBA8);
      //  outlineImageDesc.m_configuration.m_rt = RTType::RT_Write;
      //  auto outlineImage = std::make_shared<Image>();
      //  outlineImage->Initialize(outlineImageDesc);

      //  std::array<std::shared_ptr<Image>, 2> outlineImages = { outlineImage, postEffect->m_gBufferDepthStencil };
      //  postEffect->m_outlineTarget = LegacyRenderTarget(std::span(outlineImages));

        return postEffect;
    }, [&](cViewport& viewport, LuxPostEffectData& target) {
        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
        //cRendererDeferred* pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));
        //auto& sharedData = pRendererDeferred->GetSharedData(viewport);


        // as long as the output image and depth stencil image are the same, we can use the same render target
            // else we need to create a new one
        return viewport.GetSize() == target.m_size;
             //&& sharedData.m_gBuffer.m_outputImage == target.m_outputImage
             //&& sharedData.m_gBuffer.m_depthStencilImage == target.m_gBufferDepthStencil;
    }));

    auto* forgeRenderer = Interface<ForgeRenderer>::Get();

    {
        SamplerDesc samplerDesc = {};
        addSampler(forgeRenderer->Rend(), &samplerDesc,  &m_diffuseSampler);
    }
    {

		addUniformGPURingBuffer(forgeRenderer->Rend(),
                          sizeof(LuxEffectObjectUniform) * MaxObjectUniform, &m_uniformBuffer, true);

        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "dds_outline_stencil.vert";
            loadDesc.mStages[1].pFileName = "dds_outline_stencil.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_outlineStencilShader);
        }
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "dds_outline.vert";
            loadDesc.mStages[1].pFileName = "dds_outline.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_outlineShader);
        }
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "dds_flash.vert";
            loadDesc.mStages[1].pFileName = "dds_flash.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_objectFlashShader);
        }
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "fullscreen.vert";
            loadDesc.mStages[1].pFileName = "blur_posteffect_horizontal.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_blurHorizontalShader);
        }
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "fullscreen.vert";
            loadDesc.mStages[1].pFileName = "blur_posteffect_vertical.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_blurVerticalShader);
        }
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "fullscreen.vert";
            loadDesc.mStages[1].pFileName = "dds_outline_copy.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_outlineCopyShader);
        }
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "dds_enemy_glow.vert";
            loadDesc.mStages[1].pFileName = "dds_enemy_glow.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, &m_enemyGlowShader);
        }
        {
            std::array shaders = { m_blurVerticalShader, m_blurHorizontalShader, m_outlineCopyShader};
            const char* pStaticSamplers[] = { "inputSampler" };
            RootSignatureDesc rootSignatureDesc = {};
            rootSignatureDesc.ppStaticSamplers = &m_diffuseSampler;
            rootSignatureDesc.mStaticSamplerCount = 1;
            rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, &m_postProcessingRootSignature);

            DescriptorSetDesc setDesc = { m_postProcessingRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, cLuxEffectRenderer::DescriptorSetPostEffectSize   };
            for(auto& descSet: m_outlinePostprocessingDescriptorSet) {
                addDescriptorSet(forgeRenderer->Rend(), &setDesc, &descSet);
            }
        }
        {
            std::array shaders = { m_outlineStencilShader, m_outlineShader, m_enemyGlowShader, m_objectFlashShader };
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
                getRecommendedSwapchainFormat(false, false)
            };

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = CMP_LEQUAL;
            {
                VertexLayout vertexLayout = {};
                vertexLayout.mAttribCount = 2;
                vertexLayout.mBindingCount = 2;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
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
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_ALL;
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
                pipelineSettings.pShaderProgram = m_outlineShader;
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
                vertexLayout.mAttribCount = 2;
                vertexLayout.mBindingCount = 2;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
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
                pipelineSettings.pShaderProgram = m_outlineStencilShader;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_outlineStencilPipeline);
            }
            {
                VertexLayout vertexLayout = {};
                vertexLayout.mAttribCount = 3;
                vertexLayout.mBindingCount = 3;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
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
                vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
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
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN |  ColorMask::COLOR_MASK_BLUE;
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
                pipelineSettings.pShaderProgram = m_objectFlashShader;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_flashPipeline);
            }
            {
                VertexLayout vertexLayout = {};
                vertexLayout.mAttribCount = 3;
                vertexLayout.mBindingCount = 3;
                vertexLayout.mBindings[0].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float3);
                vertexLayout.mBindings[1].mStride = sizeof(float2);
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
                vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
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
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN |  ColorMask::COLOR_MASK_BLUE;
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
                pipelineSettings.pShaderProgram = m_enemyGlowShader;
                pipelineSettings.pRootSignature = m_perObjectRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_enemyGlowPipeline);
            }

            {
                DepthStateDesc depthStateDisabledDesc = {};
                depthStateDisabledDesc.mDepthWrite = false;
                depthStateDisabledDesc.mDepthTest = false;

                RasterizerStateDesc rasterStateNoneDesc = {};
                rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
                graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                graphicsPipelineDesc.pShaderProgram = m_blurHorizontalShader;
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
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_blurHorizontalPipeline);
            }
            {
                DepthStateDesc depthStateDisabledDesc = {};
                depthStateDisabledDesc.mDepthWrite = false;
                depthStateDisabledDesc.mDepthTest = false;

                RasterizerStateDesc rasterStateNoneDesc = {};
                rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
                graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                graphicsPipelineDesc.pShaderProgram = m_blurVerticalShader;
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
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_blurVerticalPipeline);
            }
            {
                BlendStateDesc blendStateDesc{};
                blendStateDesc.mSrcFactors[0] = BC_ONE;
                blendStateDesc.mDstFactors[0] = BC_ONE;
                blendStateDesc.mBlendModes[0] = BM_ADD;
                blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
                blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
                blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
                blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN |  ColorMask::COLOR_MASK_BLUE;
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
                graphicsPipelineDesc.pShaderProgram = m_outlineCopyShader;
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
                addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_combineOutlineAddPipeline);
            }
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

    auto& postEffectData = m_boundPostEffectData.resolve(*input.m_viewport);
    cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
    cRendererDeferred* pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));
    auto& sharedData = pRendererDeferred->GetSharedData(*input.m_viewport);
    auto& frame = input.m_frame;
    auto& currentGBuffer = sharedData.m_gBuffer[input.m_frame->m_frameIndex];
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
        cmdSetViewport(input.m_frame->m_cmd, 0.0f, 0.0f, (float)sharedData.m_size.x, (float)sharedData.m_size.y, 0.0f, 1.0f);
        cmdSetScissor(input.m_frame->m_cmd, 0, 0, sharedData.m_size.x, sharedData.m_size.y);
    }
    // render Flashing objects ---------------------------------------------------------------------------------------------------
    cmdBindPipeline(input.m_frame->m_cmd, m_flashPipeline);
    for(auto& flashObject: mvFlashObjects) {
        auto* pObject = flashObject.mpObject;

        auto* vertexBuffer = pObject->GetVertexBuffer();
        if (!pObject->CollidesWithFrustum(input.m_frustum))
        {
            continue;
        }
        LegacyVertexBuffer::GeometryBinding binding{};
        std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Normal, eVertexBufferElement_Texture0 };
        static_cast<LegacyVertexBuffer*>(pObject->GetVertexBuffer())->resolveGeometryBinding(frame->m_currentFrame, targets, &binding);

        GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, sizeof(LuxEffectObjectUniform::FlashUniform));
        cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;
        const cMatrixf modelViewMat = cMath::MatrixMul(mainFrustumView, worldMatrix);
        const cMatrixf mvp = cMath::MatrixMul(mainFrustumViewProj, worldMatrix);
        LuxEffectObjectUniform::FlashUniform uniform{};

        uniform.m_mvp = cMath::ToForgeMat4(mvp.GetTranspose());
        uniform.m_colorMul = float4(flashObject.mfAlpha * fGlobalAlpha);
        uniform.m_normalMat = cMath::ToForgeMat3(cMath::MatrixInverse(modelViewMat));

        std::array<DescriptorData, 2> params = {};
        DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, sizeof(LuxEffectObjectUniform::FlashUniform) };
        params[0].pName = "uniformBlock";
        params[0].ppBuffers = &uniformBlockOffset.pBuffer;
        params[0].pRanges = &range;
        params[1].pName = "diffuseMap";
        params[1].ppTextures = &pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetTexture().m_handle;
        updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

        BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
        beginUpdateResource(&updateDesc);
        (*reinterpret_cast<LuxEffectObjectUniform::FlashUniform*>(updateDesc.pMappedData)) = uniform;
        endUpdateResource(&updateDesc, NULL);

        cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
        LegacyVertexBuffer::cmdBindGeometry(frame->m_cmd, frame->m_resourcePool, binding);
        for(size_t i = 0; i < 2; i++) {
            cmdDrawIndexed(input.m_frame->m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
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
        LegacyVertexBuffer::GeometryBinding binding{};
        std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Normal, eVertexBufferElement_Texture0 };
        static_cast<LegacyVertexBuffer*>(pObject->GetVertexBuffer())->resolveGeometryBinding(frame->m_currentFrame, targets, &binding);

        GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, sizeof(LuxEffectObjectUniform::FlashUniform));
        cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;
        const cMatrixf modelViewMat = cMath::MatrixMul(mainFrustumView, worldMatrix);
        const cMatrixf mvp = cMath::MatrixMul(mainFrustumViewProj, worldMatrix);
        LuxEffectObjectUniform::FlashUniform uniform{};

        uniform.m_mvp = cMath::ToForgeMat4(mvp.GetTranspose());
        uniform.m_colorMul = float4(enemyGlow.mfAlpha * fGlobalAlpha);
        uniform.m_normalMat = cMath::ToForgeMat3(cMath::MatrixInverse(modelViewMat));

        std::array<DescriptorData, 2> params = {};
        DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, sizeof(LuxEffectObjectUniform::FlashUniform) };
        params[0].pName = "uniformBlock";
        params[0].ppBuffers = &uniformBlockOffset.pBuffer;
        params[0].pRanges = &range;
        params[1].pName = "diffuseMap";
        params[1].ppTextures = &pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetTexture().m_handle;
        updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

        BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
        beginUpdateResource(&updateDesc);
        (*reinterpret_cast<LuxEffectObjectUniform::FlashUniform*>(updateDesc.pMappedData)) = uniform;
        endUpdateResource(&updateDesc, NULL);

        cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
        LegacyVertexBuffer::cmdBindGeometry(frame->m_cmd, frame->m_resourcePool, binding);
        cmdDrawIndexed(input.m_frame->m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
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
        cmdBindRenderTargets(frame->m_cmd, 1, &postEffectData.m_outlineBuffer.m_handle, currentGBuffer.m_depthBuffer.m_handle, &loadActions, NULL, NULL, -1, -1);

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
            LegacyVertexBuffer::GeometryBinding binding{};
            std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Texture0 };
            static_cast<LegacyVertexBuffer*>(pObject->GetVertexBuffer())->resolveGeometryBinding(frame->m_currentFrame, targets, &binding);

            GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, sizeof(LuxEffectObjectUniform::OutlineUniform));
            cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;
            const cMatrixf modelViewMat = cMath::MatrixMul(mainFrustumView, worldMatrix);

            LuxEffectObjectUniform::OutlineUniform  uniform{};
            uniform.m_mvp = cMath::ToForgeMat4(cMath::MatrixMul(mainFrustumViewProj, worldMatrix).GetTranspose());
            uniform.m_feature = pMaterial->type().m_data.m_common.m_textureConfig;

            BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
            beginUpdateResource(&updateDesc);
            (*reinterpret_cast<LuxEffectObjectUniform::OutlineUniform*>(updateDesc.pMappedData)) = uniform;
            endUpdateResource(&updateDesc, NULL);

            folly::small_vector<DescriptorData, 2, folly::small_vector_policy::NoHeap> params = {};
            DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, sizeof(LuxEffectObjectUniform::OutlineUniform ) };
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
            LegacyVertexBuffer::cmdBindGeometry(frame->m_cmd, frame->m_resourcePool, binding);
            cmdDrawIndexed(input.m_frame->m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
        }
        cmdEndDebugMarker(input.m_frame->m_cmd);

        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline");
        cmdBindPipeline(frame->m_cmd, m_outlinePipeline);
        cmdSetStencilReferenceValue(frame->m_cmd, 0xff);
        for(auto& pObject: filteredObjects) {
            auto* vertexBuffer = pObject->GetVertexBuffer();
            auto* pMaterial = pObject->GetMaterial();
            auto& materialType = pMaterial->type();
            if (!pObject->CollidesWithFrustum(input.m_frustum)) {
                continue;
            }

            LegacyVertexBuffer::GeometryBinding binding{};
            std::array targets = { eVertexBufferElement_Position, eVertexBufferElement_Texture0 };
            static_cast<LegacyVertexBuffer*>(pObject->GetVertexBuffer())->resolveGeometryBinding(frame->m_currentFrame, targets, &binding);
            GPURingBufferOffset uniformBlockOffset = getGPURingBufferOffset(&m_uniformBuffer, sizeof(LuxEffectObjectUniform::OutlineUniform));

            cBoundingVolume* pBV = pObject->GetBoundingVolume();
            cVector3f vLocalSize = pBV->GetLocalMax() - pBV->GetLocalMin();
            cVector3f vScale = (cVector3f(1.0f) / vLocalSize) * 0.02f+ cVector3f(1.0f);

            cMatrixf mtxScale = cMath::MatrixMul(cMath::MatrixScale(vScale), cMath::MatrixTranslate(pBV->GetLocalCenter() * -1));
            mtxScale.SetTranslation(mtxScale.GetTranslation() + pBV->GetLocalCenter());
            cMatrixf worldMatrix = pObject->GetModelMatrixPtr() ? *pObject->GetModelMatrixPtr() : cMatrixf::Identity;

            LuxEffectObjectUniform::OutlineUniform uniform{};
            uniform.m_mvp = cMath::ToForgeMat4(cMath::MatrixMul(mainFrustumViewProj, cMath::MatrixMul(worldMatrix, mtxScale)).GetTranspose());
            uniform.m_feature = pMaterial->type().m_data.m_common.m_textureConfig;

            BufferUpdateDesc updateDesc = { uniformBlockOffset.pBuffer, uniformBlockOffset.mOffset };
            beginUpdateResource(&updateDesc);
            (*reinterpret_cast<LuxEffectObjectUniform::OutlineUniform*>(updateDesc.pMappedData)) = uniform;
            endUpdateResource(&updateDesc, NULL);

            std::array<DescriptorData, 2> params = {};
            DescriptorDataRange range = { (uint32_t)uniformBlockOffset.mOffset, sizeof(LuxEffectObjectUniform::OutlineUniform ) };
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &uniformBlockOffset.pBuffer;
            params[0].pRanges = &range;
            params[1].pName = "diffuseMap";
            params[1].ppTextures = &pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetTexture().m_handle;
            updateDescriptorSet(frame->m_renderer->Rend(), objectIndex, m_perObjectDescriptorSet[frame->m_frameIndex], params.size(), params.data());

            cmdBindDescriptorSet(input.m_frame->m_cmd, objectIndex++, m_perObjectDescriptorSet[frame->m_frameIndex]);
            LegacyVertexBuffer::cmdBindGeometry(frame->m_cmd, frame->m_resourcePool, binding);
            cmdDrawIndexed(input.m_frame->m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
        }
        cmdEndDebugMarker(input.m_frame->m_cmd);

        uint32_t postProcessingIndex = 0;
        auto requestBlur = [&](Texture** input) {
            ASSERT(input && "Invalid input texture");
            {
                cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                std::array rtBarriers = {
                    RenderTargetBarrier{ postEffectData.m_blurTarget[0].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                    RenderTargetBarrier{ postEffectData.m_blurTarget[1].m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE},
                };
                cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
            }
            {
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
                loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
                auto& blurTarget = postEffectData.m_blurTarget[0].m_handle;
                cmdBindRenderTargets(frame->m_cmd, 1, &blurTarget , NULL, &loadActions, NULL, NULL, -1, -1);

                std::array<DescriptorData, 1> params = {};
                params[0].pName = "sourceInput";
                params[0].ppTextures = input;
                updateDescriptorSet(
                    frame->m_renderer->Rend(), postProcessingIndex , m_outlinePostprocessingDescriptorSet[frame->m_frameIndex], params.size(), params.data());

                cmdSetViewport(frame->m_cmd, 0.0f, 0.0f, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight), 0.0f, 1.0f);
                cmdSetScissor(frame->m_cmd, 0, 0, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight));
                cmdBindPipeline(frame->m_cmd, m_blurHorizontalPipeline);
                cmdBindDescriptorSet(frame->m_cmd, postProcessingIndex++, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex]);
                cmdDraw(frame->m_cmd, 3, 0);

            }
            {
                cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                std::array rtBarriers = {
                    RenderTargetBarrier{
                        postEffectData.m_blurTarget[0].m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                    RenderTargetBarrier{
                        postEffectData.m_blurTarget[1].m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                };
                cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
            }
            {
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
                loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

                auto& blurTarget = postEffectData.m_blurTarget[1].m_handle;
                cmdBindRenderTargets(frame->m_cmd, 1, &blurTarget, NULL, &loadActions, NULL, NULL, -1, -1);

                std::array<DescriptorData, 1> params = {};
                params[0].pName = "sourceInput";
                params[0].ppTextures = &postEffectData.m_blurTarget[0].m_handle->pTexture;
                updateDescriptorSet(
                    frame->m_renderer->Rend(), postProcessingIndex, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex], params.size(), params.data());

                cmdSetViewport(frame->m_cmd, 0.0f, 0.0f, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight), 0.0f, 1.0f);
                cmdSetScissor(frame->m_cmd, 0, 0, static_cast<float>(blurTarget->mWidth), static_cast<float>(blurTarget->mHeight));
                cmdBindPipeline(frame->m_cmd, m_blurVerticalPipeline);

                cmdBindDescriptorSet(frame->m_cmd, postProcessingIndex++, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex]);
                cmdDraw(frame->m_cmd, 3, 0);
            }
        };

        {
            uint32_t rootConstantIndex = getDescriptorIndexFromName(m_postProcessingRootSignature, "postEffectConstants");
            float blurSize = 1.0;
            cmdBindPushConstants(frame->m_cmd, m_postProcessingRootSignature, rootConstantIndex, &blurSize);
        }
        {
            cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{postEffectData.m_outlineBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
            };
            cmdResourceBarrier(frame->m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline Blur");
        requestBlur(&postEffectData.m_outlineBuffer.m_handle->pTexture);
        for (size_t i = 0; i < 2; ++i) {
            requestBlur(&postEffectData.m_blurTarget[1].m_handle->pTexture);
        }
        cmdEndDebugMarker(input.m_frame->m_cmd);

        {
            cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{postEffectData.m_blurTarget[1].m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
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
            params[0].ppTextures = &postEffectData.m_blurTarget[1].m_handle->pTexture;
            updateDescriptorSet(
                frame->m_renderer->Rend(), postProcessingIndex, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex], params.size(), params.data());
        }

        cmdBeginDebugMarker(input.m_frame->m_cmd, 0, 0, 0, "DDS Outline Combine");
        cmdSetViewport(frame->m_cmd, 0.0f, 0.0f, static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mWidth), static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mHeight), 0.0f, 1.0f);
        cmdSetScissor(frame->m_cmd, 0, 0, static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mWidth), static_cast<float>(currentGBuffer.m_outputBuffer.m_handle->mHeight));
        cmdBindPipeline(frame->m_cmd, m_combineOutlineAddPipeline);

        cmdBindDescriptorSet(frame->m_cmd, postProcessingIndex++, m_outlinePostprocessingDescriptorSet[frame->m_frameIndex]);
        cmdDraw(frame->m_cmd, 3, 0);
        cmdEndDebugMarker(input.m_frame->m_cmd);
        {
            cmdBindRenderTargets(frame->m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{postEffectData.m_outlineBuffer.m_handle,  RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET  },
                RenderTargetBarrier{postEffectData.m_blurTarget[1].m_handle,  RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
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

