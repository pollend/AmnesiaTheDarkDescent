#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/offsetAllocator.h"

#include "Common_3/Utilities/RingBuffer.h"

namespace hpl {
    class GraphicsAllocator final {
    public:
        GraphicsAllocator(ForgeRenderer* renderer);

        static constexpr uint32_t SharedVertexBufferSize = 1 << 25;
        static constexpr uint32_t SharedIndexBufferSize = 1 << 23;
        static constexpr uint32_t ImmediateVertexBufferSize = 1 << 25;
        static constexpr uint32_t ImmediateIndexBufferSize = 1 << 23;

        struct OffsetAllocHandle {
        public:
            OffsetAllocHandle(OffsetAllocator::Allocator* allocator, OffsetAllocator::Allocation allocation ,SharedBuffer buffer);
            OffsetAllocHandle();
            
            ~OffsetAllocHandle();
            OffsetAllocHandle(OffsetAllocHandle&& handle);
            OffsetAllocHandle(const OffsetAllocHandle& handle) = delete;

            void operator=(const OffsetAllocHandle& handle) = delete;
            void operator=(OffsetAllocHandle&& handle);

        private:
            OffsetAllocator::Allocator* m_allocator;
            OffsetAllocator::Allocation m_allocation;
            SharedBuffer m_buffer;
        };

        OffsetAllocHandle allocVertexFromSharedBuffer(uint32_t size);
        OffsetAllocHandle  allocIndeciesFromSharedBuffer(uint32_t size);

        GPURingBufferOffset allocTransientVertexBuffer(uint32_t size);
        GPURingBufferOffset allocTransientIndexBuffer(uint32_t size);

    private:
        ForgeRenderer* m_renderer;
        GPURingBuffer* m_transientVertexBuffer = nullptr;
        GPURingBuffer* m_transientIndeciesBuffer = nullptr;

        OffsetAllocator::Allocator m_vertexBufferAllocator;
        SharedBuffer m_sharedVertexBuffer;
        OffsetAllocator::Allocator m_indexBufferAllocator;
        SharedBuffer m_sharedIndexBuffer;
    };
} // namespace hpl
