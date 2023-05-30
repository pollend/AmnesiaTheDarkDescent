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
#include "bgfx/bgfx.h"

#include "graphics/Image.h"
#include "graphics/RenderTarget.h"


#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include "graphics/ForgeHandles.h"

class cGlowObject
{
public:
    cGlowObject()
    {
    }
    cGlowObject(iRenderable* apObject, float afAlpha)
        : mpObject(apObject)
        , mfAlpha(afAlpha)
    {
    }

    iRenderable* mpObject;
    float mfAlpha;
};

//----------------------------------------------

class cLuxEffectRenderer : public iLuxUpdateable
{
public:
    static constexpr uint32_t MaxObjectUniform = 128;

    struct LuxPostEffectData {
    public:
        LuxPostEffectData() = default;
        LuxPostEffectData(const LuxPostEffectData&) = delete;
        LuxPostEffectData(LuxPostEffectData&& other)
            : m_outputTarget(std::move(other.m_outputTarget))
            , m_outlineTarget(std::move(other.m_outlineTarget))
            , m_outputImage(std::move(other.m_outputImage))
            , m_gBufferDepthStencil(std::move(other.m_gBufferDepthStencil))
            , m_blurTarget(std::move(other.m_blurTarget)){
        }
        LuxPostEffectData& operator=(const LuxPostEffectData&) = delete;
        void operator=(LuxPostEffectData&& other) {
            m_outputTarget = std::move(other.m_outputTarget);
            m_outlineTarget = std::move(other.m_outlineTarget);
            m_outputImage = std::move(other.m_outputImage);
            m_gBufferDepthStencil = std::move(other.m_gBufferDepthStencil);
            m_blurTarget = std::move(other.m_blurTarget);
        }
        // images from the gbuffer we need this to determine if we need to rebuild the render targets
        std::shared_ptr<Image> m_outputImage;
        std::shared_ptr<Image> m_gBufferDepthStencil;

        std::array<LegacyRenderTarget, 2> m_blurTarget;
        std::array<ForgeRenderTarget, 2> m_blurImageTarget;
        std::array<ForgeRenderTarget, ForgeRenderer::SwapChainLength> m_outlineBuffer; // the intermediary results from the outline
        std::array<ForgeRenderTarget, ForgeRenderer::SwapChainLength> m_inputDepthBuffer; // the input depth from the main render pass

        LegacyRenderTarget m_outputTarget;
        LegacyRenderTarget m_outlineTarget;
    };

    static constexpr uint16_t BlurSize = 4;
    cLuxEffectRenderer();
    ~cLuxEffectRenderer();

    void Reset() override;

    virtual void Update(float afTimeStep) override;

    void ClearRenderLists();

    void RenderSolid(cViewport::PostSolidDrawPacket& input);
    void RenderTrans(cViewport::PostTranslucenceDrawPacket& input);

    void AddOutlineObject(iRenderable* apObject);
    void ClearOutlineObjects();

    void AddFlashObject(iRenderable* apObject, float afAlpha);
    void AddEnemyGlow(iRenderable* apObject, float afAlpha);

    struct LuxEffectOutlineUniform {
        float4 color;
        mat4 mvp;
        mat3 normalMat;
    };
    union LuxEffectUniform {
        LuxEffectOutlineUniform outline;
    };

private:

    // this is reused code from PostEffect_Bloom. I think the passes can be separated out into handlers of some kind :?
    void RenderBlurPass(GraphicsContext& context, std::span<LegacyRenderTarget> blurTargets, Image& input);

    std::vector<cGlowObject> mvFlashObjects;
    std::vector<cGlowObject> mvEnemyGlowObjects;
    std::vector<iRenderable*> mvOutlineObjects;

    UniqueViewportData<LuxPostEffectData> m_boundPostEffectData;

    Shader* m_blurShader;

    // glow effect
    Shader* m_enemyGlowShader;

    // outline effect
    GPURingBuffer* m_uniformBuffer;
    Shader* m_outlineShader;
    Shader* m_outlineStencilShader;
    DescriptorSet* m_outlinePerObjectDescriptorSet;
    RootSignature* m_outlineRootSignature;
    Pipeline* m_outlinePipeline;
    Pipeline* m_outlineStencilPipeline;

    Shader* m_objectFlashShader;
    Shader* m_outlineRejectDiffuseAlphaShader;

    bgfx::ProgramHandle m_blurProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_enemyGlowProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_copyProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_objectFlashProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_alphaRejectProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_outlineProgram = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_s_diffuseMap = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_param = BGFX_INVALID_HANDLE;

    cLinearOscillation mFlashOscill;
};

