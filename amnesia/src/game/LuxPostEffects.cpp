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
#include <graphics/ForgeRenderer.h>

cLuxPostEffect_Insanity::cLuxPostEffect_Insanity(cGraphics* apGraphics, cResources* apResources)
    : iLuxPostEffect(apGraphics, apResources) {
    auto* forgeRenderer = Interface<ForgeRenderer>::Get();
    m_insanityShader.Load(forgeRenderer->Rend(),[&](Shader** shader) {
        ShaderLoadDesc loadDesc{};
        loadDesc.mStages[0].pFileName = "fullscreen.vert";
        loadDesc.mStages[1].pFileName = "dds_insanity_posteffect.frag";
        addShader(forgeRenderer->Rend(), &loadDesc, shader);
        return true;
    });
    {
        SamplerDesc samplerDesc = {};
        addSampler(forgeRenderer->Rend(), &samplerDesc, &m_inputSampler);
    }

    std::array shaders = {
        m_insanityShader.m_handle,
    };
    const char* pStaticSamplers[] = { "inputSampler" };
    RootSignatureDesc rootDesc = {};
    rootDesc.ppShaders = shaders.data();
    rootDesc.mShaderCount = shaders.size();
    rootDesc.mStaticSamplerCount = std::size(pStaticSamplers);
    rootDesc.ppStaticSamplers = &m_inputSampler;
    rootDesc.ppStaticSamplerNames = pStaticSamplers;
    addRootSignature(forgeRenderer->Rend(), &rootDesc, &m_instantyRootSignature);

    DescriptorSetDesc perFrameDescSet{ m_instantyRootSignature,
                                       DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                       cLuxPostEffect_Insanity::DescriptorSetSize };
    for (auto& desc : m_insanityPerFrameset) {
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

    std::array imageTargets = { TinyImageFormat_R8G8B8A8_UNORM };
    PipelineDesc pipelineDesc = {};
    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
    graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    graphicsPipelineDesc.mRenderTargetCount = imageTargets.size();
    graphicsPipelineDesc.pColorFormats = imageTargets.data();
    graphicsPipelineDesc.pShaderProgram = m_insanityShader.m_handle;
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

cLuxPostEffectHandler::cLuxPostEffectHandler()
    : iLuxUpdateable("LuxPostEffectHandler") {
    cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
    cResources* pResources = gpBase->mpEngine->GetResources();

    mpInsanity = hplNew(cLuxPostEffect_Insanity, (pGraphics, pResources));
    AddEffect(mpInsanity, 25);
    mpInsanity->SetActive(true);
}

void cLuxPostEffect_Insanity::RenderEffect(
    cPostEffectComposite& compositor,
    cViewport& viewport,
    const ForgeRenderer::Frame& frame,
    Texture* inputTexture,
    RenderTarget* renderTarget) {
    int lAmp0 = static_cast<int>(mfAnimCount);
    int lAmp1 = static_cast<int>(mfAnimCount + 1);
    if (lAmp1 >= static_cast<int>(m_ampMaps.size())) {
        lAmp1 = 0;
    }
    float fAmpT = cMath::GetFraction(mfAnimCount);

    struct {
        float time;
        float amplitude;
        float waveAlpha;
        float zoomAlpha;
    } param = { 0 };
    param.time = mfT;
    param.zoomAlpha = mfZoomAlpha;
    param.waveAlpha = mfWaveAlpha;
    param.amplitude = fAmpT;
    uint32_t rootConstantIndex = getDescriptorIndexFromName(m_instantyRootSignature, "postEffectConstants");
    cmdBindPushConstants(frame.m_cmd, m_instantyRootSignature, rootConstantIndex, &param);

    LoadActionsDesc loadActions = {};
    loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
    loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

    cmdBindRenderTargets(frame.m_cmd, 1, &renderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

    std::array<DescriptorData, 4> params = {};
    params[0].pName = "sourceInput";
    params[0].ppTextures = &inputTexture;
    params[1].pName = "ampMap0";
    params[1].ppTextures = &m_ampMaps[lAmp0]->GetTexture().m_handle;
    params[2].pName = "ampMap1";
    params[2].ppTextures = &m_ampMaps[lAmp1]->GetTexture().m_handle;
    params[3].pName = "zoomMap";
    params[3].ppTextures = &m_zoomImage->GetTexture().m_handle;
    updateDescriptorSet(frame.m_renderer->Rend(), m_descIndex, m_insanityPerFrameset[frame.m_frameIndex], params.size(), params.data());

    cmdSetViewport(
        frame.m_cmd, 0.0f, 0.0f, static_cast<float>(renderTarget->mWidth), static_cast<float>(renderTarget->mHeight), 0.0f, 1.0f);
    cmdSetScissor(frame.m_cmd, 0, 0, static_cast<float>(renderTarget->mWidth), static_cast<float>(renderTarget->mHeight));
    cmdBindPipeline(frame.m_cmd, m_insanityPipeline);

    cmdBindDescriptorSet(frame.m_cmd, m_descIndex, m_insanityPerFrameset[frame.m_frameIndex]);
    cmdDraw(frame.m_cmd, 3, 0);
    m_descIndex = (m_descIndex + 1) % cLuxPostEffect_Insanity::DescriptorSetSize;
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
