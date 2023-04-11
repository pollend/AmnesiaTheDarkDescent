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

#include "LuxEffectRenderer.h"
#include "engine/Interface.h"
#include <absl/container/inlined_vector.h>
#include <graphics/Enum.h>
#include <graphics/GraphicsContext.h>
#include <graphics/RenderTarget.h>
#include <math/MathTypes.h>
#include <memory>
#include <vector>


#include "bgfx/bgfx.h"
#include <bx/debug.h>

cLuxEffectRenderer::cLuxEffectRenderer()
    : iLuxUpdateable("LuxEffectRenderer")
{

     m_boundPostEffectData = std::move(UniqueViewportData<LuxPostEffectData>([](cViewport& viewport) {
        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
        cRendererDeferred* pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));
        auto& gbuffer = pRendererDeferred->GetSharedData(viewport);

        auto blurImageDesc = [&]
        {
            auto desc = ImageDescriptor::CreateTexture2D(
                gbuffer.m_size.x / cLuxEffectRenderer::BlurSize,
                gbuffer.m_size.y / cLuxEffectRenderer::BlurSize,
                false,
                bgfx::TextureFormat::Enum::RGBA8);
            desc.m_configuration.m_rt = RTType::RT_Write;
            auto image = std::make_shared<Image>();
            image->Initialize(desc);
            return image;
        };
        
        auto postEffect = std::make_unique<LuxPostEffectData>();
        postEffect->m_outputImage = gbuffer.m_outputImage;
        postEffect->m_gBufferDepthStencil = gbuffer.m_gBufferDepthStencil;

        postEffect->m_blurTarget[0] = RenderTarget(blurImageDesc());
        postEffect->m_blurTarget[1] = RenderTarget(blurImageDesc());
        
        {
            std::array<std::shared_ptr<Image>, 2> image = {postEffect->m_outputImage, postEffect->m_gBufferDepthStencil};
            postEffect->m_outputTarget = RenderTarget(image);
        }
    
        auto outlineImageDesc = ImageDescriptor::CreateTexture2D(gbuffer.m_size.x, gbuffer.m_size.y, false, bgfx::TextureFormat::RGBA8);
        outlineImageDesc.m_configuration.m_rt = RTType::RT_Write;
        auto outlineImage = std::make_shared<Image>();
        outlineImage->Initialize(outlineImageDesc);

        std::array<std::shared_ptr<Image>, 2> outlineImages = { outlineImage, postEffect->m_gBufferDepthStencil };
        postEffect->m_outlineTarget = RenderTarget(std::span(outlineImages));

        return postEffect;
    }, [&](cViewport& viewport, LuxPostEffectData& target) {
        cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
        cRendererDeferred* pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));
        auto& gbuffer = pRendererDeferred->GetSharedData(viewport);

        // as long as the output image and depth stencil image are the same, we can use the same render target 
            // else we need to create a new one
        return viewport.GetRenderTarget().IsValid() 
            && gbuffer.m_outputImage == target.m_outputImage
            && gbuffer.m_gBufferDepthStencil == target.m_gBufferDepthStencil;
    }));

    m_alphaRejectProgram = hpl::loadProgram("vs_alpha_reject", "fs_alpha_reject");
    m_blurProgram = hpl::loadProgram("vs_post_effect", "fs_posteffect_blur");
    m_enemyGlowProgram = hpl::loadProgram("vs_dds_enemy_glow", "fs_dds_enemy_glow");
    m_objectFlashProgram = hpl::loadProgram("vs_dds_flash", "fs_dds_flash");
    m_copyProgram = hpl::loadProgram("vs_post_effect", "fs_post_effect_copy");
    m_outlineProgram = hpl::loadProgram("vs_dds_outline", "fs_dds_outline");


    m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Sampler);
    m_u_param = bgfx::createUniform("u_param", bgfx::UniformType::Vec4);

    ///////////////////////////
    // Reset variables
    mFlashOscill.SetUp(0, 1, 0, 1, 1);

    ///////////////////////////
    // Reset variables
    Reset();
}

//-----------------------------------------------------------------------

cLuxEffectRenderer::~cLuxEffectRenderer()
{
    if (bgfx::isValid(m_alphaRejectProgram))
    {
        bgfx::destroy(m_alphaRejectProgram);
    }
    if (bgfx::isValid(m_blurProgram))
    {
        bgfx::destroy(m_blurProgram);
    }
    if (bgfx::isValid(m_enemyGlowProgram))
    {
        bgfx::destroy(m_enemyGlowProgram);
    }
    if (bgfx::isValid(m_objectFlashProgram))
    {
        bgfx::destroy(m_objectFlashProgram);
    }
    if (bgfx::isValid(m_copyProgram))
    {
        bgfx::destroy(m_copyProgram);
    }
    if (bgfx::isValid(m_outlineProgram))
    {
        bgfx::destroy(m_outlineProgram);
    }
    if (bgfx::isValid(m_s_diffuseMap))
    {
        bgfx::destroy(m_s_diffuseMap);
    }
    if (bgfx::isValid(m_u_param))
    {
        bgfx::destroy(m_u_param);
    }
}

void cLuxEffectRenderer::Reset()
{
    mvOutlineObjects.clear();
    ClearRenderLists();
}

void cLuxEffectRenderer::ClearRenderLists()
{
    mvFlashObjects.clear();
    mvEnemyGlowObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::Update(float afTimeStep)
{
    mFlashOscill.Update(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderSolid(cViewport::PostSolidDrawPacket&  input)
{
}

void cLuxEffectRenderer::RenderTrans(cViewport::PostTranslucenceDrawPacket&  input)
{
    /////////////////////////////////////
    // Normal and Reflection
    // Render Flashing Objects
    auto graphicsContext = input.m_context;
    auto currentFrustum = input.m_frustum;
    // render Flashing objects ---------------------------------------------------------------------------------------------------
    ([&](){
        if (mvFlashObjects.empty())
            return;

        auto& postEffectData = m_boundPostEffectData.resolve(*input.m_viewport);
        // auto& graphicsContext = apFunctions->GetGraphicsContext();
        if(!postEffectData.m_outputImage || 
            !postEffectData.m_gBufferDepthStencil) {
            return;
        }
        
        // auto currentFrustum = apFunctions->GetFrustum();
        float fGlobalAlpha = (0.5f + mFlashOscill.val * 0.5f);
        auto imageSize = postEffectData.m_outputImage->GetImageSize();
        
        GraphicsContext::ViewConfiguration viewConfiguration {postEffectData.m_outputTarget};
        viewConfiguration.m_viewRect = cRect2l(0, 0, imageSize.x, imageSize.y);
        viewConfiguration.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
        viewConfiguration.m_view = currentFrustum->GetViewMatrix().GetTranspose();
        const auto view = graphicsContext->StartPass("Render Flash Object", viewConfiguration);
        for (auto& flashObject : mvFlashObjects)
        {
            auto* pObject = flashObject.mpObject;
            if (!pObject->CollidesWithFrustum(currentFrustum))
            {
                continue;
            }

            GraphicsContext::LayoutStream layoutInput;
            GraphicsContext::ShaderProgram shaderInput;
            struct
            {
                float m_colorMul;
                float pad[3];
            } params = { 0 };
            params.m_colorMul = flashObject.mfAlpha * fGlobalAlpha;
            shaderInput.m_handle = m_objectFlashProgram;

            shaderInput.m_uniforms.push_back({ m_u_param, &params });
            shaderInput.m_textures.push_back({ m_s_diffuseMap, pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetHandle(), 0 });

            shaderInput.m_configuration.m_write = Write::RGBA;
            shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
            shaderInput.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
            shaderInput.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

            // shaderInput.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
            // shaderInput.m_view = currentFrustum->GetViewMatrix().GetTranspose();
            shaderInput.m_modelTransform = pObject->GetModelMatrixPtr() ? pObject->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
            pObject->GetVertexBuffer()->GetLayoutStream(layoutInput);

            GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
            for (int i = 0; i < 2; ++i)
            {
                input.m_context->Submit(view, drawRequest);
            }
        }
    })();
    // Render Enemy Glow ---------------------------------------------------------------------------------------------------
    ([&] () {
        if (mvEnemyGlowObjects.empty()) {
            return;
        }
    
        auto& postEffectData = m_boundPostEffectData.resolve(*input.m_viewport);
        if(!postEffectData.m_outputImage || 
            !postEffectData.m_gBufferDepthStencil) {
            return;
        }
        auto imageSize = postEffectData.m_outputImage->GetImageSize();
        
        GraphicsContext::ViewConfiguration viewConfiguration {postEffectData.m_outputTarget};
        viewConfiguration.m_viewRect = cRect2l(0, 0, imageSize.x, imageSize.y);
        viewConfiguration.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
        viewConfiguration.m_view = currentFrustum->GetViewMatrix().GetTranspose();
        
        const auto view = input.m_context->StartPass("Render Enemy Glow", viewConfiguration);
        for (auto& enemyGlow : mvEnemyGlowObjects)
        {
            auto* pObject = enemyGlow.mpObject;
            if (!pObject->CollidesWithFrustum(currentFrustum))
            {
                continue;
            }
            GraphicsContext::LayoutStream layoutInput;
            GraphicsContext::ShaderProgram shaderInput;
            struct
            {
                float m_colorMul;
                float pad[3];
            } params = { 0 };
            params.m_colorMul = enemyGlow.mfAlpha;

            shaderInput.m_handle = m_enemyGlowProgram;

            shaderInput.m_uniforms.push_back({ m_u_param, &params });
            shaderInput.m_textures.push_back({ m_s_diffuseMap, pObject->GetMaterial()->GetImage(eMaterialTexture_Diffuse)->GetHandle(), 0 });

            shaderInput.m_configuration.m_write = Write::RGBA;
            shaderInput.m_configuration.m_depthTest = DepthTest::Equal;
            shaderInput.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
            shaderInput.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

            // shaderInput.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
            // shaderInput.m_view = currentFrustum->GetViewMatrix().GetTranspose();
            shaderInput.m_modelTransform = pObject->GetModelMatrixPtr() ? pObject->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
            pObject->GetVertexBuffer()->GetLayoutStream(layoutInput);

            GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
            input.m_context->Submit(view, drawRequest);
        }
    })();

    if (input.m_renderSettings->mbIsReflection == false) {
        // Render Enemy Outlines ---------------------------------------------------------------------------------------------------
        ([&]() {
            if (mvOutlineObjects.empty()) {
                return;
            }

            auto& postEffectData = m_boundPostEffectData.resolve(*input.m_viewport);
            if (!postEffectData.m_outputImage || !postEffectData.m_gBufferDepthStencil) {
                return;
            }
            auto imageSize = postEffectData.m_outputImage->GetImageSize();

            /////////////////////////////
            // Setup vars
            cGraphics* pGraphics = gpBase->mpEngine->GetGraphics();
            // cVector2l vScreenSize = pGraphics->GetLowLevel()->GetScreenSizeInt();

            float fScaleAdd = 0.02f;

            ////////////////////////////////////
            // Get entities to be rendered
            // cFrustum* pFrustum = apFunctions->GetFrustum();
            absl::InlinedVector<iRenderable*, 15> lstObjects;
            for (size_t i = 0; i < mvOutlineObjects.size(); ++i) {
                iRenderable* pObject = mvOutlineObjects[i];

                if (pObject->CollidesWithFrustum(input.m_frustum)) {
                    lstObjects.push_back(pObject);
                }
            }

            ////////////////////////////////////
            // Build AABB from all objects
            cBoundingVolume totalBV;
            cVector3f vTotalMin = 100000.0f;
            cVector3f vTotalMax = -100000.0f;
            for (auto& object : lstObjects) {
                cBoundingVolume* pBV = object->GetBoundingVolume();
                cMath::ExpandAABB(vTotalMin, vTotalMax, pBV->GetMin(), pBV->GetMax());
            }

            // Need to scale so the outline is contained.
            cVector3f vTotalSize = vTotalMax - vTotalMin;
            cVector3f vTotalAdd = vTotalSize * (cVector3f(1.0f) / vTotalSize) * (fScaleAdd * 2);

            totalBV.SetLocalMinMax(vTotalMin - vTotalAdd, vTotalMax + vTotalAdd);

            // TODO: use clip rect to avoid rendering to the entire framebuffer
            cRect2l clipRect;
            cMath::GetClipRectFromBV(clipRect, totalBV, input.m_frustum, imageSize, -1);

            {
                GraphicsContext::ViewConfiguration viewConfiguration{ postEffectData.m_outlineTarget };
                viewConfiguration.m_viewRect = { 0, 0, static_cast<uint16_t>(imageSize.x), static_cast<uint16_t>(imageSize.y) };
                viewConfiguration.m_clear = { 0, 1.0, 0, ClearOp::Stencil | ClearOp::Color };
                bgfx::touch(input.m_context->StartPass("Clear Outline And Stencil", viewConfiguration));
            }

            {
                GraphicsContext::ViewConfiguration viewConfiguration{ postEffectData.m_outlineTarget };
                viewConfiguration.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
                viewConfiguration.m_view = currentFrustum->GetViewMatrix().GetTranspose();
                viewConfiguration.m_viewRect = { 0, 0, static_cast<uint16_t>(imageSize.x), static_cast<uint16_t>(imageSize.y) };
                const auto view = input.m_context->StartPass("Render Outline Stencil", viewConfiguration);
                for (auto& object : lstObjects) {
                    GraphicsContext::LayoutStream layoutInput;
                    GraphicsContext::ShaderProgram shaderInput;
                    struct {
                        float m_alpha;
                        float pad[3];
                    } params = { 0 };

                    shaderInput.m_handle = m_alphaRejectProgram;
                    auto alphaImage = object->GetMaterial()->GetImage(eMaterialTexture_Alpha);
                    if (alphaImage) {
                        shaderInput.m_textures.push_back({ m_s_diffuseMap, alphaImage->GetHandle(), 0 });
                        params.m_alpha = 0.5f;
                    }
                    shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
                    shaderInput.m_uniforms.push_back({ m_u_param, &params });
                    shaderInput.m_handle = m_alphaRejectProgram;
                    shaderInput.m_configuration.m_frontStencilTest = CreateStencilTest(
                        StencilFunction::Always, StencilFail::Keep, StencilDepthFail::Keep, StencilDepthPass::Replace, 0xff, 0xff);
                    // shaderInput.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
                    // shaderInput.m_view = currentFrustum->GetViewMatrix().GetTranspose();
                    shaderInput.m_modelTransform =
                        object->GetModelMatrixPtr() ? object->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
                    object->GetVertexBuffer()->GetLayoutStream(layoutInput);

                    GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                    input.m_context->Submit(view, drawRequest);
                }
            }

            {
                GraphicsContext::ViewConfiguration viewConfiguration{ postEffectData.m_outlineTarget };
                viewConfiguration.m_viewRect = { 0, 0, imageSize.x, imageSize.y };
                viewConfiguration.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
                viewConfiguration.m_view = currentFrustum->GetViewMatrix().GetTranspose();
                const auto view = input.m_context->StartPass("Render Outline", viewConfiguration);
                for (auto& object : lstObjects) {
                    GraphicsContext::LayoutStream layoutInput;
                    GraphicsContext::ShaderProgram shaderInput;
                    struct {
                        float color[3];
                        float useAlpha;
                    } params = { { 0, 0, 0.5f }, 0 };

                    shaderInput.m_handle = m_outlineProgram;
                    auto alphaImage = object->GetMaterial()->GetImage(eMaterialTexture_Alpha);
                    if (alphaImage) {
                        shaderInput.m_textures.push_back({ m_s_diffuseMap, alphaImage->GetHandle(), 0 });
                        params.useAlpha = 1.0f;
                    }
                    shaderInput.m_configuration.m_write = Write::RGBA;
                    shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
                    shaderInput.m_uniforms.push_back({ m_u_param, &params });
                    shaderInput.m_configuration.m_frontStencilTest = CreateStencilTest(
                        StencilFunction::NotEqual, StencilFail::Keep, StencilDepthFail::Keep, StencilDepthPass::Keep, 0xff, 0xff);

                    cBoundingVolume* pBV = object->GetBoundingVolume();
                    cVector3f vLocalSize = pBV->GetLocalMax() - pBV->GetLocalMin();
                    cVector3f vScale = (cVector3f(1.0f) / vLocalSize) * fScaleAdd + cVector3f(1.0f);

                    cMatrixf mtxScale = cMath::MatrixMul(cMath::MatrixScale(vScale), cMath::MatrixTranslate(pBV->GetLocalCenter() * -1));
                    mtxScale.SetTranslation(mtxScale.GetTranslation() + pBV->GetLocalCenter());

                    // shaderInput.m_projection = currentFrustum->GetProjectionMatrix().GetTranspose();
                    // shaderInput.m_view = currentFrustum->GetViewMatrix().GetTranspose();
                    shaderInput.m_modelTransform = cMath::MatrixMul(object->GetWorldMatrix(), mtxScale).GetTranspose();
                    object->GetVertexBuffer()->GetLayoutStream(layoutInput);

                    GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                    input.m_context->Submit(view, drawRequest);
                }
            }

            RenderBlurPass(*input.m_context, postEffectData.m_blurTarget, *postEffectData.m_outlineTarget.GetImage());
            for (size_t i = 0; i < 2; ++i) {
                RenderBlurPass(*input.m_context, postEffectData.m_blurTarget, *postEffectData.m_blurTarget[1].GetImage());
            }

            {
                auto imageSize = postEffectData.m_blurTarget[1].GetImage()->GetImageSize();
                GraphicsContext::LayoutStream layoutStream;
                cMatrixf projMtx;
                input.m_context->ScreenSpaceQuad(layoutStream, projMtx, imageSize.x, imageSize.y);

                GraphicsContext::ViewConfiguration viewConfiguration{ postEffectData.m_outputTarget };
                viewConfiguration.m_projection = projMtx;
                const auto view = input.m_context->StartPass("Additive Outline", viewConfiguration);
                auto outlineOutputImage = postEffectData.m_blurTarget[1].GetImage();

                GraphicsContext::ShaderProgram shaderProgram;
                GraphicsContext::ShaderProgram program;
                program.m_handle = m_copyProgram;
                program.m_configuration.m_write = Write::RGBA;
                // program.m_projection = projMtx;
                program.m_configuration.m_alphaBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                program.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

                program.m_textures.push_back({ m_s_diffuseMap, outlineOutputImage->GetHandle(), 0 });

                GraphicsContext::DrawRequest request = { layoutStream, program };
                input.m_context->Submit(view, request);
            }
        })();
    }
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddOutlineObject(iRenderable* apObject)
{
    mvOutlineObjects.push_back(apObject);
}

void cLuxEffectRenderer::ClearOutlineObjects()
{
    mvOutlineObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddFlashObject(iRenderable* apObject, float afAlpha)
{
    mvFlashObjects.push_back(cGlowObject(apObject, afAlpha));
}

void cLuxEffectRenderer::AddEnemyGlow(iRenderable* apObject, float afAlpha)
{
    mvEnemyGlowObjects.push_back(cGlowObject(apObject, afAlpha));
}


void cLuxEffectRenderer::RenderBlurPass(GraphicsContext& context, std::span<RenderTarget> blurTargets, Image& input)
{
    BX_ASSERT(blurTargets.size() == 2, "Invalid blur target count");

    struct
    {
        float u_useHorizontal;
        float u_blurSize;
        float texelSize[2];
    } blurParams = { 0 };

    auto imageSize = input.GetImageSize();

    auto image = blurTargets[1].GetImage(0);
    {
        GraphicsContext::LayoutStream layoutStream;
        cMatrixf projMtx;
        context.ScreenSpaceQuad(layoutStream, projMtx, imageSize.x, imageSize.y);
        
        GraphicsContext::ViewConfiguration viewConfiguration {blurTargets[0]};

		viewConfiguration.m_viewRect = cRect2l(0, 0, imageSize.x, imageSize.y);
        viewConfiguration.m_projection = projMtx;
        bgfx::ViewId view = context.StartPass("Blur Pass 1", viewConfiguration);
        blurParams.u_useHorizontal = 0;
        blurParams.u_blurSize = BlurSize;
        blurParams.texelSize[0] = image->GetWidth();
        blurParams.texelSize[1] = image->GetHeight();

        GraphicsContext::ShaderProgram shaderProgram;
        shaderProgram.m_configuration.m_write = Write::RGBA;
        shaderProgram.m_handle = m_blurProgram;
        // shaderProgram.m_projection = projMtx;

        shaderProgram.m_textures.push_back({ m_s_diffuseMap, input.GetHandle(), 1 });
        shaderProgram.m_uniforms.push_back({ m_u_param, &blurParams, 1 });

        GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        context.Submit(view, request);
    }

    {
        GraphicsContext::LayoutStream layoutStream;
        cMatrixf projMtx;
        context.ScreenSpaceQuad(layoutStream, projMtx, imageSize.x, imageSize.y);

        GraphicsContext::ViewConfiguration viewConfiguration {blurTargets[1]};
		viewConfiguration.m_viewRect = cRect2l(0, 0, image->GetWidth(), image->GetHeight());
        viewConfiguration.m_projection = projMtx;
        bgfx::ViewId view = context.StartPass("Blur Pass 2", viewConfiguration);

        blurParams.u_useHorizontal = 1.0;
        blurParams.u_blurSize = BlurSize;
        blurParams.texelSize[0] = image->GetWidth();
        blurParams.texelSize[1] = image->GetHeight();

        GraphicsContext::ShaderProgram shaderProgram;
        shaderProgram.m_configuration.m_write = Write::RGBA;
        shaderProgram.m_handle = m_blurProgram;
        // shaderProgram.m_projection = projMtx;

        shaderProgram.m_textures.push_back({ m_s_diffuseMap, blurTargets[0].GetImage()->GetHandle(), 1 });
        shaderProgram.m_uniforms.push_back({ m_u_param, &blurParams, 1 });

        GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        context.Submit(view, request);
    }
}
