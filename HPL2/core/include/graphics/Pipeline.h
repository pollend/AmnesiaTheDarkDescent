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
    class HPLPipeline;


    //TODO: rename this
    class HPLPipeline final {
        HPL_RTTI_CLASS(HPLPipeline, "{66526c65-ad10-4a59-af06-103f34d1cb57}")
    public:
        HPLPipeline() = default;

        void InitializeRenderer(window::NativeWindowWrapper* window);

        static constexpr uint32_t SwapChainLength = 2; // double buffered

        /**
        * tracks the resources used by a single command buffer
        */
        class CommandResourcePool {
        public:
            // could be done better ...
            CommandResourcePool() = default;

            void AddMaterial(cMaterial& material, std::span<eMaterialTexture> textures);
            void ResetPool();
        private:
            absl::InlinedVector<std::variant<ForgeTextureHandle, ForgeBufferHandle>, 1024> m_cmds;
        };

        struct Frame {
            HPLPipeline* m_pipeline = nullptr;
            size_t m_frameIndex = 0;
            size_t m_swapChainIndex = 0;
            CmdPool* m_cmdPool = nullptr;
            CommandResourcePool* m_resourcePool = nullptr;
        };


        // increment frame index and swap chain index
        void IncrementFrame() {
            acquireNextImage(m_renderer, m_swapChain, m_imageAcquiredSemaphore, nullptr, &m_swapChainIndex);
        }
        
        Renderer* Rend() { return m_renderer; }
        RootSignature* PipelineSignature() { return m_pipelineSignature; }
        
        size_t SwapChainIndex() { return m_swapChainIndex; }
        size_t CurrentFrame() { return m_currentFrameIndex; }
        // size_t FrameIndex()  { return (m_currentFrameIndex % SwapChainLength); }
        inline CommandResourcePool& ResourcePool(size_t index) { return m_commandPool[index]; }
        inline CmdPool* GetCmdPool(size_t index) { return m_cmdPools[index]; }
        
    private:
        std::array<CommandResourcePool, SwapChainLength> m_commandPool;
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