
#include "graphics/AssetBuffer.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "FixPreprocessor.h"
#include <cstdint>

namespace hpl {

    AssetBuffer::AssetBuffer(uint32_t numBytes) {
        m_buffer.resize(numBytes, 0);
    }

    AssetBuffer::AssetBuffer(const AssetBuffer& asset):
        m_buffer(asset.m_buffer){
    }
    AssetBuffer::AssetBuffer(AssetBuffer&& asset) {
    }
    std::span<uint8_t> AssetBuffer::Data() {
        return m_buffer;
    }

    void AssetBuffer::operator=(const AssetBuffer& other) {
        m_buffer = other.m_buffer;
    }
    void AssetBuffer::operator=(AssetBuffer&& other) {
        m_buffer = std::move(other.m_buffer);
    }

} // namespace hpl
