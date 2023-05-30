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

#pragma once

#include "LuxBase.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/Image.h"
#include <array>

#include <FixPreprocessor.h>
#include <graphics/ForgeHandles.h>

class iLuxPostEffect : public iPostEffect {
public:
    iLuxPostEffect(cGraphics* apGraphics, cResources* apResources)
        : iPostEffect(apGraphics, apResources, NULL) {
    }
    virtual ~iLuxPostEffect() {
    }

    virtual void Update(float afTimeStep) {
    }

protected:
    void OnSetParams() {
    }
    iPostEffectParams* GetTypeSpecificParams() {
        return NULL;
    }
};

class cLuxPostEffect_Insanity : public iLuxPostEffect {
public:
    cLuxPostEffect_Insanity(cGraphics* apGraphics, cResources* apResources);
    ~cLuxPostEffect_Insanity();
    static constexpr uint32_t NumMapAmps = 3;
    static constexpr uint32_t DescriptorSetSize = 16;

    virtual void Update(float afTimeStep) override;

    void SetWaveAlpha(float afX) {
        mfWaveAlpha = afX;
    }
    void SetZoomAlpha(float afX) {
        mfZoomAlpha = afX;
    }
    void SetWaveSpeed(float afX) {
        mfWaveSpeed = afX;
    }

    virtual void RenderEffect(
        cPostEffectComposite& compositor,
        cViewport& viewport,
        const ForgeRenderer::Frame& frame,
        Texture* inputTexture,
        RenderTarget* renderTarget) override;

private:
    uint32_t m_descIndex = 0;
    ForgeShaderHandle m_insanityShader;
    Pipeline* m_insanityPipeline;
    RootSignature* m_instantyRootSignature = nullptr;
    std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_insanityPerFrameset{};
    Sampler* m_inputSampler;

    bgfx::ProgramHandle m_program;
    std::array<Image*, 3> m_ampMaps;
    Image* m_zoomImage;

    float mfT = 0.0f;
    float mfAnimCount = 0.0f;
    float mfWaveAlpha = 0.0f;
    float mfZoomAlpha = 0.0f;
    float mfWaveSpeed = 0.0f;
};

class cLuxPostEffectHandler : public iLuxUpdateable {
public:
    cLuxPostEffectHandler();
    ~cLuxPostEffectHandler();

    void OnStart();
    void Update(float afTimeStep);
    void Reset();

    cLuxPostEffect_Insanity* GetInsanity() {
        return mpInsanity;
    }

private:
    void LoadMainConfig();
    void SaveMainConfig();
    void AddEffect(iLuxPostEffect* apPostEffect, int alPrio);

    cLuxPostEffect_Insanity* mpInsanity;

    std::vector<iLuxPostEffect*> mvPostEffects;
};
