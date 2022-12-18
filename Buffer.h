
#pragma once

#include "bgfx/bgfx.h"
#include "math/BoundingVolume.h"
#include "math/MathTypes.h"
#include <absl/types/span.h>
#include <algorithm>
#include <bx/debug.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace hpl
{

    struct BufferDefinition
    {
        enum class FormatType
        {
            Unknown,
            Uint16,
            Uint32
        };

        static BufferDefinition CreateUint16IndexBuffer(size_t elements);
        static BufferDefinition CreateUint32IndexBuffer(size_t elements);

        // static BufferDefinition CreateStructureBuffer(FormatType format, size_t elements);
        static BufferDefinition CreateStructureBuffer(bgfx::VertexLayout layout, size_t elements);

        FormatType m_format = FormatType::Unknown;
        // Only used for structure buffers
        bgfx::VertexLayout m_layout;

        size_t m_elementCount;
    };

    class Buffer
    {
    public:
        explicit Buffer(const BufferDefinition& definition)
            : _definition(definition)
        {
            m_data.resize(Stride() * NumberElements());
        }
        Buffer(const Buffer& other) = delete;
        Buffer(Buffer&& other);

        explicit Buffer()
            : m_data()
            , _definition({})
        {
        }

        ~Buffer();

        void operator=(const Buffer& other) = delete;
        void operator=(Buffer&& other) {
            m_data = std::move(other.m_data);
            _definition = other._definition;
        }

        size_t Stride() const;
        size_t NumberElements() const
        {
            return IsEmpty() ? 0 : m_data.size() / Stride();
        }
        size_t NumberBytes() const
        {
            return IsEmpty() ? 0 : m_data.size();
        }

        const bool IsEmpty() const
        {
            return m_data.empty();
        }

        template<typename TData>
        void SetElement(size_t offset, const TData& data)
        {
            // BX_ASSERT(offset < m_count, "Index is out of range");
            BX_ASSERT(sizeof(TData) == Stride(), "Data must be same size as stride");
            GetElements<TData>()[offset] = data;
        }

        template<typename TData>
        void SetElementRange(size_t offset, absl::Span<const TData> input)
        {
            if (input.empty())
            {
                return;
            }
            // BX_ASSERT(offset + input.size() <= m_count, "Index is out of range");
            BX_ASSERT(sizeof(TData) == Stride(), "Data must be same size as stride");
            auto target = GetElements<TData>().subspan(offset, input.size());

            auto inputIt = input.begin();
            auto targetIt = target.begin();
            for (; inputIt != input.end() && targetIt != target.end(); ++inputIt, ++targetIt)
            {
                (*targetIt) = (*inputIt);
            }
        }

        template<typename TData>
        absl::Span<TData> GetElements()
        {
            if (IsEmpty())
            {
                return {};
            }
            BX_ASSERT(sizeof(TData) == Stride(), "Data must be same size as stride");
            return absl::MakeSpan(reinterpret_cast<TData*>(m_data.data()), NumberElements());
        }

        template<typename TData>
        absl::Span<const TData> GetConstElements() const
        {
            if (IsEmpty())
            {
                return {};
            }
            BX_ASSERT(sizeof(TData) == Stride(), "Data must be same size as stride");
            return absl::MakeConstSpan(reinterpret_cast<const TData*>(m_data.data()), NumberElements());
        }

        void Resize(size_t count);
        void Reserve(size_t count);

        bool HasAttribute(bgfx::Attrib::Enum attribute);
        void GetAttribute(float output[4], size_t offset, bgfx::Attrib::Enum attribute);
        void SetAttribute(const float data[4], size_t offset, bool normalized, bgfx::Attrib::Enum attribute);

        absl::Span<char> GetBuffer();
        absl::Span<const char> GetBuffer() const;
        const BufferDefinition& GetDefinition() const
        {
            return _definition;
        }

    private:
        std::vector<char> m_data;
        BufferDefinition _definition;
    };

} // namespace hpl