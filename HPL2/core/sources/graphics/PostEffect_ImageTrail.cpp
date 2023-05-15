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

#include "graphics/PostEffect_ImageTrail.h"

#include "bgfx/bgfx.h"
#include "graphics/Enum.h"
#include "graphics/Graphics.h"

#include "graphics/FrameBuffer.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Image.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/RenderTarget.h"
#include "graphics/ShaderUtil.h"
#include "graphics/Texture.h"

#include "math/MathTypes.h"
#include "system/PreprocessParser.h"
#include <memory>

namespace hpl {
    cPostEffectType_ImageTrail::cPostEffectType_ImageTrail(cGraphics* apGraphics, cResources* apResources)
        : iPostEffectType("ImageTrail", apGraphics, apResources) {
        // m_program = hpl::loadProgram("vs_post_effect", "fs_posteffect_image_trail_frag");
        // m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);
        // m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
    }

    cPostEffectType_ImageTrail::~cPostEffectType_ImageTrail() {
        if (bgfx::isValid(m_program)) {
            bgfx::destroy(m_program);
        }
        if (bgfx::isValid(m_u_param)) {
            bgfx::destroy(m_u_param);
        }
        if (bgfx::isValid(m_s_diffuseMap)) {
            bgfx::destroy(m_s_diffuseMap);
        }
    }

    iPostEffect* cPostEffectType_ImageTrail::CreatePostEffect(iPostEffectParams* apParams) {
        cPostEffect_ImageTrail* pEffect = hplNew(cPostEffect_ImageTrail, (mpGraphics, mpResources, this));
        cPostEffectParams_ImageTrail* pImageTrailParams = static_cast<cPostEffectParams_ImageTrail*>(apParams);

        return pEffect;
    }

    cPostEffect_ImageTrail::cPostEffect_ImageTrail(cGraphics* apGraphics, cResources* apResources, iPostEffectType* apType)
        : iPostEffect(apGraphics, apResources, apType) {
        m_boundImageTrailData = UniqueViewportData<ImageTrailData>([&](cViewport& viewport) {
            auto desc = ImageDescriptor::CreateTexture2D(viewport.GetSize().x, viewport.GetSize().y , false, bgfx::TextureFormat::Enum::RGBA8);
            desc.m_configuration.m_rt = RTType::RT_Write;
            auto image = std::make_shared<Image>();
            image->Initialize(desc);
                
            auto trailData = std::make_unique<ImageTrailData>();
            trailData->m_accumulationBuffer = image;
            trailData->m_size = viewport.GetSize();

            mbClearFrameBuffer = true;

            return trailData;
        }, [&](cViewport& viewport, ImageTrailData& data) {
            return viewport.GetSize() == data.m_size && 
                data.m_accumulationBuffer.IsValid();
        });
        mpImageTrailType = static_cast<cPostEffectType_ImageTrail*>(mpType);
    }

    cPostEffect_ImageTrail::~cPostEffect_ImageTrail() {
    }

    void cPostEffect_ImageTrail::Reset() {
        mbClearFrameBuffer = true;
    }

    void cPostEffect_ImageTrail::OnSetParams() {
    }

    void cPostEffect_ImageTrail::OnSetActive(bool abX) {
        if (abX == false) {
            Reset();
        }
    }
    void cPostEffect_ImageTrail::RenderEffect(
        cPostEffectComposite& compositor, cViewport& viewport, GraphicsContext& context, Image& input, LegacyRenderTarget& target) {
        cVector2l vRenderTargetSize = viewport.GetSize();
        auto& imageTrailData = m_boundImageTrailData.resolve(viewport);


        GraphicsContext::LayoutStream layoutStream;
        cMatrixf projMtx;
        context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);

        GraphicsContext::ViewConfiguration viewConfig{ imageTrailData.m_accumulationBuffer };
        viewConfig.m_viewRect = { 0, 0, vRenderTargetSize.x, vRenderTargetSize.y };
        viewConfig.m_projection = projMtx;
        auto view = context.StartPass("Image Trail", viewConfig);

        GraphicsContext::ShaderProgram shaderProgram;
        shaderProgram.m_handle = mpImageTrailType->m_program;
        // shaderProgram.m_projection = projMtx;

        struct {
            float u_alpha;
            float u_padding[3];
        } u_params = { 0 };
        if (mbClearFrameBuffer) {
            u_params.u_alpha = 1.0f;
            mbClearFrameBuffer = false;
        } else {
            // Get the amount of blur depending frame time.
            //*30 is just so that good amount values are still between 0 - 1
            float fFrameTime = compositor.GetCurrentFrameTime();
            float fPow = (1.0f / fFrameTime) * mParams.mfAmount; // The higher this is, the more blur!
            float fAmount = exp(-fPow * 0.015f);
            u_params.u_alpha = fAmount;
        }

        shaderProgram.m_configuration.m_rgbBlendFunc =
            CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
        shaderProgram.m_configuration.m_alphaBlendFunc =
            CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);

        shaderProgram.m_textures.push_back({ mpImageTrailType->m_s_diffuseMap, input.GetHandle(), 0 });
        shaderProgram.m_uniforms.push_back({ mpImageTrailType->m_u_param, &u_params, 1 });

        shaderProgram.m_configuration.m_depthTest = DepthTest::None;
        shaderProgram.m_configuration.m_write = Write::RGBA;
        GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        context.Submit(view, request);

        cRect2l rect = cRect2l(0, 0, vRenderTargetSize.x, vRenderTargetSize.y);
        context.CopyTextureToFrameBuffer(*imageTrailData.m_accumulationBuffer.GetImage(), rect, target);
    }

} // namespace hpl
