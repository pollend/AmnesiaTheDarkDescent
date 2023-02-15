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

#include "graphics/PostEffect_RadialBlur.h"

#include "bgfx/bgfx.h"
#include "graphics/Graphics.h"

#include "graphics/FrameBuffer.h"
#include "graphics/GraphicsContext.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/ShaderUtil.h"
#include "graphics/Texture.h"

#include "math/MathTypes.h"
#include "system/PreprocessParser.h"

namespace hpl
{

    cPostEffectType_RadialBlur::cPostEffectType_RadialBlur(cGraphics* apGraphics, cResources* apResources)
        : iPostEffectType("RadialBlur", apGraphics, apResources)
    {
        m_program = hpl::loadProgram("vs_post_effect", "fs_posteffect_radial_blur_frag");
        m_u_uniform = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
        m_s_diffuseMap = bgfx::createUniform("diffuseMap", bgfx::UniformType::Sampler);
    }

    cPostEffectType_RadialBlur::~cPostEffectType_RadialBlur()
    {
    }

    iPostEffect* cPostEffectType_RadialBlur::CreatePostEffect(iPostEffectParams* apParams)
    {
        cPostEffect_RadialBlur* pEffect = hplNew(cPostEffect_RadialBlur, (mpGraphics, mpResources, this));
        cPostEffectParams_RadialBlur* pRadialBlurParams = static_cast<cPostEffectParams_RadialBlur*>(apParams);
        return pEffect;
    }

    cPostEffect_RadialBlur::cPostEffect_RadialBlur(cGraphics* apGraphics, cResources* apResources, iPostEffectType* apType)
        : iPostEffect(apGraphics, apResources, apType)
    {
        cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();

        mpRadialBlurType = static_cast<cPostEffectType_RadialBlur*>(mpType);
    }

    cPostEffect_RadialBlur::~cPostEffect_RadialBlur()
    {
    }

    void cPostEffect_RadialBlur::Reset()
    {
    }

    void cPostEffect_RadialBlur::OnSetParams()
    {
    }

    void cPostEffect_RadialBlur::RenderEffect(cPostEffectComposite& compositor, GraphicsContext& context, Image& input, RenderTarget& target)
    {
        cVector2l vRenderTargetSize = compositor.GetRenderTargetSize();
        cVector2f vRenderTargetSizeFloat((float)vRenderTargetSize.x, (float)vRenderTargetSize.y);

        GraphicsContext::LayoutStream layoutStream;
        cMatrixf projMtx;
        context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);
        
        GraphicsContext::ViewConfiguration viewConfiguration {target};
        viewConfiguration.m_viewRect = {0, 0, vRenderTargetSize.x, vRenderTargetSize.y};
        viewConfiguration.m_projection = projMtx;
		bgfx::ViewId view = context.StartPass("Radial Blur", viewConfiguration);

        BX_ASSERT(mpRadialBlurType, "radial blur type is null");
        BX_ASSERT(bgfx::isValid(mpRadialBlurType->m_program), "radial blur program is invalid");

        cVector2f blurStartDist = vRenderTargetSizeFloat * 0.5f;

        struct
        {
            float afSize;
            float afBlurSartDist;
            float avHalfScreenSize[2];
        } u_params = { mParams.mfSize * vRenderTargetSizeFloat.x, mParams.mfBlurStartDist, { blurStartDist.x, blurStartDist.y } };

        GraphicsContext::ShaderProgram shaderProgram;
        // shaderProgram.m_projection = projMtx;
        shaderProgram.m_handle = mpRadialBlurType->m_program;
        shaderProgram.m_uniforms.push_back({ mpRadialBlurType->m_u_uniform, &u_params });
        shaderProgram.m_textures.push_back({ mpRadialBlurType->m_s_diffuseMap, input.GetHandle() });
        shaderProgram.m_configuration.m_write = Write::RGBA | Write::Depth;

        GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        context.Submit(view, request);
    }

} // namespace hpl
