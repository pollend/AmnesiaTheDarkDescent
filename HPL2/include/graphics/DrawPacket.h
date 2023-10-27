#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include <cstdint>

namespace hpl {
    class DrawPacket {
    public:
        static constexpr uint32_t MaxVertexBindings = 15;
        enum DrawPacketType {
            Unknown,
            IndvidualBindings, // everything will be bound indvidually for the moment until
            UnifiedAllocatorBinding
        };
        struct VertexStream {
            SharedBuffer* m_buffer;
            uint64_t m_offset;
            uint32_t m_stride;
        };
        struct IndexStream {
            SharedBuffer* buffer;
            uint64_t m_offset;
        };

        static void cmdBindBuffers(Cmd* cmd, ForgeRenderer::CommandResourcePool* resourcePool, DrawPacket* packet);

        DrawPacket()
            : m_type(DrawPacketType::Unknown) {
        }

        DrawPacketType m_type;
        uint32_t m_numIndices;
        union {
            struct {
                uint32_t m_numStreams;
                VertexStream m_vertexStream[MaxVertexBindings];
                IndexStream m_indexStream;
            } m_indvidual;
            struct {

            } m_unified;
        };
    };

} // namespace hpl
