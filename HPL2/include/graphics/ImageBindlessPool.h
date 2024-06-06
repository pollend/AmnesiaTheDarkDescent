#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/IndexPool.h"

#include <cstdint>
#include <vector>

namespace hpl {
    class Image;
    class TextureDescriptorPool;
    class ImageBindlessPool {
    public:
        static uint32_t constexpr MinimumFrameCount = ForgeRenderer::SwapChainLength * 4;

        struct ImageReserveEntry {
            uint16_t m_prev = UINT16_MAX;
            uint16_t m_next = UINT16_MAX;
            uint32_t m_handle = IndexPool::InvalidHandle;
            uint32_t m_frameCount = 0;
            Image* m_image = nullptr;
        };
        ImageBindlessPool();
        ImageBindlessPool(TextureDescriptorPool* pool, uint32_t reserveSize);
        ~ImageBindlessPool();
        void reset(const ForgeRenderer::Frame& frame); // reset and prepare for the next frame
        uint32_t request(Image* image);

    private:

        TextureDescriptorPool* m_texturePool = nullptr;
        std::vector<ImageReserveEntry> m_pool;
        uint32_t m_allocSize = 0;

        uint16_t m_headIndex = UINT16_MAX;
        uint16_t m_tailIndex = UINT16_MAX;

        uint32_t m_currentFrame = 0;
    };
} // namespace hpl
