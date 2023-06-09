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
    static constexpr uint16_t BlurSize = 4;
    static constexpr uint32_t DescriptorSetPostEffectSize = 64;
    static constexpr uint32_t DescriptorOutlineSize = 64;

    union LuxEffectObjectUniform {
        struct FlashUniform {
            float4 m_colorMul;
            mat4 m_mvp;
            mat3 m_normalMat;
        } m_flashUniform;
        struct OutlineUniform {
            mat4 m_mvp;
            uint32_t m_feature;
        } m_outlineUniform;
    };

    struct LuxPostEffectData {
    public:
        LuxPostEffectData() = default;
        LuxPostEffectData(const LuxPostEffectData&) = delete;
        LuxPostEffectData(LuxPostEffectData&& other)
            : m_blurTarget(std::move(other.m_blurTarget))
            , m_outlineBuffer(std::move(other.m_outlineBuffer))
            , m_size(other.m_size){
        }
        LuxPostEffectData& operator=(const LuxPostEffectData&) = delete;
        void operator=(LuxPostEffectData&& other) {
            m_blurTarget = std::move(other.m_blurTarget);
            m_outlineBuffer = std::move(other.m_outlineBuffer);
            m_size = other.m_size;
        }
        cVector2l m_size = cVector2l(0, 0);
        ForgeRenderTarget m_outlineBuffer; // the intermediary results from the outline
        std::array<ForgeRenderTarget, 2> m_blurTarget;
    };


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

private:


    std::vector<cGlowObject> mvFlashObjects;
    std::vector<cGlowObject> mvEnemyGlowObjects;
    std::vector<iRenderable*> mvOutlineObjects;

    UniqueViewportData<LuxPostEffectData> m_boundPostEffectData;

    // outline effect
    GPURingBuffer m_uniformBuffer;
    Sampler* m_diffuseSampler;

    std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_perObjectDescriptorSet;
    Pipeline* m_outlinePipeline;
    Pipeline* m_outlineStencilPipeline;
    Pipeline* m_objectFlashPipeline;
    Pipeline* m_flashPipeline;
    Pipeline* m_enemyGlowPipeline;
    RootSignature* m_perObjectRootSignature;

    std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_outlinePostprocessingDescriptorSet;
    RootSignature* m_postProcessingRootSignature;
    Pipeline* m_blurHorizontalPipeline;
    Pipeline* m_blurVerticalPipeline;
    Pipeline* m_combineOutlineAddPipeline;

    Shader* m_blurVerticalShader;
    Shader* m_blurHorizontalShader;
    Shader* m_enemyGlowShader;
    Shader* m_outlineShader;
    Shader* m_outlineStencilShader;
    Shader* m_objectFlashShader;
    Shader* m_outlineCopyShader;

    cLinearOscillation mFlashOscill;
};

