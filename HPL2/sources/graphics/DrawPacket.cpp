#include "graphics/DrawPacket.h"
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/Enum.h"
#include "graphics/GeometrySet.h"
#include "impl/LegacyVertexBuffer.h"

namespace hpl {

    void DrawPacket::cmdBindBuffers(Cmd* cmd, ForgeRenderer::CommandResourcePool* resourcePool, DrawPacket* packet, std::span<eVertexBufferElement> elements) {
        folly::small_vector<Buffer*, 16> vbBuffer;
        folly::small_vector<uint64_t, 16> vbOffsets;
        folly::small_vector<uint32_t, 16> vbStride;
        switch (packet->m_type) {
        case DrawPacketType::DrawIndvidualBuffers:
            {
                for(auto& element: elements) {
                    for (size_t i = 0; i < packet->m_indvidual.m_numStreams; i++) {
                        auto& stream = packet->m_indvidual.m_vertexStream[i];
                        if(stream.m_element == element) {
                            resourcePool->Push(*stream.m_buffer);
                            vbBuffer.push_back(stream.m_buffer->m_handle);
                            vbOffsets.push_back(stream.m_offset);
                            vbStride.push_back(stream.m_stride);
                        }
                    }
                }
                cmdBindVertexBuffer(cmd, elements.size(), vbBuffer.data(), vbStride.data(), vbOffsets.data());
                cmdBindIndexBuffer(cmd, packet->m_indvidual.m_indexStream.buffer->m_handle, INDEX_TYPE_UINT32, packet->m_indvidual.m_indexStream.m_offset);
                break;
            }
        case DrawPacketType::DrawGeometryset: {
            for(size_t i = 0; i < elements.size(); i++) {
                ShaderSemantic semantic = hplToForgeShaderSemantic(elements[i]);
                auto stream = packet->m_unified.m_subAllocation->getStreamBySemantic(semantic);
                vbBuffer.push_back(stream->buffer().m_handle);
                vbOffsets.push_back((packet->m_unified.m_vertexOffset + packet->m_unified.m_subAllocation->vertextOffset()) * stream->stride());
                vbStride.push_back(stream->stride());
            }
            cmdBindVertexBuffer(cmd, elements.size(), vbBuffer.data(), vbStride.data(), vbOffsets.data());
            cmdBindIndexBuffer(cmd, packet->m_unified.m_subAllocation->indexBuffer().m_handle, INDEX_TYPE_UINT32,
                               (packet->m_unified.m_subAllocation->indexOffset() + packet->m_unified.m_indexOffset) * GeometrySet::IndexBufferStride );
            break;
        }
        default:
            ASSERT(false);
            break;
        }

    }
}


