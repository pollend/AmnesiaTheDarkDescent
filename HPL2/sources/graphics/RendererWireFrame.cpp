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

#include "graphics/RendererWireFrame.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/ForgeHandles.h"
#include "graphics/RenderTarget.h"
#include "graphics/VertexBuffer.h"
#include "impl/LegacyVertexBuffer.h"
#include "math/Math.h"

#include "math/MathTypes.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/PreprocessParser.h"

#include "graphics/Graphics.h"
#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderable.h"
#include "graphics/RenderList.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/DebugDraw.h"

#include "resources/Resources.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/RenderableContainer.h"
#include <memory>

namespace hpl {

    cRendererWireFrame::cRendererWireFrame(cGraphics* apGraphics, cResources* apResources, std::shared_ptr<DebugDraw> debug)
        : iRenderer("WireFrame", apGraphics, apResources)
        , m_debug(debug) {
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        m_shader.Load(forgeRenderer->Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "wireframe.vert";
            loadDesc.mStages[1].pFileName = "wireframe.frag";
            addShader(forgeRenderer->Rend(), &loadDesc, shader);
            return true;
        });

        for (auto& buffer : m_objectUniformBuffer) {
            buffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = MaxObjectUniforms;
                desc.mDesc.mStructStride = sizeof(ObjectUniform);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });
        }
        for (auto& buffer : m_frameBufferUniform) {
            buffer.Load([&](Buffer** buffer) {
                BufferLoadDesc desc = {};
                desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
                desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                desc.mDesc.mFirstElement = 0;
                desc.mDesc.mElementCount = 1;
                desc.mDesc.mStructStride = sizeof(PerFrameUniform);
                desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
                desc.ppBuffer = buffer;
                addResource(&desc, nullptr);
                return true;
            });
        }

        m_rootSignature.Load(forgeRenderer->Rend(), [&](RootSignature** sig) {
            std::array shaders = { m_shader.m_handle };
            RootSignatureDesc rootSignatureDesc = {};
            // const char* pStaticSamplers[] = { "depthSampler" };
            // rootSignatureDesc.mStaticSamplerCount = 1;
            // rootSignatureDesc.ppStaticSamplers = &m_bilinearSampler.m_handle;
            // rootSignatureDesc.ppStaticSamplerNames = pStaticSamplers;
            rootSignatureDesc.ppShaders = shaders.data();
            rootSignatureDesc.mShaderCount = shaders.size();
            addRootSignature(forgeRenderer->Rend(), &rootSignatureDesc, sig);
            return true;
        });
        m_frameDescriptorSet.Load(forgeRenderer->Rend(), [&](DescriptorSet** set) {
            DescriptorSetDesc setDesc = { m_rootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, NumberOfPerFrameUniforms };
            addDescriptorSet(forgeRenderer->Rend(), &setDesc, set);
            return true;
        });
        for (uint32_t i = 0; i < NumberOfPerFrameUniforms; i++) {
            std::array<DescriptorData, 1> params = {};
            params[0].pName = "perFrameConstants";
            params[0].ppBuffers = &m_frameBufferUniform[i].m_handle;
            updateDescriptorSet(forgeRenderer->Rend(), i, m_frameDescriptorSet.m_handle, params.size(), params.data());
        }

        m_constDescriptorSet.Load(forgeRenderer->Rend(), [&](DescriptorSet** set) {
            DescriptorSetDesc setDesc = { m_rootSignature.m_handle, DESCRIPTOR_UPDATE_FREQ_NONE, ForgeRenderer::SwapChainLength };
            addDescriptorSet(forgeRenderer->Rend(), &setDesc, set);
            return true;
        });
        for (uint32_t i = 0; i < ForgeRenderer::SwapChainLength; i++) {
            std::array<DescriptorData, 1> params = {};
            params[0].pName = "uniformObjectBuffer";
            params[0].ppBuffers = &m_objectUniformBuffer[i].m_handle;
            updateDescriptorSet(forgeRenderer->Rend(), i, m_constDescriptorSet.m_handle, params.size(), params.data());
        }

        m_pipeline.Load(forgeRenderer->Rend(), [&](Pipeline** pipline) {
            std::array colorFormats = { TinyImageFormat_R8G8B8A8_UNORM };
            VertexLayout vertexLayout = {};
#ifndef USE_THE_FORGE_LEGACY
            vertexLayout.mBindingCount = 1;
            vertexLayout.mBindings[0].mStride = sizeof(float3);
#endif
            vertexLayout.mAttribCount = 1;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_BOTH;
            rasterizerStateDesc.mFillMode = FILL_MODE_WIREFRAME;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = false;
            depthStateDesc.mDepthWrite = false;

            BlendStateDesc blendStateDesc{};
            blendStateDesc.mSrcFactors[0] = BC_ONE;
            blendStateDesc.mDstFactors[0] = BC_ZERO;
            blendStateDesc.mBlendModes[0] = BM_ADD;
            blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
            blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
            blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
#ifdef USE_THE_FORGE_LEGACY
            blendStateDesc.mMasks[0] = RED | GREEN | BLUE;
#else
            blendStateDesc.mColorWriteMasks[0] = ColorMask::COLOR_MASK_RED | ColorMask::COLOR_MASK_GREEN | ColorMask::COLOR_MASK_BLUE;
#endif
            blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
            blendStateDesc.mIndependentBlend = false;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pBlendState = nullptr;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.pBlendState = &blendStateDesc;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_rootSignature.m_handle;
            pipelineSettings.pShaderProgram = m_shader.m_handle;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &vertexLayout;
            addPipeline(forgeRenderer->Rend(), &pipelineDesc, pipline);
            return true;
        });
    }

    cRendererWireFrame::~cRendererWireFrame() {
    }

    bool cRendererWireFrame::LoadData() {
        return true;
    }

    void cRendererWireFrame::DestroyData() {
    }

    uint32_t cRendererWireFrame::prepareObjectData(const ForgeRenderer::Frame& frame, iRenderable* apObject) {
        auto objectLookup = m_objectDescriptorLookup.find(apObject);
        const bool isFound = objectLookup != m_objectDescriptorLookup.end();
        uint32_t index = isFound ? objectLookup->second : m_objectIndex++;
        if (!isFound) {
            cMatrixf modelMat = apObject->GetModelMatrixPtr() ? *apObject->GetModelMatrixPtr() : cMatrixf::Identity;

            BufferUpdateDesc updateDesc = { m_objectUniformBuffer[frame.m_frameIndex].m_handle, sizeof(ObjectUniform) * index };
            ObjectUniform uniformObjectData;
            uniformObjectData.m_modelMat = cMath::ToForgeMat4(modelMat.GetTranspose());
            beginUpdateResource(&updateDesc);
            (*reinterpret_cast<ObjectUniform*>(updateDesc.pMappedData)) = uniformObjectData;
            endUpdateResource(&updateDesc, NULL);

            m_objectDescriptorLookup[apObject] = index;
        }

        return index;
    }
    SharedRenderTarget cRendererWireFrame::GetOutputImage(uint32_t frameIndex, cViewport& viewport) {
        auto sharedData = m_boundViewportData.resolve(viewport);
        if (!sharedData) {
            return SharedRenderTarget();
        }
        return sharedData->m_outputBuffer[frameIndex];
    }

    void cRendererWireFrame::Draw(
        Cmd* cmd,
        const ForgeRenderer::Frame& frame,
        cViewport& viewport,
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects) {
        auto* forgeRenderer = Interface<ForgeRenderer>::Get();
        // keep around for the moment ...
        BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects);

        if (frame.m_currentFrame != m_activeFrame) {
            m_objectIndex = 0;
            m_objectDescriptorLookup.clear();
            m_activeFrame = frame.m_currentFrame;
        }
        auto common = m_boundViewportData.resolve(viewport);
        if (!common || common->m_size != viewport.GetSize()) {
            auto viewportData = std::make_unique<ViewportData>();
            viewportData->m_size = viewport.GetSize();
            for (auto& buffer : viewportData->m_outputBuffer) {
                buffer.Load(forgeRenderer->Rend(), [&](RenderTarget** target) {
                    ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                    RenderTargetDesc renderTargetDesc = {};
                    renderTargetDesc.mArraySize = 1;
                    renderTargetDesc.mClearValue = optimizedColorClearBlack;
                    renderTargetDesc.mDepth = 1;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    renderTargetDesc.mWidth = viewportData->m_size.x;
                    renderTargetDesc.mHeight = viewportData->m_size.y;
                    renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
                    renderTargetDesc.mSampleQuality = 0;
                    renderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                    renderTargetDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                    renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                    addRenderTarget(forgeRenderer->Rend(), &renderTargetDesc, target);
                    return true;
                });
            }

            common = m_boundViewportData.update(viewport, std::move(viewportData));
        }

        Matrix4 correctionMatrix =
            Matrix4(
                Vector4(1.0f, 0, 0, 0),
                Vector4(0, 1.0f, 0, 0),
                Vector4(0, 0, 0.5f, 0),
                Vector4(0, 0, 0.5f, 1.0f)
            );

        const Matrix4 matMainFrustumView = cMath::ToForgeMat4(apFrustum->GetViewMatrix().GetTranspose());
        const Matrix4 matMainFrustumProj = cMath::ToForgeMat4(apFrustum->GetProjectionMatrix().GetTranspose());

        m_rendererList.BeginAndReset(afFrameTime, apFrustum);
        std::array<cPlanef, 0> occlusionPlanes = {};
        rendering::detail::UpdateRenderListWalkAllNodesTestFrustumAndVisibility(
            &m_rendererList, apFrustum, apWorld->GetRenderableContainer(eWorldContainerType_Static), occlusionPlanes, 0);
        rendering::detail::UpdateRenderListWalkAllNodesTestFrustumAndVisibility(
            &m_rendererList, apFrustum, apWorld->GetRenderableContainer(eWorldContainerType_Dynamic), occlusionPlanes, 0);

        m_rendererList.End(eRenderListCompileFlag_Diffuse | eRenderListCompileFlag_Decal | eRenderListCompileFlag_Translucent);

        m_perFrameIndex = (m_perFrameIndex + 1) % NumberOfPerFrameUniforms;
        {
            BufferUpdateDesc updateDesc = { m_frameBufferUniform[m_perFrameIndex].m_handle, 0, sizeof(PerFrameUniform) };
            beginUpdateResource(&updateDesc);
            PerFrameUniform uniformData;
            uniformData.m_viewProject = ((apFrustum->GetProjectionType() == eProjectionType_Perspective ? Matrix4::identity() : correctionMatrix) * matMainFrustumProj) * matMainFrustumView ;
            (*reinterpret_cast<PerFrameUniform*>(updateDesc.pMappedData)) = uniformData;
            endUpdateResource(&updateDesc, NULL);
        }

        {
            cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{ common->m_outputBuffer[frame.m_frameIndex].m_handle,
                                     RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET,
                                },
            };
            cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        uint32_t rootConstantIndex = getDescriptorIndexFromName(m_rootSignature.m_handle, "rootConstant");
        std::array targets = {
            common->m_outputBuffer[frame.m_frameIndex].m_handle,
        };
        LoadActionsDesc loadActions = {};
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
        loadActions.mClearColorValues[0] =  ClearValue { .r = apSettings->mClearColor.r, .g = apSettings->mClearColor.g, .b = apSettings->mClearColor.b, .a = apSettings->mClearColor.a };
        cmdBindRenderTargets(frame.m_cmd, targets.size(), targets.data(), NULL, &loadActions, NULL, NULL, -1, -1);
        cmdSetViewport(frame.m_cmd, 0.0f, 0.0f, static_cast<float>(common->m_size.x), static_cast<float>(common->m_size.y), 0.0f, 1.0f);
        cmdSetScissor(frame.m_cmd, 0, 0, common->m_size.x, common->m_size.y);

        cmdBindPipeline(frame.m_cmd, m_pipeline.m_handle);
        cmdBindDescriptorSet(frame.m_cmd, m_perFrameIndex, m_frameDescriptorSet.m_handle);
        cmdBindDescriptorSet(frame.m_cmd, frame.m_frameIndex, m_constDescriptorSet.m_handle);
        for (auto& diffuseItem : m_rendererList.GetRenderableItems(eRenderListType_Diffuse)) {
            cMaterial* pMaterial = diffuseItem->GetMaterial();
            iVertexBuffer* vertexBuffer = diffuseItem->GetVertexBuffer();
            if (pMaterial == nullptr || vertexBuffer == nullptr) {
                continue;
            }
            ASSERT(pMaterial->Descriptor().m_id == MaterialID::SolidDiffuse && "Invalid material type");
            std::array targets = { eVertexBufferElement_Position };
            LegacyVertexBuffer::GeometryBinding binding;
            static_cast<LegacyVertexBuffer*>(vertexBuffer)->resolveGeometryBinding(frame.m_currentFrame, targets, &binding);

            LegacyVertexBuffer::cmdBindGeometry(frame.m_cmd, frame.m_resourcePool, binding);
            uint32_t objectIndex = prepareObjectData(frame, diffuseItem);
            cmdBindPushConstants(frame.m_cmd, m_rootSignature.m_handle, rootConstantIndex, &objectIndex);
            cmdDrawIndexed(frame.m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
        }

        cViewport::PostSolidDrawPacket postSolidEvent = cViewport::PostSolidDrawPacket();
        postSolidEvent.m_frustum = apFrustum;
        postSolidEvent.m_frame = &frame;
        postSolidEvent.m_outputTarget = &common->m_outputBuffer[frame.m_frameIndex];
        postSolidEvent.m_viewport = &viewport;
        postSolidEvent.m_renderSettings = mpCurrentSettings;
        postSolidEvent.m_debug = m_debug.get();
        viewport.SignalDraw(postSolidEvent);
        SharedRenderTarget invalidTarget;
        m_debug->flush(frame, frame.m_cmd, viewport, *apFrustum, common->m_outputBuffer[frame.m_frameIndex], invalidTarget);

        cViewport::PostTranslucenceDrawPacket translucenceEvent = cViewport::PostTranslucenceDrawPacket();
        translucenceEvent.m_frustum = apFrustum;
        translucenceEvent.m_frame = &frame;
        translucenceEvent.m_outputTarget = &common->m_outputBuffer[frame.m_frameIndex];
        translucenceEvent.m_viewport = &viewport;
        translucenceEvent.m_renderSettings = mpCurrentSettings;
        translucenceEvent.m_debug = m_debug.get();
        viewport.SignalDraw(translucenceEvent);
        m_debug->flush(frame, frame.m_cmd, viewport, *apFrustum, common->m_outputBuffer[frame.m_frameIndex], invalidTarget);

        {
            cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{ common->m_outputBuffer[frame.m_frameIndex].m_handle,
                                      RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE,
                                },
            };
            cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        // mpCurrentRenderList->Setup(mfCurrentFrameTime,apFrustum);

        // rendering::detail::UpdateRenderListWalkAllNodesTestFrustumAndVisibility(
        // 	mpCurrentRenderList, apFrustum, mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static),
        // mvCurrentOcclusionPlanes, 0); rendering::detail::UpdateRenderListWalkAllNodesTestFrustumAndVisibility( 	mpCurrentRenderList,
        // apFrustum, mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), mvCurrentOcclusionPlanes, 0);

        // mpCurrentRenderList->Compile(	eRenderListCompileFlag_Diffuse |
        // 								eRenderListCompileFlag_Decal |
        // 								eRenderListCompileFlag_Translucent);

        // // START_RENDER_PASS(WireFrame);

        // ////////////////////////////////////////////
        // // Diffuse Objects
        // // SetDepthTest(true);
        // // SetDepthWrite(true);
        // // SetBlendMode(eMaterialBlendMode_None);
        // // SetAlphaMode(eMaterialAlphaMode_Solid);
        // // SetChannelMode(eMaterialChannelMode_RGBA);

        // // SetTextureRange(NULL,0);

        // int lCount =0;

        // GraphicsContext::ViewConfiguration viewConfig {rt};
        // viewConfig.m_projection = apFrustum->GetProjectionMatrix().GetTranspose();
        // viewConfig.m_view = apFrustum->GetViewMatrix().GetTranspose();
        // viewConfig.m_viewRect = {0, 0, viewport.GetSize().x, viewport.GetSize().y};
        // auto view = context.StartPass("Wireframe", viewConfig);
        // for(auto& pObject: mpCurrentRenderList->GetRenderableItems(eRenderListType_Diffuse))
        // {
        // 	GraphicsContext::LayoutStream layoutStream;
        //     GraphicsContext::ShaderProgram shaderProgram;

        // 	if(pObject == nullptr)
        // 	{
        // 		continue;
        // 	}

        // 	iVertexBuffer* vertexBuffer = pObject->GetVertexBuffer();
        // 	if(vertexBuffer == nullptr) {
        // 		continue;
        // 	}

        // 	struct {
        // 		float m_r;
        // 		float m_g;
        // 		float m_b;
        // 		float m_a;
        // 	} color = { 1.0f, 1.0f,1.0f ,1.0f };

        // 	shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
        // 	shaderProgram.m_configuration.m_write = Write::RGBA;
        // 	shaderProgram.m_configuration.m_cull = Cull::None;
        // 	shaderProgram.m_handle = m_colorProgram;

        // 	shaderProgram.m_uniforms.push_back({ m_u_color, &color });
        // 	shaderProgram.m_modelTransform = pObject->GetModelMatrixPtr() ?  pObject->GetModelMatrixPtr()->GetTranspose() :
        // cMatrixf::Identity.GetTranspose();

        // 	vertexBuffer->GetLayoutStream(layoutStream, eVertexBufferDrawType_LineStrip);
        // 	GraphicsContext::DrawRequest drawRequest {layoutStream, shaderProgram};
        // 	context.Submit(view, drawRequest);

        // 	lCount++;
        // }

        // ////////////////////////////////////////////
        // // Decal Objects
        // // SetDepthWrite(false);

        // for(auto& pObject: mpCurrentRenderList->GetRenderableItems(eRenderListType_Decal))
        // {
        // 	cMaterial *pMaterial = pObject->GetMaterial();

        // 	// SetBlendMode(pMaterial->GetBlendMode());

        // 	// SetTexture(0,pMaterial->GetTexture(eMaterialTexture_Diffuse));

        // 	// SetMatrix(pObject->GetModelMatrixPtr());

        // 	// SetVertexBuffer(pObject->GetVertexBuffer());

        // 	// DrawCurrent(eVertexBufferDrawType_LineStrip);
        // }

        // // RunCallback(eRendererMessage_PostSolid);

        // ////////////////////////////////////////////
        // // Trans Objects
        // // SetDepthWrite(false);

        // for(auto& pObject: mpCurrentRenderList->GetRenderableItems(eRenderListType_Translucent))
        // {
        // 	cMaterial *pMaterial = pObject->GetMaterial();

        // 	pObject->UpdateGraphicsForViewport(apFrustum, mfCurrentFrameTime);

        // 	// SetBlendMode(pMaterial->GetBlendMode());

        // 	// SetTexture(0,pMaterial->GetTexture(eMaterialTexture_Diffuse));

        // 	// SetMatrix(pObject->GetModelMatrix(mpCurrentFrustum));

        // 	// SetVertexBuffer(pObject->GetVertexBuffer());

        // 	// DrawCurrent(eVertexBufferDrawType_LineStrip);
        // }

        // // RunCallback(eRendererMessage_PostTranslucent);

        // END_RENDER_PASS();
    }

} // namespace hpl
