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
#include "math/MathTypes.h"
#include "resources/Resources.h"

#include "graphics/FrameBuffer.h"
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
        if (mParams.msTextureFile == "") {
            return;
        }

        if (mpColorConvTex) {
            mpResources->GetTextureManager()->Destroy(mpColorConvTex);
        }
        cTextureManager::ImageOptions options;
		options.m_UWrap = WrapMode::Clamp;
		options.m_VWrap = WrapMode::Clamp;
        mpColorConvTex = mpResources->GetTextureManager()->Create1DImage(mParams.msTextureFile, false, eTextureUsage_Normal, 0, options);
    }

    void cPostEffect_ColorConvTex::RenderEffect(cPostEffectComposite& compositor, cViewport& viewport, GraphicsContext& context, Image& input, LegacyRenderTarget& target)
    {
		BX_ASSERT(mpColorConvTex, "ColorConvTex is null");
	    cVector2l vRenderTargetSize = viewport.GetSize();
        GraphicsContext::LayoutStream layoutStream;
        cMatrixf projMtx;
        context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);
        
        GraphicsContext::ViewConfiguration viewConfig {target};
        viewConfig.m_viewRect ={0, 0, vRenderTargetSize.x, vRenderTargetSize.y};
        viewConfig.m_projection = projMtx;
        auto view = context.StartPass("Color Conv effect", viewConfig);
        
        GraphicsContext::ShaderProgram shaderProgram;
        // shaderProgram.m_projection = projMtx;
        shaderProgram.m_handle = mpSpecificType->m_colorConv;

        struct
        {
            float u_alphaFade;
            float pad;
            float pad1;
            float pad2;
        } uniform = { 
            cMath::Clamp(mParams.mfFadeAlpha, 0.0f, 1.0f), 
            0, 
            0, 
            0 };
        shaderProgram.m_uniforms.push_back({ mpSpecificType->m_u_param, &uniform, 1 });
        
        shaderProgram.m_textures.push_back({ mpSpecificType->m_u_colorConvTex, mpColorConvTex->GetHandle(), 0 });
        shaderProgram.m_textures.push_back({ mpSpecificType->m_u_diffuseTex, input.GetHandle(), 1 });

        shaderProgram.m_configuration.m_write = Write::RGBA;
        shaderProgram.m_configuration.m_depthTest = DepthTest::None;

        GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        context.Submit(view, request);
    }

} // namespace hpl
