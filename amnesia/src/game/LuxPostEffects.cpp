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

#include "LuxPostEffects.h"

#include "LuxMapHandler.h"
#include "bgfx/bgfx.h"
#include "graphics/ShaderUtil.h"


cLuxPostEffect_Insanity::cLuxPostEffect_Insanity(cGraphics* apGraphics, cResources* apResources)
    : iLuxPostEffect(apGraphics, apResources) {
    m_program = hpl::loadProgram("vs_post_effect", "fs_dds_posteffect_insanity");
    m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
    m_s_ampMap0 = bgfx::createUniform("s_ampMap0", bgfx::UniformType::Sampler);
    m_s_ampMap1 = bgfx::createUniform("s_ampMap1", bgfx::UniformType::Sampler);
    m_s_zoomMap = bgfx::createUniform("s_zoomMap", bgfx::UniformType::Sampler);
    m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);

    for (size_t i = 0; i < m_ampMaps.size(); i++) {
        m_ampMaps[i] = mpResources->GetTextureManager()->Create2DImage("posteffect_insanity_ampmap" + cString::ToString((int)i), false);
    }
    m_zoomImage = mpResources->GetTextureManager()->Create2DImage("posteffect_insanity_zoom.jpg", false);
}


cLuxPostEffect_Insanity::~cLuxPostEffect_Insanity() {
    if (bgfx::isValid(m_program)) {
        bgfx::destroy(m_program);
    }
    if (bgfx::isValid(m_s_diffuseMap)) {
        bgfx::destroy(m_s_diffuseMap);
    }
    if (bgfx::isValid(m_s_ampMap0)) {
        bgfx::destroy(m_s_ampMap0);
    }
    if (bgfx::isValid(m_s_ampMap1)) {
        bgfx::destroy(m_s_ampMap1);
    }
    if (bgfx::isValid(m_s_zoomMap)) {
        bgfx::destroy(m_s_zoomMap);
    }
    if (bgfx::isValid(m_u_param)) {
        bgfx::destroy(m_u_param);
    }
}


void cLuxPostEffect_Insanity::Update(float afTimeStep) {
    mfT += afTimeStep * mfWaveSpeed;

    mfAnimCount += afTimeStep * 0.15f;

    float fMaxAnim = static_cast<float>(m_ampMaps.size());
    if (mfAnimCount >= fMaxAnim) {
        mfAnimCount = mfAnimCount - fMaxAnim;
    }
}

//-----------------------------------------------------------------------

// iTexture* cLuxPostEffect_Insanity::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
// {
// 	/////////////////////////
// 	// Init render states
// 	mpCurrentComposite->SetFlatProjection();
// 	mpCurrentComposite->SetBlendMode(eMaterialBlendMode_None);
// 	mpCurrentComposite->SetChannelMode(eMaterialChannelMode_RGBA);

// 	/////////////////////////
// 	// Render the to final buffer
// 	// This function sets to frame buffer if post effect is last!
// 	SetFinalFrameBuffer(apFinalTempBuffer);

// 	mpCurrentComposite->SetTexture(0, apInputTexture);

// 	//Log("AnimCount: %f - %d %d - %f\n", mfAnimCount, lAmp0, lAmp1, fAmpT);

// 	mpCurrentComposite->SetTexture(1, mvAmpMaps[lAmp0]);
// 	mpCurrentComposite->SetTexture(2, mvAmpMaps[lAmp1]);
// 	mpCurrentComposite->SetTexture(3, mpZoomMap);

// 	mpCurrentComposite->SetProgram(mpProgram);
// 	if(mpProgram)
// 	{
// 		mpProgram->SetFloat(kVar_afAlpha, 1.0f);
// 		mpProgram->SetFloat(kVar_afT, mfT);
// 		mpProgram->SetVec2f(kVar_avScreenSize, mpLowLevelGraphics->GetScreenSizeFloat());
// 		mpProgram->SetFloat(kVar_afAmpT, fAmpT);
// 		mpProgram->SetFloat(kVar_afWaveAlpha, mfWaveAlpha);
// 		mpProgram->SetFloat(kVar_afZoomAlpha, mfZoomAlpha);
// 	}

// 	DrawQuad(0,1,apInputTexture, true);

// 	mpCurrentComposite->SetTextureRange(NULL, 1);

// 	return apFinalTempBuffer->GetColorBuffer(0)->ToTexture();
// }

cLuxPostEffectHandler::cLuxPostEffectHandler()
    : iLuxUpdateable("LuxPostEffectHandler") {
    cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
    cResources* pResources = gpBase->mpEngine->GetResources();

    mpInsanity = hplNew(cLuxPostEffect_Insanity, (pGraphics, pResources));
    AddEffect(mpInsanity, 25);
    mpInsanity->SetActive(true);
}

void cLuxPostEffect_Insanity::RenderEffect(cPostEffectComposite& compositor, GraphicsContext& context, Image& input, RenderTarget& target) {
    cVector2l vRenderTargetSize = compositor.GetRenderTargetSize();
    
    GraphicsContext::LayoutStream layoutStream;
    cMatrixf projMtx;
    context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);
    GraphicsContext::ViewConfiguration viewConfig {target};
    viewConfig.m_projection = projMtx;
    viewConfig.m_viewRect = {0, 0, vRenderTargetSize.x, vRenderTargetSize.y};

    bgfx::ViewId view = context.StartPass("Bloom Pass", viewConfig);

    struct {
        float alpha;
        float fT;
        float ampT;
        float waveAlpha;

        float zoomAlpha;
        float pad[3];
    } param = { 0 };

    int lAmp0 = static_cast<int>(mfAnimCount);
    int lAmp1 = static_cast<int>(mfAnimCount + 1);
    if (lAmp1 >= static_cast<int>(m_ampMaps.size())) {
        lAmp1 = 0;
    }
    float fAmpT = cMath::GetFraction(mfAnimCount);

    // cMatrixf projMtx;
    GraphicsContext::ShaderProgram shaderProgram;
    // GraphicsContext::LayoutStream layoutStream;
    // context.ScreenSpaceQuad(layoutStream, projMtx, vRenderTargetSize.x, vRenderTargetSize.y);

    shaderProgram.m_configuration.m_write = Write::RGBA;
    shaderProgram.m_handle = m_program;
    // shaderProgram.m_projection = projMtx;

    shaderProgram.m_textures.push_back({ m_s_diffuseMap, input.GetHandle(), 0 });
    shaderProgram.m_textures.push_back({ m_s_ampMap0, m_ampMaps[lAmp0]->GetHandle(), 1 });
    shaderProgram.m_textures.push_back({ m_s_ampMap1, m_ampMaps[lAmp1]->GetHandle(), 2 });
    shaderProgram.m_textures.push_back({ m_s_zoomMap, m_zoomImage->GetHandle(), 3 });

    param.zoomAlpha = mfZoomAlpha;
    param.fT = mfT;
    param.ampT = fAmpT;
    param.waveAlpha = mfWaveAlpha;
    param.alpha = 1.0f;

    shaderProgram.m_uniforms.push_back({ m_u_param, &param, 1 });
    GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
    // request.m_width = vRenderTargetSize.x;
    // request.m_height = vRenderTargetSize.y;
    context.Submit(view, request);
}

cLuxPostEffectHandler::~cLuxPostEffectHandler() {
    for (auto pPostEffect : mvPostEffects) {
        delete pPostEffect;
    }
}

void cLuxPostEffectHandler::OnStart() {
}


void cLuxPostEffectHandler::Update(float afTimeStep) {
    for (auto& pPostEffect : mvPostEffects) {
        if (pPostEffect->IsActive()) {
            pPostEffect->Update(afTimeStep);
        }
    }
}


void cLuxPostEffectHandler::Reset() {
}

void cLuxPostEffectHandler::LoadMainConfig() {
    cConfigFile* pMainCfg = gpBase->mpMainConfig;

    mpInsanity->SetDisabled(pMainCfg->GetBool("Graphics", "PostEffectInsanity", true) == false);
}

void cLuxPostEffectHandler::SaveMainConfig() {
    cConfigFile* pMainCfg = gpBase->mpMainConfig;

    pMainCfg->SetBool("Graphics", "PostEffectInsanity", mpInsanity->IsDisabled() == false);
}

void cLuxPostEffectHandler::AddEffect(iLuxPostEffect* apPostEffect, int alPrio) {
    mvPostEffects.push_back(apPostEffect);
    apPostEffect->SetActive(false);
    gpBase->mpMapHandler->GetViewport()->GetPostEffectComposite()->AddPostEffect(apPostEffect, alPrio);
}
