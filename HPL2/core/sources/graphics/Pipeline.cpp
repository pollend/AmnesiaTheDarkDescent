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