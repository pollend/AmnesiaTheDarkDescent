#include "bgfx/bgfx.h"
#include <cstdint>
#include <graphics/VertexBufferDrawRequest.h>

namespace hpl
{
    struct SharedPointerLocker
    {
        std::shared_ptr<Buffer> buffer;
    };
    
    VertexStream::VertexStream(VertexStream&& stream)
        : m_startVertex(stream.m_startVertex)
        , m_numVertices(stream.m_numVertices)
        , m_definition(stream.m_definition)
        , m_dynamicHandle(stream.m_dynamicHandle)
        , m_handle(stream.m_handle)
    {
        stream.m_handle = BGFX_INVALID_HANDLE;
        stream.m_dynamicHandle = BGFX_INVALID_HANDLE;
    }

    void VertexStream::operator=(VertexStream&& stream)
    {
        m_startVertex = stream.m_startVertex;
        m_numVertices = stream.m_numVertices;
        m_definition = stream.m_definition;
        m_dynamicHandle = stream.m_dynamicHandle;
        m_handle = stream.m_handle;
        stream.m_handle = BGFX_INVALID_HANDLE;
        stream.m_dynamicHandle = BGFX_INVALID_HANDLE;
    }

    VertexStream::VertexStream(
        const VertexStreamDefinition& definition, std::weak_ptr<Buffer> buffer, size_t startVertex, size_t numVertices)
        : m_startVertex(startVertex)
        , m_numVertices(numVertices)
        , m_definition(definition)
    {
        auto view = buffer.lock();
        auto* locker = new SharedPointerLocker{ view };
        switch (m_definition.m_accessType)
        {
        case AccessType::AccessStatic:
            {
                m_handle = bgfx::createVertexBuffer(
                    bgfx::makeRef(
                        view->GetBuffer().data(),
                        view->NumberBytes(),
                        [](void* _ptr, void* _userData)
                        {
                            delete static_cast<SharedPointerLocker*>(_userData);
                        },
                        locker),
                    view->GetDefinition().m_layout);
            }
            break;
        case AccessType::AccessDynamic:
        case AccessType::AccessStream:
            {
                m_dynamicHandle = bgfx::createDynamicVertexBuffer(
                    bgfx::makeRef(
                        view->GetBuffer().data(),
                        view->NumberBytes(),
                        [](void* _ptr, void* _userData)
                        {
                            delete static_cast<SharedPointerLocker*>(_userData);
                        },
                        locker),
                    view->GetDefinition().m_layout);
            }
            break;
        default:
            delete locker;
            break;
        }

        Update(view, startVertex, numVertices);
    }

    VertexStream::~VertexStream()
    {
        if (bgfx::isValid(m_handle))
        {
            bgfx::destroy(m_handle);
        }
        if (bgfx::isValid(m_dynamicHandle))
        {
            bgfx::destroy(m_dynamicHandle);
        }
    }

    void VertexStream::SetRange(size_t startVertex, size_t numVertices)
    {
        m_startVertex = startVertex;
        m_numVertices = numVertices;
    }

    void VertexStream::Update(std::weak_ptr<Buffer> view, size_t startVertex, size_t numVertices)
    {
        if (numVertices == 0)
        {
            return;
        }
        switch (m_definition.m_accessType)
        {
        case AccessType::AccessStatic:
            BX_ASSERT(false, "Static vertex buffer cannot be updated");
            break;
        case AccessType::AccessDynamic:
        case AccessType::AccessStream:
            {
                auto* locker = new SharedPointerLocker{ view.lock() };
                auto buffer = view.lock();
                bgfx::update(
                    m_dynamicHandle,
                    startVertex,
                    bgfx::makeRef(
                        buffer->GetBuffer().data(),
                        buffer->Stride() * numVertices,
                        [](void* _ptr, void* _userData)
                        {
                            delete static_cast<SharedPointerLocker*>(_userData);
                        },
                        locker));
            }
            break;
        }
    }
    IndexStream::IndexStream(IndexStream&& stream): 
        m_startIndex(stream.m_startIndex)
        , m_numIndices(stream.m_numIndices)
        , m_definition(stream.m_definition)
        , m_dynamicHandle(stream.m_dynamicHandle)
        , m_handle(stream.m_handle)
    {
        stream.m_handle = BGFX_INVALID_HANDLE;
        stream.m_dynamicHandle = BGFX_INVALID_HANDLE;
    }

    void IndexStream::operator=(IndexStream &&stream) {
        m_startIndex = stream.m_startIndex;
        m_numIndices = stream.m_numIndices;
        m_definition = stream.m_definition;
        m_dynamicHandle = stream.m_dynamicHandle;
        m_handle = stream.m_handle;
        stream.m_dynamicHandle = BGFX_INVALID_HANDLE;
        stream.m_handle = BGFX_INVALID_HANDLE;
    }

    IndexStream::IndexStream(const IndexStreamDefinition& definition, std::weak_ptr<Buffer> buffer, size_t startIndex, size_t numIndices)
        : m_startIndex(startIndex)
        , m_numIndices(numIndices)
        , m_definition(definition)
    {
        auto view = buffer.lock();
        const uint16_t flags = ([&]() {
            switch(m_definition.m_format) {
                case BufferDefinition::FormatType::Uint16:
                    return 0;
                case BufferDefinition::FormatType::Uint32:
                    return BGFX_BUFFER_INDEX32;
                default:
                    BX_ASSERT(false, "Unknown index buffer format");
                    return 0;
            }
        })();

        switch (m_definition.m_accessType)
        {
        case AccessType::AccessStatic:
            {
                m_handle = bgfx::createIndexBuffer(
                    bgfx::makeRef(
                        view->GetBuffer().data(),
                        view->NumberBytes(),
                        [](void* _ptr, void* _userData)
                        {
                            delete static_cast<SharedPointerLocker*>(_userData);
                        },
                        new SharedPointerLocker{ view }),
                    flags);
            }
            break;
        case AccessType::AccessDynamic:
        case AccessType::AccessStream:
            {
                m_dynamicHandle = bgfx::createDynamicIndexBuffer(
                    bgfx::makeRef(
                        view->GetBuffer().data(),
                        view->NumberBytes(),
                        [](void* _ptr, void* _userData)
                        {
                            delete static_cast<SharedPointerLocker*>(_userData);
                        },
                        new SharedPointerLocker{ view }),
                    flags);
            }
            break;
        default:
            break;
        }
    }

    void IndexStream::Update(std::weak_ptr<Buffer> view, size_t startIndex, size_t numIndices)
    {
        if (numIndices == 0)
        {
            return;
        }
        switch (m_definition.m_accessType)
        {
        case AccessType::AccessStatic:
            BX_ASSERT(false, "Static vertex buffer cannot be updated");
            break;
        case AccessType::AccessDynamic:
        case AccessType::AccessStream:
            {
                auto* locker = new SharedPointerLocker{ view.lock() };
                auto buffer = view.lock();
                bgfx::update(
                    m_dynamicHandle,
                    startIndex,
                    bgfx::makeRef(
                        buffer->GetBuffer().data(),
                        buffer->Stride() * numIndices,
                        [](void* _ptr, void* _userData)
                        {
                            delete static_cast<SharedPointerLocker*>(_userData);
                        },
                        locker));
            }
            break;
        }
    }

} // namespace hpl