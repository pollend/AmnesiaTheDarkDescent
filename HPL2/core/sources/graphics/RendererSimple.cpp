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

#include "graphics/Enum.h"
#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
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

#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "resources/Resources.h"

#include "scene/Camera.h"
#include "scene/RenderableContainer.h"
#include "scene/World.h"

namespace hpl
{

    bool cRendererSimple::mbUseShaders = true;

    cRendererSimple::cRendererSimple(cGraphics* apGraphics, cResources* apResources)
        : iRenderer("Simple", apGraphics, apResources, 0)
    {
        ////////////////////////////////////
        // Set up render specific things
        mbSetFrameBufferAtBeginRendering = true;
        mbClearFrameBufferAtBeginRendering = true;
    }

    cRendererSimple::~cRendererSimple()
    {
    }

    bool cRendererSimple::LoadData()
    {
        cParserVarContainer programVars;

        m_flatProgram = hpl::loadProgram("vs_simple_flat", "fs_simple_flat");
        m_diffuseProgram = hpl::loadProgram("vs_simple_diffuse", "fs_simple_diffuse");

        ////////////////////////
        // Z shader
        programVars.Clear();
        programVars.Add("UseUv");

        mpFlatProgram =
            mpGraphics->CreateGpuProgramFromShaders("DiffuseShader", "deferred_base_vtx.glsl", "deferred_base_frag.glsl", &programVars);

        ////////////////////////
        // Diffuse shader
        programVars.Clear();
        programVars.Add("UseUv");
        programVars.Add("UseNormals");
        programVars.Add("UseColor");
        programVars.Add("UseDiffuse");

        mpDiffuseProgram =
            mpGraphics->CreateGpuProgramFromShaders("DiffuseShader", "deferred_base_vtx.glsl", "deferred_base_frag.glsl", &programVars);

        return true;
    }

    //-----------------------------------------------------------------------

    void cRendererSimple::DestroyData()
    {
        mpGraphics->DestroyGpuProgram(mpFlatProgram);
        mpGraphics->DestroyGpuProgram(mpDiffuseProgram);
    }

    void cRendererSimple::CopyToFrameBuffer()
    {
        // Do Nothing
    }

    void cRendererSimple::SetupRenderList()
    {
        mpCurrentRenderList->Setup(mfCurrentFrameTime, mpCurrentFrustum);

        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), 0);
        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), 0);

        mpCurrentRenderList->Compile(
            eRenderListCompileFlag_Z | eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Decal | eRenderListCompileFlag_Translucent);
    }

    //-----------------------------------------------------------------------

    void cRendererSimple::Draw(
        GraphicsContext& context,
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        std::weak_ptr<RenderViewport> apRenderTarget,
        bool abSendFrameBufferToPostEffects,
        tRendererCallbackList* apCallbackList)
    {
        BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, apRenderTarget, abSendFrameBufferToPostEffects, apCallbackList);

        mpCurrentRenderList->Setup(mfCurrentFrameTime, mpCurrentFrustum);
        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), 0);
        CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), 0);
        mpCurrentRenderList->Compile(
            eRenderListCompileFlag_Z | eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Decal | eRenderListCompileFlag_Translucent);

        // clear the screen buffer at the beginning of the pass
        {
            auto view = context.StartPass("clear target");
            const RenderTarget& target = m_currentRenderTarget ? m_currentRenderTarget->GetRenderTarget() : RenderTarget::EmptyRenderTarget;
            GraphicsContext::DrawClear clear{
                target, { 0, 1.0f, 0, ClearOp::Color | ClearOp::Depth }, 0,
                0,      static_cast<uint16_t>(mvScreenSize.x),           static_cast<uint16_t>(mvScreenSize.y)
            };
            context.ClearTarget(view, clear);
        }

        // Z pre pass, render to z buffer
        (
            [&](bool active)
            {
                if (!active)
                {
                    return;
                }
                auto view = context.StartPass("z pre pass, render to z buffer");
                for (auto& obj : mpCurrentRenderList->GetRenderableItems(eRenderListType_Z))
                {
                    auto* pMaterial = obj->GetMaterial();
                    auto* alphaImage = pMaterial->GetImage(eMaterialTexture_Alpha);
                    auto* vertexBuffer = obj->GetVertexBuffer();
                    if (!pMaterial || !vertexBuffer)
                    {
                        continue;
                    }

                    GraphicsContext::ShaderProgram shaderInput;
                    if (alphaImage)
                    {
                        shaderInput.m_handle = m_diffuseProgram;
                    }
                    else
                    {
                        shaderInput.m_handle = m_flatProgram;
                    }
                    shaderInput.m_configuration.m_write = Write::Depth;
                    shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;

                    shaderInput.m_projection = *mpCurrentProjectionMatrix;
                    shaderInput.m_modelTransform = *obj->GetModelMatrixPtr();
                    shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();

                    GraphicsContext::LayoutStream layoutInput;
                    vertexBuffer->GetLayoutStream(layoutInput);

                    const RenderTarget& target =
                        m_currentRenderTarget ? m_currentRenderTarget->GetRenderTarget() : RenderTarget::EmptyRenderTarget;
                    GraphicsContext::DrawRequest drawRequest{ target, layoutInput, shaderInput };
                    drawRequest.m_width = mvScreenSize.x;
                    drawRequest.m_height = mvScreenSize.y;
                    context.Submit(view, drawRequest);
                }
            })(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Z));

        // diffuse pass, render to color buffer
        (
            [&](bool active)
            {
                if (!active)
                {
                    return;
                }

                auto view = context.StartPass("diffuse pass, render to color buffer");
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

                    shaderInput.m_projection = *mpCurrentProjectionMatrix;
                    shaderInput.m_modelTransform = *obj->GetModelMatrixPtr();
                    shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();
                    if (const auto* image = pMaterial->GetImage(eMaterialTexture_Diffuse))
                    {
                    }

                    const RenderTarget& target =
                        m_currentRenderTarget ? m_currentRenderTarget->GetRenderTarget() : RenderTarget::EmptyRenderTarget;
                    GraphicsContext::DrawRequest drawRequest{ target, layoutInput, shaderInput };
                    drawRequest.m_width = mvScreenSize.x;
                    drawRequest.m_height = mvScreenSize.y;
                    context.Submit(view, drawRequest);
                }
            })(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Diffuse));

        // Decal pass

        [&](bool active)
        {
            if (!active)
            {
                return;
            }
            auto view = context.StartPass("decal pass, render to color buffer");
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

                shaderInput.m_projection = *mpCurrentProjectionMatrix;
                shaderInput.m_modelTransform = *obj->GetModelMatrixPtr();
                shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();

                if (const auto* image = pMaterial->GetImage(eMaterialTexture_Diffuse))
                {
                    shaderInput.m_textures.push_back({ BGFX_INVALID_HANDLE, image->GetHandle(), 0 });
                }
                const RenderTarget& target =
                    m_currentRenderTarget ? m_currentRenderTarget->GetRenderTarget() : RenderTarget::EmptyRenderTarget;
                GraphicsContext::DrawRequest drawRequest{ target, layoutInput, shaderInput };
                drawRequest.m_width = mvScreenSize.x;
                drawRequest.m_height = mvScreenSize.y;
                context.Submit(view, drawRequest);
            }
        }(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Decal));

        RunCallback(eRendererMessage_PostSolid);

        // Trans Objects
        (
            [&](bool active)
            {
                if (!active)
                {
                    return;
                }

                auto view = context.StartPass("translucence pass, render to color buffer");
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

                    shaderInput.m_projection = *mpCurrentProjectionMatrix;
                    shaderInput.m_modelTransform = *obj->GetModelMatrixPtr();
                    shaderInput.m_view = mpCurrentFrustum->GetViewMatrix();

                    if (const auto* image = pMaterial->GetImage(eMaterialTexture_Diffuse))
                    {
                        shaderInput.m_textures.push_back({ BGFX_INVALID_HANDLE, image->GetHandle(), 0 });
                    }
                    const RenderTarget& target =
                        m_currentRenderTarget ? m_currentRenderTarget->GetRenderTarget() : RenderTarget::EmptyRenderTarget;
                    GraphicsContext::DrawRequest drawRequest{ target, layoutInput, shaderInput };
                    drawRequest.m_width = mvScreenSize.x;
                    drawRequest.m_height = mvScreenSize.y;
                    context.Submit(view, drawRequest);
                }
            })(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Translucent));

        RunCallback(eRendererMessage_PostTranslucent);
    }

    void cRendererSimple::RenderObjects()
    {
        START_RENDER_PASS(Simple);

        ////////////////////////////////////////////
        // Z pre pass, only render to z buffer
        {
            SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
            SetDepthTest(true);
            SetDepthWrite(true);
            SetBlendMode(eMaterialBlendMode_None);
            SetAlphaMode(eMaterialAlphaMode_Trans);
            SetChannelMode(eMaterialChannelMode_None); // This turns of all color writing
            SetTextureRange(NULL, 0);

            cRenderableVecIterator diffIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Z);
            while (diffIt.HasNext())
            {
                iRenderable* pObject = diffIt.Next();
                cMaterial* pMaterial = pObject->GetMaterial();
                iTexture* pTex = pMaterial->GetTexture(eMaterialTexture_Alpha);

                // If shaders and an alpha channel, use a program that handles textures.
                if (mbUseShaders)
                {
                    if (pTex)
                        SetProgram(mpDiffuseProgram);
                    else
                        SetProgram(mpFlatProgram);
                }

                SetTexture(0, pTex);

                SetMatrix(pObject->GetModelMatrixPtr());

                SetVertexBuffer(pObject->GetVertexBuffer());

                DrawCurrent();
            }
        }

        ////////////////////////////////////////////
        // Diffuse Objects
        {
            SetDepthTestFunc(
                eDepthTestFunc_Equal); // Setting equal here so that alpha works. It only draws on pixel that he Z prepass drew to.
            SetDepthWrite(false);
            SetChannelMode(eMaterialChannelMode_RGBA);
            SetAlphaMode(eMaterialAlphaMode_Solid);
            SetTextureRange(NULL, 0);

            if (mbUseShaders)
            {
                SetProgram(mpDiffuseProgram);
            }

            cRenderableVecIterator diffIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Diffuse);
            while (diffIt.HasNext())
            {
                iRenderable* pObject = diffIt.Next();
                cMaterial* pMaterial = pObject->GetMaterial();

                SetTexture(0, pMaterial->GetTexture(eMaterialTexture_Diffuse));

                SetMatrix(pObject->GetModelMatrixPtr());

                SetVertexBuffer(pObject->GetVertexBuffer());

                DrawCurrent();
            }
        }

        ////////////////////////////////////////////
        // Decal Objects
        {
            SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
            SetDepthWrite(false);

            if (mbUseShaders)
            {
                SetProgram(mpDiffuseProgram);
            }

            cRenderableVecIterator decalIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Decal);
            while (decalIt.HasNext())
            {
                iRenderable* pObject = decalIt.Next();
                cMaterial* pMaterial = pObject->GetMaterial();

                SetBlendMode(pMaterial->GetBlendMode());

                SetTexture(0, pMaterial->GetTexture(eMaterialTexture_Diffuse));

                SetMatrix(pObject->GetModelMatrixPtr());

                SetVertexBuffer(pObject->GetVertexBuffer());

                DrawCurrent();
            }
        }

        RunCallback(eRendererMessage_PostSolid);

        ////////////////////////////////////////////
        // Trans Objects
        {
            SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
            SetDepthWrite(false);

            if (mbUseShaders)
            {
                SetProgram(mpDiffuseProgram);
            }

            cRenderableVecIterator transIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Translucent);
            while (transIt.HasNext())
            {
                iRenderable* pObject = transIt.Next();
                cMaterial* pMaterial = pObject->GetMaterial();

                pObject->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime);

                SetBlendMode(pMaterial->GetBlendMode());

                SetTexture(0, pMaterial->GetTexture(eMaterialTexture_Diffuse));

                SetMatrix(pObject->GetModelMatrix(mpCurrentFrustum));

                SetVertexBuffer(pObject->GetVertexBuffer());

                DrawCurrent();
            }
        }

        RunCallback(eRendererMessage_PostTranslucent);

        END_RENDER_PASS();
    }

} // namespace hpl
