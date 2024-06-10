#include <graphics/ForgeRenderer.h>

#include "Common_3/OS/Interfaces/IOperatingSystem.h"
#include "engine/IUpdateEventLoop.h"
#include "engine/Interface.h"
#include "graphics/Material.h"
#include "windowing/NativeWindow.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/RingBuffer.h"

#ifdef HPL2_RENDERDOC_ENABLED
#ifdef WIN32
#else
    #include <dlfcn.h>
#endif
#include "renderdoc_app.h"
#endif

namespace hpl {

    void ForgeRenderer::IncrementFrame() {
        if (!m_frame.m_isFinished) {
            return;
        }
        m_frame.m_isFinished = false;


        // Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
        // m_resourcePoolIndex = (m_resourcePoolIndex + 1) % ResourcePoolSize;
        //m_currentFrameCount++;
        acquireNextImage(m_renderer, m_swapChain.m_handle, m_imageAcquiredSemaphore, NULL, &m_frame.m_swapChainIndex);
        m_frame.m_cmdRingElement = getNextGpuCmdRingElement(&m_graphicsCmdRing, true, 1);
        FenceStatus fenceStatus;
        getFenceStatus(m_renderer, m_frame.m_cmdRingElement.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(m_renderer, 1, &m_frame.m_cmdRingElement.pFence);

        m_frame.m_currentFrame++;
        m_frame.m_frameIndex = (m_frame.m_frameIndex + 1) % ForgeRenderer::SwapChainLength;
        //m_frame.m_swapChainTarget = m_swapChain.m_handle->ppRenderTargets[m_frame.m_swapChainIndex];
        m_frame.m_finalRenderTarget = m_finalRenderTarget[m_frame.index()].m_handle;
        m_frame.m_resourcePool = &m_resourcePool[m_frame.index()];
        m_frame.m_cmd = m_frame.cmd();
        m_frame.m_renderer = this;


        resetCmdPool(m_renderer, m_frame.m_cmdRingElement.pCmdPool);
        m_frame.m_resourcePool->ResetPool();
        beginCmd(m_frame.cmd());

        std::array rtBarriers = {
            RenderTargetBarrier { m_frame.finalTarget(), RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
        };

        m_frame.pushResource(m_finalRenderTarget[m_frame.index()]);
        cmdResourceBarrier(m_frame.cmd(), 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
    }

    void ForgeRenderer::SubmitFrame() {
        RenderTarget* swapChainTarget = m_swapChain.m_handle->ppRenderTargets[m_frame.m_swapChainIndex];
        {
            cmdBindRenderTargets(m_frame.cmd(), NULL);
            std::array rtBarriers = {
                RenderTargetBarrier { swapChainTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
                RenderTargetBarrier{ m_frame.finalTarget(), RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE},
            };
            cmdResourceBarrier(m_frame.cmd(), 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
        {

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { swapChainTarget, LOAD_ACTION_LOAD };
            cmdBindRenderTargets(m_frame.cmd(), &bindRenderTargets);
            uint32_t rootConstantIndex = getDescriptorIndexFromName(m_finalRootSignature , "uRootConstants");

            cmdSetViewport(m_frame.cmd(), 0.0f, 0.0f, m_frame.finalTarget()->mWidth, m_frame.finalTarget()->mHeight, 0.0f, 1.0f);
            cmdSetScissor(m_frame.cmd(), 0, 0, m_frame.finalTarget()->mWidth, m_frame.finalTarget()->mHeight);
            cmdBindPipeline(m_frame.cmd(), m_finalPipeline.m_handle);
            cmdBindPushConstants(m_frame.cmd(), m_finalRootSignature, rootConstantIndex, &m_gamma);

            std::array params = {
              DescriptorData {.pName = "sourceInput", .ppTextures = &m_frame.finalTarget()->pTexture}
            };
            updateDescriptorSet(m_renderer, 0, m_finalPerFrameDescriptorSet[m_frame.index()].m_handle, params.size(), params.data());
            cmdBindDescriptorSet(m_frame.cmd(), 0, m_finalPerFrameDescriptorSet[m_frame.index()].m_handle);
            cmdDraw(m_frame.cmd(), 3, 0);
        }
        {
            cmdBindRenderTargets(m_frame.cmd(), NULL);
            std::array rtBarriers = {
                RenderTargetBarrier{ swapChainTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT },
            };
            cmdResourceBarrier(m_frame.cmd(), 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        }
		FlushResourceUpdateDesc flushUpdateDesc = {};
		flushUpdateDesc.mNodeIndex = 0;
		flushResourceUpdates(&flushUpdateDesc);
        endCmd(m_frame.cmd());

        m_frame.m_waitSemaphores.push_back(flushUpdateDesc.pOutSubmittedSemaphore);
        m_frame.m_waitSemaphores.push_back(m_imageAcquiredSemaphore);

        QueueSubmitDesc submitDesc = {};
        submitDesc.mWaitSemaphoreCount = m_frame.m_waitSemaphores.size();
        submitDesc.ppWaitSemaphores = m_frame.m_waitSemaphores.data();
        submitDesc.mCmdCount = 1;
        submitDesc.ppCmds = m_frame.RingElement().pCmds;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.ppSignalSemaphores = &m_frame.RingElement().pSemaphore;
        submitDesc.pSignalFence = m_frame.RingElement().pFence;
        queueSubmit(m_graphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = m_frame.m_swapChainIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = m_swapChain.m_handle;
        presentDesc.ppWaitSemaphores = &m_frame.RingElement().pSemaphore;
        presentDesc.mSubmitDone = true;
        queuePresent(m_graphicsQueue, &presentDesc);

        m_frame.m_waitSemaphores.clear();
        m_frame.m_isFinished = true;
    }

    void ForgeRenderer::InitializeRenderer(window::NativeWindowWrapper* window) {
        m_window = window;
        SyncToken token = {};
        #ifdef HPL2_RENDERDOC_ENABLED

            static RENDERDOC_API_1_1_2* rdoc_api = NULL;
            #ifdef WIN32
                if (HMODULE mod = LoadLibrary("renderdoc.dll")) {
                    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
                    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
                    assert(ret == 1);
                }
            #else
                if (void* mod = dlopen("./librenderdoc.so", RTLD_NOW)) {
                    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
                    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
                    assert(ret == 1);
                }
            #endif

#endif

                RendererDesc settings;
                memset(&settings, 0, sizeof(settings));
                initRenderer("HPL2", &settings, &m_renderer);

                QueueDesc queueDesc = {};
                queueDesc.mType = QUEUE_TYPE_GRAPHICS;
                queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
                addQueue(m_renderer, &queueDesc, &m_graphicsQueue);

                GpuCmdRingDesc cmdRingDesc = {};
                cmdRingDesc.pQueue = m_graphicsQueue;
                cmdRingDesc.mPoolCount = SwapChainLength;
                cmdRingDesc.mCmdPerPoolCount = 1;
                cmdRingDesc.mAddSyncPrimitives = true;
                addGpuCmdRing(m_renderer, &cmdRingDesc, &m_graphicsCmdRing);

                WindowHandle winHandle = m_window->ForgeWindowHandle();
                m_swapChainCount = getRecommendedSwapchainImageCount(m_renderer, &winHandle);
                const auto windowSize = window->GetWindowSize();
                m_swapChain.Load(m_renderer, [&](SwapChain** handle) {
                    SwapChainDesc swapChainDesc = {};
                    swapChainDesc.mWindowHandle = winHandle;
                    swapChainDesc.mPresentQueueCount = 1;
                    swapChainDesc.ppPresentQueues = &m_graphicsQueue;
                    swapChainDesc.mWidth = windowSize.x;
                    swapChainDesc.mHeight = windowSize.y;
                    swapChainDesc.mImageCount = m_swapChainCount;
                    swapChainDesc.mColorFormat = TinyImageFormat_R8G8B8A8_UNORM; // getRecommendedSwapchainFormat(false, false);
                    swapChainDesc.mColorSpace = COLOR_SPACE_SDR_LINEAR;
                    swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
                    swapChainDesc.mEnableVsync = false;
                    addSwapChain(m_renderer, &swapChainDesc, handle);
                    return true;
                });
                RootSignatureDesc graphRootDesc = {};
                addRootSignature(m_renderer, &graphRootDesc, &m_pipelineSignature);

                addSemaphore(m_renderer, &m_imageAcquiredSemaphore);
                for (auto& rt : m_finalRenderTarget) {
                    rt.Load(m_renderer, [&](RenderTarget** target) {
                        RenderTargetDesc renderTarget = {};
                        renderTarget.mArraySize = 1;
                        renderTarget.mClearValue.depth = 1.0f;
                        renderTarget.mDepth = 1;
                        renderTarget.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                        renderTarget.mWidth = windowSize.x;
                        renderTarget.mHeight = windowSize.y;
                        renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                        renderTarget.mSampleCount = SAMPLE_COUNT_1;
                        renderTarget.mSampleQuality = 0;
                        renderTarget.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                        renderTarget.pName = "final output RT";
                        addRenderTarget(m_renderer, &renderTarget, target);
                        return true;
                    });
                }
                m_pointSampler.Load(m_renderer, [&](Sampler** sampler) {
                    SamplerDesc samplerDesc = {};
                    addSampler(m_renderer, &samplerDesc, sampler);
                    return true;
                });

                {
                    m_finalShader.Load(m_renderer, [&](Shader** shader) {
                        ShaderLoadDesc shaderLoadDesc = {};
                        shaderLoadDesc.mStages[0].pFileName = "fullscreen.vert";
                        shaderLoadDesc.mStages[1].pFileName = "final_posteffect.frag";
                        addShader(m_renderer, &shaderLoadDesc, shader);
                        return true;
                    });

                    std::array samplers = { m_pointSampler.m_handle };
                    std::array samplerName = { (const char*)"inputSampler" };
                    RootSignatureDesc rootDesc = { &m_finalShader.m_handle, 1 };
                    rootDesc.ppStaticSamplers = samplers.data();
                    rootDesc.ppStaticSamplerNames = samplerName.data();
                    rootDesc.mStaticSamplerCount = 1;
                    addRootSignature(m_renderer, &rootDesc, &m_finalRootSignature);

                    DescriptorSetDesc setDesc = { m_finalRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 1 };
                    for (auto& desc : m_finalPerFrameDescriptorSet) {
                        desc.Load(m_renderer, [&](DescriptorSet** handle) {
                            addDescriptorSet(m_renderer, &setDesc, handle);
                            return true;
                        });
                    }
                    m_finalPipeline.Load(m_renderer, [&](Pipeline** pipeline) {
                        DepthStateDesc depthStateDisabledDesc = {};
                        depthStateDisabledDesc.mDepthWrite = false;
                        depthStateDisabledDesc.mDepthTest = false;

                        RasterizerStateDesc rasterStateNoneDesc = {};
                        rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

                        PipelineDesc pipelineDesc = {};
                        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                        GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
                        graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                        graphicsPipelineDesc.pShaderProgram = m_finalShader.m_handle;
                        graphicsPipelineDesc.pRootSignature = m_finalRootSignature;
                        graphicsPipelineDesc.mRenderTargetCount = 1;
                        graphicsPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
                        graphicsPipelineDesc.pVertexLayout = NULL;
                        graphicsPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
                        graphicsPipelineDesc.pDepthState = &depthStateDisabledDesc;
                        graphicsPipelineDesc.pBlendState = NULL;

                        graphicsPipelineDesc.pColorFormats = &m_swapChain.m_handle->ppRenderTargets[0]->mFormat;
                        graphicsPipelineDesc.mSampleCount = m_swapChain.m_handle->ppRenderTargets[0]->mSampleCount;
                        graphicsPipelineDesc.mSampleQuality = m_swapChain.m_handle->ppRenderTargets[0]->mSampleQuality;
                        addPipeline(m_renderer, &pipelineDesc, pipeline);
                        return true;
                    });
                }
                {
                    ShaderLoadDesc postProcessCopyShaderDec = {};
                    postProcessCopyShaderDec.mStages[0].pFileName = "fullscreen.vert";
                    postProcessCopyShaderDec.mStages[1].pFileName = "post_processing_copy.frag";
                    addShader(m_renderer, &postProcessCopyShaderDec, &m_copyShader);

                    RootSignatureDesc rootDesc = { &m_copyShader, 1 };
                    addRootSignature(m_renderer, &rootDesc, &m_copyPostProcessingRootSignature);
                    DescriptorSetDesc setDesc = { m_copyPostProcessingRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, MaxCopyFrames };
                    addDescriptorSet(m_renderer, &setDesc, &m_copyPostProcessingDescriptorSet[0]);
                    addDescriptorSet(m_renderer, &setDesc, &m_copyPostProcessingDescriptorSet[1]);

                    DepthStateDesc depthStateDisabledDesc = {};
                    depthStateDisabledDesc.mDepthWrite = false;
                    depthStateDisabledDesc.mDepthTest = false;

                    RasterizerStateDesc rasterStateNoneDesc = {};
                    rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

                    PipelineDesc pipelineDesc = {};
                    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                    GraphicsPipelineDesc& copyPipelineDesc = pipelineDesc.mGraphicsDesc;
                    copyPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                    copyPipelineDesc.pShaderProgram = m_copyShader;
                    copyPipelineDesc.pRootSignature = m_copyPostProcessingRootSignature;
                    copyPipelineDesc.mRenderTargetCount = 1;
                    copyPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
                    copyPipelineDesc.pVertexLayout = NULL;
                    copyPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
                    copyPipelineDesc.pDepthState = &depthStateDisabledDesc;
                    copyPipelineDesc.pBlendState = NULL;

                    {
                        TinyImageFormat format = TinyImageFormat_R8G8B8A8_UNORM;
                        copyPipelineDesc.pColorFormats = &format;
                        copyPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
                        copyPipelineDesc.mSampleQuality = m_swapChain.m_handle->ppRenderTargets[0]->mSampleQuality;
                        addPipeline(m_renderer, &pipelineDesc, &m_copyPostProcessingPipelineToUnormR8G8B8A8);
                    }

                    {
                        copyPipelineDesc.pColorFormats = &m_swapChain.m_handle->ppRenderTargets[0]->mFormat;
                        copyPipelineDesc.mSampleCount = m_swapChain.m_handle->ppRenderTargets[0]->mSampleCount;
                        copyPipelineDesc.mSampleQuality = m_swapChain.m_handle->ppRenderTargets[0]->mSampleQuality;
                        addPipeline(m_renderer, &pipelineDesc, &m_copyPostProcessingPipelineToSwapChain);
                    }
                }

                m_windowEventHandler.Connect(window->NativeWindowEvent());
                IncrementFrame();
    }

    Sampler* ForgeRenderer::resolve(SamplerPoolKey key) {
        ASSERT(key.m_id < SamplerPoolKey::NumOfVariants);
        auto& sampler = m_samplerPool[key.m_id];
        if (!sampler) {
            auto renderer = Interface<ForgeRenderer>::Get()->Rend();
            SamplerDesc samplerDesc = {};
            samplerDesc.mAddressU = key.m_field.m_addressMode;
            samplerDesc.mAddressV = key.m_field.m_addressMode;
            samplerDesc.mAddressW = key.m_field.m_addressMode;
            addSampler(renderer, &samplerDesc, &sampler);
        }
        return sampler;
    }

    ForgeRenderer::ForgeRenderer():
        m_windowEventHandler(BroadcastEvent::OnPostBufferSwap, [&](window::WindowEventPayload& event) {
            switch (event.m_type) {
            case window::WindowEventType::ResizeWindowEvent: {
                    waitQueueIdle(m_graphicsQueue);
                    const auto windowSize = m_window->GetWindowSize();
                    m_swapChain.Load(m_renderer, [&](SwapChain** handle) {
                        SwapChainDesc swapChainDesc = {};
                        swapChainDesc.mWindowHandle = m_window->ForgeWindowHandle();
                        swapChainDesc.mPresentQueueCount = 1;
                        swapChainDesc.ppPresentQueues = &m_graphicsQueue;
                        swapChainDesc.mWidth = windowSize.x;
                        swapChainDesc.mHeight = windowSize.y;
                        swapChainDesc.mImageCount = m_swapChainCount;
                        swapChainDesc.mColorFormat = TinyImageFormat_R8G8B8A8_UNORM;//getRecommendedSwapchainFormat(false, false);
		                swapChainDesc.mColorSpace = COLOR_SPACE_SDR_LINEAR;
                        swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
                        swapChainDesc.mEnableVsync = true;
                        addSwapChain(m_renderer, &swapChainDesc, handle);
                        return true;
                    });
                    //removeSemaphore(m_renderer, m_imageAcquiredSemaphore);
                    //addSemaphore(m_renderer, &m_imageAcquiredSemaphore);
                    for(auto& rt: m_finalRenderTarget) {
                        rt.Load(m_renderer,[&](RenderTarget** target) {
                            RenderTargetDesc renderTarget = {};
                            renderTarget.mArraySize = 1;
                            renderTarget.mClearValue.depth = 1.0f;
                            renderTarget.mDepth = 1;
                            renderTarget.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
                            renderTarget.mWidth = windowSize.x;
                            renderTarget.mHeight = windowSize.y;
                            renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                            renderTarget.mSampleCount = SAMPLE_COUNT_1;
                            renderTarget.mSampleQuality = 0;
                            renderTarget.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                            renderTarget.pName = "final RT";
                            addRenderTarget(m_renderer, &renderTarget, target);
                            return true;
                        });
                    }
                break;
            }
            default:
                break;
            }
        }){
    }


    void ForgeRenderer::cmdCopyTexture(Cmd* cmd, Texture* srcTexture, RenderTarget* dstTexture) {
        ASSERT(srcTexture !=  nullptr);
        ASSERT(dstTexture !=  nullptr);
        m_copyRegionDescriptorIndex = (m_copyRegionDescriptorIndex + 1) % MaxCopyFrames;

        std::array params = {
           DescriptorData { .pName = "inputMap", .ppTextures = &srcTexture}
        };

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { dstTexture, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, static_cast<float>(dstTexture->mWidth), static_cast<float>(dstTexture->mHeight), 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, dstTexture->mWidth, dstTexture->mHeight);

        updateDescriptorSet(m_renderer, m_copyRegionDescriptorIndex, m_copyPostProcessingDescriptorSet[m_frame.index()], params.size(), params.data());
        auto swapChainFormat = TinyImageFormat_R8G8B8A8_UNORM;//getSupportedSwapchainFormat(m_renderer, false, false);
        switch(dstTexture->mFormat) {
            case TinyImageFormat_R8G8B8A8_UNORM:
                cmdBindPipeline(cmd, m_copyPostProcessingPipelineToUnormR8G8B8A8);
                break;
            default:
                ASSERT(false && "Unsupported format");
                break;
        }

        cmdBindDescriptorSet(cmd, m_copyRegionDescriptorIndex, m_copyPostProcessingDescriptorSet[m_frame.index()]);
        cmdDraw(cmd, 3, 0);
    }

}; // namespace hpl
