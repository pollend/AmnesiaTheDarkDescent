#pragma once


#include <absl/container/fixed_array.h>
#include <bgfx/bgfx.h>
#include "absl/container/inlined_vector.h"
#include "bx/allocator.h"
#include "graphics/Buffer.h"
#include "graphics/GraphicsContext.h"
#include <absl/types/span.h>
#include <memory>

namespace hpl {

    enum class AccessType {
        AccessStatic,
        AccessDynamic,
        AccessStream
    };

    class VertexStream {
    public:
        struct VertexStreamDefinition {
            AccessType m_accessType = AccessType::AccessStatic;
            bgfx::VertexLayout m_layout;
        };

        VertexStream() = default;
        VertexStream(VertexStream&& stream);
        VertexStream(const VertexStream& stream) = delete;

        void operator=(const VertexStream& stream) = delete;
        void operator=(VertexStream&& stream);

        ~VertexStream();
        explicit VertexStream(const VertexStreamDefinition& definition, std::weak_ptr<Buffer> view, size_t startVertex, size_t numVertices);

        void SetRange(size_t startVertex, size_t numVertices);
        void Update(std::weak_ptr<Buffer> view, size_t startVertex, size_t numVertices);

        VertexStreamDefinition& GetDefinition() {
            return m_definition;
        }
    private:
        size_t m_startVertex;
        size_t m_numVertices;

        VertexStreamDefinition m_definition;
        bgfx::VertexBufferHandle m_handle = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
    };

    class IndexStream {
    public:
        struct IndexStreamDefinition {
            AccessType m_accessType = AccessType::AccessStatic;
            BufferDefinition::FormatType m_format = BufferDefinition::FormatType::Unknown;
        };

        IndexStream() = default;

        IndexStream(const IndexStreamDefinition& definition, std::weak_ptr<Buffer> buffer, size_t startIndex, size_t numIndices);
        IndexStream(const VertexStream& stream) = delete;
        IndexStream(IndexStream&& stream);

        void operator=(const IndexStream& stream) = delete;
        void operator=(IndexStream&& stream);

        void SetRange(size_t startIndex, size_t numIndex) {
            m_startIndex = startIndex;
            m_numIndices = numIndex;
        }

        void Update(std::weak_ptr<Buffer> view, size_t startIndex, size_t numIndices);

        bgfx::IndexBufferHandle GetHandle() {
            return m_handle;
        }

        bgfx::DynamicIndexBufferHandle GetDynamicHandle() {
            return m_dynamicHandle;
        }

    private:
        size_t m_startIndex;
        size_t m_numIndices;

        bgfx::IndexBufferHandle m_handle = BGFX_INVALID_HANDLE;
        bgfx::DynamicIndexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
        IndexStreamDefinition m_definition;
    };

    struct LayoutStream {
        std::weak_ptr<IndexStream> m_indexStream = {};

        absl::InlinedVector<std::weak_ptr<VertexStream>, 10> m_vertexStream = {};
    };

};