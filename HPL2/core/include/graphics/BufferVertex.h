
#pragma once

#include "bgfx/bgfx.h"
#include <absl/types/span.h>
#include <algorithm>
#include <graphics/BufferIndex.h>
#include "graphics/GraphicsContext.h"
#include "math/BoundingVolume.h"
#include "math/MathTypes.h"
#include <cstddef>
#include <functional>
#include <vector>
#include <bx/debug.h>

namespace hpl
{
    
    class BufferVertex
    {
    public:
        enum class AccessType {
            AccessStatic,
            AccessDynamic,
            AccessStream
        };

        struct VertexDefinition
        {
            AccessType m_accessType = AccessType::AccessStatic;
            bgfx::VertexLayout m_layout;
        };

        explicit BufferVertex(const VertexDefinition& defintion);
        explicit BufferVertex(const VertexDefinition& defintion, std::vector<char>&& data);
        explicit BufferVertex(const VertexDefinition& defintion, std::vector<char>& data);
        explicit BufferVertex(BufferVertex&&);
        explicit BufferVertex() {}
        ~BufferVertex();

        void Initialize();
        void Invalidate();
        void operator=(BufferVertex&& other);

        const VertexDefinition& Definition() const;
        size_t NumberElements() const;
        size_t NumberBytes() const;
        size_t Stride() const;

        void Update();
        void Update(size_t offset, size_t size);
        void Resize(size_t count);
        void Reserve(size_t count);
        
        absl::Span<char> GetBuffer();
        absl::Span<const char> GetConstBuffer();
        
        template<typename TData>
        absl::Span<TData> GetElements()
        {
            BX_ASSERT(sizeof(TData) == _definition.m_layout.getStride(), "Data must be same size as stride");
            return absl::MakeSpan(reinterpret_cast<TData*>(_buffer.data()), _buffer.size() / sizeof(TData));
        }

        template<typename TData>
        absl::Span<const TData> GetConstElements() const
        {
            BX_ASSERT(sizeof(TData) == _definition.m_layout.getStride(), "Data must be same size as stride");
            return absl::MakeSpan(reinterpret_cast<TData*>(_buffer.data()), _buffer.size() / sizeof(TData));
        }

        template<typename TData>
        void Append(const TData& data)
        {
            BX_ASSERT(sizeof(TData) == _definition.m_layout.getStride(), "Data must be same size as stride");
            _buffer.insert(_buffer.end(), reinterpret_cast<const char*>(&data), reinterpret_cast<const char*>(&data) + sizeof(TData));
        }

        template<typename TData>
        void AppendRange(absl::Span<const TData&> data)
        {
            BX_ASSERT(sizeof(TData) == _definition.m_layout.getStride(), "Data must be same size as stride");
            _buffer.insert(_buffer.end(), reinterpret_cast<const char*>(&data.begin()), reinterpret_cast<const char*>(&data.end()));
        }

        template<typename TData>
        void SetElement(size_t offset, const TData& data)
        {
            BX_ASSERT(sizeof(TData) == _definition.m_layout.getStride(), "Data must be same size as stride");
            GetElements<TData>()[offset] = data;
        }

        template<typename TData>
        void SetElementRange(size_t offset, absl::Span<const TData> data)
        {
            BX_ASSERT(sizeof(TData) == _definition.m_layout.getStride(), "Data must be same size as stride");
            GetElements<TData>().subspan(offset, data.size()) = data;
        }

        const VertexDefinition GetDefinition() const { return _definition; }

        bgfx::VertexBufferHandle GetHandle() const { return _handle; }
        bgfx::DynamicVertexBufferHandle GetDynamicHandle() const { return _dynamicHandle; }

    private:
        VertexDefinition _definition;
        std::vector<char> _buffer;

        bgfx::VertexBufferHandle _handle = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle _dynamicHandle = BGFX_INVALID_HANDLE;
    };

    class BufferVertexView 
    {
        public:
            BufferVertexView(BufferVertex* buffer, size_t offset, size_t count)
                : _vertexBuffer(buffer)
                , _offset(offset)
                , _count(count)
            {
                if (_vertexBuffer)
                {
                    BX_ASSERT(offset + count <= buffer->NumberElements(), "Offset + count is larger then buffer size");
                }
                else
                {
                    BX_ASSERT(_offset == 0 && _count == 0, "Invalid offset and size");
                }
            }

            BufferVertexView(BufferVertex* buffer)
                : _vertexBuffer(buffer)
                , _offset(0)
                , _count(buffer ? buffer->NumberElements() : 0)
            {
            }

            BufferVertexView(): 
                _vertexBuffer(nullptr),
                _offset(0),
                _count(0) {

            }

            const bool IsEmpty() const {
                return _count == 0;
            }

            template<typename TData>
            void SetElement(size_t offset, const TData& data)
            {
                BX_ASSERT(_vertexBuffer, "View is Invalid");
                BX_ASSERT(offset < _count, "Index is out of range");
                _vertexBuffer->SetElement(_offset + offset, data);
            }

            template<typename TData>
            void SetElementRange(size_t offset, absl::Span<const TData> data)
            {
                if(data.empty()) {
                    return;
                }
                BX_ASSERT(_vertexBuffer, "View is Invalid");
                BX_ASSERT(offset + data.size() <= _count, "Index is out of range");
                _vertexBuffer->SetElementRange(_offset + offset, data);
            }

            template<typename TData>
            absl::Span<TData> GetElements()
            {
                if(IsEmpty()) {
                    return {};
                }
                return _vertexBuffer->GetElements<TData>().subspan(_offset, _count);
            }

            template<typename TData>
            absl::Span<TData> GetConstElements() const
            {
                if(IsEmpty()) {
                    return {};
                }
                return _vertexBuffer->GetConstElements<TData>().subspan(_offset, _count);
            }
            
            size_t Stride() const {
                if(IsEmpty()) {
                    return 0;
                } 
                return _vertexBuffer->Stride(); 
            }
            size_t NumberElements() const { return _count; }
            size_t NumberBytes() const { return _count * Stride(); }

            void Transform(const cMatrixf& mtx);
            void UpdateTangentsFromPositionTexCoords(const BufferIndexView& index);
            cBoundingVolume GetBoundedVolume();
            void Update();

            absl::Span<char> GetBuffer();
            absl::Span<const char> GetConstBuffer();

            BufferVertex& GetVertexBuffer() { return *_vertexBuffer; }
            const BufferVertex& GetVertexBuffer() const { return *_vertexBuffer; }

             size_t Offset() const{
                return _offset;
            }

            size_t Count() const{
                return _count;
            }

        private:
            BufferVertex* _vertexBuffer;
            size_t _offset;
            size_t _count;
    };

} // namespace hpl