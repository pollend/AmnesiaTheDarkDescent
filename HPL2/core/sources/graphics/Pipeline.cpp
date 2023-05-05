#include <graphics/Pipeline.h>
#include "engine/Interface.h"
#include "graphics/Material.h"
#include "windowing/NativeWindow.h"

namespace hpl {

    void ForgeRenderer::CommandResourcePool::AddMaterial(cMaterial& material, std::span<eMaterialTexture> textures) {
        for(auto& tex: textures) {
            auto image = material.GetImage(tex);
            if(image) {
                // auto texture = image->GetTexture();
                // if(texture) {
                //     // m_cmds.push_back(texture);
                // }
            }
        }
    }


    void ForgeRenderer::CommandResourcePool::AddTexture(ForgeTextureHandle texture) {
        m_cmds.push_back(texture);
    }

    void ForgeRenderer::CommandResourcePool::ResetPool() {
        m_cmds.clear();
    }

    void ForgeRenderer::IncrementFrame() {
        // Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
        FenceStatus fenceStatus;
        auto& completeFence = m_renderCompleteFences[CurrentFrameIndex()];
        getFenceStatus(m_renderer, completeFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE) {
            waitForFences(m_renderer, 1, &completeFence);
        }
        acquireNextImage(m_renderer, m_swapChain, m_imageAcquiredSemaphore, nullptr, &m_swapChainIndex);
        m_currentFrameIndex++;

        auto frame = GetFrame();

        frame.m_resourcePool->ResetPool();
        resetCmdPool(m_renderer, frame.m_cmdPool);

        beginCmd(m_cmds[CurrentFrameIndex()]);

        auto&   swapChainImage = frame.m_swapChain->ppRenderTargets[frame.m_swapChainIndex];

        RenderTargetBarrier rtBarriers[] = {
            { swapChainImage, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, 1, rtBarriers);
    }

    void ForgeRenderer::SubmitFrame() {
        auto frame = GetFrame();
        cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
        auto&   swapChainImage = frame.m_swapChain->ppRenderTargets[frame.m_swapChainIndex];
    
        RenderTargetBarrier rtBarriers[] = {
            { swapChainImage, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT },
        };
        cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, 1, rtBarriers);

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
        initRenderer("test", &desc, &m_renderer);

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(m_renderer, &queueDesc, &m_graphicsQueue);

        for(size_t i =0; i < m_cmds.size(); i++) {
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
		swapChainDesc.mWidth = 1920;
		swapChainDesc.mHeight = 1080;
		swapChainDesc.mImageCount = SwapChainLength;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mColorClearValue = { {1, 1, 1, 1} };
		swapChainDesc.mEnableVsync = false;
        addSwapChain(m_renderer, &swapChainDesc, &m_swapChain);
       
		RootSignatureDesc graphRootDesc = {};
		// graphRootDesc.mShaderCount = 1;
		// graphRootDesc.ppShaders = &pGraphShader;
		addRootSignature(m_renderer, &graphRootDesc, &m_pipelineSignature);


		addSemaphore(m_renderer, &m_imageAcquiredSemaphore);
        for(auto& completeSem: m_renderCompleteSemaphores) {
            addSemaphore(m_renderer, &completeSem);
        }
        for(auto& completeFence: m_renderCompleteFences) {
            addFence(m_renderer, &completeFence);
        }


    }
};