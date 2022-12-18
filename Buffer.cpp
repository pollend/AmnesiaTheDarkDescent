
#include <graphics/Buffer.h>

#include "absl/types/span.h"
#include "bgfx/bgfx.h"
#include "bx/readerwriter.h"
#include "math/MathTypes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <graphics/BufferIndex.h>
#include <math/BoundingVolume.h>
#include <math/Math.h>
#include <utility>
#include <vector>

namespace hpl
{

    Buffer::~Buffer()
    {
    }

    Buffer::Buffer(Buffer&& other):
        m_data(std::move(other.m_data)),
        _definition(other._definition){
    }

    absl::Span<char> Buffer::GetBuffer()
    {
        if(IsEmpty()) {
            return {};
        }
        return absl::Span<char>(reinterpret_cast<char*>(m_data.data()), m_data.size());
    }

    absl::Span<const char> Buffer::GetBuffer() const
    {
        if(IsEmpty()) {
            return {};
        }
        return absl::Span<const char>(reinterpret_cast<const char*>(m_data.data()), m_data.size());
    }


    size_t Buffer::Stride() const {
        switch(_definition.m_format) {
            case BufferDefinition::FormatType::Unknown:
                return _definition.m_layout.getStride();
            case BufferDefinition::FormatType::Uint16:
                return sizeof(uint16_t);
            case BufferDefinition::FormatType::Uint32:
                return sizeof(uint32_t);
            default:
                return 0;
        }
    }

    BufferDefinition BufferDefinition::CreateUint16IndexBuffer(size_t elementCount) {
        BufferDefinition def = {};
        def.m_elementCount = elementCount;
        def.m_format = BufferDefinition::FormatType::Uint16;
        return def;
    }

    BufferDefinition BufferDefinition::CreateUint32IndexBuffer(size_t elementCount) {
        BufferDefinition def = {};
        def.m_elementCount = elementCount;
        def.m_format = BufferDefinition::FormatType::Uint32;
        return def;
    }

    BufferDefinition BufferDefinition::CreateStructureBuffer(bgfx::VertexLayout layout, size_t elementCount) {
        BufferDefinition def = {};
        def.m_elementCount = elementCount;
        def.m_layout = layout;
        return def;
    }

    // BufferDefinition BufferDefinition::CreateStructureBuffer(FormatType format, size_t elementCount) {
    //     BufferDefinition def = {};
    //     def.m_elementCount = elementCount;
    //     def.m_format = format;
    //     return def;
    // }


    bool Buffer::HasAttribute(bgfx::Attrib::Enum attribute)
    {
        return _definition.m_layout.has(attribute);
    }

    void Buffer::GetAttribute(float output[4], size_t offset, bgfx::Attrib::Enum attribute)
    {
        // BX_ASSERT(offset < _count, "Offset is out of bounds");
        if(IsEmpty()) {
            return;
        }
        bgfx::vertexUnpack(output, attribute, _definition.m_layout, m_data.data(), offset);
    }

    void Buffer::SetAttribute(const float data[4], size_t offset, bool normalized, bgfx::Attrib::Enum attribute)
    {
        // BX_ASSERT(offset < _count, "Offset is out of bounds");
        if(IsEmpty()) {
            return;
        }
        bgfx::vertexPack(data, normalized, attribute, _definition.m_layout, m_data.data(), offset);
    }


} // namespace hpl