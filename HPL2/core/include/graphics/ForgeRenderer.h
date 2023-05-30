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

#include <engine/RTTI.h>

#include "graphics/ForgeHandles.h"
#include "windowing/NativeWindow.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "FixPreprocessor.h"

namespace hpl {
    class cMaterial;
    class ForgeRenderer;

    struct HPL2Blend {
        BlendMode mode;
        BlendConstant src;
        BlendConstant dst;
    };

    // helper to convert from eMaterialBlendMode to The-Forge's BlendMode
    static std::array<HPL2Blend, eMaterialBlendMode_LastEnum> HPL2BlendTable = ([]() {
        std::array<HPL2Blend, eMaterialBlendMode_LastEnum> result{};
        result[eMaterialBlendMode_Add] = {BM_ADD, BC_ONE, BC_ONE};
        result[eMaterialBlendMode_Mul] = {BM_ADD, BC_ZERO, BC_SRC_COLOR};
        result[eMaterialBlendMode_MulX2] = {BM_ADD, BC_DST_COLOR, BC_SRC_COLOR};
        result[eMaterialBlendMode_Alpha] = {BM_ADD, BC_SRC_ALPHA, BC_ONE_MINUS_SRC_ALPHA};
        result[eMaterialBlendMode_PremulAlpha] = {BM_ADD, BC_ONE, BC_ONE_MINUS_SRC_ALPHA};
        return result;
    })();

    class ForgeRenderer final {
        HPL_RTTI_CLASS(ForgeRenderer, "{66526c65-ad10-4a59-af06-103f34d1cb57}")
    public:
        ForgeRenderer() = default;

        enum CopyPipelines: uint8_t {
            CopyPipelineToSwapChain = 0,
            CopyPipelineToUnormR8G8B8A8 = 1,
            CopyPipelineCount = 2
        };

        static constexpr uint32_t MaxCopyFrames = 32;
        static constexpr uint32_t SwapChainLength = 2; // double buffered
        static constexpr uint32_t ResourcePoolSize = 4; // double buffered

        void InitializeRenderer(window::NativeWindowWrapper* window);
        void InitializeResource();

        struct SamplerPoolKey {
            union {
                uint8_t m_id;
                struct {
                    AddressMode m_addressMode : 2;
                } m_field;
            };
            static constexpr size_t NumOfVariants = 4;
        };
        Sampler* resolve(SamplerPoolKey key);


        /**
        * tracks the resources used by a single command buffer
        */
        class CommandResourcePool {
        public:
            using VariantTypes = std::variant<ForgeTextureHandle, ForgeBufferHandle, ForgeRenderTarget>;
            // could be done better ...
            CommandResourcePool() = default;

            template<class T>
            inline void Push(const T& handle) {
                if(handle.IsValid()) {
                    m_cmds.emplace_back(VariantTypes{handle});
                }
            }

            inline void ResetPool() {
                m_cmds.clear();
            }
        private:
            std::vector<VariantTypes> m_cmds;
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
            frame.m_resourcePool = &m_resourcePool[CurrentFrameIndex()];
            return frame;
        }
        // void BeginFrame() {}

        // increment frame index and swap chain index
        void IncrementFrame();
        void SubmitFrame();
        inline Renderer* Rend() {
            ASSERT(m_renderer);
            return m_renderer;
        }
        RootSignature* PipelineSignature() { return m_pipelineSignature; }

        size_t SwapChainIndex() { return m_swapChainIndex; }
        size_t CurrentFrameIndex() { return m_currentFrameCount % SwapChainLength; }
        size_t FrameCount() { return m_currentFrameCount; }
        inline SwapChain* GetSwapChain() { return m_swapChain; }

        void cmdCopyTexture(Cmd* cmd, Texture* srcTexture, RenderTarget* dstTexture);

    private:
        std::array<Sampler*, SamplerPoolKey::NumOfVariants> m_samplerPool;
        std::array<CommandResourcePool, SwapChainLength> m_resourcePool;
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

        Shader* m_copyShader = nullptr;
        Pipeline* m_copyPostProcessingPipelineToSwapChain = nullptr;
        Pipeline* m_copyPostProcessingPipelineToUnormR8G8B8A8 = nullptr;

        RootSignature* m_copyPostProcessingRootSignature = nullptr;
        DescriptorSet* m_copyPostProcessingDescriptorSet = nullptr;
        uint32_t m_copyRegionDescriptorIndex = 0;

        uint32_t m_currentFrameCount = 0;
        uint32_t m_swapChainIndex = 0;
        // uint32_t m_resourcePoolIndex = 0;
    };

} // namespace hpl
