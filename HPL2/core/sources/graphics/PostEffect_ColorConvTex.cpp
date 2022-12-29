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

#include "graphics/PostEffect_ColorConvTex.h"

#include "graphics/Enum.h"
#include "graphics/Graphics.h"
#include "graphics/GraphicsContext.h"
#include "graphics/ShaderUtil.h"
#include "resources/Resources.h"

#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/Texture.h"

#include "resources/TextureManager.h"

#include "system/PreprocessParser.h"
#include "system/String.h"

#include "math/Math.h"

namespace hpl
{

    cPostEffectType_ColorConvTex::cPostEffectType_ColorConvTex(cGraphics* apGraphics, cResources* apResources)
        : iPostEffectType("ColorConvTex", apGraphics, apResources)
    {
        m_colorConv = hpl::loadProgram("vs_post_effect", "fs_posteffect_color_conv");
        m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
        m_u_colorConvTex = bgfx::createUniform("s_convMap", bgfx::UniformType::Sampler);
        m_u_diffuseTex = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
    }

    cPostEffectType_ColorConvTex::~cPostEffectType_ColorConvTex()
    {
    }


    iPostEffect* cPostEffectType_ColorConvTex::CreatePostEffect(iPostEffectParams* apParams)
    {
        cPostEffect_ColorConvTex* pEffect = hplNew(cPostEffect_ColorConvTex, (mpGraphics, mpResources, this));
        cPostEffectParams_ColorConvTex* pBloomParams = static_cast<cPostEffectParams_ColorConvTex*>(apParams);

        return pEffect;
    }

    cPostEffect_ColorConvTex::cPostEffect_ColorConvTex(cGraphics* apGraphics, cResources* apResources, iPostEffectType* apType)
        : iPostEffect(apGraphics, apResources, apType)
    {
        mpSpecificType = static_cast<cPostEffectType_ColorConvTex*>(mpType);

        mpColorConvTex = NULL;
    }

    cPostEffect_ColorConvTex::~cPostEffect_ColorConvTex()
    {
    }


    void cPostEffect_ColorConvTex::OnSetParams()
    {
        if (mParams.msTextureFile == "")
            return;

        if (mpColorConvTex)
        {
            mpResources->GetTextureManager()->Destroy(mpColorConvTex);
        }
        mpColorConvTex = mpResources->GetTextureManager()->Create1DImage(mParams.msTextureFile, false);
    }

    void cPostEffect_ColorConvTex::RenderEffect(GraphicsContext& context, Image& input, RenderTarget& target)
    {
		BX_ASSERT(mpColorConvTex, "ColorConvTex is null");
	
        auto view = context.StartPass("Color Conv effect");
        cVector2l vRenderTargetSize = mpCurrentComposite->GetRenderTargetSize();

        GraphicsContext::LayoutStream layoutStream;
        context.ScreenSpaceQuad(layoutStream, vRenderTargetSize.x, vRenderTargetSize.y);
        GraphicsContext::ShaderProgram shaderProgram;
        shaderProgram.m_handle = mpSpecificType->m_colorConv;
        struct
        {
            float u_alphaFade;
            float pad;
            float pad1;
            float pad2;
        } uniform = { cMath::Min(1.0f, cMath::Max(mParams.mfFadeAlpha, 0.0f)), 0, 0, 0 };
        shaderProgram.m_uniforms.push_back({ mpSpecificType->m_u_param, &uniform, sizeof(uniform) });
        shaderProgram.m_textures.push_back({ mpSpecificType->m_u_colorConvTex, mpColorConvTex->GetImage().GetHandle(), 0 });
        shaderProgram.m_textures.push_back({ mpSpecificType->m_u_diffuseTex, input.GetHandle(), 1 });

        shaderProgram.m_configuration.m_write = Write::RGBA;
        shaderProgram.m_configuration.m_depthTest = DepthTest::None;

        GraphicsContext::DrawRequest request{ target, layoutStream, shaderProgram };
        request.m_width = vRenderTargetSize.x;
        request.m_height = vRenderTargetSize.y;
        context.Submit(view, request);
    }

} // namespace hpl
