#pragma once


#include <absl/container/fixed_array.h>
#include <bgfx/bgfx.h>
#include "absl/container/inlined_vector.h"
#include "bx/allocator.h"
#include "graphics/Buffer.h"
#include "graphics/GraphicsContext.h"
#include <absl/types/span.h>

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

        explicit VertexStream(const VertexStreamDefinition& definition, const Buffer& view, size_t startVertex, size_t numVertices)
            : m_startVertex(startVertex)
            , m_numVertices(numVertices)
            , m_definition(definition)
        {
            m_layoutHandle = bgfx::createVertexLayout(view.GetDefinition().m_layout);
        }

        void SetRange(size_t startVertex, size_t numVertices) {
            m_startVertex = startVertex;
            m_numVertices = numVertices;
        }

        void Update(Buffer& view, size_t startVertex, size_t numVertices) {
            if(numVertices == 0) {
                return;
            }
            switch(m_definition.m_accessType) {
                case AccessType::AccessStatic: 
                    BX_ASSERT(false, "Static vertex buffer cannot be updated");
                break;
                case AccessType::AccessDynamic:
                case AccessType::AccessStream: {
                    bgfx::update(m_dynamicHandle, startVertex, bgfx::makeRef(view.GetBuffer().data(), view.Stride() * numVertices));
                }
                break;
            }
        }
    private:
        size_t m_startVertex;
        size_t m_numVertices;

        VertexStreamDefinition m_definition;
        bgfx::VertexBufferHandle m_handle = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
        bgfx::VertexLayoutHandle m_layoutHandle = BGFX_INVALID_HANDLE;
    };

    class IndexStream {
    public:
        struct IndexStreamDefinition {
            AccessType m_accessType = AccessType::AccessStatic;
            BufferDefinition::FormatType m_format = BufferDefinition::FormatType::Unknown;
        };

        IndexStream() = default;

        explicit IndexStream(const IndexStreamDefinition& definition) : 
            m_definition(definition) {
        }

        void SetRange(size_t startIndex, size_t numIndex) {
            m_startIndex = startIndex;
            m_numIndices = numIndex;
        }

        IndexStream(const IndexStreamDefinition& definition, const Buffer& view, size_t startIndex, size_t numIndices)
            : m_startIndex(startIndex)
            , m_numIndices(numIndices)
            , m_definition(definition)
        {
            switch(m_definition.m_accessType) {
                case AccessType::AccessStatic: {
                    switch(view.GetDefinition().m_format) {
                        case BufferDefinition::FormatType::Uint16:
                            m_handle = bgfx::createIndexBuffer(bgfx::makeRef(view.GetBuffer().data(), view.NumberBytes()), 0);
                            break;
                        case BufferDefinition::FormatType::Uint32:
                            m_handle = bgfx::createIndexBuffer(bgfx::makeRef(view.GetBuffer().data(), view.NumberBytes()), BGFX_BUFFER_INDEX32);
                            break;
                        default:
                            BX_ASSERT(false, "Unknown index buffer format");
                    }
                }
                break;
                case AccessType::AccessDynamic:
                case AccessType::AccessStream: {
                    switch(view.GetDefinition().m_format) {
                        case BufferDefinition::FormatType::Uint16:
                            m_dynamicHandle = bgfx::createDynamicIndexBuffer(bgfx::makeRef(view.GetBuffer().data(), view.NumberBytes()), 0);
                            break;
                        case BufferDefinition::FormatType::Uint32:
                            m_dynamicHandle = bgfx::createDynamicIndexBuffer(bgfx::makeRef(view.GetBuffer().data(), view.NumberBytes()), BGFX_BUFFER_INDEX32);
                            break;
                        default:
                            BX_ASSERT(false, "Unknown index buffer format");
                    }
                }
                break;
            }
         
        }

        void Update(Buffer& view, size_t startIndex, size_t numIndices) {
            switch(m_definition.m_accessType) {
                case AccessType::AccessStatic:
                    BX_ASSERT(false, "Static index buffer cannot be updated");
                    break;
                case AccessType::AccessDynamic:
                case AccessType::AccessStream:
                    bgfx::update(m_dynamicHandle, startIndex, bgfx::makeRef(view.GetBuffer().data(), view.Stride() * numIndices));
                    break;
            }
        }

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
        IndexStream* m_indexStream = nullptr;
        absl::InlinedVector<VertexStream*, 10> m_vertexStream = {};
    };

};