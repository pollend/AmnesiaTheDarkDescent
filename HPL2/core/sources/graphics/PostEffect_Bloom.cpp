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

#include "graphics/PostEffect_Bloom.h"

#include "bgfx/bgfx.h"
#include "graphics/Graphics.h"

#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/Image.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/RenderTarget.h"
#include "graphics/ShaderUtil.h"
#include "graphics/Texture.h"

#include "math/MathTypes.h"
#include "system/PreprocessParser.h"
#include <memory>

namespace hpl
{

    cPostEffectType_Bloom::cPostEffectType_Bloom(cGraphics* apGraphics, cResources* apResources)
        : iPostEffectType("Bloom", apGraphics, apResources)
    {
        m_blurProgram = hpl::loadProgram("vs_post_effect", "fs_posteffect_blur");
        m_bloomProgram = hpl::loadProgram("vs_post_effect", "fs_posteffect_bloom_add");

        m_u_blurMap = bgfx::createUniform("s_blurMap", bgfx::UniformType::Sampler);
        m_u_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);

        m_u_rgbToIntensity = bgfx::createUniform("u_rgbToIntensity", bgfx::UniformType::Vec4);
        m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
    }

    cPostEffectType_Bloom::~cPostEffectType_Bloom()
    {
    }

    iPostEffect* cPostEffectType_Bloom::CreatePostEffect(iPostEffectParams* apParams)
    {
        cPostEffect_Bloom* pEffect = hplNew(cPostEffect_Bloom, (mpGraphics, mpResources, this));
        cPostEffectParams_Bloom* pBloomParams = static_cast<cPostEffectParams_Bloom*>(apParams);

        return pEffect;
    }

    cPostEffect_Bloom::cPostEffect_Bloom(cGraphics* apGraphics, cResources* apResources, iPostEffectType* apType)
        : iPostEffect(apGraphics, apResources, apType)
    {
        cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();
        OnViewportChanged(vSize);
        
        mpBloomType = static_cast<cPostEffectType_Bloom*>(mpType);
    }

    void cPostEffect_Bloom::OnViewportChanged(const cVector2l& avSize) {
        auto ColorImage = [&]
        {
            auto desc = ImageDescriptor::CreateTexture2D(avSize.x / 4.0f, avSize.y/ 4.0f , false, bgfx::TextureFormat::Enum::RGBA8);
            desc.m_configuration.m_rt = RTType::RT_Write;
            auto image = std::make_shared<Image>();
            image->Initialize(desc);
            return image;
        };

        m_blurTarget[0] = RenderTarget(ColorImage());
        m_blurTarget[1] = RenderTarget(ColorImage());

    }


    cPostEffect_Bloom::~cPostEffect_Bloom()
    {
    }

    void cPostEffect_Bloom::OnSetParams()
    {
    }

    void cPostEffect_Bloom::RenderEffect(cPostEffectComposite& compositor, GraphicsContext& context, Image& input, RenderTarget& target)
    {
        cVector2l vRenderTargetSize = compositor.GetRenderTargetSize();

        auto requestBlur = [&](Image& input){
            struct {
                float u_useHorizontal;
                float u_blurSize;
                float texelSize[2];
            } blurParams = {0};
            
            auto image = m_blurTarget[1].GetImage(0);
            {
		        GraphicsContext::LayoutStream layoutStream;
                cMatrixf projMtx;
                context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);
                
                GraphicsContext::ViewConfiguration viewConfig {m_blurTarget[0]};
                viewConfig.m_viewRect ={0, 0, image->GetWidth(), image->GetHeight()};
                viewConfig.m_projection = projMtx;
                bgfx::ViewId view = context.StartPass("Blur Pass 1", viewConfig);
                blurParams.u_useHorizontal = 0;
                blurParams.u_blurSize = mParams.mfBlurSize;
                blurParams.texelSize[0] = image->GetWidth();
                blurParams.texelSize[1] = image->GetHeight();

                GraphicsContext::ShaderProgram shaderProgram;
                shaderProgram.m_configuration.m_write = Write::RGBA;
                shaderProgram.m_handle = mpBloomType->m_blurProgram;
                // shaderProgram.m_projection = projMtx;
                
                shaderProgram.m_textures.push_back({ mpBloomType->m_u_diffuseMap, input.GetHandle(), 1 });
               
                shaderProgram.m_uniforms.push_back({ mpBloomType->m_u_param, &blurParams, 1 });
                
                GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
                context.Submit(view, request);
            }

            {

		        cMatrixf projMtx;
                GraphicsContext::LayoutStream layoutStream;
                context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);
                GraphicsContext::ViewConfiguration viewConfig {m_blurTarget[1]};
                viewConfig.m_viewRect ={0, 0, image->GetWidth(), image->GetHeight()};
                viewConfig.m_projection = projMtx;
                bgfx::ViewId view = context.StartPass("Blur Pass 2", viewConfig);

                blurParams.u_useHorizontal = 1.0;
                blurParams.u_blurSize = mParams.mfBlurSize;
                blurParams.texelSize[0] = image->GetWidth();
                blurParams.texelSize[1] = image->GetHeight();

                GraphicsContext::ShaderProgram shaderProgram;
                shaderProgram.m_configuration.m_write = Write::RGBA;
                shaderProgram.m_handle = mpBloomType->m_blurProgram;
                // shaderProgram.m_projection = projMtx;
                
                shaderProgram.m_textures.push_back({ mpBloomType->m_u_diffuseMap, m_blurTarget[0].GetImage()->GetHandle(), 1 });
                shaderProgram.m_uniforms.push_back({ mpBloomType->m_u_param, &blurParams, 1 });
                
                GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
                context.Submit(view, request);
            }
        };

        requestBlur(input);
        for (int i = 1; i < mParams.mlBlurIterations; ++i)
        {
            requestBlur(*m_blurTarget[1].GetImage());
        }

        {
            GraphicsContext::LayoutStream layoutStream;
            cMatrixf projMtx;
            context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);
            GraphicsContext::ViewConfiguration viewConfig {target};
            viewConfig.m_viewRect ={0, 0, vRenderTargetSize.x, vRenderTargetSize.y};
            viewConfig.m_projection = projMtx;

            GraphicsContext::ShaderProgram shaderProgram;
            shaderProgram.m_configuration.m_write = Write::RGBA;
            shaderProgram.m_handle = mpBloomType->m_bloomProgram;
            // shaderProgram.m_projection = projMtx;

            float rgbToIntensity[4] = { mParams.mvRgbToIntensity.x, mParams.mvRgbToIntensity.y, mParams.mvRgbToIntensity.z, 0.0f };
            shaderProgram.m_textures.push_back({ mpBloomType->m_u_diffuseMap, input.GetHandle(), 0 });
            shaderProgram.m_textures.push_back({ mpBloomType->m_u_blurMap, m_blurTarget[1].GetImage()->GetHandle(), 1 });
            shaderProgram.m_uniforms.push_back({ mpBloomType->m_u_rgbToIntensity, &rgbToIntensity, 1 });
            
            bgfx::ViewId view = context.StartPass("Bloom Pass", viewConfig);
            GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
            context.Submit(view, request);
        }
    }
} // namespace hpl
