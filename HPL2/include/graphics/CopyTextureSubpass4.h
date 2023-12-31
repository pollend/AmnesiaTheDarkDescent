#pragma once

#include "graphics/ForgeRenderer.h"
#include "graphics/ForgeHandles.h"

namespace hpl {

    class CopyTextureSubpass4 {
    public:

        static constexpr uint32_t NumberOfDescriptors = 32;
        CopyTextureSubpass4(CopyTextureSubpass4&&) = default;
        CopyTextureSubpass4(CopyTextureSubpass4&) = delete;
        CopyTextureSubpass4(hpl::ForgeRenderer& renderer);
        void operator=(CopyTextureSubpass4&) = delete;
        CopyTextureSubpass4& operator=(CopyTextureSubpass4&&) = default;

        void Dispatch(ForgeRenderer::Frame& frame, Texture* src, Texture* dest);
    private:
        SharedRootSignature m_copyRootSignature;
        std::array<SharedDescriptorSet, ForgeRenderer::SwapChainLength> m_copyPerFrameSet;
        SharedPipeline m_copyPipline;
        SharedShader m_copyShader;
        std::array<uint32_t, ForgeRenderer::SwapChainLength> m_index;
    };
} // namespace hpl
