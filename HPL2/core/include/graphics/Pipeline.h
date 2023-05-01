#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <queue>
#include <span>
#include <variant>
#include <vector>

#include "absl/container/inlined_vector.h"

#include "graphics/GraphicsTypes.h"

#include <engine/RTTI.h>

#include "windowing/NativeWindow.h"
#include "graphics/ForgeHandles.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "FixPreprocessor.h"

namespace hpl {
    class cMaterial;
    class ForgeRenderer;


    //TODO: rename this
    class ForgeRenderer final {
        HPL_RTTI_CLASS(ForgeRenderer, "{66526c65-ad10-4a59-af06-103f34d1cb57}")
    public:
        ForgeRenderer() = default;

        void InitializeRenderer(window::NativeWindowWrapper* window);

        static constexpr uint32_t SwapChainLength = 2; // double buffered

        /**
        * tracks the resources used by a single command buffer
        */
        class CommandResourcePool {
        public:
            // could be done better ...
            CommandResourcePool() = default;
            void AddTexture(ForgeTextureHandle texture);
            void AddMaterial(cMaterial& material, std::span<eMaterialTexture> textures);
            void ResetPool();
        private:
            absl::InlinedVector<std::variant<ForgeTextureHandle, ForgeBufferHandle>, 1024> m_cmds;
        };

        /**
        * tracks the resources used by a single command buffer
        */
        struct Frame {
            size_t m_currentFrame = 0;
            size_t m_frameIndex = 0;
            size_t m_swapChainIndex = 0;
            ForgeRenderer* m_renderer = nullptr;
            SwapChain* m_swapChain = nullptr;
            Cmd* m_cmd = nullptr;
            CmdPool* m_cmdPool = nullptr;
            Fence* m_renderCompleteFence = nullptr;
            Semaphore* m_renderCompleteSemaphore = nullptr;
            CommandResourcePool* m_resourcePool = nullptr;
        };

        const inline Frame GetFrame() {
            Frame frame;
            frame.m_currentFrame = FrameCount();
            frame.m_frameIndex = CurrentFrameIndex();
            frame.m_swapChainIndex = SwapChainIndex();
            frame.m_renderer = this;
            frame.m_swapChain = m_swapChain;

            frame.m_cmd = m_cmds[CurrentFrameIndex()];
            frame.m_cmdPool = m_cmdPools[CurrentFrameIndex()];
            frame.m_renderCompleteFence = m_renderCompleteFences[CurrentFrameIndex()];
            frame.m_renderCompleteSemaphore = m_renderCompleteSemaphores[CurrentFrameIndex()];
            frame.m_resourcePool = &m_commandPool[CurrentFrameIndex()];
            return frame;
        }
        // void BeginFrame() {}

        // increment frame index and swap chain index
        void IncrementFrame() {
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

            resetCmdPool(m_renderer, frame.m_cmdPool);

            beginCmd(m_cmds[CurrentFrameIndex()]);
    
            auto&   swapChainImage = frame.m_swapChain->ppRenderTargets[frame.m_swapChainIndex];

            RenderTargetBarrier rtBarriers[] = {
                { swapChainImage, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
            };
            cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, 1, rtBarriers);

            

        }

        void SubmitFrame() {
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
        
        inline Renderer* Rend() { 
            ASSERT(m_renderer);
            return m_renderer; 
        }
        RootSignature* PipelineSignature() { return m_pipelineSignature; }
        
        size_t SwapChainIndex() { return m_swapChainIndex; }
        size_t CurrentFrameIndex() { return m_currentFrameIndex % SwapChainLength; }
        size_t FrameCount() { return m_currentFrameIndex; }
        inline SwapChain* GetSwapChain() { return m_swapChain; }

        // size_t FrameIndex()  { return (m_currentFrameIndex % SwapChainLength); }
        inline CommandResourcePool& ResourcePool(size_t index) { return m_commandPool[index]; }
        inline CmdPool* GetCmdPool(size_t index) { return m_cmdPools[index]; }
        
    private:
        std::array<CommandResourcePool, SwapChainLength> m_commandPool;
        std::array<Fence*, SwapChainLength> m_renderCompleteFences;
        std::array<Semaphore*, SwapChainLength> m_renderCompleteSemaphores;
        std::array<CmdPool*, SwapChainLength> m_cmdPools;
        std::array<Cmd*, SwapChainLength> m_cmds;

        Renderer* m_renderer = nullptr;
        RootSignature* m_pipelineSignature = nullptr;
        SwapChain* m_swapChain = nullptr;
        Semaphore* m_imageAcquiredSemaphore = nullptr;
        Queue* m_graphicsQueue = nullptr;
        window::NativeWindowWrapper* m_window = nullptr;

        uint32_t m_currentFrameIndex = 0;
        uint32_t m_swapChainIndex = 0;
    };
}