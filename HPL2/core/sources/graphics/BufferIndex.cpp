#include "absl/types/span.h"
#include <bgfx/bgfx.h>
#include <bx/debug.h>
#include <graphics/BufferIndex.h>

namespace hpl
{
    size_t BufferIndex::GetIndexSize(IndexFormatType format)
    {
        switch (format)
        {
        case IndexFormatType::Uint16:
            return sizeof(uint16_t);
        case IndexFormatType::Uint32:
            return sizeof(uint32_t);
        default:
            break;
        }
        BX_ASSERT(false, "Invalid index format")
        return 0;
    }

    BufferIndex::BufferIndex(const BufferIndexDefinition& defintion)
        : _definition(defintion)
    {
    }

    BufferIndex::BufferIndex(const BufferIndexDefinition& defintion, std::vector<char>&& data)
        : _definition(defintion)
        , _buffer(std::move(data))
    {
    }

    BufferIndex::BufferIndex(const BufferIndexDefinition& defintion, std::vector<char>& data)
        : _definition(defintion)
        , _buffer(data)
    {
    }

    BufferIndex::~BufferIndex()
    {
        Invalidate();
    }

    void BufferIndex::operator=(BufferIndex&& other)
    {
        _definition = other._definition;
        _buffer = std::move(other._buffer);
    }

    void BufferIndex::Resize(size_t count)
    {
        _buffer.resize(count * GetIndexSize(_definition.m_format));
    }

    void BufferIndex::Reserve(size_t count)
    {
        _buffer.reserve(count * GetIndexSize(_definition.m_format));
    }

    absl::Span<char> BufferIndex::GetBuffer()
    {
        return absl::MakeSpan(_buffer.data(), _buffer.size());
    }

    absl::Span<const char> BufferIndex::GetConstBuffer() const
    {
        return absl::MakeSpan(_buffer.data(), _buffer.size());
    }

    absl::Span<uint16_t> BufferIndex::GetBufferUint16()
    {
        return absl::MakeSpan(reinterpret_cast<uint16_t*>(_buffer.data()), _buffer.size() / sizeof(uint16_t));
    }

    absl::Span<const uint16_t> BufferIndex::GetConstBufferUint16() const
    {
        return absl::MakeSpan(reinterpret_cast<const uint16_t*>(_buffer.data()), _buffer.size() / sizeof(uint16_t));
    }

    absl::Span<uint32_t> BufferIndex::GetBufferUint32()
    {
        return absl::MakeSpan(reinterpret_cast<uint32_t*>(_buffer.data()), _buffer.size() / sizeof(uint32_t));
    }

    absl::Span<const uint32_t> BufferIndex::GetConstBufferUint32() const
    {
        return absl::MakeSpan(reinterpret_cast<const uint32_t*>(_buffer.data()), _buffer.size() / sizeof(uint32_t));
    }

    void BufferIndex::Append(uint32_t value)
    {
        size_t indexSize = GetIndexSize(_definition.m_format);
        char data[4] = { 0 };
        switch (_definition.m_format)
        {
        case IndexFormatType::Uint16:
            *reinterpret_cast<uint16_t*>(data) = value;
            break;
        case IndexFormatType::Uint32:
            *reinterpret_cast<uint32_t*>(data) = value;
            break;
        }
        _buffer.insert(_buffer.end(), std::begin(data), std::begin(data) + indexSize);
    }

    void BufferIndex::AppendRange(absl::Span<const uint32_t> value)
    {
        for (size_t i = 0; i < value.size(); ++i)
        {
            Append(value[i]);
        }
    }

    void BufferIndex::SetRange(size_t offset, absl::Span<const uint32_t> data)
    {
        BX_ASSERT(offset + data.size() <= NumberElements(), "Index out of range");
        switch (_definition.m_format)
        {
        case IndexFormatType::Uint16:
            for (size_t i = 0; i < data.size(); ++i)
            {
                GetBufferUint16()[offset + i] = data[i];
            }
            break;
        case IndexFormatType::Uint32:
            for (size_t i = 0; i < data.size(); ++i)
            {
                GetBufferUint32()[offset + i] = data[i];
            }
            break;
        default:
            break;
        }
    }

    void BufferIndex::SetIndex(size_t index, uint32_t value)
    {
        size_t indexSize = GetIndexSize(_definition.m_format);
        BX_ASSERT(index < (_buffer.size() / indexSize), "Index out of range");
        switch (_definition.m_format)
        {
        case IndexFormatType::Uint16:
            *reinterpret_cast<uint16_t*>(_buffer.data() + index * indexSize) = value;
            break;
        case IndexFormatType::Uint32:
            *reinterpret_cast<uint32_t*>(_buffer.data() + index * indexSize) = value;
            break;
        default:
            break;
        }
    }

    void BufferIndex::Initialize()
    {
        if (!bgfx::isValid(_handle))
        {
            _handle = bgfx::createIndexBuffer(
                bgfx::makeRef(_buffer.data(), _buffer.size()),
                (_definition.m_format == BufferIndex::IndexFormatType::Uint16 ? 0 : BGFX_BUFFER_INDEX32));
        }
    }

    size_t BufferIndex::NumberElements()
    {
        return _buffer.size() / GetIndexSize(_definition.m_format);
    }

    void BufferIndex::Invalidate()
    {
        if (bgfx::isValid(_handle))
        {
            bgfx::destroy(_handle);
            _handle.idx = bgfx::kInvalidHandle;
        }
    }
    BufferIndexView::BufferIndexView()
        : m_indexBuffer(nullptr)
        , m_offset(0)
        , m_count(0)
    {
    }

    BufferIndexView::BufferIndexView(BufferIndex* buffer)
        : m_indexBuffer(buffer)
        , m_offset(0)
        , m_count(buffer ? buffer->NumberElements() : 0)
    {
    }

    BufferIndexView::BufferIndexView(BufferIndex* buffer, size_t offset, size_t size)
        : m_indexBuffer(buffer)
        , m_offset(offset)
        , m_count(size)
    {
        if (m_indexBuffer)
        {
            BX_ASSERT(m_offset + m_count <= buffer->NumberElements(), "View is Larger then IndexBuffer");
        }
        else
        {
            BX_ASSERT(m_offset == 0 && m_count == 0, "Invalid offset and size");
        }
    }

    void BufferIndexView::SetRange(size_t offset, absl::Span<const uint32_t> data)
    {
        BX_ASSERT(m_indexBuffer, "IndexBuffer is not valid")
        BX_ASSERT(!IsEmpty(), "IndexBuffer is empty");
        BX_ASSERT(offset + data.size() <= m_count, "Index out of range");
        m_indexBuffer->SetRange(m_offset + offset, data);
    }

    void BufferIndexView::SetIndex(size_t index, uint32_t value)
    {
        BX_ASSERT(m_indexBuffer, "IndexBuffer is not valid");
        BX_ASSERT(!IsEmpty(), "IndexBuffer is empty");
        BX_ASSERT(index < m_count, "Index out of range");
        m_indexBuffer->SetIndex(m_offset + index, value);
    }

    absl::Span<char> BufferIndexView::GetBuffer()
    {
        if (IsEmpty())
        {
            return {};
        }
        BX_ASSERT(m_indexBuffer, "IndexBuffer is not valid")
        size_t elementSize = m_indexBuffer->GetIndexSize(m_indexBuffer->GetDefinition().m_format);
        return absl::MakeSpan(m_indexBuffer->GetBuffer().data() + m_offset * elementSize, m_count * elementSize);
    }

    absl::Span<const char> BufferIndexView::GetConstBuffer() const
    {
        if (IsEmpty())
        {
            return {};
        }
        BX_ASSERT(m_indexBuffer, "IndexBuffer is not valid")
        size_t elementSize = m_indexBuffer->GetIndexSize(m_indexBuffer->GetDefinition().m_format);
        return absl::MakeSpan(m_indexBuffer->GetConstBuffer().data() + m_offset * elementSize, m_count * elementSize);
    }

    size_t BufferIndexView::GetNumElements() const
    {
        return m_count;
    }

    absl::Span<uint16_t> BufferIndexView::GetBufferUint16()
    {
        if (IsEmpty())
        {
            return {};
        }
        return m_indexBuffer->GetBufferUint16().subspan(m_offset, m_count);
    }

    absl::Span<const uint16_t> BufferIndexView::GetConstBufferUint16() const
    {
        if (IsEmpty())
        {
            return {};
        }
        return m_indexBuffer->GetConstBufferUint16().subspan(m_offset, m_count);
    }

    absl::Span<uint32_t> BufferIndexView::GetBufferUint32()
    {
        if (IsEmpty())
        {
            return {};
        }
        return m_indexBuffer->GetBufferUint32().subspan(m_offset, m_count);
    }

    absl::Span<const uint32_t> BufferIndexView::GetConstBufferUint32() const
    {
        if (IsEmpty())
        {
            return {};
        }
        return m_indexBuffer->GetConstBufferUint32().subspan(m_offset, m_count);
    }

} // namespace hpl