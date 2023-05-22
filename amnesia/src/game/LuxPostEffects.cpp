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

#include <graphics/ForgeRenderer.h>
#include <folly/FixedString.h>


cLuxPostEffect_Insanity::cLuxPostEffect_Insanity(cGraphics* apGraphics, cResources* apResources)
    : iLuxPostEffect(apGraphics, apResources) {
    auto* forgeRenderer = Interface<ForgeRenderer>::Get();
    {
        ShaderLoadDesc loadDesc = {};
        loadDesc.mStages[0] = { "fullscreen.vert", nullptr, 0 };
        loadDesc.mStages[1] = { "dds_insanity_posteffect.frag", nullptr, 0 };
        addShader(forgeRenderer->Rend(), &loadDesc, &m_insanityShader);
    }
    RootSignatureDesc rootDesc = { &m_insanityShader, 1 };
    addRootSignature(forgeRenderer->Rend(), &rootDesc, &m_instantyRootSignature);

    DescriptorSetDesc perFrameDescSet{m_instantyRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 1};
    for(auto &desc: m_insanityPerFrameset) {
        addDescriptorSet(forgeRenderer->Rend(), &perFrameDescSet, &desc);
    }

    for (size_t i = 0; i < m_ampMaps.size(); i++) {
        m_ampMaps[i] = mpResources->GetTextureManager()->Create2DImage("posteffect_insanity_ampmap" + cString::ToString((int)i), false);
    }
    m_zoomImage = mpResources->GetTextureManager()->Create2DImage("posteffect_insanity_zoom.jpg", false);


    DepthStateDesc depthStateDisabledDesc = {};
    depthStateDisabledDesc.mDepthWrite = false;
    depthStateDisabledDesc.mDepthTest = false;

    RasterizerStateDesc rasterStateNoneDesc = {};
    rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

    std::array imageTargets = {
        iPostEffect::PostEffectImageFormat
    };
    PipelineDesc pipelineDesc = {};
    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
    graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    graphicsPipelineDesc.mRenderTargetCount = imageTargets.size();
    graphicsPipelineDesc.pColorFormats = imageTargets.data();
    graphicsPipelineDesc.pShaderProgram = m_insanityShader;
    graphicsPipelineDesc.pRootSignature = m_instantyRootSignature;
    graphicsPipelineDesc.mRenderTargetCount = 1;
    graphicsPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
    graphicsPipelineDesc.pVertexLayout = NULL;
    graphicsPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
    graphicsPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
    graphicsPipelineDesc.pDepthState = &depthStateDisabledDesc;
    graphicsPipelineDesc.pBlendState = NULL;

    addPipeline(forgeRenderer->Rend(), &pipelineDesc, &m_insanityPipeline);
}


cLuxPostEffect_Insanity::~cLuxPostEffect_Insanity() {
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


cLuxPostEffectHandler::cLuxPostEffectHandler()
    : iLuxUpdateable("LuxPostEffectHandler") {
    cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
    cResources* pResources = gpBase->mpEngine->GetResources();

    mpInsanity = hplNew(cLuxPostEffect_Insanity, (pGraphics, pResources));
    AddEffect(mpInsanity, 25);
    mpInsanity->SetActive(true);
}

void cLuxPostEffect_Insanity::RenderEffect(cPostEffectComposite& compositor, cViewport& viewport, GraphicsContext& context, Image& input, LegacyRenderTarget& target) {

    auto viewportSize = viewport.GetSize();
    cMatrixf projMtx;
    GraphicsContext::ShaderProgram shaderProgram;
    GraphicsContext::LayoutStream layoutStream;
    context.ScreenSpaceQuad(layoutStream, projMtx, viewportSize.x, viewportSize.y);

    GraphicsContext::ViewConfiguration viewConfiguration {target};
    viewConfiguration.m_viewRect = cRect2l(0, 0, viewportSize.x, viewportSize.y);
    viewConfiguration.m_projection = projMtx;
    bgfx::ViewId view = context.StartPass("DDS_Insanity", viewConfiguration);

    struct {
        float alpha;
        float fT;
        float ampT;
        float waveAlpha;

        float zoomAlpha;
        float screenSize[2];
        float pad;
    } param = { 0 };

    int lAmp0 = static_cast<int>(mfAnimCount);
    int lAmp1 = static_cast<int>(mfAnimCount + 1);
    if (lAmp1 >= static_cast<int>(m_ampMaps.size())) {
        lAmp1 = 0;
    }
    float fAmpT = cMath::GetFraction(mfAnimCount);


    shaderProgram.m_configuration.m_write = Write::RGBA;
    shaderProgram.m_handle = m_program;

    shaderProgram.m_textures.push_back({ m_s_diffuseMap, input.GetHandle(), 0 });
    shaderProgram.m_textures.push_back({ m_s_ampMap0, m_ampMaps[lAmp0]->GetHandle(), 1 });
    shaderProgram.m_textures.push_back({ m_s_ampMap1, m_ampMaps[lAmp1]->GetHandle(), 2 });
    shaderProgram.m_textures.push_back({ m_s_zoomMap, m_zoomImage->GetHandle(), 3 });

    param.zoomAlpha = mfZoomAlpha;
    param.fT = mfT;
    param.ampT = fAmpT;
    param.waveAlpha = mfWaveAlpha;
    param.alpha = 1.0f;
    param.screenSize[0] = viewportSize.x;
    param.screenSize[1] = viewportSize.y;

    shaderProgram.m_uniforms.push_back({ m_u_param, &param, 2 });
    GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };

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
