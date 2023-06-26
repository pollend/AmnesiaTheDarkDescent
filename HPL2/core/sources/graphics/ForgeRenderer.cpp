#include <graphics/ForgeRenderer.h>

#include "engine/IUpdateEventLoop.h"
#include "engine/Interface.h"
#include "graphics/Material.h"
#include "windowing/NativeWindow.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"

#ifdef HPL2_RENDERDOC_ENABLED
#include <dlfcn.h>
#include "renderdoc_app.h"
#endif

namespace hpl {


    static RENDERDOC_API_1_1_2 *rdoc_api = NULL;
    void ForgeRenderer::IncrementFrame() {
        // Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
        // m_resourcePoolIndex = (m_resourcePoolIndex + 1) % ResourcePoolSize;
        m_currentFrameCount++;
        auto frame = GetFrame();

        FenceStatus fenceStatus;
        auto& completeFence = frame.m_renderCompleteFence;
        getFenceStatus(m_renderer, completeFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE) {
            waitForFences(m_renderer, 1, &completeFence);
        }
        acquireNextImage(m_renderer, m_swapChain, m_imageAcquiredSemaphore, nullptr, &m_swapChainIndex);

        resetCmdPool(m_renderer, frame.m_cmdPool);
        frame.m_resourcePool->ResetPool();
        beginCmd(frame.m_cmd);

        auto& swapChainImage = frame.m_swapChain->ppRenderTargets[m_swapChainIndex];
        std::array rtBarriers = {
            RenderTargetBarrier { swapChainImage, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
    }

    void ForgeRenderer::SubmitFrame() {
        auto frame = GetFrame();
        cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
        auto& swapChainImage = frame.m_swapChain->ppRenderTargets[frame.m_swapChainIndex];

        std::array rtBarriers = {
            RenderTargetBarrier{ swapChainImage, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT },
        };
        cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
        endCmd(m_cmds[CurrentFrameIndex()]);

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = 1;
        submitDesc.ppCmds = &frame.m_cmd;
        submitDesc.ppSignalSemaphores = &frame.m_renderCompleteSemaphore;
        submitDesc.ppWaitSemaphores = &m_imageAcquiredSemaphore;
        submitDesc.pSignalFence = frame.m_renderCompleteFence;
        queueSubmit(m_graphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = m_swapChainIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = m_swapChain;
        presentDesc.ppWaitSemaphores = &frame.m_renderCompleteSemaphore;
        presentDesc.mSubmitDone = true;
        queuePresent(m_graphicsQueue, &presentDesc);
    }

    void ForgeRenderer::InitializeRenderer(window::NativeWindowWrapper* window) {
        SyncToken token = {};
        RendererDesc desc{};
        #ifdef HPL2_RENDERDOC_ENABLED
            if(void* mod = dlopen("./librenderdoc.so", RTLD_NOW))
            {
                pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
                int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0 , (void **)&rdoc_api);
                assert(ret == 1);
            }
        #endif

        initRenderer("test", &desc, &m_renderer);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(m_renderer, &queueDesc, &m_graphicsQueue);

        for (size_t i = 0; i < m_cmds.size(); i++) {
            CmdPoolDesc cmdPoolDesc = {};
            cmdPoolDesc.pQueue = m_graphicsQueue;
            addCmdPool(m_renderer, &cmdPoolDesc, &m_cmdPools[i]);
            CmdDesc cmdDesc = {};
            cmdDesc.pPool = m_cmdPools[i];
            addCmd(m_renderer, &cmdDesc, &m_cmds[i]);
        }

        const auto windowSize = window->GetWindowSize();
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = window->ForgeWindowHandle();
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &m_graphicsQueue;
        swapChainDesc.mWidth = windowSize.x;
        swapChainDesc.mHeight = windowSize.y;
        swapChainDesc.mImageCount = SwapChainLength;
        swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(false, false);
        swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
        swapChainDesc.mEnableVsync = false;
        addSwapChain(m_renderer, &swapChainDesc, &m_swapChain);

        RootSignatureDesc graphRootDesc = {};
        addRootSignature(m_renderer, &graphRootDesc, &m_pipelineSignature);

        addSemaphore(m_renderer, &m_imageAcquiredSemaphore);
        for (auto& completeSem : m_renderCompleteSemaphores) {
            addSemaphore(m_renderer, &completeSem);
        }
        for (auto& completeFence : m_renderCompleteFences) {
            addFence(m_renderer, &completeFence);
        }

        {
            ShaderLoadDesc postProcessCopyShaderDec = {};
            postProcessCopyShaderDec.mStages[0].pFileName = "fullscreen.vert";
            postProcessCopyShaderDec.mStages[1].pFileName = "post_processing_copy.frag";
            addShader(m_renderer, &postProcessCopyShaderDec, &m_copyShader);

            RootSignatureDesc rootDesc = { &m_copyShader, 1 };
            addRootSignature(m_renderer, &rootDesc, &m_copyPostProcessingRootSignature);
            DescriptorSetDesc setDesc = { m_copyPostProcessingRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, MaxCopyFrames };
            addDescriptorSet(m_renderer, &setDesc, &m_copyPostProcessingDescriptorSet);

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
                copyPipelineDesc.mSampleQuality = m_swapChain->ppRenderTargets[0]->mSampleQuality;
                addPipeline(m_renderer, &pipelineDesc, &m_copyPostProcessingPipelineToUnormR8G8B8A8);
            }

            {
                copyPipelineDesc.pColorFormats = &m_swapChain->ppRenderTargets[0]->mFormat;
                copyPipelineDesc.mSampleCount = m_swapChain->ppRenderTargets[0]->mSampleCount;
                copyPipelineDesc.mSampleQuality = m_swapChain->ppRenderTargets[0]->mSampleQuality;
                addPipeline(m_renderer, &pipelineDesc, &m_copyPostProcessingPipelineToSwapChain );
            }
        }
        m_window = window;
        m_windowEventHandler.Connect(window->NativeWindowEvent());
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
                    removeSwapChain(m_renderer, m_swapChain);

                    const auto windowSize = m_window->GetWindowSize();
                    SwapChainDesc swapChainDesc = {};
                    swapChainDesc.mWindowHandle = m_window->ForgeWindowHandle();
                    swapChainDesc.mPresentQueueCount = 1;
                    swapChainDesc.ppPresentQueues = &m_graphicsQueue;
                    swapChainDesc.mWidth = windowSize.x;
                    swapChainDesc.mHeight = windowSize.y;
                    swapChainDesc.mImageCount = SwapChainLength;
                    swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(false, false);
                    swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
                    swapChainDesc.mEnableVsync = false;
                    addSwapChain(m_renderer, &swapChainDesc, &m_swapChain);
                break;
            }
            default:
                break;
            }
        }){
    }

    void ForgeRenderer::InitializeResource() {
    }

    void ForgeRenderer::cmdCopyTexture(Cmd* cmd, Texture* srcTexture, RenderTarget* dstTexture) {
        ASSERT(srcTexture !=  nullptr);
        ASSERT(dstTexture !=  nullptr);

        std::array<DescriptorData, 1> params = {};
        params[0].pName = "inputMap";
        params[0].ppTextures = &srcTexture;

        LoadActionsDesc loadActions = {};
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
        loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

        cmdBindRenderTargets(cmd, 1, &dstTexture, NULL, &loadActions, NULL, NULL, -1, -1);
        cmdSetViewport(cmd, 0.0f, 0.0f, static_cast<float>(dstTexture->mWidth), static_cast<float>(dstTexture->mHeight), 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, dstTexture->mWidth, dstTexture->mHeight);

        updateDescriptorSet(m_renderer, m_copyRegionDescriptorIndex, m_copyPostProcessingDescriptorSet, params.size(), params.data());
        auto swapChainFormat = getRecommendedSwapchainFormat(false, false);
        if(dstTexture->mFormat  == swapChainFormat) {
            cmdBindPipeline(cmd, m_copyPostProcessingPipelineToSwapChain);
        } else {
            switch(dstTexture->mFormat) {
                case TinyImageFormat_R8G8B8A8_UNORM:
                    cmdBindPipeline(cmd, m_copyPostProcessingPipelineToUnormR8G8B8A8);
                    break;
                default:
                    ASSERT(false && "Unsupported format");
                    break;
            }
        }

        cmdBindDescriptorSet(cmd, m_copyRegionDescriptorIndex, m_copyPostProcessingDescriptorSet);
        cmdDraw(cmd, 3, 0);
        m_copyRegionDescriptorIndex = (m_copyRegionDescriptorIndex + 1) % MaxCopyFrames;
    }

}; // namespace hpl
