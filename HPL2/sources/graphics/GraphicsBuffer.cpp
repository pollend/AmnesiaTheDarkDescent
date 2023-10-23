
#include "graphics/GraphicsBuffer.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "FixPreprocessor.h"
#include <cstdint>

namespace hpl {

    GraphicsBuffer::GraphicsBuffer(const MappedBufferDesc& desc):
        m_mapped({
            desc.m_size,
            desc.m_dstOffset,
            desc.m_mappedData
        }),
        m_type(BufferType::MappedBuffer){
    }

    GraphicsBuffer::GraphicsBuffer(const GraphicsBuffer& asset):
        m_buffer(asset.m_buffer),
        m_mapped(asset.m_mapped),
        m_type(asset.m_type){
    }
    GraphicsBuffer::GraphicsBuffer(GraphicsBuffer&& asset):
        m_buffer(std::move(asset.m_buffer)),
        m_mapped(asset.m_mapped),
        m_type(asset.m_type){
    }
    std::span<uint8_t> GraphicsBuffer::Data() {
        return m_buffer;
    }

    void GraphicsBuffer::operator=(const GraphicsBuffer& other) {
        m_buffer = other.m_buffer;
        m_mapped = other.m_mapped;
        m_type = other.m_type;
    }
    void GraphicsBuffer::operator=(GraphicsBuffer&& other) {
        m_buffer = std::move(other.m_buffer);
        m_mapped = other.m_mapped;
        m_type = other.m_type;
    }

} // namespace hpl
