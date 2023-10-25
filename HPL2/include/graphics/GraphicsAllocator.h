#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/offsetAllocator.h"

#include "Common_3/Utilities/RingBuffer.h"

namespace hpl {
    class GraphicsAllocator final {
    public:
        GraphicsAllocator();

        static constexpr uint32_t SharedVertexBufferSize = 1 << 25;
        static constexpr uint32_t SharedIndexBufferSize = 1 << 23;

        struct OffsetAllocHandle {
        public:
        private:
            OffsetAllocator::Allocator* m_allocator;
            SharedBuffer m_buffer;
            uint32_t m_offset;
            uint32_t m_size;
        };

        void allocTransientVertexBuffer(uint32_t size);
        void allocTransientIndexBuffer(uint32_t size);

    private:
        GPURingBuffer* m_transientVertexBuffer = nullptr;
        GPURingBuffer* m_transientIndeciesBuffer = nullptr;

        OffsetAllocator::Allocator m_vertexBufferAllocator;
        SharedBuffer m_sharedVertexBuffer;
        OffsetAllocator::Allocator m_indexBufferAllocator;
        SharedBuffer m_sharedIndexBuffer;
    };
} // namespace hpl
