#pragma once

#include <bgfx/bgfx.h>
#include <absl/types/span.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <bx/debug.h>

namespace hpl {

    class BufferIndex {
    public:
        enum class IndexFormatType {
            Uint16,
            Uint32
        };
        
        enum class AccessType {
            AccessStatic,
            AccessDynamic,
            AccessStream
        };
        static size_t GetIndexSize(IndexFormatType format);

        struct BufferIndexDefinition {
            IndexFormatType m_format = IndexFormatType::Uint16;
            AccessType m_accessType = AccessType::AccessStatic;
        };

        explicit BufferIndex(const BufferIndexDefinition& defintion);
        explicit BufferIndex(const BufferIndexDefinition& defintion, std::vector<char>&& data);
        explicit BufferIndex(const BufferIndexDefinition& defintion, std::vector<char>& data);
        explicit BufferIndex() {}
        ~BufferIndex();

        void Resize(size_t count);
        void Reserve(size_t count);

        void Append(uint32_t value);
        void AppendRange(absl::Span<const uint32_t> value);
        void SetRange(size_t offset, absl::Span<const uint32_t> data);
        void SetIndex(size_t index, uint32_t value);
        
        size_t NumberElements();
        absl::Span<char> GetBuffer();
        absl::Span<const char> GetConstBuffer() const;

        absl::Span<uint16_t> GetBufferUint16();
        absl::Span<const uint16_t> GetConstBufferUint16() const;

        absl::Span<uint32_t> GetBufferUint32();
        absl::Span<const uint32_t> GetConstBufferUint32() const;

        const BufferIndexDefinition GetDefinition() const { return _definition; }

        bgfx::IndexBufferHandle GetHandle() const { return _handle; }
        bgfx::DynamicIndexBufferHandle GetDynamicHandle() const { return _dynamicHandle; }

        void Update();
        void Initialize();
        void Invalidate();

        void operator=(BufferIndex&& other);
    private:
        BufferIndexDefinition _definition;
        std::vector<char> _buffer;
        bgfx::IndexBufferHandle _handle = BGFX_INVALID_HANDLE;
        bgfx::DynamicIndexBufferHandle _dynamicHandle  = BGFX_INVALID_HANDLE;
    };
    
    class BufferIndexView {
        public:
            BufferIndexView(BufferIndex* buffer);
            BufferIndexView(BufferIndex* buffer, size_t offset, size_t size);
            BufferIndexView();
            
            absl::Span<char> GetBuffer();
            absl::Span<const char> GetConstBuffer() const;

            absl::Span<uint16_t> GetBufferUint16();
            absl::Span<const uint16_t> GetConstBufferUint16() const;

            absl::Span<uint32_t> GetBufferUint32();
            absl::Span<const uint32_t> GetConstBufferUint32() const;

            const bool IsEmpty() const {
                return m_count == 0;
            }

            size_t GetNumElements() const;

            void SetRange(size_t offset, absl::Span<const uint32_t> data);
            void SetIndex(size_t index, uint32_t value);

            BufferIndex& GetIndexBuffer() {
                BX_ASSERT(m_indexBuffer, "Index buffer is null")
                return *m_indexBuffer;
            }

            const BufferIndex& GetIndexBuffer() const {
                BX_ASSERT(m_indexBuffer, "Index buffer is null")
                return *m_indexBuffer;
            }
            
            size_t Offset() const {
                return m_offset;
            }

            size_t Count() const {
                return m_count;
            }

        private:
            BufferIndex* m_indexBuffer;
            size_t m_offset = 0;
            size_t m_count = 0;
    };
}