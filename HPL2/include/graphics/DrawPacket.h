#pragma once

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/GraphicsAllocator.h"
#include "graphics/GraphicsTypes.h"
#include <cstdint>

namespace hpl {
    class DrawPacket {
    public:
        static constexpr uint32_t MaxVertexBindings = 15;

        enum DrawPacketType {
            Unknown,
            DrawIndvidualBuffers, // everything will be bound indvidually for the moment until
            DrawGeometryset
        };

        struct IndvidualBindings {
            uint32_t m_numIndices;
            uint32_t m_numStreams;
            struct {
                SharedBuffer* m_buffer;
                uint64_t m_offset;
                uint32_t m_stride;
            } m_vertexStream[MaxVertexBindings];
            struct {
                SharedBuffer* buffer;
                uint64_t m_offset;
            } m_indexStream;
        };

        struct GeometrySetBinding {
            uint32_t m_numStreams;
            eVertexBufferElement m_elements[MaxVertexBindings];
            GraphicsAllocator::AllocationSet m_set;
            GeometrySet::GeometrySetSubAllocation* m_subAllocation;
            uint32_t m_numIndices;
            uint32_t m_vertexOffset;
            uint32_t m_indexOffset;
        };
        inline uint32_t numberOfIndecies() {
            switch(m_type) {
                case DrawPacketType::DrawGeometryset:
                    return m_unified.m_numIndices;
                case DrawPacketType::DrawIndvidualBuffers:
                    return m_indvidual.m_numIndices;
            }
            return 0;
        }

        static void cmdBindBuffers(Cmd* cmd, ForgeRenderer::CommandResourcePool* resourcePool, DrawPacket* packet);

        DrawPacket()
            : m_type(DrawPacketType::Unknown) {
        }
        DrawPacketType m_type;
        union {
            struct IndvidualBindings m_indvidual;
            struct GeometrySetBinding m_unified;
        };
    };

} // namespace hpl
