#pragma  once

#include "absl/types/span.h"
#include "bgfx/bgfx.h"
#include "math/MathTypes.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace hpl {

class Vector4Trait {
public:
    static constexpr uint8_t Num = 4;
    static constexpr bgfx::AttribType::Enum Type = bgfx::AttribType::Float;
    struct Vec4 {
        float x;
        float y;
        float z;
        float w;
    };
    using Base = Vec4; 
};

class Vector3Trait {
public:
    static constexpr uint8_t Num = 3;
    static constexpr bgfx::AttribType::Enum Type = bgfx::AttribType::Float;
    struct Vec3 {
        float x;
        float y;
        float z;
    };
    using Base = Vec3; 
};

class Vector2Trait {
public:
    static constexpr uint8_t Num = 2;
    static constexpr bgfx::AttribType::Enum Type = bgfx::AttribType::Float;
    struct Vec2 {
        float x;
        float y;
    };
    using Base = Vec2;
};

class IndexU32Trait {
public:
    using Base = uint32_t;
    static constexpr BufferDefinition::FormatType FormatType = BufferDefinition::FormatType::Uint32;
};

class IndexU16Trait {
public:
    using Base = uint16_t;
    static constexpr BufferDefinition::FormatType FormatType = BufferDefinition::FormatType::Uint16;
};

template<typename T>
class IndexElementWrapper {
public:
    IndexElementWrapper(std::weak_ptr<Buffer> buffer, std::weak_ptr<IndexStream> stream): 
        m_buffer(buffer), 
        m_stream(stream) {
        // BX_ASSERT(m_buffer->GetDefinition().m_format == m_stream->, "Buffer and stream must have the same stride");
    }

    IndexElementWrapper(size_t elements, AccessType accessType): m_buffer(){
        auto definition = {
            .m_format = T::FormatType,
            .m_elementCount = elements
        };
        m_buffer = std::shared_ptr<Buffer>(new Buffer(definition));
        m_stream = std::shared_ptr<IndexStream>(new IndexStream(IndexStream::IndexStreamDefinition {
            .m_accessType = accessType,
            .m_format  = T::FormatType
        }, m_buffer,0, elements));
    }

    IndexElementWrapper(const IndexElementWrapper& other) = delete;
    IndexElementWrapper(IndexElementWrapper&& other): 
        m_buffer(std::move(other.m_buffer)), 
        m_stream(std::move(other.m_stream)) {
    }
    IndexElementWrapper(): 
        m_buffer(), 
        m_stream(){
    }
    ~IndexElementWrapper() {
    }

    size_t NumberElements() const {
        return m_buffer->NumberElements();
    }

    absl::Span<typename T::Base> GetElements() {
        return m_buffer->GetElements<typename T::Base>();
    }

    void operator=(const IndexElementWrapper& other) = delete;
    void operator=(IndexElementWrapper&& other) {
        m_buffer = std::move(other.m_buffer);
        m_stream = std::move(other.m_stream);
    }
    
    void Update(size_t startVertex, size_t numVertices) {
        m_stream->Update(m_buffer, startVertex, numVertices);
    }

    std::weak_ptr<Buffer> getBuffer() {
        return m_buffer;
    }

    std::weak_ptr<IndexStream> getStream() {
        return m_stream;
    }
    

private:
    std::shared_ptr<Buffer> m_buffer;
    std::shared_ptr<IndexStream> m_stream;
};

template<typename T>
class VertexElementWrapper {
public:

    VertexElementWrapper(std::weak_ptr<Buffer> buffer, std::weak_ptr<VertexStream> stream): 
        m_buffer(buffer), 
        m_stream(stream) {
        BX_ASSERT(m_buffer->GetDefinition().m_layout.getStride() == m_stream->GetDefinition().m_layout.getStride(), "Buffer and stream must have the same stride");
    }

    VertexElementWrapper(size_t elements, bgfx::Attrib::Enum attr, AccessType accessType): m_buffer(){
        bgfx::VertexLayout layout;
        layout.begin()
            .add( attr, T::Num, T::Type)
            .end();
        auto definition = BufferDefinition::CreateStructureBuffer(layout, elements);
        m_buffer = std::shared_ptr<Buffer>(new Buffer(definition));
        m_stream = std::shared_ptr<VertexStream>(new VertexStream(VertexStream::VertexStreamDefinition {
            .m_accessType = accessType,
            .m_layout = layout
        }, m_buffer,0, elements));
    }


    VertexElementWrapper(const VertexElementWrapper& other) = delete;
    VertexElementWrapper(VertexElementWrapper&& other): 
        m_buffer(std::move(other.m_buffer)), 
        m_stream(std::move(other.m_stream)) {
    }
    VertexElementWrapper(): 
        m_buffer(), 
        m_stream(){
    }
    ~VertexElementWrapper() {

    }

    size_t NumberElements() {
        return m_buffer->NumberElements();
    }

    absl::Span<typename T::Base> GetElements() {
        return m_buffer->GetElements<typename T::Base>();
    }

    void operator=(const VertexElementWrapper& other) = delete;
    void operator=(VertexElementWrapper&& other) {
        m_buffer = std::move(other.m_buffer);
        m_stream = std::move(other.m_stream);
    }

    void Update(size_t startVertex, size_t numVertices) {
        m_stream->Update(m_buffer, startVertex, numVertices);
    }

    std::weak_ptr<Buffer> getBuffer() {
        return m_buffer;
    }

    std::weak_ptr<VertexStream> getStream() {
        return m_stream;
    }

private:
    std::shared_ptr<Buffer> m_buffer;
    std::shared_ptr<VertexStream> m_stream;
};

}