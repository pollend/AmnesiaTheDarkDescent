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

#include "graphics/Image.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/RenderTarget.h"
#include "graphics/ShaderUtil.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"

#include "system/PreprocessParser.h"
#include <memory>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PROGRAM VARS
	//////////////////////////////////////////////////////////////////////////

	#define kVar_avRgbToIntensity	0

	#define kVar_afBlurSize			2


	cPostEffectType_Bloom::cPostEffectType_Bloom(cGraphics *apGraphics, cResources *apResources) : iPostEffectType("Bloom",apGraphics,apResources)
	{

		m_blurProgram = hpl::loadProgram("vs_post_effect", "fs_posteffect_bloom_blur");
		m_bloomProgram = hpl::loadProgram("vs_post_effect", "fs_posteffect_bloom_add");
		
		m_u_blurMap = bgfx::createUniform("s_blurMap", bgfx::UniformType::Enum::Sampler);
		m_u_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Enum::Sampler);

		m_u_rgbToIntensity = bgfx::createUniform("u_rgbToIntensity", bgfx::UniformType::Enum::Vec4);
		m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Enum::Vec4);
	}

	//-----------------------------------------------------------------------

	cPostEffectType_Bloom::~cPostEffectType_Bloom()
	{

	}

	//-----------------------------------------------------------------------

	iPostEffect * cPostEffectType_Bloom::CreatePostEffect(iPostEffectParams *apParams)
	{
		cPostEffect_Bloom *pEffect = hplNew(cPostEffect_Bloom, (mpGraphics,mpResources,this));
		cPostEffectParams_Bloom *pBloomParams = static_cast<cPostEffectParams_Bloom*>(apParams);

		return pEffect;
	}

	cPostEffect_Bloom::cPostEffect_Bloom(cGraphics *apGraphics, cResources *apResources, iPostEffectType *apType) : iPostEffect(apGraphics,apResources,apType)
	{
		cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();
		
		auto colorDesc = [&] {
			auto desc = ImageDescriptor::CreateTexture2D(vSize.x/4, vSize.y/4, false, bgfx::TextureFormat::Enum::RGBA32F);
			desc.m_configuration.m_rt = RTType::RT_Write;
			return desc;
		}();

		m_blurTarget[0] = RenderTarget(std::shared_ptr<Image>(new Image(colorDesc)));
		m_blurTarget[1] = RenderTarget(std::shared_ptr<Image>(new Image(colorDesc)));

		mpBloomType = static_cast<cPostEffectType_Bloom*>(mpType);
	}

	cPostEffect_Bloom::~cPostEffect_Bloom()
	{

	}

	void cPostEffect_Bloom::OnSetParams()
	{

	}

	void cPostEffect_Bloom::RenderEffect(GraphicsContext& context, Image& input, RenderTarget& target) {
		cVector2l vRenderTargetSize = mpCurrentComposite->GetRenderTargetSize();
		
		struct u_blur_params {
			float u_useHorizontal;
			float u_blurSize;
			float u_padding[2];
		};
		// cVector2f vRenderTargetSizeFloat((float)vRenderTargetSize.x, (float)vRenderTargetSize.y);
		auto requestBlur = [&](Image& input) {
			auto& imageDesc = m_blurTarget[1].GetImage(0).lock()->GetDescriptor();
			u_blur_params blurParams = {0};
			{
				blurParams.u_useHorizontal = 0;
				blurParams.u_blurSize = mParams.mfBlurSize;
				
				GraphicsContext::LayoutStream layoutStream;
				context.ScreenSpaceQuad(layoutStream, vRenderTargetSize.x, vRenderTargetSize.y);

				GraphicsContext::ShaderProgram shaderProgram;
				shaderProgram.m_configuration.m_write = Write::RGBA;
				shaderProgram.m_handle = mpBloomType->m_blurProgram;
				shaderProgram.m_textures.push_back({ mpBloomType->m_u_diffuseMap, input.GetHandle(), 0 });
				shaderProgram.m_uniforms.push_back({ mpBloomType->m_u_param, &blurParams, 1  });

				
				bgfx::ViewId view = context.StartPass("Blur Pass 1");
				GraphicsContext::DrawRequest request { m_blurTarget[0], layoutStream, shaderProgram };
				request.m_width = imageDesc.m_width;
				request.m_height = imageDesc.m_height;

				context.Submit(view, request);
			}

			{
				blurParams.u_useHorizontal = 1.0;
				blurParams.u_blurSize = mParams.mfBlurSize;
				
				GraphicsContext::LayoutStream layoutStream;
				context.ScreenSpaceQuad(layoutStream, vRenderTargetSize.x, vRenderTargetSize.y);

				GraphicsContext::ShaderProgram shaderProgram;
				shaderProgram.m_configuration.m_write = Write::RGBA;
				shaderProgram.m_handle = mpBloomType->m_blurProgram;
				shaderProgram.m_textures.push_back({ mpBloomType->m_u_diffuseMap, m_blurTarget[0].GetImage(0).lock()->GetHandle(), 0 });
				shaderProgram.m_uniforms.push_back({ mpBloomType->m_u_param, &blurParams, 1  });

				bgfx::ViewId view = context.StartPass("Blur Pass 2");
				GraphicsContext::DrawRequest request { m_blurTarget[1], layoutStream, shaderProgram };
				request.m_width = imageDesc.m_width;
				request.m_height = imageDesc.m_height;
				
				context.Submit(view, request);
			}
		};

		requestBlur(input);
		for(int i=1; i<mParams.mlBlurIterations;++i)
		{
			requestBlur(*m_blurTarget[1].GetImage(0).lock());
		}

		{
			GraphicsContext::LayoutStream layoutStream;
			context.ScreenSpaceQuad(layoutStream, vRenderTargetSize.x, vRenderTargetSize.y);

			GraphicsContext::ShaderProgram shaderProgram;
			shaderProgram.m_configuration.m_write = Write::RGBA;
			shaderProgram.m_handle = mpBloomType->m_bloomProgram;

			float rgbToIntensity[4] = {mParams.mvRgbToIntensity.x, mParams.mvRgbToIntensity.y, mParams.mvRgbToIntensity.z, 0.0f};
			shaderProgram.m_textures.push_back({ mpBloomType->m_u_diffuseMap, input.GetHandle(), 0 });
			shaderProgram.m_textures.push_back({ mpBloomType->m_u_blurMap, m_blurTarget[1].GetImage(0).lock()->GetHandle(), 0 });
			shaderProgram.m_uniforms.push_back({ mpBloomType->m_u_rgbToIntensity, &rgbToIntensity, 1  });
			bgfx::ViewId view = context.StartPass("Bloom Pass");
			GraphicsContext::DrawRequest request { target, layoutStream, shaderProgram };
			request.m_width = vRenderTargetSize.x;
			request.m_height = vRenderTargetSize.y;
			context.Submit(view, request);

		}
		

	}

	iTexture* cPostEffect_Bloom::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
	{
		
	}

}
