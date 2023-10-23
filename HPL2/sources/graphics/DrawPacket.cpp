#include "graphics/DrawPacket.h"
#include "impl/LegacyVertexBuffer.h"

namespace hpl {

    void DrawPacket::cmdBindBuffers(Cmd* cmd, ForgeRenderer::CommandResourcePool* resourcePool, DrawPacket* packet) {
        folly::small_vector<Buffer*, 16> vbBuffer;
        folly::small_vector<uint64_t, 16> vbOffsets;
        folly::small_vector<uint32_t, 16> vbStride;
        switch(packet->m_type) {
            case DrawPacketType::IndvidualBindings: {
                for(size_t i = 0; i < packet->m_indvidual.m_numStreams; i++) {
                    auto& stream = packet->m_indvidual.m_vertexStream[i];
                    resourcePool->Push(*stream.m_buffer);
                    vbBuffer.push_back(stream.m_buffer->m_handle);
                    vbOffsets.push_back(stream.m_offset);
                    vbStride.push_back(stream.m_stride);
                }
                break;
            }
            default:
                ASSERT(false);
                break;
        }

        cmdBindVertexBuffer(cmd, packet->m_indvidual.m_numStreams, vbBuffer.data(), vbStride.data(), vbOffsets.data());
        cmdBindIndexBuffer(cmd, packet->m_indvidual.m_indexStream.buffer->m_handle, INDEX_TYPE_UINT32, packet->m_indvidual.m_indexStream.m_offset);

    }
}


