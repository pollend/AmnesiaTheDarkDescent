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

#include "graphics/RendererSimple.h"

#include "bgfx/bgfx.h"
#include "graphics/Enum.h"
#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/RenderTarget.h"
#include "graphics/ShaderUtil.h"
#include "graphics/VertexBuffer.h"
#include "math/Math.h"

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/String.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/Mesh.h"
#include "graphics/RenderList.h"
#include "graphics/Renderable.h"
#include "graphics/SubMesh.h"
#include "graphics/Texture.h"
#include <graphics/RenderViewport.h>
#include <graphics/GraphicsContext.h>

#include "graphics/GPUShader.h"
#include "resources/Resources.h"

#include "scene/Camera.h"
#include "scene/RenderableContainer.h"
#include "scene/World.h"

namespace hpl
{

    cRendererSimple::cRendererSimple(cGraphics* apGraphics, cResources* apResources)
        : iRenderer("Simple", apGraphics, apResources, 0)
    {
        mbSetFrameBufferAtBeginRendering = true;
        mbClearFrameBufferAtBeginRendering = true;
    }

    cRendererSimple::~cRendererSimple()
    {
    }

    bool cRendererSimple::LoadData()
    {
        m_s_diffuseMap = bgfx::createUniform("s_diffuseMap", bgfx::UniformType::Enum::Sampler);
        m_flatProgram = hpl::loadProgram("vs_simple_flat", "fs_simple_flat");
        m_diffuseProgram = hpl::loadProgram("vs_simple_diffuse", "fs_simple_diffuse");
        return true;
    }

    void cRendererSimple::DestroyData()
    {
        if(bgfx::isValid(m_flatProgram)) {
            bgfx::destroy(m_flatProgram);
        }
        if(bgfx::isValid(m_diffuseProgram)) {
            bgfx::destroy(m_diffuseProgram);
        }
    }

    
    void cRendererSimple::Draw(
        GraphicsContext& context,
        cViewport& viewport,
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects,
        tRendererCallbackList* apCallbackList)
    {
        auto screenSize = viewport.GetSize();
        mpCurrentRenderList->Setup(mfCurrentFrameTime, mpCurrentFrustum);

        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), 0);
        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), 0);

        mpCurrentRenderList->Compile(
            eRenderListCompileFlag_Z | eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Decal | eRenderListCompileFlag_Translucent);

		cRendererCallbackFunctions handler(context, viewport, this);

        BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects, apCallbackList);
        // auto target = m_currentRenderTarget.GetRenderTarget();
        // auto& outputTarget = (target && target->IsValid()) ? *target : RenderTarget::EmptyRenderTarget;

        mpCurrentRenderList->Setup(mfCurrentFrameTime, mpCurrentFrustum);
        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), 0);
        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), 0);
        mpCurrentRenderList->Compile(
            eRenderListCompileFlag_Z | eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Decal | eRenderListCompileFlag_Translucent);

        // clear the screen buffer at the beginning of the pass
        [&](bool active)
        {
            if (!active)
            {
                return;
            }
            GraphicsContext::ViewConfiguration viewConfig {viewport.GetRenderTarget()};
			viewConfig.m_viewRect = {0, 0, screenSize.x, screenSize.y};
			viewConfig.m_clear = {0, 1, 0, ClearOp::Color | ClearOp::Depth};
			bgfx::touch(context.StartPass("clear target", viewConfig));
		
            // auto view = context.StartPass("clear target");
            
            // GraphicsContext::DrawClear clear{
            //     outputTarget, { 0, 1.0f, 0, ClearOp::Color | ClearOp::Depth }, 0,
            //     0,      static_cast<uint16_t>(screenSize.x),           static_cast<uint16_t>(screenSize.y)
            // };
            // context.ClearTarget(view, clear);
        }(true);

        // Z pre pass, render to z buffer

        [&](bool active)
        {
            if (!active)
            {
                return;
            }
            GraphicsContext::ViewConfiguration viewConfiguration {viewport.GetRenderTarget()};
		    viewConfiguration.m_viewRect = cRect2l(0, 0, screenSize.x, screenSize.y);
            viewConfiguration.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
            viewConfiguration.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
            auto view = context.StartPass("z pre pass, render to z buffer", viewConfiguration );
            for (auto& obj : mpCurrentRenderList->GetRenderableItems(eRenderListType_Z))
            {
                auto* pMaterial = obj->GetMaterial();
                auto* vertexBuffer = obj->GetVertexBuffer();
                if (!pMaterial || !vertexBuffer)
                {
                    continue;
                }

                GraphicsContext::ShaderProgram shaderInput;
                shaderInput.m_handle = m_flatProgram;
                shaderInput.m_configuration.m_write = Write::Depth;
                shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;

                // shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
                shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
                // shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();

                GraphicsContext::LayoutStream layoutInput;
                vertexBuffer->GetLayoutStream(layoutInput);

                GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                context.Submit(view, drawRequest);
            }
        }(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Z));

        // diffuse pass, render to color buffer

        [&](bool active)
        {
            if (!active)
            {
                return;
            }

            GraphicsContext::ViewConfiguration viewConfiguration {viewport.GetRenderTarget()};
		    viewConfiguration.m_viewRect = cRect2l(0, 0, screenSize.x, screenSize.y);
            viewConfiguration.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
            viewConfiguration.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
            auto view = context.StartPass("diffuse pass, render to color buffer", viewConfiguration);
            for (auto& obj : mpCurrentRenderList->GetRenderableItems(eRenderListType_Diffuse))
            {
                auto* pMaterial = obj->GetMaterial();
                auto* vertexBuffer = obj->GetVertexBuffer();
                if (!pMaterial || !vertexBuffer)
                {
                    continue;
                }
                GraphicsContext::ShaderProgram shaderInput;
                GraphicsContext::LayoutStream layoutInput;
                vertexBuffer->GetLayoutStream(layoutInput);

                shaderInput.m_handle = m_diffuseProgram;

                shaderInput.m_configuration.m_write = Write::RGBA;
                shaderInput.m_configuration.m_depthTest = DepthTest::Equal;

                // shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
                shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
                // shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();
                if (const auto* image = pMaterial->GetImage(eMaterialTexture_Diffuse))
                {
                    shaderInput.m_textures.push_back({m_s_diffuseMap, image->GetHandle(), 0});
                }

                GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                context.Submit(view, drawRequest);
            }
        }(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Diffuse));

        // Decal pass

        [&](bool active)
        {
            if (!active)
            {
                return;
            }

            GraphicsContext::ViewConfiguration viewConfiguration {viewport.GetRenderTarget()};
		    viewConfiguration.m_viewRect = cRect2l(0, 0, screenSize.x, screenSize.y);
            viewConfiguration.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
            viewConfiguration.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
            auto view = context.StartPass("decal pass, render to color buffer", viewConfiguration);
            for (auto& obj : mpCurrentRenderList->GetRenderableItems(eRenderListType_Decal))
            {
                auto* pMaterial = obj->GetMaterial();
                auto* vertexBuffer = obj->GetVertexBuffer();
                if (!pMaterial || !vertexBuffer)
                {
                    continue;
                }

                GraphicsContext::ShaderProgram shaderInput;
                GraphicsContext::LayoutStream layoutInput;
                vertexBuffer->GetLayoutStream(layoutInput);

                shaderInput.m_handle = m_diffuseProgram;

                shaderInput.m_configuration.m_write = Write::RGBA;
                shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
                shaderInput.m_configuration.m_rgbBlendFunc =
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);

                // shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
                shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
                // shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();

                if (const auto* image = pMaterial->GetImage(eMaterialTexture_Diffuse))
                {
                    shaderInput.m_textures.push_back({m_s_diffuseMap, image->GetHandle(), 0});
                }
                GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                context.Submit(view, drawRequest);
            }
        }(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Decal));

        RunCallback(eRendererMessage_PostSolid, handler);

        // Trans Objects
        [&](bool active)
        {
            if (!active)
            {
                return;
            }

            GraphicsContext::ViewConfiguration viewConfiguration {viewport.GetRenderTarget()};
		    viewConfiguration.m_viewRect = cRect2l(0, 0, screenSize.x, screenSize.y);
            viewConfiguration.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
            viewConfiguration.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
            auto view = context.StartPass("translucence pass, render to color buffer", viewConfiguration);
            for (auto& obj : mpCurrentRenderList->GetRenderableItems(eRenderListType_Translucent))
            {
                auto* pMaterial = obj->GetMaterial();
                auto* vertexBuffer = obj->GetVertexBuffer();
                if (!pMaterial || !vertexBuffer)
                {
                    continue;
                }

                GraphicsContext::ShaderProgram shaderInput;
                GraphicsContext::LayoutStream layoutInput;
                vertexBuffer->GetLayoutStream(layoutInput);

                shaderInput.m_handle = m_diffuseProgram;

                shaderInput.m_configuration.m_write = Write::RGBA;
                shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
                shaderInput.m_configuration.m_rgbBlendFunc =
                    CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);

                // shaderInput.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
                shaderInput.m_modelTransform = obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
                // shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();

                if (const auto* image = pMaterial->GetImage(eMaterialTexture_Diffuse))
                {
                    shaderInput.m_textures.push_back({ BGFX_INVALID_HANDLE, image->GetHandle(), 0 });
                }
                GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                context.Submit(view, drawRequest);
            }
        }(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Translucent));

        RunCallback(eRendererMessage_PostTranslucent, handler);
    }

} // namespace hpl
